#define DEBUG 

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include "ATcommands.h"


#define uS_TO_MIN_FACTOR 60000000 //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  15         //Time ESP will go to sleep (in minutes)
#define MCU_RX 16 // Remember MCU RX connects to module TX and vice versa
#define MCU_TX 17
#define RST 5 // MCU pin to control module reset
#define PSM 18 // MCU pin to control module wake up pin (PSM-EINT_N)

HardwareSerial *moduleSerial = &Serial2;

long lastMsg = 0;
char msg[50];
int value = 0;
String MQTTtopic = "Test/NB";

//Sensors definitions
Adafruit_BME280 bme; // I2C

ATcommands module = ATcommands(RST, PSM, true);

String JSONmessage;
float temp;
int NBdelay = 1000;

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
    Wire.begin (21, 22);

    if (!bme.begin(0x76)) {
      #ifdef DEBUG
      Serial.println("Could not find a valid bme280 sensor, check wiring!");
      #endif
      //while (1);
    }

    //BME280 forced mode
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF,
                    Adafruit_BME280::STANDBY_MS_500); 
}

void readSensors(){
    if (bme.takeForcedMeasurement()) {
    temp = bme.readTemperature();

    #ifdef DEBUG
    Serial.print(F("BME280 on adress 0x76 temp: "));
    Serial.print(temp);
    Serial.println(" °C");
    Serial.println();
    #endif
  } else {
    #ifdef DEBUG
    Serial.println("Forced measurement failed!");
    #endif
  }
}

void transmitData() {
  char msg[200];
  StaticJsonDocument<200> doc;

    doc["temp"] = temp;


  serializeJson(doc, JSONmessage);
  JSONmessage.toCharArray(msg, JSONmessage.length() + 1);
  

  #ifdef DEBUG
  Serial.print("Publish message: ");
  Serial.println(msg);
  Serial.print("RAW JSON: ");
  Serial.println(JSONmessage);
  Serial.print("Lenght of message: ");
  Serial.println(JSONmessage.length());
  delay(1000);
  #endif

  moduleSerial->begin(115200);
  module.begin(*moduleSerial);
  delay(100);

  String MQTTsend = "AT+QMTPUB=0,0,0,0,";
  String result = MQTTsend + MQTTtopic + "," + strlen(msg) + "," + msg;
  char finResult[200];
  result.toCharArray(finResult, result.length()+1);

  bool next = true;
  
  while(next){
    if(module.wakeUp(HIGH,10,"DEEPSLEEP")) next = false;
    delay(NBdelay);
  }
  next = true;

  while(next){
    if(module.sendCommand("ATE0", "OK")) next = false;
    delay(NBdelay);
  }
  next = true;

  while(next){
    if(module.sendCommand("AT+QSCLK=0", "OK")) next = false;
    delay(NBdelay);
  }
  next = true;

  while(next){
    if(module.sendCommand("AT+CGPADDR?", "+CGPADDR: 0,")) next = false;
    delay(NBdelay);
  }
  next = true;
  
  while(next){
    if(module.sendCommand("AT+QMTOPEN=0,89.221.217.87,1883", "+QMTOPEN: 0,0")) next = false;
    delay(NBdelay);
  }
  next = true;

  while(next){
    if(module.sendCommand("AT+QMTCONN=0,DevKit-0123456", "+QMTCONN: 0,0,0")) next = false;
    delay(NBdelay);
  }
  next = true;

  while(next){
    if(module.sendCommand(finResult, "+QMTPUB: 0,0,0")) next = false;
    delay(NBdelay);
  }
  next = true;

  while(next){
    if(module.sendCommand("AT+QSCLK=1", "OK")) next = false;
    delay(NBdelay);
  }
  next = true;

}

void setup() {
  Serial.begin(115200);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_MIN_FACTOR);

  #ifdef DEBUG
  print_wakeup_reason();
  Serial.println("ESP will sleep for " + String(TIME_TO_SLEEP) +" minutes");
  #endif

  setupSensors();
  readSensors();
  transmitData();
  delay(1000);

  #ifdef DEBUG
  Serial.println("Going to sleep now");
  Serial.flush();
  #endif
  
  esp_deep_sleep_start();
}

void loop() {

}