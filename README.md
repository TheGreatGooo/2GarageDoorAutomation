# 2 Door Garage door automation via esp8266

### What you need
  - [NodeMCU v3](https://www.amazon.com/gp/search/ref=as_li_qf_sp_sr_tl?ie=UTF8&tag=thegreatgooo-20&keywords=nodemcu%20v3&index=aps&camp=1789&creative=9325&linkCode=ur2&linkId=14a522e579554ca4b9964695a174d1b8)
  - [2 relay module](https://www.amazon.com/gp/search/ref=as_li_qf_sp_sr_tl?ie=UTF8&tag=thegreatgooo-20&keywords=2%20relay%20module&index=aps&camp=1789&creative=9325&linkCode=ur2&linkId=8f5aa404b80905b994b29f5653ec4644)
  - [speaker wire](https://www.amazon.com/gp/search/ref=as_li_qf_sp_sr_tl?ie=UTF8&tag=thegreatgooo-20&keywords=speaker%20wire&index=aps&camp=1789&creative=9325&linkCode=ur2&linkId=e7bb3765d1487271b112a3f32945af1e)
  - [magnetic door switch](https://www.amazon.com/gp/search/ref=as_li_qf_sp_sr_tl?ie=UTF8&tag=thegreatgooo-20&keywords=magnetic%20door%20switch&index=aps&camp=1789&creative=9325&linkCode=ur2&linkId=395c1fe52b1163067dcdab3fd76b26e6)
  - [3d printed case](https://www.thingiverse.com/thing:1867799)
  
  ### Pins
  #### NodeMCU v3
    - D5 --> IN1 on relay module
    - D6 --> IN2 on relay module
    - D1 --> Speaker wire 1 (+)
    - D2 --> Speaker wire 2 (+)
    - 3v --> relay module vcc
    - GND --> Speaker wire 1/2 (-) & GND on relay module
    - VV --> if using 5v relays connect to JD-VCC on relay module
   #### Relay Module
    - Relay 1 common --> Speaker wire 3 (+)
    - Relay 1 NO     --> Speaker wire 3 (-)
    - Relay 2 common --> Speaker wire 4 (+)
    - Relay 2 NO     --> Speaker wire 4 (-)
  
![Case setup](images/garage_door_cutaway.png)
![3d printed case](images/3d_printed_case.png)
![garage door opener](images/garage_door_opener.png)
![garage door sensor](images/garage_door_sensor.JPG)
![mounted case in garage](images/mounted_case_in_garage.png)
