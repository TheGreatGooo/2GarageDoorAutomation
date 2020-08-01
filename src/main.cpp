#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char monitor_name[40] = "EnergyMonitor";
char mqtt_server[40];
char mqtt_port[6] = "8080";

//topics to publish
//Garage door 1 state
const char* garage_door_1_state_topic;
//Garage door 2 state
const char* garage_door_2_state_topic;

//topics to listen to
//Garage door 1 command (0=open, 1=close)
const char* garage_door_1_command_topic;
//Garage door 2 state (0=open, 1=close)
const char* garage_door_2_command_topic;

//flag for saving data
bool shouldSaveConfig = false;

WiFiClient esp_wifi_client;
PubSubClient mqtt_client(esp_wifi_client);

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();
  Serial.println("Reading congfig from FileSystem");
  readConfigsFromFileSystem();
  Serial.println("Setting up Wifi");
  setupWiFi();
  Serial.println("Setting up MQTT Client");
  initializeMQTTClient();
}

void initializeMQTTClient() {
  mqtt_client.setServer(mqtt_server, atoi(mqtt_port));
  mqtt_client.setCallback(mqtt_callback);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  if(strcmp(topic, reset_topic) == 0) {
    WiFi.disconnect(true);
    ESP.reset();
    delay(5000);
  }
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

void loop() {
  unsigned long now = millis();
  mqtt_loop();
  if (now - last_message_publish > 500) {
    
  }
}