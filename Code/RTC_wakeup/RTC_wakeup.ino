#include <ESP32Time.h>
 
//ESP32Time rtc;
ESP32Time rtc(0);  // offset in seconds GMT+1

#define uS_TO_MIN_FACTOR 1000 //Conversion factor from uSeconds to minutes
#define TIME_TO_SLEEP  5000         //Time ESP will go to sleep (in miliseconds)
#define BUTTON_PIN_BITMASK 0x4           //(2^15 + 2^2) in hex (We use pin 15 and 2 to wake up device)

    RTC_DATA_ATTR int bootCount = 0;        //Store data in RTC memory which doesnt wipe in deep sleep
    RTC_DATA_ATTR unsigned int count = 0;
    RTC_DATA_ATTR long ep = 0;

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

void print_GPIO_wake_up(){                  //Function to determine which pin was used to wake up device
  uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status(); //Fnc returns 2^(GPIO)
  Serial.print("GPIO that triggered the wake up: GPIO ");
  int GPIO = (log(GPIO_reason))/log(2);     
  if(GPIO == 2){
    ++count;
  }
  Serial.println(GPIO, 0);
}

void callBack(){                            //Callback function to interrupt
    ++count;
    Serial.println("Counter value: " + String(count));
}
 
void setup() {
    Serial.begin(115200);
    delay(500);
  //rtc.setTime(30, 24, 15, 17, 1, 2021);   17th Jan 2021 15:24:30
    rtc.setTime(ep);  // Unix timestamp
  //rtc.offset = 7200; // change offset value
 

    Serial.println(rtc.getEpoch());

//attachInterrupt(2, callBack, HIGH);     //Interrupt on pin 2 to increment value if device is on


    print_wakeup_reason();
    print_GPIO_wake_up();


    ++bootCount;
    Serial.println("Boot number: " + String(bootCount));

    Serial.println("Counter value: " + String(count));
    

    //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);      //Enable ext1 wakeup when pins 15 or 2 are HIGH
     esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_MIN_FACTOR);

    Serial.println("Waiting 5s to test interrupt!");
    
    delay(5000);
    ep = rtc.getEpoch();
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
}
 
void loop() {
//  Serial.println(rtc.getTime());          //  (String) 15:24:38
//  Serial.println(rtc.getDate());          //  (String) Sun, Jan 17 2021
//  Serial.println(rtc.getDate(true));      //  (String) Sunday, January 17 2021
//  Serial.println(rtc.getDateTime());      //  (String) Sun, Jan 17 2021 15:24:38
//  Serial.println(rtc.getDateTime(true));  //  (String) Sunday, January 17 2021 15:24:38
//  Serial.println(rtc.getTimeDate());      //  (String) 15:24:38 Sun, Jan 17 2021
//  Serial.println(rtc.getTimeDate(true));  //  (String) 15:24:38 Sunday, January 17 2021
//
//  Serial.println(rtc.getMicros());        //  (long)    723546
//  Serial.println(rtc.getMillis());        //  (long)    723
//  Serial.println(rtc.getEpoch());         //  (long)    1609459200
//  Serial.println(rtc.getSecond());        //  (int)     38    (0-59)
//  Serial.println(rtc.getMinute());        //  (int)     24    (0-59)
//  Serial.println(rtc.getHour());          //  (int)     3     (0-12)
//  Serial.println(rtc.getHour(true));      //  (int)     15    (0-23)
//  Serial.println(rtc.getAmPm());          //  (String)  pm
//  Serial.println(rtc.getAmPm(true));      //  (String)  PM
//  Serial.println(rtc.getDay());           //  (int)     17    (1-31)
//  Serial.println(rtc.getDayofWeek());     //  (int)     0     (0-6)
//  Serial.println(rtc.getDayofYear());     //  (int)     16    (0-365)
//  Serial.println(rtc.getMonth());         //  (int)     0     (0-11)
//  Serial.println(rtc.getYear());          //  (int)     2021
 
//  Serial.println(rtc.getLocalEpoch());         //  (long)    1609459200 epoch without offset
//  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));   // (String) returns time with specified format 
  // formating options  http://www.cplusplus.com/reference/ctime/strftime/
 
 
//  struct tm timeinfo = rtc.getTimeStruct();
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");   //  (tm struct) Sunday, January 17 2021 07:24:38
  
 // delay(1000);
}