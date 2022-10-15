


#define BUTTON_PIN_BITMASK 0x8004           //(2^15 + 2^2) in hex (We use pin 15 and 2 to wake up device)

    RTC_DATA_ATTR int bootCount = 0;        //Store data in RTC memory which doesnt wipe in deep sleep
    RTC_DATA_ATTR unsigned int count = 0;


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

void setup(){
    attachInterrupt(2, callBack, HIGH);     //Interrupt on pin 2 to increment value if device is on
    Serial.begin(115200);
    delay(500);

    print_wakeup_reason();
    print_GPIO_wake_up();


    ++bootCount;
    Serial.println("Boot number: " + String(bootCount));

    Serial.println("Counter value: " + String(count));
    

    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);      //Enable ext1 wakeup when pins 15 or 2 are HIGH

    Serial.println("Waiting 5s to test interrupt!");
    delay(5000);
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
}

void loop(){
  //This is not going to be called
}