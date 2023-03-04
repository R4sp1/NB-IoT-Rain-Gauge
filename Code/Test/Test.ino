#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <credentials.h>
#include <ESP32Time.h>            // https://github.com/fbiego/ESP32Time
#include <OneWire.h>              // https://www.arduino.cc/reference/en/libraries/onewire/
#include <DallasTemperature.h>    // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <Quectel_BC660.h>
#include "esp_adc_cal.h"          // https://github.com/espressif/arduino-esp32/
#include <BH1750.h>               // https://github.com/claws/BH1750
#include <Wire.h>

#define DEBUG   //Comment this line if you want to "debug" with Serial.print()
//#define NB    //Comment this line if you want to use WiFi instead

//Deep sleep related definitions
#define uS_TO_S_FACTOR 1000000     //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  300         //Time ESP will go to sleep (in minutes)

#define SERIAL_PORT Serial2
#define TPIN 32     // DS18B20 pin
#define VOL_SENS 34 // Pin to measure voltage of battery
#define CAL 2.25       // Voltage devider ratio
#define EN 33       // Pin to power on sensors

 //WIFI definitions
const char* networkName = mySSID;
const char* networkPswd = myPASSWORD;
const char * udpAddress = "192.168.1.158";
const int udpPort = 1234;

QuectelBC660 quectel = QuectelBC660(5, false);
WiFiUDP udp;

int rsrp;
int rsrq;
int rssi_nb;
int sinr;
int rssi_wifi;
float Voltage;      // Voltage in mV
int volSens_Result; // Raw value from ADC
float vol;
float temp1;
float temp2;
float light;

OneWire oneWire(TPIN); //OneWire
DallasTemperature oneWireTemp(&oneWire);
ESP32Time rtc;  
BH1750 lightMeter(0x23);

int timeToSleep = 0;
ulong currentEpoch = 0;

//Data stored in RTC memory
RTC_DATA_ATTR int sendCount = 0;
RTC_DATA_ATTR ulong wakeUpEpoch = 0;
RTC_DATA_ATTR int rainCount = 0;

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

uint32_t readADC_Cal(int ADC_Raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, &adc_chars);
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

double round2(double val) {           //Round fun to get only two decimal places
   return (int)(val*100+0.5)/100.0;
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

void transmitData() {
    #ifdef DEBUG
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(networkName);
    #endif
    WiFi.begin(networkName, networkPswd);

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
    udp.begin(WiFi.localIP(),udpPort);
    quectel.begin(&SERIAL_PORT);
    setupSensors();
    delay(200);
    volSens_Result = analogRead(VOL_SENS);
    Voltage = readADC_Cal(volSens_Result) * CAL;
    delay(50);
    quectel.getData();
    rsrp = quectel.engineeringData.RSRP;
    rsrq = quectel.engineeringData.RSRQ;
    rssi_nb = quectel.engineeringData.RSSI;
    sinr = quectel.engineeringData.SINR;
    rssi_wifi = WiFi.RSSI();
    vol = round2(Voltage/1000.0);
    oneWireTemp.requestTemperatures();
    temp1 = round2(oneWireTemp.getTempCByIndex(0));
    temp2 = round2(oneWireTemp.getTempCByIndex(1));
    if (lightMeter.measurementReady()) {
        light = round2(lightMeter.readLightLevel());
    }   

    udp.beginPacket(udpAddress,udpPort);
    udp.printf("%d,%d,%d,%d,%d,%f,%f,%f,%f,%d", rsrp,rsrq,rssi_nb,sinr,rssi_wifi,vol,temp1,temp2,light, rainCount);
    udp.endPacket();
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

void loop()
{
    
}