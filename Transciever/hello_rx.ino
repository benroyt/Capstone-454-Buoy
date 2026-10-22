// Prints whatever the radio hears.
// Same wiring as the sender.
#include <SoftwareSerial.h>

SoftwareSerial radio(10, 11);   // RX, TX

void setup() {
  Serial.begin(115200);
  radio.begin(9600);
  Serial.println("listening...");
}

void loop() {
  while (radio.available()) {
    Serial.write(radio.read());
  }
}
