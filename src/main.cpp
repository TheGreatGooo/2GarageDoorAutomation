#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char monitor_name[40] = "GarageDoorAutomation";
char mqtt_server[40];
char mqtt_port[6] = "1883";

//topics to publish
//Garage door 1 state
const char* garage_door_1_state_topic;
//Garage door 2 state
const char* garage_door_2_state_topic;

//topics to listen to
//Garage door 1 command (1=open, 0=close)
const char* garage_door_1_command_topic;
//Garage door 2 state (1=open, 0=close)
const char* garage_door_2_command_topic;
const char* reset_topic;

static const uint8_t GARAGE_DOOR_1_COMMAND_PIN=D5;
static const uint8_t GARAGE_DOOR_2_COMMAND_PIN=D6;

static const uint8_t GARAGE_DOOR_1_SENDOR_PIN=D1;
static const uint8_t GARAGE_DOOR_2_SENDOR_PIN=D2;

static const uint8_t GARAGE_DOOR_OPEN_COMMAND=1;
static const uint8_t GARAGE_DOOR_CLOSE_COMMAND=0;

static const uint8_t GARAGE_DOOR_OPEN_STATE=1;
static const uint8_t GARAGE_DOOR_CLOSE_STATE=0;
static const uint8_t GARAGE_DOOR_OPENING_STATE=2;
static const uint8_t GARAGE_DOOR_CLOSING_STATE=3;

static const uint16 GARAGE_DOOR_OPENING_TIME_MILLIS=5000;
static const uint16 GARAGE_DOOR_CLOSING_TIME_MILLIS=30000;
static const uint16 RELAY_ACTIVATION_MILLIS=1000;
static const unsigned long MAX_PUBLISH_DELAY = 100000;

//flag for saving data
bool shouldSaveConfig = false;

WiFiClient esp_wifi_client;
PubSubClient mqtt_client(esp_wifi_client);

uint8_t garage_door_1_state = 0;
uint8_t garage_door_2_state = 0;

unsigned long last_published_millis = 0;

uint8_t garage_door_1_last_published_value = 0;
uint8_t garage_door_2_last_published_value = 0;

unsigned long garage_door_1_last_command_millis = 0;
unsigned long garage_door_2_last_command_millis = 0;

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupGIPO(){
  pinMode(GARAGE_DOOR_1_COMMAND_PIN,OUTPUT);
  pinMode(GARAGE_DOOR_2_COMMAND_PIN,OUTPUT);
  pinMode(GARAGE_DOOR_1_SENDOR_PIN,INPUT_PULLUP);
  pinMode(GARAGE_DOOR_2_SENDOR_PIN,INPUT_PULLUP);
  digitalWrite(GARAGE_DOOR_1_COMMAND_PIN,1);
  digitalWrite(GARAGE_DOOR_2_COMMAND_PIN,1);
}

uint8_t checkCommandForNewActions(uint8_t new_ideal_state, uint8_t garage_door_state, uint8_t pin){
  if(new_ideal_state == GARAGE_DOOR_OPEN_COMMAND && garage_door_state == GARAGE_DOOR_CLOSE_STATE) {
    Serial.println("Opening garage door");
    digitalWrite(pin,0);
    return GARAGE_DOOR_OPENING_STATE;
  }
  if(new_ideal_state == GARAGE_DOOR_CLOSE_COMMAND && garage_door_state == GARAGE_DOOR_OPEN_STATE) {
    Serial.println("Closing garage door");
    digitalWrite(pin,0);
    return GARAGE_DOOR_CLOSING_STATE;
  }
  return garage_door_state;
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  if(strcmp(topic, reset_topic) == 0) {
    WiFi.disconnect(true);
    ESP.reset();
    delay(5000);
  }
  
  if(length == 1 && (payload[0] == '0' || payload[0] == '1')){
    uint8_t new_ideal_state = payload[0]-'0';
    if(strcmp(topic, garage_door_1_command_topic) == 0) {
      uint8_t new_state = checkCommandForNewActions(new_ideal_state,garage_door_1_state,GARAGE_DOOR_1_COMMAND_PIN);
      if(new_state != garage_door_1_state){
        garage_door_1_last_command_millis = millis();
        garage_door_1_state = new_state;
      }
    }else if(strcmp(topic, garage_door_2_command_topic) == 0) {
      uint8_t new_state = checkCommandForNewActions(new_ideal_state,garage_door_2_state,GARAGE_DOOR_2_COMMAND_PIN);
      if(new_state != garage_door_2_state){
        garage_door_2_last_command_millis = millis();
        garage_door_2_state = new_state;
      }
    }
  } else
  {
    Serial.println("Got unexpected message on mqtt topic");
  }

}

void initializeMQTTClient() {
  mqtt_client.setServer(mqtt_server, atoi(mqtt_port));
  mqtt_client.setCallback(mqtt_callback);
}

void setupWiFi() {
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_monitor_name("monitor_name", "monitor name", monitor_name, 40);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_monitor_name);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(monitor_name, "Id0ntknow")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(monitor_name, custom_monitor_name.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument jsonBuffer(10000);
    JsonObject json = jsonBuffer.to<JsonObject>();
    json["monitor_name"] = monitor_name;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(json, Serial);
    serializeJson(json, configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void readConfigsFromFileSystem() {
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument jsonBuffer(10000);
        DeserializationError error = deserializeJson(jsonBuffer, buf.get());
        JsonObject json = jsonBuffer.as<JsonObject>();
        serializeJson(json, Serial);
        if (!error) {
          Serial.println("\nparsed json");
          strcpy(monitor_name, json["monitor_name"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);

          String monitor_name_string = String(monitor_name);
          garage_door_1_state_topic = (new String(monitor_name_string + "/garage_door_1/state"))->c_str();
          garage_door_2_state_topic = (new String(monitor_name_string + "/garage_door_2/state"))->c_str();
          garage_door_1_command_topic = (new String(monitor_name_string + "/garage_door_1/command"))->c_str();
          garage_door_2_command_topic = (new String(monitor_name_string + "/garage_door_2/command"))->c_str();
          reset_topic = (new String(monitor_name_string + "/reset"))->c_str();
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  setupGIPO();

  //clean FS, for testing
  //SPIFFS.format();
  Serial.println("Reading congfig from FileSystem");
  readConfigsFromFileSystem();
  Serial.println("Setting up Wifi");
  setupWiFi();
  Serial.println("Setting up MQTT Client");
  initializeMQTTClient();
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = String(monitor_name);
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str())) {
      Serial.println("connected");
      mqtt_client.subscribe(garage_door_1_command_topic);
      mqtt_client.subscribe(garage_door_2_command_topic);
      mqtt_client.subscribe(reset_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqtt_loop() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();
}

void resetRelay(uint8_t garage_door_state, unsigned long millis_since_last_command, uint8_t pin){
  bool is_door_opening_moving = (garage_door_state == GARAGE_DOOR_OPENING_STATE || garage_door_state == GARAGE_DOOR_CLOSING_STATE);
  bool is_relay_activation_in_progress = millis_since_last_command<RELAY_ACTIVATION_MILLIS;
  if(is_door_opening_moving && !is_relay_activation_in_progress){
    Serial.println("Stopping relay activation");
    digitalWrite(pin,1);
  }
}

uint8_t getNewGarageDoorState(uint8_t previous_garage_door_state, uint8_t current_sensor_value, unsigned long millis_since_last_command){
  if(previous_garage_door_state != current_sensor_value){
    bool isGarageDoorStillOpening = 
      previous_garage_door_state == GARAGE_DOOR_OPENING_STATE &&
      current_sensor_value == GARAGE_DOOR_CLOSE_STATE &&
      millis_since_last_command<GARAGE_DOOR_OPENING_TIME_MILLIS;
    bool isGarageDoorStillClosing =
      previous_garage_door_state == GARAGE_DOOR_CLOSING_STATE &&
      current_sensor_value == GARAGE_DOOR_OPEN_STATE &&
      millis_since_last_command<GARAGE_DOOR_CLOSING_TIME_MILLIS;
    if(!isGarageDoorStillOpening && !isGarageDoorStillClosing){
      return current_sensor_value;
    }
  }
  return previous_garage_door_state;
}

void loop() {
  mqtt_loop();
  unsigned long now = millis();
  unsigned long millis_since_garage_door_1_command = now-garage_door_1_last_command_millis;
  unsigned long millis_since_garage_door_2_command = now-garage_door_2_last_command_millis;
  resetRelay(garage_door_1_state,millis_since_garage_door_1_command,GARAGE_DOOR_1_COMMAND_PIN);
  resetRelay(garage_door_2_state,millis_since_garage_door_2_command,GARAGE_DOOR_2_COMMAND_PIN);
  //sensors return 1 if open 0 if closed
  uint8_t current_garage_door_1_state = digitalRead(GARAGE_DOOR_1_SENDOR_PIN);
  uint8_t current_garage_door_2_state = digitalRead(GARAGE_DOOR_2_SENDOR_PIN);
  uint8_t new_garage_door_1_state = getNewGarageDoorState(garage_door_1_state, current_garage_door_1_state, millis_since_garage_door_1_command);
  if(garage_door_1_state != new_garage_door_1_state  || garage_door_1_last_published_value != new_garage_door_1_state || now-last_published_millis > MAX_PUBLISH_DELAY){
    garage_door_1_state = new_garage_door_1_state;
    mqtt_client.publish(garage_door_1_state_topic, String(garage_door_1_state).c_str());
    garage_door_1_last_published_value = garage_door_1_state;
  }
  uint8_t new_garage_door_2_state = getNewGarageDoorState(garage_door_2_state, current_garage_door_2_state, millis_since_garage_door_2_command);
  if(garage_door_2_state != new_garage_door_2_state || garage_door_2_last_published_value != new_garage_door_2_state || now-last_published_millis > MAX_PUBLISH_DELAY){
    garage_door_2_state = new_garage_door_2_state;
    mqtt_client.publish(garage_door_2_state_topic, String(garage_door_2_state).c_str());
    garage_door_2_last_published_value = garage_door_2_state;
  }

  if(now-last_published_millis > MAX_PUBLISH_DELAY){
    last_published_millis = now;
  }
}
