#define DEBUG 

#include <ESP32Time.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "ATcommands.h"
#include "esp_adc_cal.h"


#define uS_TO_S_FACTOR 1000000 //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  300         //Time ESP will go to sleep (in minutes)
#define MCU_RX 16 // Remember MCU RX connects to module TX and vice versa
#define MCU_TX 17
#define RST 19 // MCU pin to control module reset
#define PSM 18 // MCU pin to control module wake up pin (PSM-EINT_N)
#define TPIN 32 // DS18B20 pin
#define VOL_SENS    34
#define EN 33
#define CAL 1.99

RTC_DATA_ATTR int sendCount = 0;
RTC_DATA_ATTR ulong wakeUpEpoch = 0;
RTC_DATA_ATTR int rainCount = 0;

HardwareSerial *moduleSerial = &Serial2;

long lastMsg = 0;
char msg[50];
int value = 0;
String MQTTtopic = "Test/NB";

//Sensors definitions
OneWire oneWire(TPIN); //OneWire
DallasTemperature oneWireTemp(&oneWire);
ESP32Time rtc;  
ATcommands module = ATcommands(RST, PSM, true);

String JSONmessage;
float temp;
int NBdelay = 1000;

int timeToSleep = 0;
ulong currentEpoch = 0;

int volSens_Result = 0;
float Voltage = 0.0;

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
    digitalWrite(EN, HIGH);
    delay(100);
    oneWireTemp.begin(); 
}

void readSensors(){
  oneWireTemp.requestTemperatures();
  temp = oneWireTemp.getTempCByIndex(0); //Index 0 => first sensor on wire
  #ifdef DEBUG
  Serial.print("DS18B on wire temp: ");
  Serial.print(temp);
  Serial.println(" °C");
  Serial.println();
  #endif

  volSens_Result = analogRead(VOL_SENS);
  Voltage = readADC_Cal(volSens_Result) * CAL;
  #ifdef DEBUG
  Serial.print(Voltage/1000.0); // Print Voltage (in V)
  Serial.println(" V");
  Serial.println(Voltage);      // Print Voltage (in mV)
  Serial.println(" mV");
  #endif
  digitalWrite(EN, LOW);
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
    Serial.println("Wake = 2; pushbutton");
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
  char msg[200];
  StaticJsonDocument<200> doc;
    doc["send"] = sendCount;
    doc["temp"] = temp;
    doc["vol"] = Voltage;
    doc["rain"] = rainCount;
  serializeJson(doc, JSONmessage);
  JSONmessage.toCharArray(msg, JSONmessage.length() + 1);
  
  #ifdef DEBUG
  Serial.print("Publish message: ");
  Serial.println(msg);
  Serial.print("Lenght of message: ");
  Serial.println(JSONmessage.length());
  Serial.println();
  delay(1000);
  #endif

  String MQTTsend = "AT+QMTPUB=0,0,0,0,";
  String result = MQTTsend + MQTTtopic + "," + strlen(msg) + "," + msg;
  
  return result;
}

void transmitData() {
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
      delay(5000);
      Serial.println("Going to sleep now");
      #endif
      delay(500);
      esp_deep_sleep_start();
    } else {
      ++sendCount;
      setupSensors();
      readSensors();
      transmitData();
      rainCount = 0;
      #ifdef DEBUG
      Serial.println("Data send!");
      delay(5000);
      Serial.println("Going to sleep now");
      #endif
      delay(500);
      esp_deep_sleep_start();
    }
  }

  void loop() {
  }