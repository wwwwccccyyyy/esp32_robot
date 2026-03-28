#ifndef RGB_LED_H
#define RGB_LED_H

#include <Arduino.h>

class RgbLed {
public:
    void begin();
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    bool startBlink(uint8_t r, uint8_t g, uint8_t b, uint8_t times, uint16_t durationMs, bool restoreAfter = true);
    void loop();
    bool isBlinking() const;
    void off();
    String parseCommand(const String& args);
    String getStatus();

private:
    void applyColor(uint8_t r, uint8_t g, uint8_t b);

    uint8_t curR = 0, curG = 0, curB = 0;

    bool blinking = false;
    bool blinkRestoreAfter = true;
    bool blinkStateOn = false;
    uint8_t blinkR = 0, blinkG = 0, blinkB = 0;
    uint8_t savedR = 0, savedG = 0, savedB = 0;
    int blinkTransitionsLeft = 0;
    unsigned long blinkPhaseMs = 0;
    unsigned long nextBlinkToggleAt = 0;
};

#endif // RGB_LED_H
