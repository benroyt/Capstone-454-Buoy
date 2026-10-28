#include <Wire.h>

const byte LIGHT_I2C_ADDRESS = 0x23;

bool readLight(float &lux) {
  Wire.beginTransmission(LIGHT_I2C_ADDRESS);
  Wire.write(0x10);

  if (Wire.endTransmission() != 0) {
    return false;
  } 

  delay(180);

  if (Wire.requestFrom(LIGHT_I2C_ADDRESS, (uint8_t)2) != 2) {
    return false;
  }

  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  uint16_t raw = ((uint16_t)hi << 8) | lo;
  lux = raw / 1.2;

  return true;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
}

void loop() {
  float lux;

  if (readLight(lux)) {
    Serial.print("ambient light: ");
    Serial.print(lux);
    Serial.println(" lux");
  } else {
    Serial.println("error: sensor reading failed");
  }

  delay(1000);
}