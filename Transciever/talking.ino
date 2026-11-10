#include <SoftwareSerial.h>

SoftwareSerial radio(10, 11);   // RX, TX

void setup() {
  Serial.begin(115200);         // USB debug
  radio.begin(9600);            // E32 factory UART rate
  randomSeed(analogRead(A0));   // floating pin gives a different seed each run
}

void loop() {
  int value = random(0, 1000);
  radio.print("random: ");
  radio.println(value);
  Serial.print("sent random: ");
  Serial.println(value);
  delay(1000);
}
 