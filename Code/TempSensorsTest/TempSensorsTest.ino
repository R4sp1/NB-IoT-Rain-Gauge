#define DEBUG 

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <credentials.h>
#include <BH1750.h>


  const char* ssid = mySSID;
  const char* password = myPASSWORD;
  const char* mqtt_server = mqttSERVER;

#define uS_TO_MIN_FACTOR 60000000 //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  2         //Time ESP will go to sleep (in minutes)
RTC_DATA_ATTR int bootCount = 0;  //Count how many times ESP woke up without power interruption


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//Sensors definitions
Adafruit_BME280 bme_1; // I2C
Adafruit_BME280 bme_2; // I2C
DHT dht(19, DHT22);
OneWire oneWire(32); //OneWire
DallasTemperature oneWireTemp(&oneWire);
BH1750 lightMeter(0x23);

//Internal temp sensor
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();


String JSONmessage;
    float temp_bme280_1;
    float temp_bme280_2;
    float temp_dht22;
    float temp_ds18b_wire;
    float temp_ds18b_ic;
    float light;

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

void setup_wifi() {
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
}

void reconnect() {
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
}

void setupSensors(){
    Wire.begin (21, 22);

    //BME280 on address 0x76
    if (!bme_1.begin(0x76)) {
      #ifdef DEBUG
      Serial.println("Could not find a valid bme280 sensor, check wiring!");
      #endif
      //while (1);
    }

    //BME280 forced mode
    bme_1.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF,
                    Adafruit_BME280::STANDBY_MS_500);

    //BME280 on adress 0x77
    if (!bme_2.begin(0x77)) {
      #ifdef DEBUG
      Serial.println("Could not find a valid bme280 sensor, check wiring!");
      #endif
      temp_bme280_2 = NULL;
    }

    //BME280 forced mode
    bme_2.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF,
                    Adafruit_BME280::STANDBY_MS_500);

    //DHT 22 begin
    dht.begin();

    //DS18B sensors begin
    oneWireTemp.begin(); 

    /*Begin light sensor in one time High resolution mode
        - Low Resolution Mode - (4 lx precision, 16ms measurement time)
        - High Resolution Mode - (1 lx precision, 120ms measurement time)
        - High Resolution Mode 2 - (0.5 lx precision, 120ms measurement time)
        Full mode list:

          BH1750_CONTINUOUS_LOW_RES_MODE
          BH1750_CONTINUOUS_HIGH_RES_MODE (default)
          BH1750_CONTINUOUS_HIGH_RES_MODE_2

          BH1750_ONE_TIME_LOW_RES_MODE
          BH1750_ONE_TIME_HIGH_RES_MODE
          BH1750_ONE_TIME_HIGH_RES_MODE_2
    */
    if (lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE_2)) {
      #ifdef DEBUG
      Serial.println(F("BH1750 Advanced begin"));
      #endif
    } else {
      #ifdef DEBUG
      Serial.println(F("Error initialising BH1750"));
      #endif
    }

}

void readSensors() {
  //BME280 on adress 0x76
  if (bme_1.takeForcedMeasurement()) {
    temp_bme280_1 = bme_1.readTemperature();

    #ifdef DEBUG
    Serial.print(F("BME280 on adress 0x76 temp: "));
    Serial.print(temp_bme280_1);
    Serial.println(" °C");
    Serial.println();
    #endif
  } else {
    #ifdef DEBUG
    Serial.println("Forced measurement failed!");
    #endif
  }

  //BME280 on adress 0x77
  if (bme_2.takeForcedMeasurement()) {
    temp_bme280_2 = bme_2.readTemperature();

    #ifdef DEBUG
    Serial.print(F("BME280 on adress 0x77 temp: "));
    Serial.print(temp_bme280_2);
    Serial.println(" °C");
    Serial.println();
    #endif
  } else {
    #ifdef DEBUG
    Serial.println("Forced measurement failed!");
    #endif
  }

  //DHT22
  temp_dht22 = dht.readTemperature();
  #ifdef DEBUG
  Serial.print("DHT temp: ");
  Serial.print(temp_dht22);
  Serial.println(" °C");
  Serial.println();
  #endif

  //DS18B on wire
  oneWireTemp.requestTemperatures();
  temp_ds18b_wire = oneWireTemp.getTempCByIndex(1); //Index 0 => first sensor on wire
  #ifdef DEBUG
  Serial.print("DS18B on wire temp: ");
  Serial.print(temp_ds18b_wire);
  Serial.println(" °C");
  Serial.println();
  #endif

  //DS18B ic
  oneWireTemp.requestTemperatures();
  temp_ds18b_ic = oneWireTemp.getTempCByIndex(0); //Index 0 => first sensor on wire
  #ifdef DEBUG
  Serial.print("DS18B ic temp: ");
  Serial.print(temp_ds18b_ic);
  Serial.println(" °C");
  Serial.println();
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

void transmitData() {
  char msg[200];
  StaticJsonDocument<200> doc;

    doc["bme280_1"] = temp_bme280_1;
    doc["bme280_2"] = temp_bme280_2;
    doc["DHT22"] = temp_dht22;
    doc["DS18B_1"] = temp_ds18b_wire;
    doc["DS18B_2"] = temp_ds18b_ic;
    doc["Light"] = light;
    doc["bootCount"] = bootCount;


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

  client.publish("ESP32/TempSensorTest", msg);
  delay(100);
}

void setup() {
  ++bootCount;
  Serial.begin(115200);
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH);
  delay(200);
  /*esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_MIN_FACTOR);

  #ifdef DEBUG
  print_wakeup_reason();
  Serial.println("ESP will sleep for " + String(TIME_TO_SLEEP) +" minutes");
  #endif

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

*/
  setupSensors();
  readSensors();
  /*transmitData();

  #ifdef DEBUG
  Serial.println("Going to sleep now");
  Serial.flush();
  #endif
  
  esp_deep_sleep_start();
  */
}

void loop() {

}
