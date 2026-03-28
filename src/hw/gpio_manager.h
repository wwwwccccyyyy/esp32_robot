#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include <Arduino.h>

class GpioManager {
public:
    void begin();
    bool isValidPin(int pin);
    String setPin(int pin, int value);
};

#endif // GPIO_MANAGER_H
