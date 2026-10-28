const int PH_PIN = A0;

void setup() {
    Serial.begin(9600);
}

void loop() {
    int phRaw = analogRead(PH_PIN);
    float phVoltage = phRaw * (5.0 / 1023.0);

    Serial.print("pH ADC: ");
    Serial.print(phRaw);

    Serial.print(" | pH voltage: ");
    Serial.print(phVoltage, 3);
    Serial.println(" V");

    delay(1000);
}