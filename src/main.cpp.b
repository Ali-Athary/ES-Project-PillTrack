#include <Arduino.h>
#include <WiFiManager.h>
#include "HX711.h"

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 12; // Changed to GPIO 12
const int LOADCELL_SCK_PIN = 14; // Changed to GPIO 14

HX711 scale;

void setup() {
    Serial.begin(115200);
    delay(1000);

    WiFiManager wm;

    // Only for test
    wm.resetSettings();

    Serial.println("Starting WiFiManager...");

    bool res = wm.autoConnect("ESP32-Setup", "12345678");

    if (!res) {
        Serial.println("Failed to connect.");
        // Retry or sleep, depending on your device logic
        ESP.restart();
    }

    Serial.println("Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.println("=====================================");
    Serial.println("HX711 Load Cell Test");
    Serial.println("ESP32 Pins: DT=GPIO12, SCK=GPIO14");
    Serial.println("=====================================");
    delay(2000);

    Serial.println("Initializing the scale...");

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    // Give some time for stabilization
    delay(2000);

    Serial.println("Testing basic readings:");
    Serial.print("Raw read: ");
    Serial.println(scale.read());

    Serial.print("10x average: ");
    Serial.println(scale.read_average(10));

    // Initialize scale
    scale.set_scale(1.0); // Temporary calibration factor
    scale.tare(); // Reset to zero

    Serial.println("Scale initialized and tared (set to zero)");
    Serial.println("Starting continuous readings...");
    Serial.println("Raw\t|\tAverage\t|\tUnits");
    Serial.println("----------------------------------------");
}

void loop() {
    // Read and display values
    long raw = scale.read();
    float average = scale.read_average(5);
    float units = scale.get_units(5);

    Serial.print(raw);
    Serial.print("\t|\t");
    Serial.print(average, 0); // 0 decimal places
    Serial.print("\t|\t");
    Serial.print(units, 2); // 2 decimal places
    Serial.println(" units");

    // Blink built-in LED to show it's working (if available)
    static bool ledState = false;
    digitalWrite(2, ledState ? HIGH : LOW); // GPIO 2 often has built-in LED
    ledState = !ledState;

    delay(500); // Read twice per second
}
