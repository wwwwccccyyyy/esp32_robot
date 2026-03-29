#ifndef RGB_LED_DEVICE_H
#define RGB_LED_DEVICE_H

#include "device.h"

class RgbLed;

class RgbLedDevice : public Device {
public:
    explicit RgbLedDevice(RgbLed& led);

    const char* name() const override;
    int actionNames(const char* out[], int max) const override;
    const char* actionSchema() const override;
    bool execute(const ActionItem& item, String& error) override;
    void loop() override;
    bool isBusy() const override;

private:
    RgbLed& led;
    bool blinkActive = false;
};

#endif // RGB_LED_DEVICE_H
