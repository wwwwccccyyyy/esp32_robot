#ifndef GPIO_DEVICE_H
#define GPIO_DEVICE_H

#include "device.h"

class GpioManager;

class GpioDevice : public Device {
public:
    explicit GpioDevice(GpioManager& gpio);

    const char* name() const override;
    int actionNames(const char* out[], int max) const override;
    const char* actionSchema() const override;
    bool execute(const ActionItem& item, String& error) override;
    void loop() override;
    bool isBusy() const override;

private:
    GpioManager& gpio;
};

#endif // GPIO_DEVICE_H
