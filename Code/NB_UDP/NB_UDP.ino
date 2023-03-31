#include <credentials.h>
#include <ESP32Time.h>            // https://github.com/fbiego/ESP32Time
#include <OneWire.h>              // https://www.arduino.cc/reference/en/libraries/onewire/
#include <DallasTemperature.h>    // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <Quectel_BC660.h>
#include "esp_adc_cal.h"          // https://github.com/espressif/arduino-esp32/
#include <BH1750.h>               // https://github.com/claws/BH1750
#include <Wire.h>

// #define DEBUG   //Comment this line if you want to "debug" with Serial.print()

//Deep sleep related definitions
#define uS_TO_S_FACTOR 1000000     //Conversion factor from uSeconds to seconds
#define TIME_TO_SLEEP  300         //Time ESP will go to sleep (in seconds)

  //NB related definitions
  #define SERIAL_PORT Serial2
  #define RST 18        // MCU pin to control module reset
  #define PSM 5        // MCU pin to control module wake up pin (PSM-EINT_N)
  #define NBdelay 1000  // Delay between two AT commands
  #define DEB true     // True or False debug option
  const char* nbServer = nbSERVER;
  int nbPort = 1883;

  QuectelBC660 quectel = QuectelBC660(PSM, DEB);


//Sensors related definitions
#define TPIN 32     // DS18B20 pin
#define VOL_SENS 34 // Pin to measure voltage of battery
#define CAL 2       // Voltage devider ratio
#define EN 33       // Pin to power on sensors


//Data stored in RTC memory
RTC_DATA_ATTR int sendCount = 0;
RTC_DATA_ATTR ulong wakeUpEpoch = 0;
RTC_DATA_ATTR int rainCount = 0;

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

float round2(float val) {           //Round fun to get only two decimal places
    if(val < 0)
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

void transmitData() {
    quectel.begin(&SERIAL_PORT);
    delay(100); 
    
    quectel.setDeepSleep();   // Disable deep sleep
    delay(1000);
    if(quectel.getRegistrationStatus(5)) // Check if nb module is registred to network  
    {
        #ifdef DEBUG
        Serial.println("Module is registered to network");
        #endif
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
        sprintf(msg, "%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d", quectel.engineeringData.epoch, quectel.engineeringData.RSRP, quectel.engineeringData.RSRQ, quectel.engineeringData.RSSI, quectel.engineeringData.SINR, t1, t2, l, v, rainCount);
        Serial.println(msg);
        //Serial.println(msgch);
        quectel.sendDataUDP(msg, strlen(msg));
        free(msg);
        delay(1000);
        quectel.closeUDP();
    }
    quectel.setDeepSleep(1);
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