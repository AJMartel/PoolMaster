/*
Arduino/Controllino-Maxi/ATmega2560 based Ph/ORP regulator for home pool sysem
(c) Loic74 <loic74650@gmail.com> 2018-2019

***how to compile***
- select the target board type in the Arduino IDE (either "Arduino Mega 2560" or "Controllino Maxi")

***Compatibility***
For this sketch to work on your setup you must change the following in the code:
- possibly the pinout definitions in case you are not using a CONTROLLINO MAXI board
- MAC address of DS18b20 water temperature sensor
- MAC and IP address of the Ethernet shield
- MQTT broker IP address and login credentials
- possibly the topic names on the MQTT broker to subscribe and publish to
- the Kp,Ki,Kd parameters for both PID loops in case your peristaltic pumps have a different throughput than 1.5Liters/hour for the pH pump and 3.0Liters/hour for the Chlorine pump. 
Also the default Kp values were adjusted for a 50m3 pool volume. You might have to adjust the Kp values in case of a different pool volume and/or peristaltic pumps throughput
(start by adjusting it proportionally). In any case these parameters are likely to require adjustments for every pool

***Brief description:***
Four main metrics are measured and periodically reported over MQTT: water temperature and pressure, PH and ORP values
Pumps states, tank-level states and other parmaters are also periodically reported
Two PID regulation loops are running in parallel: one for PH, one for ORP
PH is regulated by injecting Acid from a tank into the pool water (a relay starts/stops the Acid peristaltic pump)
ORP is regulated by injecting Chlorine from a tank into the pool water (a relay starts/stops the Chlorine peristaltic pump)
Defined time-slots and water temperature are used to start/stop the filtration pump for a daily given amount of time (a relay starts/stops the filtration pump) 
A lightweight webserver provides a simple dynamic webpage with a summary of all system parameters
Communication with the system is performed using the MQTT protocol over an Ethernet connection to the local network/MQTT broker.

Every 30 seconds (by default), the system will publish on the "PoolTopic" (see in code below) the following payloads in Json format:

{"Tmp":818,"pH":321,"PSI":56,"Orp":583,"FilUpT":8995,"PhUpT":0,"ChlUpT":0,"IO":11,"IO2":0}

Tmp: measured Water temperature value in °C x100 (8.18°C in the above example payload)
pH: measured pH value x100 (3.21 in the above example payload)
Orp: measured Orp (aka Redox) value in mV (583mV in the above example payload)
PSI: measured Water pressure value in bar x100 (0.56bar in the above example payload)
FiltUpT: current running time of Filtration pump in seconds (reset every 24h. 8995secs in the above example payload)
PhUpT: current running time of Ph pump in seconds (reset every 24h. 0secs in the above example payload)
ChlUpT: current running time of Chl pump in seconds (reset every 24h. 0secs in the above example payload)
IO: a variable of type BYTE where each individual bit is the state of a digital input on the Arduino. These are :

FiltPump: current state of Filtration Pump (0=on, 1=off)
PhPump: current state of Ph Pump (0=on, 1=off)
ChlPump: current state of Chl Pump (0=on, 1=off)
PhlLevel: current state of Acid tank level (0=empty, 1=ok)
ChlLevel: current state of Chl tank level (0=empty, 1=ok)
PSIError: over-pressure error
pHErr: pH pump overtime error flag
ChlErr: Chl pump overtime error flag

IO2: a variable of type BYTE where each individual bit is the state of a digital input on the Arduino. These are :

pHPID: current state of pH PID regulation loop (1=on, 0=off)
OrpPID: current state of Orp PID regulation loop (1=on, 0=off)
Mode: (0=manual, 1=auto)

   
***MQTT API***
Below are the Payloads/commands to publish on the "PoolTopicAPI" topic (see in code below) in Json format in order to launch actions on the Arduino:
{"Mode":1} or {"Mode":0}         -> set "Mode" to manual (0) or Auto (1). In Auto, filtration starts/stops at set times of the day 
{"FiltPump":1} or {"FiltPump":0} -> manually start/stop the filtration pump. 
{"ChlPump":1} or {"ChlPump":0}   -> manually start/stop the Chl pump to add more Chlorine
{"PhPump":1} or {"PhPump":0}     -> manually start/stop the Acid pump to lower the Ph
{"PhPID":1} or {"PhPID":0}       -> start/stop the Ph PID regulation loop
{"OrpPID":1} or {"OrpPID":0}     -> start/stop the Orp PID regulation loop
{"PhCalib":[4.02,3.8,9.0,9.11]}  -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
{"OrpCalib":[450,465,750,784]}   -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
{"PhSetPoint":7.4}               -> set the Ph setpoint, 7.4 in this example
{"OrpSetPoint":750.0}            -> set the Orp setpoint, 750mV in this example
{"WSetPoint":27.0}               -> set the water temperature setpoint, 27.0deg in this example (for future use. Water heating not handled yet)
{"WTempLow":10.0}                -> set the water low-temperature threshold below which there is no need to regulate Orp and Ph (ie. in winter)
{"OrpPIDParams":[2857,0,0]}      -> respectively set Kp,Ki,Kd parameters of the Orp PID loop. In this example they are set to 2857, 0 and 0
{"PhPIDParams":[1330000,0,0.0]}  -> respectively set Kp,Ki,Kd parameters of the Ph PID loop. In this example they are set to 1330000, 0 and 0.0
{"OrpPIDWSize":3600000}          -> set the window size of the Orp PID loop (in msec), 60mins in this example
{"PhPIDWSize":600000}            -> set the window size of the Ph PID loop (in msec), 20mins in this example
{"Date":[1,1,1,18,13,32,0]}      -> set date/time of RTC module in the following format: (Day of the month, Day of the week, Month, Year, Hour, Minute, Seconds), in this example: Monday 1st January 2018 - 13h32mn00secs
{"FiltT0":9}                     -> set the earliest hour (9:00 in this example) to run filtration pump. Filtration pump will not run beofre that hour
{"FiltT1":20}                    -> set the latest hour (20:00 in this example) to run filtration pump. Filtration pump will not run after that hour
{"PubPeriod":30}                 -> set the periodicity (in seconds) at which the system info (pumps states, tank levels states, measured values, etc) will be published to the MQTT broker
{"PumpsMaxUp":1800}              -> set the Max Uptime (in secs) for the Ph and Chl pumps over a 24h period. If over, PID regulation is stopped and a warning flag is raised
{"Clear":1}                      -> reset the pH and Orp pumps overtime error flags in order to let the regulation loops continue. "Mode", "PhPID" and "OrpPID" commands need to be switched back On (1) after an error flag was raised
{"DelayPID":60}                  -> Delay (in mins) after FiltT0 before the PID regulation loops will start. This is to let the Orp and pH readings stabilize first. 30mins in this example. Should not be > 59mins
{"TempExt":4.2}                  -> Provide the external temperature. Should be updated regularly and will be used to start filtration for 10mins every hour when temperature is negative. 4.2deg in this example
{"PSIHigh":1.0}                  -> set the water high-pressure threshold (1.0bar in this example). When water pressure is over that threshold, an error flag is set.

***Dependencies and respective revisions used to compile this project***
https://github.com/256dpi/arduino-mqtt/releases (rev 2.4.3)
https://github.com/CONTROLLINO-PLC/CONTROLLINO_Library (rev 3.0.4)
https://github.com/PaulStoffregen/OneWire (rev 2.3.4)
https://github.com/milesburton/Arduino-Temperature-Control-Library (rev 3.7.2)
https://github.com/RobTillaart/Arduino/tree/master/libraries/RunningMedian (rev 0.1.15)
https://github.com/prampec/arduino-softtimer (rev 3.1.3)
https://github.com/bricofoy/yasm (rev 0.9.2)
https://github.com/br3ttb/Arduino-PID-Library (rev 1.2.0)
https://github.com/bblanchon/ArduinoJson (rev 5.13.4)
https://github.com/arduino-libraries/LiquidCrystal (rev 1.0.7)
https://github.com/thijse/Arduino-EEPROMEx (rev 1.0.0)
https://github.com/sdesalas/Arduino-Queue.h (rev )
https://github.com/Loic74650/Pump (rev 0.0.1)
https://github.com/PaulStoffregen/Time (rev 1.5)
https://github.com/adafruit/RTClib (rev 1.2.0)

*/
#if defined(CONTROLLINO_MAXI) //Controllino Maxi board specifics

  #include <Controllino.h>
  
  //output relays pin definitions
  #define FILTRATION_PUMP CONTROLLINO_R4  //CONTROLLINO_RELAY_4
  #define PH_PUMP    CONTROLLINO_R3       //CONTROLLINO_RELAY_3
  #define CHL_PUMP   CONTROLLINO_R5       //CONTROLLINO_RELAY_5
  
  //Digital input pins connected to Acid and Chl tank level reed switches
  #define CHL_LEVEL  CONTROLLINO_D1       //CONTROLLINO_D1 pin 3
  #define PH_LEVEL   CONTROLLINO_D3       //CONTROLLINO_D3 pin 5
  
  //Analog input pins connected to Phidgets 1130_0 pH/ORP Adapters. 
  //Galvanic isolation circuitry between Adapters and Arduino required!
  #define ORP_MEASURE CONTROLLINO_A2      //CONTROLLINO_A2 pin A2 on pin header connector, not on screw terminal (/!\)
  #define PH_MEASURE  CONTROLLINO_A4      //CONTROLLINO_A4 pin A4 on pin header connector, not on screw terminal (/!\)
  
  //Analog input pin connected to pressure sensor
  #define PSI_MEASURE CONTROLLINO_A3      //CONTROLLINO_A3 pin A3 on pin header connector, not on screw terminal (/!\)

#else //Mega2560 board specifics

  #include <Wire.h>
  #include "RTClib.h"
  RTC_DS3231 rtc;

  #define FILTRATION_PUMP 38
  #define PH_PUMP         36
  #define CHL_PUMP        42
  
  //Digital input pins connected to Acid and Chl tank level reed switches
  #define CHL_LEVEL       28
  #define PH_LEVEL        30
  
  //Analog input pins connected to Phidgets 1130_0 pH/ORP Adapters. 
  //Galvanic isolation circuitry between Adapters and Arduino required!
  #define ORP_MEASURE     A9
  #define PH_MEASURE      A8
  
  //Analog input pin connected to pressure sensor
  #define PSI_MEASURE     A7


#endif

#include <SPI.h>
#include <Ethernet.h>
#include <MQTT.h>
#include <SD.h>
#include "OneWire.h"
#include <DallasTemperature.h>
#include <TimeLib.h>

#include <RunningMedian.h>
#include <SoftTimer.h>
#include <yasm.h>
#include <PID_v1.h>
#include <Streaming.h>
#include <LiquidCrystal.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <ArduinoJson.h>
#include <EEPROMex.h>
#include <Queue.h>
#include <Time.h>
#include "Pump.h"

// Firmware revision
String Firmw = "3.0.0";

//Version of config stored in Eeprom
//Random value. Change this value (to any other value) to revert the config to default values
#define CONFIG_VERSION 100

//Starting point address where to store the config data in EEPROM
#define memoryBase 32
int configAdress=0;
const int maxAllowedWrites = 200;//not sure what this is for

//Queue object to store incoming JSON commands (up to 10)
Queue<String> queue = Queue<String>(10);

//LCD init.
//LCD connected on pin header 2 connector, not on screw terminal (/!\)
//pin definitions, may vary in your setup
const int rs = 9, en = 10, d4 = 11, d5 = 12, d6 = 13, d7 = 42;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
bool LCDToggle = true;

//buffers for MQTT string payload
#define LCD_BufferLength (20*4)+1
char LCD_Buffer[LCD_BufferLength];

//buffer used to capture HTTP requests
String readString;

//buffers for MQTT string payload
#define PayloadBufferLength 128
char Payload[PayloadBufferLength];

//Settings structure and its default values
struct StoreStruct 
{
    uint8_t ConfigVersion;   // This is for testing if first time using eeprom or not
    bool Ph_RegulationOnOff, Orp_RegulationOnOff;
    uint8_t FiltrationStart, FiltrationDuration, FiltrationStopMax, FiltrationStop, DelayPIDs;  
    unsigned long PhPumpUpTimeLimit, ChlPumpUpTimeLimit;
    unsigned long PhPIDWindowSize, OrpPIDWindowSize, PhPIDwindowStartTime, OrpPIDwindowStartTime;
    double Ph_SetPoint, Orp_SetPoint, PSI_HighThreshold, PSI_MedThreshold, WaterTempLowThreshold, WaterTemp_SetPoint, TempExternal, pHCalibCoeffs0, pHCalibCoeffs1, OrpCalibCoeffs0, OrpCalibCoeffs1, PSICalibCoeffs0, PSICalibCoeffs1;
    double Ph_Kp, Ph_Ki, Ph_Kd, Orp_Kp, Orp_Ki, Orp_Kd, PhPIDOutput, OrpPIDOutput, TempValue, PhValue, OrpValue, PSIValue;
} storage = 
{                     //default values. Change the value of CONFIG_VERSION in order to restore the default values
    CONFIG_VERSION,
    0, 0,
    8, 12, 20, 20, 59,
    1800, 1800,
    3600000, 1200000, 0, 0,
    7.4, 730.0, 0.5, 0.25, 10.0, 27.0, 3.0, 4.23, -2.28, -1268.78, 2718.63, 1.0, 0.0,
    1330000.0, 0.0, 0.0, 2857.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.4
};

bool PSIError = 0;

Pump FiltrationPump(FILTRATION_PUMP, NO_TANK);
Pump PhPump(PH_PUMP, PH_LEVEL);
Pump ChlPump(CHL_PUMP, CHL_LEVEL);

//Tank level error flags
bool PhLevelError = 0;
bool ChlLevelError = 0;

//PIDs instances
//Specify the links and initial tuning parameters
PID PhPID(&storage.PhValue, &storage.PhPIDOutput, &storage.Ph_SetPoint, storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd, REVERSE);
PID OrpPID(&storage.OrpValue, &storage.OrpPIDOutput, &storage.Orp_SetPoint, storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd, DIRECT);

//Filtration/regulation mode: auto (1) or manual (0)
bool AutoMode = 0;

//Filtration anti freeze mode
bool AntiFreezeFiltering = false;

//BitMaps with GPIO states
uint8_t BitMap = 0;
uint8_t BitMap2 = 0;

//MQTT publishing periodicity of system info, in msecs
unsigned long PublishPeriod = 30000;

// Data wire is connected to input digital pin 20 on the Arduino
#define ONE_WIRE_BUS_A 20

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire_A(ONE_WIRE_BUS_A);

// Pass our oneWire reference to Dallas Temperature library instance 
DallasTemperature sensors_A(&oneWire_A);

//12bits (0,06°C) temperature sensor resolution
#define TEMPERATURE_RESOLUTION 12

//Signal filtering library. Only used in this case to compute the average
//over multiple measurements but offers other filtering functions such as median, etc. 
RunningMedian samples_Temp = RunningMedian(10);
RunningMedian samples_Ph = RunningMedian(10);
RunningMedian samples_Orp = RunningMedian(10);
RunningMedian samples_PSI = RunningMedian(10);

//MAC Address of DS18b20 water temperature sensor
DeviceAddress DS18b20_0 = { 0x28, 0x92, 0x25, 0x41, 0x0A, 0x00, 0x00, 0xEE };
String sDS18b20_0;
                                                 
// MAC address of Ethernet shield (in case of Controllino board, set an arbitrary MAC address)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
String sArduinoMac;
IPAddress ip(192, 168, 0, 21);  //IP address, needs to be adapted depending on local network topology
EthernetServer server(80);      //Create a server at port 80
EthernetClient net;             //Ethernet client to connect to MQTT server

//MQTT stuff including local broker/server IP address, login and pwd
MQTTClient MQTTClient;
const char* MqttServerIP = "192.168.0.38";
const char* MqttServerClientID = "ArduinoPool2"; // /!\ choose a client ID which is unique to this Arduino board
const char* MqttServerLogin = "XXX";
const char* MqttServerPwd = "XXX";
const char* PoolTopic = "Home/Pool";
const char* PoolTopicAPI = "Home/Pool/API";
const char* PoolTopicStatus = "Home/Pool/status";

//Date-Time variables for use with internal RTC (Real Time Clock) module
char TimeBuffer[25];

//serial printing stuff
String _endl = "\n";

//State Machine
//Getting a 12 bits temperature reading on a DS18b20 sensor takes >750ms
//Here we use the sensor in asynchronous mode, request a temp reading and use
//the nice "YASM" state-machine library to do other things while it is being obtained
YASM gettemp;

//Callbacks
//Here we use the SoftTimer library which handles multiple timers (Tasks)
//It is more elegant and readable than a single loop() functtion, especially
//when tasks with various frequencies are to be used
void EthernetClientCallback(Task* me);
void OrpRegulationCallback(Task* me);
void PHRegulationCallback(Task* me);
void GenericCallback(Task* me);
void PublishDataCallback(Task* me);
void LCDCallback(Task* me);

Task t1(500, EthernetClientCallback);         //Check for Ethernet client every 0.5 secs
Task t2(1000, OrpRegulationCallback);         //ORP regulation loop every 1 sec
Task t3(1100, PHRegulationCallback);          //PH regulation loop every 1.1 sec
Task t4(30000, PublishDataCallback);          //Publish data to MQTT broker every 30 secs
Task t5(600, GenericCallback);                //Various things handled/updated in this loop every 0.6 secs
Task t6(6000, LCDCallback);                   //Toggle between LCD screens every 6 secs


void setup()
{
   //Serial port for debug info
    Serial.begin(9600);
    delay(200);

    //Initialize Eeprom
    EEPROM.setMemPool(memoryBase, EEPROMSizeMega); 

    //Get address of "ConfigVersion" setting
    configAdress = EEPROM.getAddress(sizeof(StoreStruct));

    //Read ConfigVersion. If does not match expected value, restore default values
    uint8_t vers = EEPROM.readByte(configAdress);

    if(vers == CONFIG_VERSION) 
    {
      Serial<<F("Stored config version: ")<<CONFIG_VERSION<<F(". Loading settings from eeprom")<<_endl;
      loadConfig();//Restore stored values from eeprom
    }
    else
    {
      Serial<<F("Stored config version: ")<<CONFIG_VERSION<<F(". Loading default settings, not from eeprom")<<_endl;
      saveConfig();//First time use. Save default values to eeprom
    }
      
    // set up the LCD's number of columns and rows:
    lcd.begin(20, 4);
    lcd.clear();
  
    //RTC Stuff (embedded battery operated clock). In case board is MEGA_2560, need to initialize the date time!
    #if defined(CONTROLLINO_MAXI)
      Controllino_RTC_init(0);
      setTime((uint8_t)Controllino_GetHour(),(uint8_t)Controllino_GetMinute(),(uint8_t)Controllino_GetSecond(),(uint8_t)Controllino_GetDay(),(uint8_t)Controllino_GetMonth(),(uint8_t)Controllino_GetYear()+2000);     
    #else
      if (! rtc.begin()) 
      {
        Serial<<F("Couldn't find RTC")<<endl;
        while (1);
      }
      else
      {  
        DateTime now = rtc.now();
        setTime(now.hour(),(uint8_t)now.minute(),(uint8_t)now.second(),(uint8_t)now.day(),(uint8_t)now.month(),(uint16_t)now.year());     
      }      
    #endif
    
    //Define pins directions
    pinMode(FILTRATION_PUMP, OUTPUT);
    pinMode(PH_PUMP, OUTPUT);
    pinMode(CHL_PUMP, OUTPUT);

    pinMode(CHL_LEVEL, INPUT_PULLUP);
    pinMode(PH_LEVEL, INPUT_PULLUP);

    pinMode(ORP_MEASURE, INPUT);
    pinMode(PH_MEASURE, INPUT);
    pinMode(PSI_MEASURE, INPUT);

    //String for MAC address of Ethernet shield for the log & XML file
    sArduinoMac = F("0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED");

    //8 seconds watchdog timer to reset system in case it freezes for more than 8 seconds
    wdt_enable(WDTO_8S);
    
    // initialize Ethernet device  
    Ethernet.begin(mac, ip); 
    
    // start to listen for clients
    server.begin();  
   
    //Start temperature measurement state machine
    gettemp.next(gettemp_start);

    //Init MQTT
    MQTTClient.setOptions(60,false,10000);
    MQTTClient.setWill(PoolTopicStatus,"offline",true,LWMQTT_QOS1);
    MQTTClient.begin(MqttServerIP, net);
    MQTTClient.onMessage(messageReceived);
    MQTTConnect();
   
    //Initialize PIDs
    storage.PhPIDwindowStartTime = millis();
    storage.OrpPIDwindowStartTime = millis();

    //Limit the PIDs output range in order to limit max. pumps runtime (safety first...)
    PhPID.SetTunings(storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd);
    PhPID.SetOutputLimits(0, 600000);//Whatever happens, don't allow continuous injection of Acid for more than 10mins within a PID Window
    OrpPID.SetTunings(storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd);
    OrpPID.SetOutputLimits(0, 600000);//Whatever happens, don't allow continuous injection of Chl for more than 10mins within a PID Window

    //let the PIDs off at start
    SetPhPID(false);
    SetOrpPID(false);

    //Initialize pumps
    FiltrationPump.SetMaxUpTime(0); //no runtime limit for the filtration pump
    PhPump.SetMaxUpTime(storage.PhPumpUpTimeLimit*1000);
    ChlPump.SetMaxUpTime(storage.ChlPumpUpTimeLimit*1000);

    //Initialize Filtration schedule
    storage.FiltrationDuration = 12;
    storage.FiltrationStop = storage.FiltrationStart + storage.FiltrationDuration;
        
    //Ethernet client check loop
    SoftTimer.add(&t1);

    //Orp regulation loop
    SoftTimer.add(&t2);

    //PH regulation loop
    SoftTimer.add(&t3);

    //Publish loop
    SoftTimer.add(&t4);

    //Generic loop
    SoftTimer.add(&t5);

    //LCD loop
    SoftTimer.add(&t6);

    //display remaining RAM space. For debug
    Serial<<F("[memCheck]: ")<<freeRam()<<F("b")<<_endl;
}

//Connect to MQTT broker and subscribe to the PoolTopicAPI topic in order to receive future commands
//then publish the "online" message on the "status" topic. If Ethernet connection is ever lost
//"status" will switch to "offline". Very useful to check that the Arduino is alive and functional
void MQTTConnect() 
{
//  MQTTClient.connect(MqttServerClientID, MqttServerLogin, MqttServerPwd);
  int8_t Count=0;
  while (!MQTTClient.connect(MqttServerClientID, MqttServerLogin, MqttServerPwd) && (Count<4))
  {
    Serial<<F(".")<<_endl;
    delay(500);
    Count++;
  }

  if(MQTTClient.connected())
  {
    //String PoolTopicAPI = "Home/Pool/Api";
    //Topic to which send/publish API commands for the Pool controls
    MQTTClient.subscribe(PoolTopicAPI);
  
    //tell status topic we are online
    if(MQTTClient.publish(PoolTopicStatus,"online",true,LWMQTT_QOS1))
        Serial<<F("published: Home/Pool/status - online")<<_endl;
  }
  else
  Serial<<F("Failed to connect to the MQTT broker")<<_endl;
  
}

//MQTT callback
//This function is called when messages are published on the MQTT broker on the PoolTopicAPI topic to which we subscribed
//Add the received command to a message queue for later processing and exit the callback
void messageReceived(String &topic, String &payload) 
{
  String TmpStrPool(PoolTopicAPI);

  //Pool commands. This check might be redundant since we only subscribed to this topic
  if(topic == TmpStrPool)
  {
    queue.push(payload); 
    Serial<<"FreeRam: "<<freeRam()<<" - Qeued messages: "<<queue.count()<<_endl;
  }
}

//Loop where various tasks are updated/handled
void GenericCallback(Task* me)
{
    //clear watchdog timer
    wdt_reset();
    //Serial<<F("Watchdog Reset")<<_endl;

    //request temp reading
    gettemp.run();

    //Update MQTT thread
    MQTTClient.loop(); 

    //update pumps
    FiltrationPump.loop();
    PhPump.loop();
    ChlPump.loop();

    //Process queued incoming JSON commands if any
    if(queue.count()>0)
      ProcessCommand(queue.pop());

    //reset time counters at midnight
    if((hour() == 0) && (minute() == 0))
    {
      FiltrationPump.ResetUpTime();
      PhPump.ResetUpTime();
      ChlPump.ResetUpTime(); 
    }

    //compute next Filtering duration and stop time (in hours)
    if((hour() == storage.FiltrationStart + 1) && (minute() == 0))
    {
      storage.FiltrationDuration = round(storage.TempValue/2);
      if(storage.FiltrationDuration<3) storage.FiltrationDuration = 3;
      storage.FiltrationStop = storage.FiltrationStart + storage.FiltrationDuration;
      Serial<<F("storage.FiltrationDuration: ")<<storage.FiltrationDuration<<_endl; 
      if(storage.FiltrationStop>storage.FiltrationStopMax)
        storage.FiltrationStop=storage.FiltrationStopMax;
    }

    //start filtration pump as scheduled
    if(AutoMode && !PSIError && (hour() == storage.FiltrationStart) && (minute() == 0))
        FiltrationPump.Start();
        
    //start PIDs with delay after FiltrationStart in order to let the readings stabilize
    if(AutoMode && !PhPID.GetMode() && (FiltrationPump.UpTime/1000/60 > storage.DelayPIDs) && (hour() >= storage.FiltrationStart) && (hour() < storage.FiltrationStop))
    {
        //Initialize PIDs StartTime
        storage.PhPIDwindowStartTime = millis();
        storage.OrpPIDwindowStartTime = millis();

        //Start PIDs
        SetPhPID(true);
        SetOrpPID(true);
    }
        
    //stop filtration pump and PIDs as scheduled unless we are in AntiFreeze mode
    if(AutoMode && FiltrationPump.IsRunning() && !AntiFreezeFiltering && ((hour() == storage.FiltrationStop) && (minute() == 0)))
    {
        SetPhPID(false);
        SetOrpPID(false);
        FiltrationPump.Stop(); 
    }

    //start filtration pump every hour for 10 mins in cold temperatures (<-2.0deg)
    if(AutoMode&& !PSIError && (!FiltrationPump.IsRunning()) && ((hour() < storage.FiltrationStart) || (hour() > storage.FiltrationStop)) && (minute() == 0) && (storage.TempExternal<-2.0))
    {
        FiltrationPump.Start();
        AntiFreezeFiltering = true;
    }

    //stop filtration pump every hour after 10 mins in cold temperatures (<2.0deg)
    if(AutoMode && FiltrationPump.IsRunning() && ((hour() < storage.FiltrationStart) || (hour() > storage.FiltrationStop)) && (minute() == 10))
    {
        FiltrationPump.Stop(); 
        AntiFreezeFiltering = false;
    }

    //If filtration pump has been running for over 2secs but pressure is still low, stop the filtration pump, something is wrong, set error flag 
    if(FiltrationPump.IsRunning() && ((millis() - FiltrationPump.StartTime)>2000) && (storage.PSIValue < storage.PSI_MedThreshold))
    {
      FiltrationPump.Stop();
      PSIError = true;
    }
    
    //UPdate LCD
    if(LCDToggle)
      LCDScreen1();
    else
      LCDScreen2();
}

//PublishData loop. Publishes system info/data to MQTT broker every XX secs (30 secs by default)
void PublishDataCallback(Task* me)
{      
     //Store the GPIO states in one Byte (more efficient over MQTT)
      EncodeBitmap();
      
      if (!MQTTClient.connected()) 
      {
        MQTTConnect();
        //Serial.println("MQTT reconnecting...");
      }

      if(MQTTClient.connected())
      {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 128 bytes
        //Will publish something like {"Tmp":818,"pH":321,"PSI":56,"Orp":583,"FilUpT":8995,"PhUpT":0,"ChlUpT":0,"IO":11,"IO2":0}
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
  
        root.set<int>("Tmp", (int)(storage.TempValue*100));
        root.set<int>("pH", (int)(storage.PhValue*100));
        root.set<int>("PSI", (int)(storage.PSIValue*100));
        root.set<int>("Orp", (int)storage.OrpValue);
        root.set<unsigned long>("FilUpT", FiltrationPump.UpTime/1000);
        root.set<unsigned long>("PhUpT", PhPump.UpTime/1000);
        root.set<unsigned long>("ChlUpT", ChlPump.UpTime/1000);
        root.set<uint8_t>("IO", BitMap);
        root.set<uint8_t>("IO2", BitMap2);
  
        //char Payload[PayloadBufferLength];
        if(jsonBuffer.size() < PayloadBufferLength)
        {
          root.printTo(Payload,PayloadBufferLength);
          MQTTClient.publish(PoolTopic,Payload,strlen(Payload),false,LWMQTT_QOS1);
          
          Serial<<F("Payload: ")<<Payload<<F(" - ");
          Serial<<F("Payload size: ")<<jsonBuffer.size()<<_endl;   
        }
        else
        {
          Serial<<F("MQTT Payload buffer overflow! - ");
          Serial<<F("Payload size: ")<<jsonBuffer.size()<<_endl;  
        }
      }
      else
      Serial<<F("Failed to connect to the MQTT broker")<<_endl;

}

void PHRegulationCallback(Task* me)
{
 //do not compute PID if filtration pump is not running
 //because if Ki was non-zero that would let the OutputError increase
 if(FiltrationPump.IsRunning())
  {
    PhPID.Compute(); 
  
   /************************************************
   * turn the Acid pump on/off based on pid output
   ************************************************/
    if (millis() - storage.PhPIDwindowStartTime > storage.PhPIDWindowSize)
    { 
      //time to shift the Relay Window
      storage.PhPIDwindowStartTime += storage.PhPIDWindowSize;
    }
    if (storage.PhPIDOutput < millis() - storage.PhPIDwindowStartTime)
      PhPump.Stop();
    else 
      PhPump.Start();
  }
}

//Orp regulation loop
void OrpRegulationCallback(Task* me)
{
  //do not compute PID if filtration pump is not running
  //because if Ki was non-zero that would let the OutputError increase
  if(FiltrationPump.IsRunning())
  {
    OrpPID.Compute(); 
  
   /************************************************
   * turn the Acid pump on/off based on pid output
   ************************************************/
    if (millis() - storage.OrpPIDwindowStartTime > storage.OrpPIDWindowSize)
    { 
      //time to shift the Relay Window
      storage.OrpPIDwindowStartTime += storage.OrpPIDWindowSize;
    }
    if (storage.OrpPIDOutput < millis() - storage.OrpPIDwindowStartTime)
      ChlPump.Stop();
    else 
      ChlPump.Start();
  }
}

//Toggle between LCD screens
void LCDCallback(Task* me)
{
  LCDToggle = !LCDToggle;
}

//Enable/Disable Chl PID
void SetPhPID(bool Enable)
{
  if(Enable)
  {
     //Stop PhPID
     PhPump.ClearErrors();
     storage.PhPIDOutput = 0.0;
     PhPID.SetMode(1);
     storage.Ph_RegulationOnOff = 1;
  }
  else
  {
     //Stop PhPID
     PhPID.SetMode(0);
     storage.Ph_RegulationOnOff = 0;
     storage.PhPIDOutput = 0.0;   
     PhPump.Stop();
  }
}

//Enable/Disable Orp PID
void SetOrpPID(bool Enable)
{
  if(Enable)
  {
     //Stop OrpPID
     ChlPump.ClearErrors();
     storage.OrpPIDOutput = 0.0;
     OrpPID.SetMode(1);
     storage.Orp_RegulationOnOff = 1;

  }
  else
  {
     //Stop OrpPID
     OrpPID.SetMode(0);
     storage.Orp_RegulationOnOff = 0;
     storage.OrpPIDOutput = 0.0;   
     ChlPump.Stop();
  }
}

//Encode digital inputs states into one Byte (more efficient to send over MQTT)
void EncodeBitmap()
{
    BitMap = 0;
    BitMap2 = 0;
    BitMap |= (FiltrationPump.IsRunning() & 1) << 7;
    BitMap |= (PhPump.IsRunning() & 1) << 6;
    BitMap |= (ChlPump.IsRunning() & 1) << 5;
    BitMap |= (PhPump.TankLevel() & 1) << 4;
    BitMap |= (ChlPump.TankLevel() & 1) << 3;
    BitMap |= (PSIError & 1) << 2;
    BitMap |= (PhPump.UpTimeError & 1) << 1;
    BitMap |= (ChlPump.UpTimeError & 1) << 0;
    
    BitMap2 |= (PhPID.GetMode() & 1) << 7;
    BitMap2 |= (OrpPID.GetMode() & 1) << 6;
    BitMap2 |= (AutoMode & 1) << 5;
}
  
//Update temperature, Ph and Orp values
void getMeasures(DeviceAddress deviceAddress_0)
{
  Serial<<TimeBuffer<<F(" - ");

  //Water Temperature
  samples_Temp.add(sensors_A.getTempC(deviceAddress_0));
  storage.TempValue = samples_Temp.getAverage(10);
  if (storage.TempValue == -127.00) {
    Serial<<F("Error getting temperature from DS18b20_0")<<_endl;
  } else {
    Serial<<F("DS18b20_0: ")<<storage.TempValue<<F("°C")<<F(" - ");
  }

  //Ph
  float ph_sensor_value = analogRead(PH_MEASURE)* 5.0 / 1023.0;                                         // from 0.0 to 5.0 V
  //storage.PhValue = 7.0 - ((2.5 - ph_sensor_value)/(0.257179 + 0.000941468 * storage.TempValue));     // formula to compute pH which takes water temperature into account
  //storage.PhValue = (0.0178 * ph_sensor_value * 200.0) - 1.889;                                       // formula to compute pH without taking temperature into account (assumes 27deg water temp)
  storage.PhValue = (storage.pHCalibCoeffs0 * ph_sensor_value) + storage.pHCalibCoeffs1;                //Calibrated sensor response based on multi-point linear regression
  samples_Ph.add(storage.PhValue);                                                                      // compute average of pH from last 5 measurements
  storage.PhValue = samples_Ph.getAverage(10);
  Serial<<F("Ph: ")<<storage.PhValue<<F(" - ");

  //ORP
  float orp_sensor_value = analogRead(ORP_MEASURE) * 5.0 / 1023.0;                                      // from 0.0 to 5.0 V
  //storage.OrpValue = ((2.5 - orp_sensor_value) / 1.037) * 1000.0;                                     // from -2000 to 2000 mV where the positive values are for oxidizers and the negative values are for reducers
  storage.OrpValue = (storage.OrpCalibCoeffs0 * orp_sensor_value) + storage.OrpCalibCoeffs1;            //Calibrated sensor response based on multi-point linear regression
  samples_Orp.add(storage.OrpValue);                                                                    // compute average of ORP from last 5 measurements
  storage.OrpValue = samples_Orp.getAverage(10);
  Serial<<F("Orp: ")<<orp_sensor_value<<" - "<<storage.OrpValue<<F("mV")<<_endl;

  //PSI (water pressure)
  float psi_sensor_value = analogRead(PSI_MEASURE) * 2.0 / 1023.0;                                      // from 0.0 to 2.0 Bar (depends on sensor ref!)
  psi_sensor_value = 0.45;                                                                        // Remove this line when sensor is integrated!!!
  storage.PSIValue = (storage.PSICalibCoeffs0 * psi_sensor_value) + storage.PSICalibCoeffs1;            //Calibrated sensor response based on multi-point linear regression
  samples_PSI.add(storage.PSIValue);                                                                    // compute average of PSI from last 5 measurements
  storage.PSIValue = samples_PSI.getAverage(10);
  Serial<<F("PSI: ")<<psi_sensor_value<<" - "<<storage.PSIValue<<F("Bar")<<_endl;
}

bool loadConfig() 
{
  EEPROM.readBlock(configAdress, storage);

  Serial<<storage.ConfigVersion<<", "<<storage.Ph_RegulationOnOff<<", "<<storage.Orp_RegulationOnOff<<'\n';
  Serial<<storage.FiltrationStart<<", "<<storage.FiltrationDuration<<", "<<storage.FiltrationStopMax<<", "<<storage.FiltrationStop<<", "<<storage.DelayPIDs<<'\n';  
  Serial<<storage.PhPumpUpTimeLimit<<", "<<storage.ChlPumpUpTimeLimit<<'\n';
//  Serial<<storage.FiltrationPumpTimeCounter<<", "<<storage.PhPumpTimeCounter<<", "<<storage.ChlPumpTimeCounter<<", "<<storage.FiltrationPumpTimeCounterStart<<", "<<storage.PhPumpTimeCounterStart<<", "<<storage.ChlPumpTimeCounterStart<<'\n';
  Serial<<storage.PhPIDWindowSize<<", "<<storage.OrpPIDWindowSize<<", "<<storage.PhPIDwindowStartTime<<", "<<storage.OrpPIDwindowStartTime<<'\n';
  Serial<<storage.Ph_SetPoint<<", "<<storage.Orp_SetPoint<<", "<<storage.PSI_HighThreshold<<", "<<storage.PSI_MedThreshold<<", "<<storage.WaterTempLowThreshold<<", "<<storage.WaterTemp_SetPoint<<", "<<storage.TempExternal<<", "<<storage.pHCalibCoeffs0<<", "<<storage.pHCalibCoeffs1<<", "<<storage.OrpCalibCoeffs0<<", "<<storage.OrpCalibCoeffs1<<", "<<storage.PSICalibCoeffs0<<", "<<storage.PSICalibCoeffs1<<'\n';
  Serial<<storage.Ph_Kp<<", "<<storage.Ph_Ki<<", "<<storage.Ph_Kd<<", "<<storage.Orp_Kp<<", "<<storage.Orp_Ki<<", "<<storage.Orp_Kd<<", "<<storage.PhPIDOutput<<", "<<storage.OrpPIDOutput<<", "<<storage.TempValue<<", "<<storage.PhValue<<", "<<storage.OrpValue<<", "<<storage.PSIValue<<'\n';

  return (storage.ConfigVersion == CONFIG_VERSION);
}

void saveConfig() 
{
   //update function only writes to eeprom if the value is actually different. Increases the eeprom lifetime
   EEPROM.writeBlock(configAdress, storage);
}

// Print Screen1 to the LCD.
void LCDScreen1()
{ 
  //Concatenate data into one buffer then print it ot the 20x4 LCD
  ///!\LCD driver wrapps lines in a strange order: line 1, then 3, then 2 then 4
  lcd.setCursor(0, 0);
  memset(LCD_Buffer, 0, sizeof(LCD_Buffer));
  char *p = &LCD_Buffer[0];
  char buff2[10];
  char buff3[13];
  uint8_t Nb = 0;
  uint8_t TNb = 0;

  Nb = sprintf(p, "Rx:%3dmV       ",(int)storage.OrpValue); p += Nb; TNb += Nb; 
  Nb = sprintf(p, "(%3d)",(int)storage.Orp_SetPoint); p += Nb; TNb += Nb; 
  dtostrf(storage.TempValue,5,2,buff2);
  Nb = sprintf(p, "W:%5sC",buff2); p += Nb; TNb += Nb; 
  dtostrf(storage.TempExternal,1,1,buff2);
  sprintf(buff3, "A:%sC",buff2);
  Nb = sprintf(p, "%12s",buff3); p += Nb; TNb += Nb; 
  dtostrf(storage.PhValue,4,2,buff2);
  Nb = sprintf(p, "pH:%4s       ",buff2); p += Nb; TNb += Nb; 
  dtostrf(storage.Ph_SetPoint,3,1,buff2);
  Nb = sprintf(p, " (%3s)",buff2); p += Nb; TNb += Nb; 
  dtostrf(storage.PSIValue,4,2,buff2);
  Nb = sprintf(p, "P: %4sb      ",buff2); p += Nb; TNb += Nb; 
  Nb = sprintf(p, "%2d/%2dh",storage.FiltrationStart,storage.FiltrationStop); TNb += Nb; 

  if(TNb <= sizeof(LCD_Buffer))
  {
    lcd.print(LCD_Buffer);
    Serial<<F("Printed some characters to LCD Screen1: ")<<TNb<<endl;
  }
  else
  {
    sprintf(LCD_Buffer, "Trying to print %d chars to LCD1",TNb);
    MQTTClient.publish("Home/Pool/Err",LCD_Buffer,true,LWMQTT_QOS1);
    Serial<<F("Trying to print too many characters to LCD Screen1: ")<<TNb<<endl;
  }
}
/*
// Print Screen1 to the LCD.
void LCDScreen1()
{ 
  //Concatenate data into one buffer then print it ot the 20x4 LCD
  ///!\LCD driver wrapps lines in a strange order: line 1, then 3, then 2 then 4
  //Here we reuse the MQTT Payload buffer while it is not being used
  lcd.setCursor(0, 0);
  memset(Payload, 0, sizeof(Payload));
  char *p = &Payload[0];
  char buff2[10];
  char *p2 = &buff2[0];
      
  p += sprintf(p, "Redox:%4dmV   ",(int)storage.OrpValue);
  p += sprintf(p, "(%3d)",(int)storage.Orp_SetPoint);
  dtostrf(storage.TempValue,5,2,buff2);
  p += sprintf(p, "Water:%5sC  ",buff2);
  p += sprintf(p, "ON:%2dh",storage.FiltrationStart);
  dtostrf(storage.PhValue,4,2,buff2);
  p += sprintf(p, "pH:    %4s   ",buff2);
  dtostrf(storage.Ph_SetPoint,3,1,buff2);
  p += sprintf(p, " (%3s)",buff2);
  dtostrf(storage.TempExternal,6,2,buff2);
  p += sprintf(p, "Air: %6sC  ",buff2);
  p += sprintf(p, "OF:%2dh",storage.FiltrationStop);
  lcd.print(Payload);
}
*/

// Print Screen2 to the LCD.
void LCDScreen2()
{
  //Concatenate data into one buffer then print it ot the 20x4 LCD
  ///!\LCD driver wrapps lines in a strange order: line 1, then 3, then 2 then 4
  //Here we reuse the MQTT Payload buffer while it is not being used
  lcd.setCursor(0, 0);
  memset(LCD_Buffer, 0, sizeof(LCD_Buffer));
  char *p = &LCD_Buffer[0];
  uint8_t Nb = 0;
  uint8_t TNb = 0;
  
  Nb = sprintf(p, "Auto:%d      ",AutoMode); p += Nb; TNb += Nb;
  Nb = sprintf(p, "%02d:%02d:%02d",hour(),minute(),second()); p += Nb; TNb += Nb;
  Nb = sprintf(p, "Cl pump:%2dmn  ",(int)(ChlPump.UpTime/1000/60));  p += Nb; TNb += Nb;
  Nb = sprintf(p, " Err:%d",ChlPump.UpTimeError); p += Nb; TNb += Nb;
  Nb = sprintf(p, "pH pump:%2dmn  ",(int)(PhPump.UpTime/1000/60)); p += Nb; TNb += Nb;
  Nb = sprintf(p, " Err:%d",PhPump.UpTimeError); p += Nb; TNb += Nb;
  Nb = sprintf(p, "pHTank : %d  ",PhPump.TankLevel()); p += Nb; TNb += Nb;
  Nb = sprintf(p, "ClTank:%d",ChlPump.TankLevel());  TNb += Nb;

  if(TNb <= sizeof(LCD_Buffer))
  {
    lcd.print(LCD_Buffer);
    Serial<<F("Printed some characters to LCD Screen2: ")<<TNb<<endl;
  }
  else
  {
    sprintf(LCD_Buffer, "Trying to print %d chars to LCD2",TNb);
    MQTTClient.publish("Home/Pool/Err",LCD_Buffer,true,LWMQTT_QOS1);
    Serial<<F("Trying to print too many characters to LCD Screen2: ")<<TNb<<endl;
  }
}

void ProcessCommand(String JSONCommand)
{
        //Json buffer
      StaticJsonBuffer<200> jsonBuffer;
      
      //Parse Json object and find which command it is
      JsonObject& command = jsonBuffer.parseObject(JSONCommand);
      
      // Test if parsing succeeds.
      if (!command.success()) 
      {
        Serial<<F("Json parseObject() failed");
        return;
      }
      else
      {
        Serial<<F("Json parseObject() success - ")<<endl;

        //Provide the external temperature. Should be updated regularly and will be used to start filtration for 10mins every hour when temperature is negative
        if(command.containsKey("TempExt"))
        {
          storage.TempExternal = (float)command["TempExt"];
          Serial<<F("External Temperature: ")<<storage.TempExternal<<F("deg")<<endl;
        }
        else
        //"PhCalib" command which computes and sets the calibration coefficients of the pH sensor response based on a multi-point linear regression
        //{"PhCalib":[4.02,3.8,9.0,9.11]}  -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
        if (command.containsKey("PhCalib"))
        {
          float CalibPoints[12];//Max six calibration point-couples! Should be plenty enough
          int NbPoints = command["PhCalib"].as<JsonArray>().copyTo(CalibPoints);
          Serial<<F("PhCalib command - ")<<NbPoints<<F(" points received: ");
          for(int i=0;i<NbPoints;i+=2)
            Serial<<CalibPoints[i]<<F(",")<<CalibPoints[i+1]<<F(" - ");
          Serial<<_endl;

          if(NbPoints == 2)//Only one pair of points. Perform a simple offset calibration
          {
            Serial<<F("2 points. Performing a simple offset calibration")<<_endl;

            //compute offset correction
            storage.pHCalibCoeffs1 += CalibPoints[1] - CalibPoints[0];

            //Set slope back to default value
            storage.pHCalibCoeffs0 = 3.76;

            //Store the new coefficients in eeprom
            saveConfig();
            Serial<<F("Calibration completed. Coeffs are: ")<<storage.pHCalibCoeffs0<<F(",")<<storage.pHCalibCoeffs1<<_endl;   
          }
          else
          if((NbPoints>3) && (NbPoints%2 == 0))//we have at least 4 points as well as an even number of points. Perform a linear regression calibration
          {         
            Serial<<NbPoints/2<<F(" points. Performing a linear regression calibration")<<_endl;

            float xCalibPoints[NbPoints/2];
            float yCalibPoints[NbPoints/2];

            //generate array of x sensor values (in volts) and y rated buffer values
            //storage.PhValue = (storage.pHCalibCoeffs0 * ph_sensor_value) + storage.pHCalibCoeffs1;
            for(int i=0;i<NbPoints;i+=2)
            {
              xCalibPoints[i/2] = (CalibPoints[i] - storage.pHCalibCoeffs1)/storage.pHCalibCoeffs0;
              yCalibPoints[i/2] = CalibPoints[i+1];
            }

            //Compute linear regression coefficients
            simpLinReg(xCalibPoints, yCalibPoints, storage.pHCalibCoeffs0, storage.pHCalibCoeffs1, NbPoints/2);

            //Store the new coefficients in eeprom
            saveConfig();
            
            Serial<<F("Calibration completed. Coeffs are: ")<<storage.pHCalibCoeffs0<<F(",")<<storage.pHCalibCoeffs1<<_endl;   
          }  
        }
        else
        //"OrpCalib" command which computes and sets the calibration coefficients of the Orp sensor response based on a multi-point linear regression
        //{"OrpCalib":[450,465,750,784]}   -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
        if (command.containsKey("OrpCalib"))
        {
          float CalibPoints[12];//Max six calibration point-couples! Should be plenty enough
          int NbPoints = command["OrpCalib"].as<JsonArray>().copyTo(CalibPoints);
          Serial<<F("OrpCalib command - ")<<NbPoints<<F(" points received: ");
          for(int i=0;i<NbPoints;i+=2)
            Serial<<CalibPoints[i]<<F(",")<<CalibPoints[i+1]<<F(" - ");
          Serial<<_endl;

          if(NbPoints == 2)//Only one pair of points. Perform a simple offset calibration
          {
            Serial<<F("2 points. Performing a simple offset calibration")<<_endl;

            //compute offset correction
            storage.OrpCalibCoeffs1 += CalibPoints[1] - CalibPoints[0];

            //Set slope back to default value
            storage.OrpCalibCoeffs0 = -1282.39;

            //Store the new coefficients in eeprom
            saveConfig();
            Serial<<F("Calibration completed. Coeffs are: ")<<storage.OrpCalibCoeffs0<<F(",")<<storage.OrpCalibCoeffs1<<_endl;   
         }
          else
          if((NbPoints>3) && (NbPoints%2 == 0))//we have at least 4 points as well as an even number of points. Perform a linear regression calibration
          {         
            Serial<<NbPoints/2<<F(" points. Performing a linear regression calibration")<<_endl;

            float xCalibPoints[NbPoints/2];
            float yCalibPoints[NbPoints/2];

            //generate array of x sensor values (in volts) and y rated buffer values
            //storage.OrpValue = (storage.OrpCalibCoeffs0 * orp_sensor_value) + storage.OrpCalibCoeffs1; 
            for(int i=0;i<NbPoints;i+=2)
            {
              xCalibPoints[i/2] = (CalibPoints[i] - storage.OrpCalibCoeffs1)/storage.OrpCalibCoeffs0;
              yCalibPoints[i/2] = CalibPoints[i+1];
            }

            //Compute linear regression coefficients
            simpLinReg(xCalibPoints, yCalibPoints, storage.OrpCalibCoeffs0, storage.OrpCalibCoeffs1, NbPoints/2);

            //Store the new coefficients in eeprom
            saveConfig();
           
            Serial<<F("Calibration completed. Coeffs are: ")<<storage.OrpCalibCoeffs0<<F(",")<<storage.OrpCalibCoeffs1<<_endl;   
          }   
        }
        else //"Mode" command which sets regulation and filtration to manual or auto modes
        if (command.containsKey("Mode"))
        {
            if((int)command["Mode"]==0)
            {
              AutoMode = 0;
              
              //Stop PIDs
              SetPhPID(false);
              SetOrpPID(false);
            }
            else
            {
              AutoMode = 1;
            }
        }
        else 
        if (command.containsKey("FiltPump")) //"FiltPump" command which starts or stops the filtration pump
        {
            if((int)command["FiltPump"]==0)
              FiltrationPump.Stop();  //stop filtration pump
            else
              FiltrationPump.Start();   //start filtration pump
        }
        else  
        if(command.containsKey("PhPump"))//"PhPump" command which starts or stops the Acid pump
        {          
            if((int)command["PhPump"]==0)
              PhPump.Stop();          //stop Acid pump
            else
              PhPump.Start();           //start Acid pump
        } 
        else
        if(command.containsKey("ChlPump"))//"ChlPump" command which starts or stops the Acid pump
        {          
            if((int)command["ChlPump"]==0)
              ChlPump.Stop();          //stop Chl pump
            else
              ChlPump.Start();           //start Chl pump
        } 
        else
        if(command.containsKey("PhPID"))//"PhPID" command which starts or stops the Ph PID loop
        {          
            if((int)command["PhPID"]==0)
            {                
              //Stop PID
              SetPhPID(false);
            }
            else
            {
              //Initialize PIDs StartTime
              storage.PhPIDwindowStartTime = millis();
              storage.OrpPIDwindowStartTime = millis();

              //Start PID
              SetPhPID(true);
            }
        } 
        else
        if(command.containsKey("OrpPID"))//"OrpPID" command which starts or stops the Orp PID loop
        {          
            if((int)command["OrpPID"]==0)
            {    
              //Stop PID
              SetOrpPID(false);
            }
            else
            {      
              //Start PID
              SetOrpPID(true);           
            }
        }  
        else  
        if(command.containsKey("PhSetPoint"))//"PhSetPoint" command which sets the setpoint for Ph
        {          
           storage.Ph_SetPoint = (float)command["PhSetPoint"];
           saveConfig();
        } 
        else
        if(command.containsKey("OrpSetPoint"))//"OrpSetPoint" command which sets the setpoint for ORP
        {          
           storage.Orp_SetPoint = (float)command["OrpSetPoint"];
           saveConfig();
        }       
        else 
        if(command.containsKey("WSetPoint"))//"WSetPoint" command which sets the setpoint for Water temp (currently not in use)
        {          
           storage.WaterTemp_SetPoint = (float)command["WSetPoint"];
           saveConfig();
        }   
        else
        if(command.containsKey("WTempLow"))//"WTempLow" command which sets the setpoint for Water temp low threshold
        {          
           storage.WaterTempLowThreshold = (float)command["WTempLow"];
           saveConfig();
        }
        else 
        if(command.containsKey("PumpsMaxUp"))//"PumpsMaxUp" command which sets the Max UpTime for pumps
        {          
           storage.PhPumpUpTimeLimit = (unsigned int)command["PumpsMaxUp"];
           PhPump.SetMaxUpTime(storage.PhPumpUpTimeLimit*1000);
           storage.ChlPumpUpTimeLimit = (unsigned int)command["PumpsMaxUp"];
           ChlPump.SetMaxUpTime(storage.ChlPumpUpTimeLimit*1000);
           saveConfig();
        }
        else
        if(command.containsKey("OrpPIDParams"))//"OrpPIDParams" command which sets the Kp, Ki and Kd values for Orp PID loop
        {          
           storage.Orp_Kp = (double)command["OrpPIDParams"][0];
           storage.Orp_Ki = (double)command["OrpPIDParams"][1];
           storage.Orp_Kd = (double)command["OrpPIDParams"][2];
           saveConfig();
           OrpPID.SetTunings(storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd);
        }
        else 
        if(command.containsKey("PhPIDParams"))//"PhPIDParams" command which sets the Kp, Ki and Kd values for Ph PID loop
        {          
           storage.Ph_Kp = (double)command["PhPIDParams"][0];
           storage.Ph_Ki = (double)command["PhPIDParams"][1];
           storage.Ph_Kd = (double)command["PhPIDParams"][2];
           saveConfig();
           PhPID.SetTunings(storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd);
        }
        else
        if(command.containsKey("OrpPIDWSize"))//"OrpPIDWSize" command which sets the window size of the Orp PID loop
        {          
           storage.OrpPIDWindowSize = (unsigned long)command["OrpPIDWSize"];
           saveConfig();
        }
        else
        if(command.containsKey("PhPIDWSize"))//"PhPIDWSize" command which sets the window size of the Ph PID loop
        {          
           storage.PhPIDWindowSize = (unsigned long)command["PhPIDWSize"];
           saveConfig();
        }
        else
        if(command.containsKey("Date"))//"Date" command which sets the Date of RTC module
        {         
          #if defined(CONTROLLINO_MAXI)
           Controllino_SetTimeDate((uint8_t)command["Date"][0],(uint8_t)command["Date"][1],(uint8_t)command["Date"][2],(uint8_t)command["Date"][3],(uint8_t)command["Date"][4],(uint8_t)command["Date"][5],(uint8_t)command["Date"][6]); // set initial values to the RTC chip. (Day of the month, Day of the week, Month, Year, Hour, Minute, Second) 
          #endif

          setTime((uint8_t)command["Date"][4],(uint8_t)command["Date"][5],(uint8_t)command["Date"][6],(uint8_t)command["Date"][1],(uint8_t)command["Date"][2],(uint8_t)command["Date"][3]); //(Day of the month, Day of the week, Month, Year, Hour, Minute, Second)    
        }
        else
        if(command.containsKey("FiltT0"))//"FiltT0" command which sets the earliest hour when starting Filtration pump
        {          
           storage.FiltrationStart = (unsigned int)command["FiltT0"];
           saveConfig();
        }
        else 
        if(command.containsKey("FiltT1"))//"FiltT1" command which sets the latest hour for running Filtration pump
        {          
           storage.FiltrationStopMax = (unsigned int)command["FiltT1"];
           saveConfig();
        }
        else
        if(command.containsKey("PubPeriod"))//"PubPeriod" command which sets the periodicity for publishing system info to MQTT broker
        {    
          PublishPeriod = (unsigned long)command["PubPeriod"]*1000; //in secs
          t4.setPeriodMs(PublishPeriod); //in msecs
          saveConfig();
        }
        else
        if(command.containsKey("Clear"))//"Clear" command which clears the UpTime and pressure errors of the Pumps
        { 
          if(PSIError)
            PSIError = false;
            
          if(PhPump.UpTimeError) 
            PhPump.ClearErrors();

          if(ChlPump.UpTimeError) 
            ChlPump.ClearErrors();  
        }
        else
        if(command.containsKey("DelayPID"))//"DelayPID" command which sets the delay from filtering start before PID loops start regulating
        {
          storage.DelayPIDs = (unsigned int)command["DelayPID"];
          saveConfig();
        }
        else
        if(command.containsKey("PSIHigh"))//"PSIHigh" command which sets the water high-pressure threshold
        {
          storage.PSI_HighThreshold = (float)command["PSIHigh"];
          saveConfig();
        }
                        
        //Publish/Update on the MQTT broker the status of our variables
        PublishDataCallback(NULL);    
      }
}

//Compute free RAM
//useful to check if it does not shrink over time
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


////////////////////////gettemp state machine///////////////////////////////////
//Init DS18B20 one-wire library
void gettemp_start()
{
    //String containing MAC address of temp sensor to be written to XML file
    sDS18b20_0 = F("<DS18b20_0 Mac='0x28, 0x92, 0x25, 0x41, 0x0A, 0x00, 0x00, 0xEE'>");
    
    // Start up the library
    sensors_A.begin();
       
    // set the resolution
    sensors_A.setResolution(DS18b20_0, TEMPERATURE_RESOLUTION);

    //don't wait ! Asynchronous mode
    sensors_A.setWaitForConversion(false); 
       
    gettemp.next(gettemp_request);
} 

//Request temperature asynchronously
void gettemp_request()
{
  sensors_A.requestTemperatures();
  gettemp.next(gettemp_wait);
}

//Wait asynchronously for requested temperature measurement
void gettemp_wait()
{ //we need to wait that time for conversion to finish
  if (gettemp.elapsed(1000/(1<<(12-TEMPERATURE_RESOLUTION))))
    gettemp.next(gettemp_read);
}

//read and print temperature measurement
void gettemp_read()
{
    sprintf(TimeBuffer,"%d-%02d-%02d %02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());  
    getMeasures(DS18b20_0); 
    gettemp.next(gettemp_request);
}

//Linear regression coefficients calculation function
// pass x and y arrays (pointers), lrCoef pointer, and n.  
//The lrCoef array is comprised of the slope=lrCoef[0] and intercept=lrCoef[1].  n is the length of the x and y arrays.
//http://jwbrooks.blogspot.com/2014/02/arduino-linear-regression-function.html
void simpLinReg(float* x, float* y, double &lrCoef0, double &lrCoef1, int n)
{
  // initialize variables
  float xbar=0;
  float ybar=0;
  float xybar=0;
  float xsqbar=0;
  
  // calculations required for linear regression
  for (int i=0; i<n; i++)
  {
    xbar+=x[i];
    ybar+=y[i];
    xybar+=x[i]*y[i];
    xsqbar+=x[i]*x[i];
  }
  
  xbar/=n;
  ybar/=n;
  xybar/=n;
  xsqbar/=n;
  
  // simple linear regression algorithm
  lrCoef0=(xybar-xbar*ybar)/(xsqbar-xbar*xbar);
  lrCoef1=ybar-lrCoef0*xbar;
}

//Ethernet client checking loop (the Web server sending the system webpage to the browser and/or the XML file containing the system info)
//call http://localIP/Info to obtain an XML file containing the system info
void EthernetClientCallback(Task* me)
{
    EthernetClient client = server.available();  // try to get client
    //readString = "";

    if (client) 
    {  // got client?
        boolean currentLineIsBlank = true;
        //Serial<<F("GotClient, current line is blank")<<_endl;
        while (client.connected()) 
        {
            if (client.available()) 
            {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                //Serial.println(F("GotClient, reading data"));
                //read char by char HTTP request
                if (readString.length() < 100) 
                {
                  //store characters to string
                  readString += c;
                  //Serial<<c;
                }
                //else
                //Serial<<"length over 100";
                
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) 
                {
                  // send a standard http response header
                    client<<F("HTTP/1.1 200 OK")<<_endl;
                    //Serial<<"replying HTTP/1.1 200 OK";
                    //Serial.println(F("HTTP/1.1 200 OK"));
                    // remainder of header follows below, depending on if
                    // web page or XML page is requested
                    
                    // Ajax request - send XML file
                    if(readString.indexOf("Info") >0)
                    { //checks for "Info" string
                        // send rest of HTTP header
                        client<<F("Content-Type: text/xml")<<_endl;
                        client<<F("Connection: keep-alive")<<_endl;
                        client<<_endl;
                        Serial.println(F("sending XML file"));
                        // send XML file containing input states
                        XML_response(client);
                        readString=F("");
                        break;
                    } 
                    else
                    {  
                        // web page request
                        // send rest of HTTP header
                        client<<F("Content-Type: text/html")<<_endl;
                        client<<F("Connection: keep-alive")<<_endl;
                        client<<_endl;
                        client<<F("<!DOCTYPE html>")<<_endl;
                        client<<F("<html>")<<_endl;
                            client<<F("<head>")<<_endl; 
                            
                               client<<F("<style>")<<_endl;
                               client<<F("table.blueTable {")<<_endl;
                               client<<F("border: 1px solid #1C6EA4;")<<_endl;
                               client<<F("background-color: #EEEEEE;")<<_endl;
                               client<<F("width: 70\%;")<<_endl;
                               client<<F("text-align: left;")<<_endl;
                               client<<F("border-collapse: collapse;")<<_endl;
                               client<<F("}")<<_endl;
                               client<<F("table.blueTable td, table.blueTable th {")<<_endl;
                               client<<F("border: 1px solid #AAAAAA;")<<_endl;
                               client<<F("padding: 3px 2px;")<<_endl;
                               client<<F("}")<<_endl;
                               client<<F("table.blueTable tbody td {")<<_endl;
                               client<<F("font-size: 14px;")<<_endl;
                               client<<F("}")<<_endl;
                               client<<F("table.blueTable tr:nth-child(even) {")<<_endl;
                               client<<F("background: #D0E4F5;")<<_endl;
                               client<<F("}")<<_endl;
                               client<<F("table.blueTable tfoot td {")<<_endl;
                               client<<F("font-size: 14px;")<<_endl;
                               client<<F("}")<<_endl;
                               client<<F("table.blueTable tfoot .links {")<<_endl;
                               client<<F("text-align: right;")<<_endl;
                               client<<F("}")<<_endl;
                               client<<F("table.blueTable tfoot .links a{")<<_endl;
                               client<<F("display: inline-block;")<<_endl;
                               client<<F("background: #1C6EA4;")<<_endl;
                               client<<F("color: #FFFFFF;")<<_endl;
                               client<<F("padding: 2px 8px;")<<_endl;
                               client<<F("border-radius: 5px;")<<_endl;
                               client<<F("}")<<_endl;
                               client<<F("</style>")<<_endl;
                               client<<F("<title></title>")<<_endl;

                               //Ajax script function which requests the Info XML file 
                               //from the Arduino and populates the web page with it, every two secs
                               client<<F("<script>")<<_endl;
                                client<<F("function GetData()")<<_endl;
                                client<<F("{")<<_endl;
                                    client<<F("nocache = \"&nocache=\" + Math.random() * 1000000;")<<_endl;
                                    client<<F("var request = new XMLHttpRequest();")<<_endl;
                                    client<<F("request.onreadystatechange = function()")<<_endl;
                                    client<<F("{")<<_endl;
                                        client<<F("if (this.readyState == 4) {")<<_endl;
                                            client<<F("if (this.status == 200) {")<<_endl;
                                                client<<F("if (this.responseXML != null) {")<<_endl;
                                                    // extract XML data from XML file
                                                    client<<F("document.getElementById(\"Date\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Date')[0].childNodes[0].nodeValue;")<<_endl;
          
                                                    client<<F("document.getElementById(\"WaterTemp\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('DS18b20_0')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pH\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Value')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORP\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Value')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"Filtration\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Pump')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"FiltStart\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Start')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"FiltStop\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Stop')[0].childNodes[0].nodeValue;")<<_endl;

                                                    client<<F("document.getElementById(\"pH2\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Value')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORP2\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Value')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHPump\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Pump')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPPump\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Pump')[2].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHTank\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('TankLevel')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPTank\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('TankLevel')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHPID\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('PIDMode')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPPID\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('PIDMode')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHSetPoint\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('SetPoint')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPSetPoint\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('SetPoint')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHCal0\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('CalibCoeff0')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHCal1\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('CalibCoeff1')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPCal0\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('CalibCoeff0')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPCal1\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('CalibCoeff1')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHKp\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Kp')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPKp\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Kp')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHKi\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Ki')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPKi\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Ki')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"pHKd\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Kd')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ORPKd\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('Kd')[1].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"PhPumpEr\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('PhPumpEr')[0].childNodes[0].nodeValue;")<<_endl;
                                                    client<<F("document.getElementById(\"ChlPumpEr\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('ChlPumpEr')[0].childNodes[0].nodeValue;")<<_endl; 
                                                    client<<F("document.getElementById(\"pHPT\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('UpT')[0].childNodes[0].nodeValue;")<<_endl; 
                                                    client<<F("document.getElementById(\"ChlPT\").innerHTML =")<<_endl;
                                                    client<<F("this.responseXML.getElementsByTagName('UpT')[1].childNodes[0].nodeValue;")<<_endl;
                                                    
          
                                                    client<<F("}")<<_endl;
                                            client<<F("}")<<_endl;
                                        client<<F("}")<<_endl;
                                    client<<F("}")<<_endl;
                                    client<<F("request.open(\"GET\", \"Info\" + nocache, true);")<<_endl;
                                    client<<F("request.send(null);")<<_endl;
                                    client<<F("setTimeout('GetData()', 4000);")<<_endl;
                                client<<F("}")<<_endl;
                               client<<F("</script>")<<_endl;
            
                            client<<F("</head>")<<_endl;
                            client<<F("<body onload=\"GetData()\">")<<_endl;
                            client<<F("<h1>Pool Master</h1>")<<_endl; 
                            client<<F("<h3><span id=\"Date\">...</span></h3>")<<_endl<<_endl; 
                  
                            //First table: Water temp, pH and ORP values
                            client<<F("<table class=\"blueTable\">")<<_endl;
                            client<<F("<tbody>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Water temp. (deg): <span id=\"WaterTemp\">...</span></td>")<<_endl;
                            client<<F("<td>pH: <span id=\"pH\">...</span></td>")<<_endl;
                            client<<F("<td>ORP (mV): <span id=\"ORP\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Filtration: <span id=\"Filtration\">...</span></td>")<<_endl;
                            client<<F("<td>Start: <span id=\"FiltStart\">...</span></td>")<<_endl;
                            client<<F("<td>Stop: <span id=\"FiltStop\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;                        
                            client<<F("</tbody>")<<_endl;
                            client<<F("</table>")<<_endl;
                            client<<F("<br>")<<_endl;

                            //Second table, pH and ORP parameters
                            client<<F("<table class=\"blueTable\">")<<_endl;
                            client<<F("<tbody>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<td></td>")<<_endl;
                            client<<F("<td>pH</td>")<<_endl;
                            client<<F("<td>ORP/Chl</td>")<<_endl;
                            client<<F("</tr>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Value</td>")<<_endl;
                            client<<F("<td><span id=\"pH2\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORP2\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;
                            client<<F("<td>SetPoint</td>")<<_endl;
                            client<<F("<td><span id=\"pHSetPoint\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPSetPoint\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Pump</td>")<<_endl;
                            client<<F("<td><span id=\"pHPump\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPPump\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;    
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Tank level</td>")<<_endl;
                            client<<F("<td><span id=\"pHTank\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPTank\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;               
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>PID</td>")<<_endl;
                            client<<F("<td><span id=\"pHPID\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPPID\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;  
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Calib. coeff0</td>")<<_endl;
                            client<<F("<td><span id=\"pHCal0\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPCal0\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;                         
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Calib. coeff1</td>")<<_endl;
                            client<<F("<td><span id=\"pHCal1\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPCal1\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Kp</td>")<<_endl;
                            client<<F("<td><span id=\"pHKp\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPKp\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;                       
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Ki</td>")<<_endl;
                            client<<F("<td><span id=\"pHKi\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPKi\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;                          
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Kd</td>")<<_endl;
                            client<<F("<td><span id=\"pHKd\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ORPKd\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl; 
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Pumps Uptime (sec):</td>")<<_endl;
                            client<<F("<td><span id=\"pHPT\">...</span></td>")<<_endl;
                            client<<F("<td><span id=\"ChlPT\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;                               
                            client<<F("</tbody>")<<_endl;
                            client<<F("</table>")<<_endl;
                            client<<F("<br>")<<_endl;

                            //Third table: Messages
                            client<<F("<table class=\"blueTable\">")<<_endl;
                            client<<F("<tbody>")<<_endl;
                            client<<F("<tr>")<<_endl;
                            client<<F("<td>Errors: <span id=\"Errors\">...</span></td>")<<_endl;
                            client<<F("<td>Acid Pump: <span id=\"PhPumpEr\">...</span></td>")<<_endl;
                            client<<F("<td>Chl Pump: <span id=\"ChlPumpEr\">...</span></td>")<<_endl;
                            client<<F("</tr>")<<_endl;
                            client<<F("</tbody>")<<_endl;
                            client<<F("</table>")<<_endl;
                            client<<F("<br>")<<_endl;
       
                            client<<F("</body>")<<_endl;
                        client<<F("</html>")<<_endl;              
                    }
                    // display received HTTP request on serial port
                    Serial<<readString<<_endl;
                        
                    // reset buffer index and all buffer elements to 0
                    readString=F("");
                    break;                
                }
                 
                // every line of text received from the client ends with \r\n
                if (c == '\n') 
                {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') 
                {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)  
}


//Return an XML file with system data
//call http://localIP/Info to obtain an XML file containing the system info
void XML_response(EthernetClient cl)
{
    cl<<F("<?xml version = \"1.0\" encoding=\"UTF-8\"?>");
    cl<<F("<root>");
    
        cl<<F("<device>");
            cl<<F("<Date>");//day, weekday, month, year, hour, minute, sec
                cl<<TimeBuffer;
            cl<<F("</Date>");
            cl<<F("<Firmware>");
                cl<<Firmw;
            cl<<F("</Firmware>");
            cl<<F("<FreeRam>");
                cl<<freeRam();
            cl<<F("</FreeRam>");
            cl<<F("<IP>");
                cl<<Ethernet.localIP();
            cl<<F("</IP>");
            cl<<F("<Mac>");
                cl<<sArduinoMac;
            cl<<F("</Mac>");
            cl<<sDS18b20_0;
                cl<<storage.TempValue;
            cl<<F("</DS18b20_0>");
            cl<<F("<PhPumpEr>");
                cl<<PhPump.UpTimeError;
            cl<<F("</PhPumpEr>");
            cl<<F("<ChlPumpEr>");
                cl<<ChlPump.UpTimeError;
            cl<<F("</ChlPumpEr>");
        cl<<F("</device>");

        cl<<F("<Filtration>");
            cl<<F("<Pump>");
                cl<<FiltrationPump.IsRunning();
            cl<<F("</Pump>"); 
            cl<<F("<Duration>");
                cl<<storage.FiltrationDuration;
            cl<<F("</Duration>"); 
            cl<<F("<Start>");
                cl<<storage.FiltrationStart;
            cl<<F("</Start>"); 
            cl<<F("<Stop>");
                cl<<storage.FiltrationStop;
            cl<<F("</Stop>");
            cl<<F("<StopMax>");
                cl<<storage.FiltrationStopMax;
            cl<<F("</StopMax>");
        cl<<F("</Filtration>");
                    
        cl<<F("<pH>");
            cl<<F("<Value>");
                cl<<storage.PhValue;
            cl<<F("</Value>");           
            cl<<F("<Pump>");
                cl<<PhPump.IsRunning();
            cl<<F("</Pump>");
            cl<<F("<UpT>");
                cl<<PhPump.UpTime/1000;
            cl<<F("</UpT>");
            cl<<F("<TankLevel>");
                cl<<PhPump.TankLevel();
            cl<<F("</TankLevel>");
            cl<<F("<PIDMode>");
                cl<<storage.Ph_RegulationOnOff;
            cl<<F("</PIDMode>");
            cl<<F("<Kp>");
                cl<<storage.Ph_Kp;
            cl<<F("</Kp>");
            cl<<F("<Ki>");
                cl<<storage.Ph_Ki;
            cl<<F("</Ki>");
            cl<<F("<Kd>");
                cl<<storage.Ph_Kd;
            cl<<F("</Kd>");
            cl<<F("<SetPoint>");
                cl<<storage.Ph_SetPoint;
            cl<<F("</SetPoint>");
            cl<<F("<CalibCoeff0>");
                cl<<storage.pHCalibCoeffs0;
            cl<<F("</CalibCoeff0>");
            cl<<F("<CalibCoeff1>");
                cl<<storage.pHCalibCoeffs1;
            cl<<F("</CalibCoeff1>");       
        cl<<F("</pH>");

        cl<<F("<ORP>");
            cl<<F("<Value>");
                cl<<storage.OrpValue;
            cl<<F("</Value>");           
            cl<<F("<Pump>");
                cl<<ChlPump.IsRunning();
            cl<<F("</Pump>");
            cl<<F("<UpT>");
                cl<<ChlPump.UpTime/1000;
            cl<<F("</UpT>");
            cl<<F("<TankLevel>");
                cl<<ChlPump.TankLevel();
            cl<<F("</TankLevel>");
            cl<<F("<PIDMode>");
                cl<<storage.Orp_RegulationOnOff;
            cl<<F("</PIDMode>");
            cl<<F("<Kp>");
                cl<<storage.Orp_Kp;
            cl<<F("</Kp>");
            cl<<F("<Ki>");
                cl<<storage.Orp_Ki;
            cl<<F("</Ki>");
            cl<<F("<Kd>");
                cl<<storage.Orp_Kd;
            cl<<F("</Kd>");
            cl<<F("<SetPoint>");
                cl<<storage.Orp_SetPoint;
            cl<<F("</SetPoint>");
            cl<<F("<CalibCoeff0>");
                cl<<storage.OrpCalibCoeffs0;
            cl<<F("</CalibCoeff0>");
            cl<<F("<CalibCoeff1>");
                cl<<storage.OrpCalibCoeffs1;
            cl<<F("</CalibCoeff1>");
        cl<<F("</ORP>");
        
    cl<<F("</root>");
}
