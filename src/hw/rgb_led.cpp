#include "rgb_led.h"
#include "pins.h"

// ESP32-S3 has built-in RMT-based WS2812 driver
#include <esp32-hal-rgb-led.h>

void RgbLed::begin() {
    off();
    Serial.println("RGB LED initialized on GPIO " + String(RGB_LED_PIN));
}

void RgbLed::setColor(uint8_t r, uint8_t g, uint8_t b) {
    curR = r; curG = g; curB = b;
    neopixelWrite(RGB_LED_PIN, r, g, b);
}

void RgbLed::off() {
    setColor(0, 0, 0);
}

String RgbLed::parseCommand(const String& args) {
    String a = args;
    a.trim();

    // Predefined color names
    if (a == "red")     { setColor(255, 0, 0);   return "RGB: red"; }
    if (a == "green")   { setColor(0, 255, 0);   return "RGB: green"; }
    if (a == "blue")    { setColor(0, 0, 255);    return "RGB: blue"; }
    if (a == "white")   { setColor(255, 255, 255); return "RGB: white"; }
    if (a == "yellow")  { setColor(255, 255, 0);  return "RGB: yellow"; }
    if (a == "cyan")    { setColor(0, 255, 255);  return "RGB: cyan"; }
    if (a == "purple")  { setColor(128, 0, 255);  return "RGB: purple"; }
    if (a == "orange")  { setColor(255, 100, 0);  return "RGB: orange"; }
    if (a == "off")     { off();                   return "RGB: off"; }

    // Parse "r g b" format
    int i1 = a.indexOf(' ');
    if (i1 == -1) {
        return "Usage: /rgb <color> or /rgb <r> <g> <b>\n"
               "Colors: red green blue white yellow cyan purple orange off";
    }
    int i2 = a.indexOf(' ', i1 + 1);
    if (i2 == -1) {
        return "Usage: /rgb <r> <g> <b>\nExample: /rgb 255 0 128";
    }

    int r = constrain(a.substring(0, i1).toInt(), 0, 255);
    int g = constrain(a.substring(i1 + 1, i2).toInt(), 0, 255);
    int b = constrain(a.substring(i2 + 1).toInt(), 0, 255);

    setColor(r, g, b);
    return "RGB: (" + String(r) + ", " + String(g) + ", " + String(b) + ")";
}

String RgbLed::getStatus() {
    return "RGB(" + String(curR) + "," + String(curG) + "," + String(curB) + ")";
}
