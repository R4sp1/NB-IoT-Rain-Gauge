#include <Arduino.h>
#line 1 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
#include <credentials.h>
#include <ESP32Time.h>            // https://github.com/fbiego/ESP32Time
#include <OneWire.h>              // https://www.arduino.cc/reference/en/libraries/onewire/
#include <DallasTemperature.h>    // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <Quectel_BC660.h>
#include "esp_adc_cal.h"          // https://github.com/espressif/arduino-esp32/
#include <BH1750.h>               // https://github.com/claws/BH1750
#include <Wire.h>
#include "Adafruit_VEML7700.h"

// #define DEBUG   //Comment this line if you want to print out to serial "debug" messages
#define DEVICE_ADDRESS 3          //Device address (1-255) used to identify device on server

//Deep sleep related definitions
#define uS_TO_S_FACTOR 1000000     //Conversion factor from uSeconds to seconds
#define TIME_TO_SLEEP  300         //Time ESP will go to sleep (in seconds)

//NB related definitions
#define SERIAL_PORT Serial2
#define RST 18        // MCU pin to control module reset
#define PSM 5        // MCU pin to control module wake up pin (PSM-EINT_N)
#define NBdelay 1000  // Delay between two AT commands
#define DEB false     // True or False debug option
const char* nbServer = nbSERVER;
int nbPort = 1883;
QuectelBC660 quectel = QuectelBC660(PSM, DEB);


//Sensors related definitions
#define TPIN 32     // DS18B20 pin
#define VOL_SENS 34 // Pin to measure voltage of battery
#define CAL 2       // Voltage devider ratio
#define EN 33       // Pin to power on sensors


//Data stored in RTC memory
RTC_DATA_ATTR ulong wakeUpEpoch = 0;
RTC_DATA_ATTR int rainCount = 0;
RTC_DATA_ATTR int connectionTries = 0;

float temp1 = -127;
float temp2 = -127;
float light = -127;
float Voltage = 0;      // Voltage in mV
int volSens_Result =0; // Raw value from ADC

//Sensors definitions
OneWire oneWire(TPIN); //OneWire
DallasTemperature oneWireTemp(&oneWire);
ESP32Time rtc;  
Adafruit_VEML7700 veml = Adafruit_VEML7700();
int vemlFix = 0;

int timeToSleep = 0;
ulong currentEpoch = 0;

unsigned long lastDebounceTime = 0;  // the last time the raingauge was overturned
unsigned long debounceDelay = 200;   // the debounce time

// Function to round float to two decimal places
#line 61 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
float round2(float val);
#line 69 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void print_wakeup_reason();
#line 83 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void setupSensors();
#line 97 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void readSensors();
#line 144 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
bool sleepLogic();
#line 176 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void checkConnection();
#line 204 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void transmitData();
#line 233 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void callBack();
#line 247 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void setup();
#line 280 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
void loop();
#line 61 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
float round2(float val) {
    if(val < 0)                      //Check if we are rounding negative number
    {
      return (int)(val*100-0.5)/100.0;
    }
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
    //Wire.begin (21, 22);    //I2C
    oneWireTemp.begin();      //Onewire
    if(!veml.begin()){        //Light meter
    vemlFix = 1;
    #ifdef DEBUG
    Serial.println("Sensor VEML7700 not found");
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
  volSens_Result = 0;
  for(int i = 0; i < 16; i++) //Read 16 times and average
  {
    volSens_Result += analogReadMilliVolts(VOL_SENS);
    delay(5);
  }
  Voltage = CAL*(volSens_Result/16.0); //Calculate voltage by deviding by 16 (16 reads) and multiplying by 2 (voltage devider) - voltage is in mV
  #ifdef DEBUG
  Serial.print(Voltage/1000.0); // Print Voltage (in V)
  Serial.println(" V");
  Serial.println(Voltage);      // Print Voltage (in mV)
  Serial.println(" mV");
  #endif

  if(vemlFix == 0){
    light = veml.readLux(VEML_LUX_AUTO);
  }
  #ifdef DEBUG
  Serial.print("Light: ");
  Serial.print(light);
  Serial.println(" lx");
  #endif
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
    Serial.println("Wake = 2; rain sensor");
    #endif
    return true;
  } else {
    #ifdef DEBUG
    Serial.println("Wake = 4; timer");
    #endif
    return false;
  }
}

void checkConnection(){
  if(quectel.getRegistrationStatus(5)) // Check if nb module is registred to network  
  {
    #ifdef DEBUG
    Serial.println("Module is registered to network");
    #endif
  } else {
    if(connectionTries < 3){
      connectionTries++;
      #ifdef DEBUG
      Serial.println("Module is not registered to network, trying again after deepsleep");
      #endif
      delay(10);
      esp_deep_sleep_start();
    } else {
      connectionTries = 0;
      #ifdef DEBUG
      Serial.println("Module is not registered to network, trying again after UE functionality reset");
      #endif
      quectel.setUEFun(0);
      delay(1000);
      quectel.setUEFun(1);
      delay(5000);
      checkConnection();
    }
  }
}

void transmitData() {
    quectel.begin(&SERIAL_PORT);
    delay(100); 
    
    quectel.setDeepSleep(0);   // Disable deep sleep
    delay(1000);
    checkConnection();
    delay(1000);
    quectel.getData();        // Init struct with engineering data
    delay(1000);
    quectel.openUDP(nbServer, nbPort);
    delay(1000);
    float t1 = round2(temp1);
    float t2 = round2(temp2);
    float l = round2(light);
    float v = round2(Voltage/1000);
    char *msg;
    msg = (char *) malloc(255);
    sprintf(msg, "%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d", DEVICE_ADDRESS, quectel.engineeringData.RSRP, quectel.engineeringData.RSRQ, quectel.engineeringData.RSSI, quectel.engineeringData.SINR, t1, t2, l, v, rainCount);
    Serial.println(msg);
    //Serial.println(msgch);
    quectel.sendDataUDPn(msg, strlen(msg),1);
    free(msg);
    delay(1000);
    quectel.closeUDP();
    delay(300);
    quectel.setDeepSleep(1);
}

void callBack(){
  if ((millis() - lastDebounceTime) > debounceDelay) {
    rainCount++;
    lastDebounceTime = millis();
    #ifdef DEBUG
    Serial.println("Rain count incremented by interrupt to: " + String(rainCount));
    #endif
  } else {
    #ifdef DEBUG
    Serial.println("Interrupt ignored by debounce");
    #endif
    }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  attachInterrupt(4, callBack, HIGH);

  if(sleepLogic()){
      rainCount++;
      lastDebounceTime = millis();
      #ifdef DEBUG
      Serial.println("Rain count: " + String(rainCount));
      print_wakeup_reason();
      delay(1000);
      Serial.println("Going to sleep now");
      #endif
      digitalWrite(EN, LOW);
      delay(5);
      esp_deep_sleep_start();
    } else {
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
      delay(5);
      esp_deep_sleep_start();
    }
  }

  void loop() {
  }
