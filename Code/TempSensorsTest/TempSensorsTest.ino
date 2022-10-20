//#define DEBUG 

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


const char* ssid = mySSID;
const char* password = myPASSWORD;
const char* mqtt_server = mqttSERVER;

#define uS_TO_MIN_FACTOR 60000000 //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  5         //Time ESP will go to sleep (in minutes)
RTC_DATA_ATTR int bootCount = 0;  //Count how many times ESP woke up without power interruption


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//Sensors definitions
Adafruit_BME280 bme; // I2C
DHT dht(19, DHT22);
OneWire oneWire(18); //OneWire
DallasTemperature sensors(&oneWire);

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
    float temp_bme280;
    float temp_dht22;
    float temp_ds18b;
    float temp_esp32;

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
    if (!bme.begin(0x76)) {
      #ifdef DEBUG
      Serial.println("Could not find a valid bme280 sensor, check wiring!");
      #endif
      while (1);
    }

    //BME280 forced mode
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF,
                    Adafruit_BME280::STANDBY_MS_500);

    dht.begin();
    sensors.begin(); 

}

void readSensors() {
  //BME280
  if (bme.takeForcedMeasurement()) {
    temp_bme280 = bme.readTemperature();

    #ifdef DEBUG
    Serial.print(F("BME280 temp: "));
    Serial.print(temp_bme280);
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

  //DS18B
  sensors.requestTemperatures();
  temp_ds18b = sensors.getTempCByIndex(0); //Index 0 => first sensor on wire
  #ifdef DEBUG
  Serial.print("DS18B temp: ");
  Serial.print(temp_ds18b);
  Serial.println(" °C");
  Serial.println();
  #endif

  //ESP32 internal temp sensor
  temp_esp32 = (temprature_sens_read() - 32) / 1.8; //Convert RAW temp in °F to °C
  #ifdef DEBUG
  Serial.print("ESP32 temp: ");
  Serial.print(temp_esp32);
  Serial.println(" °C");
  Serial.println();
  #endif

}

void transmitData() {
  char msg[200];
  StaticJsonDocument<200> doc;

    doc["bme280"] = temp_bme280;
    doc["DHT22"] = temp_dht22;
    doc["DS18B"] = temp_ds18b;
    doc["ESP32"] = temp_esp32;
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
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_MIN_FACTOR);

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


  setupSensors();
  readSensors();
  transmitData();

  #ifdef DEBUG
  Serial.println("Going to sleep now");
  Serial.flush();
  #endif
  
  esp_deep_sleep_start();
}

void loop() {

}
