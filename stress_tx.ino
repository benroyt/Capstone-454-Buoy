// Packet stress-test transmitter (Uno + E32). Sends numbered, checksummed frames at a fixed rate.
// Wiring is the same as random_tx.ino: E32 TXD -> D10, E32 RXD <- D11 via 1k/2k divider,
// M0 and M1 -> GND, VCC -> 5 V, GND -> GND.
//
// Frame (PAYLOAD_PAD + 4 bytes total):
//   [0xA5][seq high][seq low][PAD_BYTE x PAYLOAD_PAD][CRC-8 of all previous bytes]
// Keep the total <= 58 bytes: the E32 splits longer writes into several air packets.
#include <SoftwareSerial.h>

const uint8_t  PAYLOAD_PAD = 11;      // total frame = 15 bytes (same size as the sensor packet)
const uint16_t INTERVAL_MS = 1000;    // time between frames; try 1000, 500, 200, 100
const uint16_t FRAME_COUNT = 300;     // frames per run; 0 = run forever
const uint8_t  PAD_BYTE    = 0x55;
const uint16_t START_DELAY_MS = 5000; // time to start the logger before frames begin

SoftwareSerial radio(10, 11);         // RX, TX

uint8_t crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
  }
  return crc;
}

void setup() {
  Serial.begin(115200);
  radio.begin(9600);                  // E32 factory UART rate
  Serial.print(F("stress_tx: frame bytes="));
  Serial.print(PAYLOAD_PAD + 4);
  Serial.print(F(" interval_ms="));
  Serial.print(INTERVAL_MS);
  Serial.print(F(" count="));
  Serial.println(FRAME_COUNT);
  delay(START_DELAY_MS);
}

void loop() {
  static uint16_t seq = 0;
  static uint32_t next = millis();

  if (FRAME_COUNT != 0 && seq >= FRAME_COUNT) {
    Serial.println(F("done"));
    while (true) {}                   // stop; press reset to run again
  }

  uint8_t frame[PAYLOAD_PAD + 4];
  frame[0] = 0xA5;
  frame[1] = seq >> 8;
  frame[2] = seq & 0xFF;
  for (uint8_t i = 0; i < PAYLOAD_PAD; i++) frame[3 + i] = PAD_BYTE;
  frame[PAYLOAD_PAD + 3] = crc8(frame, PAYLOAD_PAD + 3);

  radio.write(frame, sizeof(frame));
  Serial.print(F("sent seq "));
  Serial.println(seq);
  seq++;

  next += INTERVAL_MS;
  while ((int32_t)(millis() - next) < 0) {}   // fixed-rate schedule (no drift)
}
