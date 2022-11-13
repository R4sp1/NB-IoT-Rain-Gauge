/*
 * There are three serial ports on the ESP known as U0UXD, U1UXD and U2UXD.
 * 
 * U0UXD is used to communicate with the ESP32 for programming and during reset/boot.
 * U1UXD is unused and can be used for your projects. Some boards use this port for SPI Flash access though
 * U2UXD is unused and can be used for your projects.
 * 
*/

#define RXD2 16
#define TXD2 17
int PSM_EINT_N = 5;
int AT = 13;

String sending;
String receiving;

void atcommand(const String _atcommand) { //WIP
  Serial.print("== ");
  Serial.println(_atcommand);
  Serial2.println(_atcommand);
  delay(1000);
  while (Serial2.available())
    Serial.write(Serial2.read());
}

void setup() {
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  //Serial1.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  pinMode(PSM_EINT_N, OUTPUT);
  digitalWrite(PSM_EINT_N, LOW);
  pinMode(AT, INPUT_PULLUP);
  delay(500);
  digitalWrite(PSM_EINT_N, HIGH);
  delay(5);
  digitalWrite(PSM_EINT_N, LOW);
  delay(100);
  atcommand("AT+QSCLK=0");
  delay(50);
  atcommand("ATE1");
}

void loop() { //Choose Serial1 or Serial2 as required
  
  if (Serial2.available() > 0) { // Keep reading from BC660K and send to Arduino Serial Monitor
    receiving = Serial2.readStringUntil('\n');
    Serial.println(receiving);
    }
  /*if (Serial.available() > 0) { // Keep reading from Arduino Serial Monitor and send to BC660K
    sending = Serial.readStringUntil('\n');
    Serial2.println(sending);
  }*/
  
  if (digitalRead(AT) == LOW) {
    atcommand("AT");
    }
  }
