#include <Wire.h>                   // enables I2C comm with light sensor through SDA (A4) and SCL (A5)
#include <OneWire.h>                // enables comm with the water-temp sensor using one digital pin (9)
#include <DallasTemperature.h>      // provides functions to request and read water-temp readings using OneWire
#include <DHT.h>                    // enables comm & provides functions to read air temp and humidity using one digital pin (2)
#include <math.h>                   // provides mathematical functions

// ---------------- PIN DEFINITIONS ----------------

// analog inputs
const int PH_PIN = A0;              // reads pH sensor voltage
const int HALL_PIN = A2;            // reads Hall sensor voltage

// digital sensor pins
const int WATER_TEMP_PIN = 9;       // water-temp sensor
const int HUMIDITY_PIN = 2;         // air temp and humidity sensor

// digital output
const int LED_PIN = 3;              // controls LED

// ---------------- SETTINGS ----------------

const int HUMIDITY_TYPE = DHT11;

// reporting interval for sensor readings (every 30min)
const unsigned long REPORT_INTERVAL = 30UL * 60UL * 1000UL;
const unsigned long LIGHT_INTERVAL = 1000UL;

// light-checking interval (every 1s)
const float LIGHT_ON_THRESHOLD = 20.0;   // turn LED on below 20 lux
const float LIGHT_OFF_THRESHOLD = 30.0;  // turn LED off above 30 lux
const byte LIGHT_I2C_ADDRESS = 0x23;     // address to identify sensor on I2C bus 

// ---------------- SENSOR OBJECTS ----------------

OneWire oneWire(WATER_TEMP_PIN);                  // create OneWire comm interface associated with digital pin 9
DallasTemperature waterTempSensor(&oneWire);      // create water-temp sensor object and connect it to that OneWire interface
DHT humiditySensor(HUMIDITY_PIN, HUMIDITY_TYPE);  // create humidity sensor object using digital pin 2 and DHT11 model

// ---------------- TIMING INITIALIZATION ----------------

unsigned long lastReport = 0;
unsigned long lastLightCheck = 0;

bool firstReport = true;                          // take sensor readings immediately
bool ledIsOn = false;                             // program initially considers LED off

// ---------------- LIGHT SENSOR ----------------

bool readLight(float &lux) {                   // readLight receives a reference to a variable that it will update (lux)
  Wire.beginTransmission(LIGHT_I2C_ADDRESS);
  Wire.write(0x10);                            // request high-resolution light measurement

  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(20);

  // ask sensor to send back 2 bytes of measurement data
  if (Wire.requestFrom(LIGHT_I2C_ADDRESS, (uint8_t)2) != 2) {
    return false;
  }

  // convert raw sensor value into lux using the conversion factor of 1.2 (taken from datasheet)
  uint16_t raw = ((uint16_t)Wire.read() << 8) | Wire.read();
  lux = raw / 1.2;

  return true;
}

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin();
  waterTempSensor.begin();
  humiditySensor.begin();

  Serial.println("water-quality buoy is starting!");
}

// ---------------- MAIN LOOP ----------------

void loop() {
  unsigned long now = millis();

  // check ambient light every second
  if (now - lastLightCheck >= LIGHT_INTERVAL) {
    lastLightCheck = now;

    float lux;

    if (readLight(lux)) {
      Serial.print("light: ");
      Serial.print(lux);
      Serial.println(" lux");

      // turn LED on when it is dark
      if (!ledIsOn && lux < LIGHT_ON_THRESHOLD) {
        ledIsOn = true;
        digitalWrite(LED_PIN, HIGH);
      }
      // turn LED off when it is bright
      else if (ledIsOn && lux > LIGHT_OFF_THRESHOLD) {
        ledIsOn = false;
        digitalWrite(LED_PIN, LOW);
      }

    } else {
      Serial.println("light sensor read failed!");
    }
  }

  // read all sensors at startup and every 30 minutes
  if (firstReport || now - lastReport >= REPORT_INTERVAL) {

    firstReport = false;
    lastReport = now;

    // NEED A CALIBRATION RELATIONSHIP HERE
    int phRaw = analogRead(PH_PIN);
    float phVoltage = phRaw * (5.0 / 1024.0);

    waterTempSensor.requestTemperatures();
    float waterC = waterTempSensor.getTempCByIndex(0);

    float airC = humiditySensor.readTemperature();
    float humidity = humiditySensor.readHumidity();

    // NEED A CALIBRATION RELATIONSHIP HERE
    int hallRaw = analogRead(HALL_PIN);

    Serial.println("\n--- sensor readings ---");

    Serial.print("pH ADC: ");
    Serial.println(phRaw);

    Serial.print("pH interface voltage: ");
    Serial.print(phVoltage, 3);
    Serial.println(" V");

    Serial.print("water temperature: ");
    if (waterC != DEVICE_DISCONNECTED_C) {
      Serial.print(waterC, 2);
      Serial.println(" C");
    } else {
      Serial.println("ERROR");
    }

    Serial.print("air temperature: ");
    if (!isnan(airC)) {
      Serial.print(airC, 2);
      Serial.println(" C");
    } else {
      Serial.println("ERROR");
    }

    Serial.print("humidity: ");
    if (!isnan(humidity)) {
      Serial.print(humidity, 2);
      Serial.println(" %");
    } else {
      Serial.println("ERROR");
    }

    Serial.print("hall sensor ADC: ");
    Serial.println(hallRaw);

    Serial.print("LED: ");
    Serial.println(ledIsOn ? "ON" : "OFF");
  }
}