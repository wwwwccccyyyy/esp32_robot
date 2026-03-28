#include "gpio_manager.h"
#include "pins.h"

// Pins that are safe to control remotely
// Add your own pins here as needed
static const int ALLOWED_PINS[] = {
    RGB_LED_PIN,      // Onboard RGB LED
    4, 5, 6, 7,       // General purpose GPIOs
    15, 16, 17, 18,
    8, 3,              // Common breakout pins
};
static const int ALLOWED_PIN_COUNT = sizeof(ALLOWED_PINS) / sizeof(ALLOWED_PINS[0]);

void GpioManager::begin() {
    // GPIO manager ready (RGB LED handled by RgbLed class)
    Serial.println("GPIO Manager initialized");
}

bool GpioManager::isValidPin(int pin) {
    for (int i = 0; i < ALLOWED_PIN_COUNT; i++) {
        if (ALLOWED_PINS[i] == pin) return true;
    }
    return false;
}

String GpioManager::setPin(int pin, int value) {
    if (!isValidPin(pin)) {
        return "Invalid pin. Allowed: 2,3,4,5,6,7,8,15,16,17,18";
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, value ? HIGH : LOW);
    return "GPIO " + String(pin) + " set to " + String(value ? "HIGH" : "LOW");
}
