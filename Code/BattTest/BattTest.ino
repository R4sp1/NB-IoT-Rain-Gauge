#define DEBUG

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <cred.h>

const char* ssid = mySSID;
const char* password = myPASSWORD;
const char* mqtt_server = mqttSERVER;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
unsigned long entry;

#define uS_TO_MIN_FACTOR 60000000 //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  20         //Time ESP will go to sleep (in minutes)


const int adc = A0;  // This creates a constant integer in memory that stores the analog pin
float ADCvalue = 0;  // This creates an integer location in memory to store the analog value
const float voltageDeviderConst = 69.2191392227; //constant for voltage deviders

const int CHRGpin = D5;
const int DONEpin = D6;
int CHRG = 0;
int DONE = 0;

String JSONmessage;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
#ifdef DEBUG
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif

  WiFi.begin(ssid, password);
  
  entry=millis();
  while ((WiFi.status() != WL_CONNECTED)) {
    delay(500);
    Serial.print(".");
  }

#ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
#ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
#endif
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
#ifdef DEBUG
      Serial.println("connected");
#endif
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
  }
}

void transmitData() {
  char msg[200];
  StaticJsonDocument<200> doc;

    doc["voltage"] = ADCvalue;
    doc["CHRG"] = CHRG;
    doc["DONE"] = DONE;

  serializeJson(doc, JSONmessage);
  JSONmessage.toCharArray(msg, JSONmessage.length() + 1);
#ifdef DEBUG
  Serial.print("Publish message: ");
  Serial.println(msg);
  Serial.println(JSONmessage);
  Serial.println(JSONmessage.length());
  delay(1000);
#endif
  client.publish("batTest/values", msg);
}

void sleep() {
  int sleepTime = uS_TO_MIN_FACTOR * TIME_TO_SLEEP;
#ifdef DEBUG
  Serial.println("Going to sleep");
#endif
  ESP.deepSleep(sleepTime); 
}


void readSensors(){
  ADCvalue = analogRead(adc)/voltageDeviderConst;  // This reads the analog in value
  if(digitalRead(CHRGpin) == HIGH){
    CHRG = 0;
  } else {
    CHRG = 1;
  }
  if(digitalRead(DONEpin) == HIGH){
    DONE = 0;
  } else {
    DONE = 1;
  }
  if(digitalRead(CHRGpin) == LOW && digitalRead(DONEpin) == LOW){
    CHRG = 0;
    DONE = 0;
  }
  #ifdef DEBUG
  Serial.print("ADC Value: ");
  Serial.println(value);
  #endif
}

void setup() {

  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  Serial.println();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  readSensors();
  transmitData();
  delay(250);
  sleep();
}

void loop() {
  yield();
}
