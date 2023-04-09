//Data stored in RTC memory
RTC_DATA_ATTR int rainCount = 0;

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 200;    // the debounce time; increase if the output flickers

void callBack(){
  if ((millis() - lastDebounceTime) > debounceDelay) {
    rainCount++;
    lastDebounceTime = millis();
    Serial.println("Rain count incremented by interrupt to: " + String(rainCount));
  } else {
    Serial.println("Interrupt ignored");
    }
}

void setup() {
    rainCount++;
    lastDebounceTime = millis();
    Serial.begin(115200);
    delay(100);
    Serial.println("Wake up from deep sleep, rain count: " + String(rainCount));
    int lastRainCount = rainCount;
    attachInterrupt(4, callBack, HIGH);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1);
    Serial.println("Waiting for interrupt for 5 seconds");
    delay(5000);
    Serial.println("Rain count after interrupt: " + String(rainCount-lastRainCount));
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
}

void loop() {
    
}