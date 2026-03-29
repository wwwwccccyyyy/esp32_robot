#include "gpio_device.h"
#include "engine/action_queue.h"
#include "hw/gpio_manager.h"
#include <string.h>

GpioDevice::GpioDevice(GpioManager& gpio) : gpio(gpio) {}

const char* GpioDevice::name() const { return "gpio"; }

int GpioDevice::actionNames(const char* out[], int max) const {
    int n = 0;
    if (n < max) out[n++] = "set_gpio";
    return n;
}

const char* GpioDevice::actionSchema() const {
    return
        "  set_gpio: {\"action\":\"set_gpio\",\"pin\":P,\"value\":V}  pin:[48,4,5,6,7,15,16,17,18,8,3], value:0/1\n";
}

bool GpioDevice::execute(const ActionItem& item, String& error) {
    if (strcmp(item.actionName, "set_gpio") != 0) {
        error = "unknown gpio action";
        return false;
    }
    if (!gpio.isValidPin(item.pin)) {
        error = "set_gpio.pin is not allowed";
        return false;
    }
    gpio.setPin(item.pin, item.value);
    return true;
}

void GpioDevice::loop() {
    // GPIO has no periodic work.
}

bool GpioDevice::isBusy() const {
    return false;
}
