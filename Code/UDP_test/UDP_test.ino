#include <WiFi.h>
#include <WiFiUdp.h>
#include <Quectel_BC660.h>
#include <credentials.h>
#include "esp_adc_cal.h" 

#define SERIAL_PORT Serial2
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

boolean connected = false;
int rsrp;
int rsrq;
int rssi_nb;
int sinr;
int rssi_wifi;
float Voltage;      // Voltage in mV
int volSens_Result; // Raw value from ADC
float vol;

double round2(double val) {           //Round fun to get only two decimal places
   return (int)(val*100+0.5)/100.0;
}

uint32_t readADC_Cal(int ADC_Raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, &adc_chars);
  return(esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

void connectToWiFi(const char * ssid, const char * pwd){
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);
  
  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

//wifi event handler
void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          //When connected set 
          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());  
          //initializes the UDP state
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(),udpPort);
          connected = true;
          break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
          Serial.println("WiFi lost connection");
          connected = false;
          break;
      default: break;
    }
}

void setup(){
    Serial.begin(115200);
    connectToWiFi(networkName, networkPswd);
    delay(200);
    quectel.begin(&SERIAL_PORT);
    delay(200);
}

void loop(){
    pinMode(EN, OUTPUT);
    digitalWrite(EN, HIGH);   //Turn on sensors
    delay(100);               //Delay to let sensors "boot"
    volSens_Result = analogRead(VOL_SENS);
    Voltage = readADC_Cal(volSens_Result) * CAL;
    delay(50);
    digitalWrite(EN, LOW);
    quectel.getData();
    rsrp = quectel.engineeringData.RSRP;
    rsrq = quectel.engineeringData.RSRQ;
    rssi_nb = quectel.engineeringData.RSSI;
    sinr = quectel.engineeringData.SINR;
    rssi_wifi = WiFi.RSSI();
    vol = round2(Voltage/1000.0);

    if(connected){
        //Send a packet
        udp.beginPacket(udpAddress,udpPort);
        udp.printf("%d,%d,%d,%d,%d,%f", rsrp,rsrq,rssi_nb,sinr,rssi_wifi,vol);
        udp.endPacket();
    } else{
        connectToWiFi(networkName, networkPswd);
        //Send a packet
        udp.beginPacket(udpAddress,udpPort);
        udp.printf("%d,%d,%d,%d,%d,%f", rsrp,rsrq,rssi_nb,sinr,rssi_nb,vol);
        udp.endPacket();
    }
    delay(600000);
}

