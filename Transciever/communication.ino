// Buoy transmitter: waits for a "read" command over the E32 radio, then reads
// the sensors (same logic as sensors.cpp) and sends the values back as one line.
//
// Radio wiring: E32 TXD -> D10, E32 RXD <- D11 (through divider if 5 V board),
//               E32 M0 and M1 -> GND (normal mode), VCC and GND to supply.
// Sensor wiring: pH -> A0, water temp -> D9, DHT11 -> D2, LED -> D3,
//                light sensor on I2C (SDA A4, SCL A5).
//
// Commands (sent from the receiver, one per line, case-insensitive):
//   read   -> take a full set of sensor readings and send them back
//
// Reply format (one line):
//   pH_ADC=512,pH_V=2.502,water_C=21.50,air_C=22.00,hum=45.00,lux=123.4,LED=OFF
// A failed sensor read is sent as ERR, e.g. water_C=ERR

#include <SoftwareSerial.h>
#include <Wire.h>                   // I2C comm with light sensor through SDA (A4) and SCL (A5)
#include <OneWire.h>                // comm with the water-temp sensor using one digital pin (9)
#include <DallasTemperature.h>      // request and read water-temp readings using OneWire
#include <DHT.h>                    // read air temp and humidity using one digital pin (2)
#include <math.h>

// RADIO
SoftwareSerial radio(10, 11);       // RX, TX

// PIN DEFS
const int PH_PIN = A0;              // reads pH sensor voltage
const int WATER_TEMP_PIN = 9;       // water-temp sensor
const int HUMIDITY_PIN = 2;         // air temp and humidity sensor
const int LED_PIN = 3;              // controls LED

// SETTINGS
const int HUMIDITY_TYPE = DHT11;

// light-checking interval (every 1 second)
const unsigned long LIGHT_INTERVAL = 1000UL;

const float LIGHT_ON_THRESHOLD = 20.0;    // turn LED on below 20 lux
const float LIGHT_OFF_THRESHOLD = 30.0;   // turn LED off above 30 lux
const byte LIGHT_I2C_ADDRESS = 0x23;      // address to identify sensor on I2C bus

// SENSOR OBJS
OneWire oneWire(WATER_TEMP_PIN);
DallasTemperature waterTempSensor(&oneWire);
DHT humiditySensor(HUMIDITY_PIN, HUMIDITY_TYPE);

// TIMING / STATE
unsigned long lastLightCheck = 0;
bool ledIsOn = false;               // program initially considers LED off
float lastLux = NAN;                // most recent light reading (NAN = read failed / none yet)

// COMMAND INPUT
const byte CMD_MAX = 32;
char cmdBuf[CMD_MAX];               // characters received so far for the current command
byte cmdLen = 0;

// LIGHT SENSOR
bool readLight(float &lux) {
  Wire.beginTransmission(LIGHT_I2C_ADDRESS);
  Wire.write(0x10);                            // request high-resolution light measurement

  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(180);

  // ask sensor to send back 2 bytes of measurement data
  if (Wire.requestFrom(LIGHT_I2C_ADDRESS, (uint8_t)2) != 2) {
    return false;
  }

  // convert raw value into lux using the conversion factor of 1.2 (from datasheet)
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  uint16_t raw = ((uint16_t)hi << 8) | lo;
  lux = raw / 1.2;

  return true;
}

// write "label=value" to the given output, or "label=ERR" if the reading is invalid
void printField(Print &out, const char *label, float value, int decimals, bool valid) {
  out.print(label);
  out.print('=');
  if (valid) {
    out.print(value, decimals);
  } else {
    out.print("ERR");
  }
}

// build the full report line on any output (radio or USB serial)
void writeReport(Print &out, int phRaw, float phVoltage, float waterC, float airC, float humidity) {
  out.print("pH_ADC=");
  out.print(phRaw);
  out.print(',');
  printField(out, "pH_V", phVoltage, 3, true);          // [need to change to calibrated pH]
  out.print(',');
  printField(out, "water_C", waterC, 2, waterC != DEVICE_DISCONNECTED_C);
  out.print(',');
  printField(out, "air_C", airC, 2, !isnan(airC));
  out.print(',');
  printField(out, "hum", humidity, 2, !isnan(humidity));
  out.print(',');
  printField(out, "lux", lastLux, 1, !isnan(lastLux));
  out.print(",LED=");
  out.println(ledIsOn ? "ON" : "OFF");
}

// read every sensor and send the values over the radio (echoed to USB for debugging)
void takeAndSendReading() {
  // read pH value [NEED A CALIBRATION RELATIONSHIP HERE]
  int phRaw = analogRead(PH_PIN);
  float phVoltage = phRaw * (5.0 / 1023.0);

  // read water temperature
  waterTempSensor.requestTemperatures();
  float waterC = waterTempSensor.getTempCByIndex(0);

  // read air temperature and humidity
  float airC = humiditySensor.readTemperature();
  float humidity = humiditySensor.readHumidity();

  writeReport(radio, phRaw, phVoltage, waterC, airC, humidity);
  Serial.print("sent: ");
  writeReport(Serial, phRaw, phVoltage, waterC, airC, humidity);
}

// act on one complete command line
void handleCommand(const char *cmd) {
  Serial.print("received command: ");
  Serial.println(cmd);

  if (strcasecmp(cmd, "read") == 0) {
    takeAndSendReading();
  } else {
    radio.print("unknown command: ");
    radio.println(cmd);
  }
}

// collect characters from the radio; run a command when a full line arrives
void checkRadio() {
  while (radio.available()) {
    char c = radio.read();

    if (c == '\n' || c == '\r') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        handleCommand(cmdBuf);
        cmdLen = 0;
      }
    } else if (cmdLen < CMD_MAX - 1) {
      cmdBuf[cmdLen++] = c;
    }
    // characters past CMD_MAX are dropped
  }
}

// SETUP
void setup() {
  Serial.begin(115200);         // USB debug
  radio.begin(9600);            // E32 factory UART rate

  // initialize LED to be turned off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin();
  waterTempSensor.begin();
  humiditySensor.begin();

  Serial.println("buoy ready, waiting for \"read\" command...");
}

// MAIN LOOP
void loop() {
  unsigned long now = millis();

  // check ambient light every second and control the LED locally
  if (now - lastLightCheck >= LIGHT_INTERVAL) {
    lastLightCheck = now;

    float lux;
    if (readLight(lux)) {
      lastLux = lux;

      // turn LED on when it is dark outside
      if (!ledIsOn && lux < LIGHT_ON_THRESHOLD) {
        ledIsOn = true;
        digitalWrite(LED_PIN, HIGH);
      }
      // turn LED off when it is bright outside
      else if (ledIsOn && lux > LIGHT_OFF_THRESHOLD) {
        ledIsOn = false;
        digitalWrite(LED_PIN, LOW);
      }
    } else {
      lastLux = NAN;
      Serial.println("light sensor read failed");
    }
  }

  // only read and send the sensors when the receiver asks
  checkRadio();
}
