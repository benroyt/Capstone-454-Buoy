#include <DHT.h>

// code to test the DHT11 (air temperature/humidity sensor) every two seconds

#define HUMIDITY_PIN 2
#define HUMIDITY_TYPE DHT11

DHT humiditySensor(HUMIDITY_PIN, HUMIDITY_TYPE);

void setup() {
    Serial.begin(9600);
    humiditySensor.begin();
}

void loop() {
    float humidity = humiditySensor.readHumidity();
    float airTempC = humiditySensor.readTemperature();

    if (isnan(humidity) || isnan(airTempC)) {
        Serial.println("error: sensor readings failed");
    } else {
        Serial.print("humidity: ");
        Serial.print(humidity);
        Serial.println(" %");

        Serial.print("air temperature: ");
        Serial.print(airTempC);
        Serial.println(" deg C");
    }

    delay(2000);
}