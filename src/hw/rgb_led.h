#ifndef RGB_LED_H
#define RGB_LED_H

#include <Arduino.h>

class RgbLed {
public:
    void begin();
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void off();
    String parseCommand(const String& args);
    String getStatus();

private:
    uint8_t curR = 0, curG = 0, curB = 0;
};

#endif // RGB_LED_H
