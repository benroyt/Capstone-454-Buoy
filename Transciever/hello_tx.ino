// sends "hello N" once a second.
// Wiring: E32 TXD -> D10, E32 RXD <- D11 (through divider if 5 V board),
//         E32 M0 and M1 -> GND (normal mode), VCC and GND to supply.
#include <SoftwareSerial.h>

SoftwareSerial radio(10, 11);   // RX, TX
int count = 0;

void setup() {
  Serial.begin(115200);
  radio.begin(9600);            // E32 factory UART rate
}

void loop() {
  radio.print("hello ");
  radio.println(count);
  Serial.print("sent hello ");
  Serial.println(count);
  count++;
  delay(1000); // send once every second
}
