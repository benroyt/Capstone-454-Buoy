#include <OneWire.h>
#include <DallasTemperature.h>

const int WATER_TEMP_PIN = 9;

OneWire oneWire(WATER_TEMP_PIN);
DallasTemperature waterTempSensor(&oneWire);

void setup() {
    Serial.begin(9600);
    waterTempSensor.begin();
}

void loop() {
    waterTempSensor.requestTemperatures();

    float waterTempC = waterTempSensor.getTempCByIndex(0);
    if (waterTempC != DEVICE_DISCONNECTED_C) {
        Serial.print("water temperature: ");
        Serial.print(waterTempC);
        Serial.println(" deg C");
    } else {
        Serial.println("error: sensor reading failed");
    }

    delay(2000);
}