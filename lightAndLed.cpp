#include <Wire.h>

const int LED_PIN = 3;
const byte LIGHT_I2C_ADDRESS = 0x23;

const float LIGHT_ON_THRESHOLD = 20.0;
const float LIGHT_OFF_THRESHOLD = 30.0;

bool ledIsOn = false;

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
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Wire.begin();
}

void loop() {
    float lux;

    if (readLight(lux)) {
        Serial.print("ambient light: ");
        Serial.print(lux);
        Serial.println(" lux");

        if (!ledIsOn && lux < LIGHT_ON_THRESHOLD) {
            ledIsOn = true;
            digitalWrite(LED_PIN, HIGH);
        } else if (ledIsOn && lux > LIGHT_OFF_THRESHOLD) {
            ledIsOn = false;
            digitalWrite(LED_PIN, LOW);
        }

        Serial.print("LED: ");
        Serial.println(ledIsOn ? "ON" : "OFF");
    } 
    
    else {
        Serial.println("error: light sensor reading failed");
    }

    delay(1000);
}