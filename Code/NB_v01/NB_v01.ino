#include <WiFi.h>
#include <PubSubClient.h>
#include <credentials.h>
#include <ESP32Time.h>            // https://github.com/fbiego/ESP32Time
#include <OneWire.h>              // https://www.arduino.cc/reference/en/libraries/onewire/
#include <DallasTemperature.h>    // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include "ATcommands.h"           // https://github.com/R4sp1/AT-Command-Library
#include "esp_adc_cal.h"          // https://github.com/espressif/arduino-esp32/
#include <BH1750.h>               // https://github.com/claws/BH1750
#include <Wire.h>

#define DEBUG   //Comment this line if you want to "debug" with Serial.print()
#define NB    //Comment this line if you want to use WiFi instead

//Deep sleep related definitions
#define uS_TO_S_FACTOR 1000000     //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  300         //Time ESP will go to sleep (in minutes)

#ifdef NB
  //NB related definitions
  #define MCU_RX 16     // Remember MCU RX connects to module TX and vice versa
  #define MCU_TX 17
  #define RST 18        // MCU pin to control module reset
  #define PSM 5        // MCU pin to control module wake up pin (PSM-EINT_N)
  #define NBdelay 1000  // Delay between two AT commands

  HardwareSerial *moduleSerial = &Serial2;
  ATcommands module = ATcommands(RST, PSM, true);
#else
  //WIFI definitions
  const char* ssid = mySSID;
  const char* password = myPASSWORD;
  const char* mqtt_server = mqttSERVER;
  WiFiClient espClient;
  PubSubClient client(espClient);
#endif

//Sensors related definitions
#define TPIN 32     // DS18B20 pin
#define VOL_SENS 34 // Pin to measure voltage of battery
#define CAL 2       // Voltage devider ratio
#define EN 33       // Pin to power on sensors


//Data stored in RTC memory
RTC_DATA_ATTR int sendCount = 0;
RTC_DATA_ATTR ulong wakeUpEpoch = 0;
RTC_DATA_ATTR int rainCount = 0;


char msg[200];
String MQTTtopic = "Test/NB";

String JSONmessage;
float temp1;
float temp2;
float light;
float Voltage;      // Voltage in mV
int volSens_Result; // Raw value from ADC

//Sensors definitions
OneWire oneWire(TPIN); //OneWire
DallasTemperature oneWireTemp(&oneWire);
ESP32Time rtc;  
BH1750 lightMeter(0x23);

int timeToSleep = 0;
ulong currentEpoch = 0;

double round2(double val) {           //Round fun to get only two decimal places
   return (int)(val*100+0.5)/100.0;
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setupSensors(){
    pinMode(EN, OUTPUT);
    digitalWrite(EN, HIGH);   //Turn on sensors
    delay(100);               //Delay to let sensors "boot"
    Wire.begin (21, 22);      //I2C
    oneWireTemp.begin();      //Onewire
    if (lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE_2)) {   //BH1750 lightmeter
      #ifdef DEBUG
      Serial.println(F("BH1750 Advanced begin"));
      #endif
    } else {
      #ifdef DEBUG
      Serial.println(F("Error initialising BH1750"));
      #endif
    }
}

void readSensors(){
  //First Temp sensor
  oneWireTemp.requestTemperatures();      //Send command to measure temperatures on each sensor on the bus
  temp1 = oneWireTemp.getTempCByIndex(0); //Index 0 => first sensor on wire
  #ifdef DEBUG
  Serial.print("First DS18B temp: ");
  Serial.print(temp1);
  Serial.println(" °C");
  Serial.println();
  #endif

  //Second Temp sensor
  //oneWireTemp.requestTemperatures();    //Temp requested before first sensor read
  temp2 = oneWireTemp.getTempCByIndex(1); //Index 1 => second sensor on wire
  #ifdef DEBUG
  Serial.print("Second DS18B temp: ");
  Serial.print(temp2);
  Serial.println(" °C");
  Serial.println();
  #endif

  //Voltage of battery
  volSens_Result = analogRead(VOL_SENS);
  Voltage = readADC_Cal(volSens_Result) * CAL;
  #ifdef DEBUG
  Serial.print(Voltage/1000.0); // Print Voltage (in V)
  Serial.println(" V");
  Serial.println(Voltage);      // Print Voltage (in mV)
  Serial.println(" mV");
  #endif

  //Light meter
  if (lightMeter.measurementReady()) {
    light = lightMeter.readLightLevel();
    #ifdef DEBUG
    Serial.print("Light: ");
    Serial.print(light);
    Serial.println(" lx");
    #endif
  }
}


uint32_t readADC_Cal(int ADC_Raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return(esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

bool sleepLogic(){
  currentEpoch = rtc.getEpoch();

  if(currentEpoch < wakeUpEpoch){
    timeToSleep = (wakeUpEpoch - currentEpoch) * uS_TO_S_FACTOR;
    #ifdef DEBUG
    Serial.println("ESP will sleep for: " + String(timeToSleep/1000000) + " seconds");
    #endif
  } else {
    timeToSleep = TIME_TO_SLEEP * uS_TO_S_FACTOR;
    wakeUpEpoch = (currentEpoch + TIME_TO_SLEEP);
    #ifdef DEBUG
    Serial.println("ESP will sleep for: " + String(timeToSleep/1000000) + " seconds");
    #endif
  }

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1);
  esp_sleep_enable_timer_wakeup(timeToSleep);

  if(esp_sleep_get_wakeup_cause() == 2){      // 2=ext0, 4=timer
    #ifdef DEBUG
    Serial.println("Wake = 2; pushbutton/rain sensor");
    #endif
    return true;
  } else {
    #ifdef DEBUG
    Serial.println("Wake = 4; timer");
    #endif
    return false;
  }
}

String prepMSG(){
  //char msg[200];
  StaticJsonDocument<200> doc;
    doc["send"] = sendCount;
    doc["temp"] = round2(temp1);
    doc["temp2"] = round2(temp2);
    doc["vol"] = round2(Voltage/1000.0);
    doc["light"] = round2(light);
    doc["rain"] = rainCount;
  serializeJson(doc, JSONmessage);
  //JSONmessage.toCharArray(msg, JSONmessage.length() + 1);
  
  #ifdef DEBUG
  Serial.print("Publish message: ");
  Serial.println(JSONmessage);
  Serial.print("Lenght of message: ");
  Serial.println(JSONmessage.length());

  Serial.println();
  delay(1000);
  #endif

  #ifdef NB
  String MQTTsend = "AT+QMTPUB=0,0,0,0,";
  //String result = MQTTsend + MQTTtopic + "," + strlen(msg) + "," + msg;
  String result = MQTTsend + MQTTtopic + "," + JSONmessage.length() + "," + JSONmessage;
  #else
  String result = JSONmessage;
  #endif

  return result;
}

void transmitData() {

  #ifdef NB
  moduleSerial->begin(115200);
  module.begin(*moduleSerial);
  delay(100); 
  

  /*
  if(module.wakeUp(HIGH,10,"DEEPSLEEP")) Serial.println("Wake up succes!"); //Wakeup module
  delay(NBdelay);
  */
  if(module.sendCommand("AT", "DEEPSLEEP")) Serial.println("Set AT succes!");  //Turn off deepsleep
  delay(NBdelay);

  if(module.sendCommand("AT+QSCLK=0", "OK", 5000)) Serial.println("Set QSCLK=0 succes!");  //Turn off deepsleep
  delay(NBdelay);

  /*
  if(module.sendCommand("ATE0", "OK")) Serial.println("Set ATE0 succes!"); //Turn off local echo
  delay(NBdelay);
  */
  
  if(module.sendCommand("AT+CGPADDR?", "+CGPADDR: 0,")) Serial.println("Check IP succes!");  //Check if we have IP addr assigned
  delay(NBdelay);

  if(module.sendCommand("AT+QMTOPEN=0,89.221.217.87,1883", "+QMTOPEN: 0,0")) Serial.println("Set QMTOPEN succes!");  //Open MQTT connection
  delay(NBdelay);

  if(module.sendCommand("AT+QMTCONN=0,DevKit-0123456", "+QMTCONN: 0,0,0")) Serial.println("Set QMTCONN succes!");  //Connect to MQTT server with ID
  delay(NBdelay);
  
  char MQTTpub[200];
  String res = prepMSG();
  res.toCharArray(MQTTpub, res.length()+1);
  if(module.sendCommand(MQTTpub, "+QMTPUB: 0,0,0")) Serial.println("Send QMTPUB succes!");  //Publish msg
  delay(NBdelay);

  if(module.sendCommand("AT+QMTCLOSE=0", "+QMTCLOSE: 0,0")) Serial.println("Close QMTCLOSE succes!");  //Close MQTT connection
  delay(NBdelay);

  if(module.sendCommand("AT+QSCLK=1", "OK")) Serial.println("Set QSCLK=1 succes!");  //Turn on deepsleep
  delay(NBdelay);
  #else
  //Setup WiFi connection
  delay(10);
  #ifdef DEBUG
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    #ifdef DEBUG
    Serial.print(".");
    #endif
  }
  #ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  #endif

  //WiFi MQTT connect
  client.setServer(mqtt_server, 1883);
  while (!client.connected()) {
    #ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
    #endif
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
    #ifdef DEBUG
      Serial.println("connected");
    #endif
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    }
  }

  //WiFi MQTT publish
  char MQTTpub[200];
  char MQTTtop[200];
  String res = prepMSG();
  res.toCharArray(MQTTpub, res.length()+1);
  MQTTtopic.toCharArray(MQTTtop, MQTTtopic.length()+1);

  client.publish(MQTTtop, MQTTpub);

  //End WiFi relation
  client.disconnect();
  delay(100);
  WiFi.disconnect(true, false);
  #endif
}

void callBack(){
  rainCount++;
  #ifdef DEBUG
  Serial.println("Rain count incremented by interrupt to: " + String(rainCount));
  #endif
}

void setup() {
  Serial.begin(115200);
  delay(100);
  attachInterrupt(4, callBack, HIGH);

  if(sleepLogic()){
      rainCount++;
      #ifdef DEBUG
      Serial.println("Rain count: " + String(rainCount));
      print_wakeup_reason();
      delay(1000);
      Serial.println("Going to sleep now");
      #endif
      digitalWrite(EN, LOW);
      delay(100);
      esp_deep_sleep_start();
    } else {
      ++sendCount;
      setupSensors();
      readSensors();
      transmitData();
      rainCount = 0;
      #ifdef DEBUG
      Serial.println("Data send!");
      delay(1000);
      Serial.println("Going to sleep now");
      #endif
      digitalWrite(EN, LOW);
      delay(100);
      esp_deep_sleep_start();
    }
  }

  void loop() {
  }