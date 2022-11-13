#define DEBUG

#include <ESP32Time.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <credentials.h>

const char* ssid = mySSID;
const char* password = myPASSWORD;

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

ESP32Time rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


int s = 0;
int m = 0;
int h = 0;
int d = 0;
int mo = 0;
int y = 0;



void print_wakeup_reason(){                 //Function to determine reason of wake up

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

void get_NTPtime(){
    timeClient.begin();
    timeClient.setTimeOffset(3600); //Offset to GMT+1

    while(!timeClient.update()) {
      timeClient.forceUpdate();
    }

    String formattedDate = timeClient.getFormattedDate();
    
    // Extract date
    y = formattedDate.substring(0, formattedDate.length()-16).toInt();
    mo = formattedDate.substring(5, formattedDate.length()-13).toInt();
    d = formattedDate.substring(8, formattedDate.length()-10).toInt();

    
    // Extract time
    h = formattedDate.substring(11, formattedDate.length()-7).toInt();
    m = formattedDate.substring(14, formattedDate.length()-4).toInt();
    s = formattedDate.substring(17, formattedDate.length()-1).toInt();

}

void setup() {
  Serial.begin(115200);

  print_wakeup_reason();

  if(rtc.getEpoch() == 0){            // When ESP is restarted info about time is lost so epoch == 0 then we connect to NTP server to get time
    setup_wifi();
    get_NTPtime();
    rtc.setTime(s, m, h, d, mo, y);
  }

  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));   // (String) returns time with specified format
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {
  
}
