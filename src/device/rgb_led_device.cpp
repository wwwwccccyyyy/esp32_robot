#include "rgb_led_device.h"
#include "engine/action_queue.h"
#include "hw/rgb_led.h"
#include <string.h>

RgbLedDevice::RgbLedDevice(RgbLed& led) : led(led) {}

const char* RgbLedDevice::name() const { return "rgb_led"; }

int RgbLedDevice::actionNames(const char* out[], int max) const {
    int n = 0;
    if (n < max) out[n++] = "set_rgb";
    if (n < max) out[n++] = "blink_rgb";
    return n;
}

const char* RgbLedDevice::actionSchema() const {
    return
        "  set_rgb: {\"action\":\"set_rgb\",\"color\":[R,G,B]}  R/G/B: 0-255\n"
        "  blink_rgb: {\"action\":\"blink_rgb\",\"color\":[R,G,B],\"times\":N,\"duration_ms\":MS}  times:1-20, duration_ms:100-60000\n";
}

bool RgbLedDevice::execute(const ActionItem& item, String& error) {
    if (strcmp(item.actionName, "set_rgb") == 0) {
        led.setColor(item.r, item.g, item.b);
        blinkActive = false;
        return true;
    }
    if (strcmp(item.actionName, "blink_rgb") == 0) {
        if (led.startBlink(item.r, item.g, item.b, item.times, item.durationMs, true)) {
            blinkActive = true;
            return true;
        }
        error = "blink_rgb start failed";
        return false;
    }
    error = "unknown rgb_led action";
    return false;
}

void RgbLedDevice::loop() {
    led.loop();
    if (blinkActive && !led.isBlinking()) {
        blinkActive = false;
    }
}

bool RgbLedDevice::isBusy() const {
    return blinkActive || led.isBlinking();
}
