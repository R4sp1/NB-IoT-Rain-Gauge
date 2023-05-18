# 1 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino"
# 2 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 3 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 4 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 5 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 6 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 7 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 8 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 9 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2
# 10 "C:\\Users\\jirka\\Documents\\GitHub\\bachelor-thesis\\Code\\NB_UDP\\NB_UDP.ino" 2

// #define DEBUG   //Comment this line if you want to print out to serial "debug" messages


//Deep sleep related definitions



//NB related definitions





const char* nbServer = nbSERVER;
int nbPort = 1883;
QuectelBC660 quectel = QuectelBC660(5 /* MCU pin to control module wake up pin (PSM-EINT_N)*/, false /* True or False debug option*/);


//Sensors related definitions






//Data stored in RTC memory
__attribute__((section(".rtc.data" "." "28"))) ulong wakeUpEpoch = 0;
__attribute__((section(".rtc.data" "." "29"))) int rainCount = 0;
__attribute__((section(".rtc.data" "." "30"))) int connectionTries = 0;

float temp1 = -127;
float temp2 = -127;
float light = -127;
float Voltage = 0; // Voltage in mV
int volSens_Result =0; // Raw value from ADC

//Sensors definitions
OneWire oneWire(32 /* DS18B20 pin*/); //OneWire
DallasTemperature oneWireTemp(&oneWire);
ESP32Time rtc;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
int vemlFix = 0;

int timeToSleep = 0;
ulong currentEpoch = 0;

unsigned long lastDebounceTime = 0; // the last time the raingauge was overturned
unsigned long debounceDelay = 200; // the debounce time

// Function to round float to two decimal places
float round2(float val) {
    if(val < 0) //Check if we are rounding negative number
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
    pinMode(33 /* Pin to power on sensors*/, 0x03);
    digitalWrite(33 /* Pin to power on sensors*/, 0x1); //Turn on sensors
    delay(100); //Delay to let sensors "boot"
    //Wire.begin (21, 22);    //I2C
    oneWireTemp.begin(); //Onewire
    if(!veml.begin()){ //Light meter
    vemlFix = 1;



    }
}

void readSensors(){
  //First Temp sensor
  oneWireTemp.requestTemperatures(); //Send command to measure temperatures on each sensor on the bus
  temp1 = oneWireTemp.getTempCByIndex(0); //Index 0 => first sensor on wire







  //Second Temp sensor
  //oneWireTemp.requestTemperatures();    //Temp requested before first sensor read
  temp2 = oneWireTemp.getTempCByIndex(1); //Index 1 => second sensor on wire







  //Voltage of battery
  volSens_Result = 0;
  for(int i = 0; i < 16; i++) //Read 16 times and average
  {
    volSens_Result += analogReadMilliVolts(34 /* Pin to measure voltage of battery*/);
    delay(5);
  }
  Voltage = 2 /* Voltage devider ratio*/*(volSens_Result/16.0); //Calculate voltage by deviding by 16 (16 reads) and multiplying by 2 (voltage devider) - voltage is in mV







  if(vemlFix == 0){
    light = veml.readLux(VEML_LUX_AUTO);
  }





}


bool sleepLogic(){
  currentEpoch = rtc.getEpoch();

  if(currentEpoch < wakeUpEpoch){
    timeToSleep = (wakeUpEpoch - currentEpoch) * 1000000 /*Conversion factor from uSeconds to seconds*/;



  } else {
    timeToSleep = 300 /*Time ESP will go to sleep (in seconds)*/ * 1000000 /*Conversion factor from uSeconds to seconds*/;
    wakeUpEpoch = (currentEpoch + 300 /*Time ESP will go to sleep (in seconds)*/);



  }

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1);
  esp_sleep_enable_timer_wakeup(timeToSleep);

  if(esp_sleep_get_wakeup_cause() == 2){ // 2=ext0, 4=timer



    return true;
  } else {



    return false;
  }
}

void checkConnection(){
  if(quectel.getRegistrationStatus(5)) // Check if nb module is registred to network  
  {



  } else {
    if(connectionTries < 3){
      connectionTries++;



      delay(10);
      esp_deep_sleep_start();
    } else {
      connectionTries = 0;



      quectel.setUEFun(0);
      delay(1000);
      quectel.setUEFun(1);
      delay(5000);
      checkConnection();
    }
  }
}

void transmitData() {
    quectel.begin(&Serial2);
    delay(100);

    quectel.setDeepSleep(0); // Disable deep sleep
    delay(1000);
    checkConnection();
    delay(1000);
    quectel.getData(); // Init struct with engineering data
    delay(1000);
    quectel.openUDP(nbServer, nbPort);
    delay(1000);
    float t1 = round2(temp1);
    float t2 = round2(temp2);
    float l = round2(light);
    float v = round2(Voltage/1000);
    char *msg;
    msg = (char *) malloc(255);
    sprintf(msg, "%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d", 3 /*Device address (1-255) used to identify device on server*/, quectel.engineeringData.RSRP, quectel.engineeringData.RSRQ, quectel.engineeringData.RSSI, quectel.engineeringData.SINR, t1, t2, l, v, rainCount);
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



  } else {



    }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  attachInterrupt(4, callBack, 0x1);

  if(sleepLogic()){
      rainCount++;
      lastDebounceTime = millis();






      digitalWrite(33 /* Pin to power on sensors*/, 0x0);
      delay(5);
      esp_deep_sleep_start();
    } else {
      setupSensors();
      readSensors();
      transmitData();
      rainCount = 0;





      digitalWrite(33 /* Pin to power on sensors*/, 0x0);
      delay(5);
      esp_deep_sleep_start();
    }
  }

  void loop() {
  }
