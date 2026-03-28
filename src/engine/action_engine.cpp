#include "action_engine.h"

#include "hw/rgb_led.h"
#include "hw/gpio_manager.h"

ActionEngine::ActionEngine(RgbLed& rgb, GpioManager& gpio)
    : rgb(rgb), gpio(gpio) {}

void ActionEngine::loop() {
    rgb.loop();

    if (waitActive) {
        if ((long)(millis() - waitUntil) < 0) {
            return;
        }
        waitActive = false;
    }

    if (blinkActive) {
        if (rgb.isBlinking()) {
            return;
        }
        blinkActive = false;
    }

    if (queueCount <= 0) {
        return;
    }

    ActionItem action;
    if (!dequeueAction(action)) {
        return;
    }

    switch (action.type) {
        case ACT_SET_RGB:
            rgb.setColor(action.r, action.g, action.b);
            break;

        case ACT_BLINK_RGB:
            if (rgb.startBlink(action.r, action.g, action.b, action.times, action.durationMs, true)) {
                blinkActive = true;
            }
            break;

        case ACT_SET_GPIO:
            gpio.setPin(action.pin, action.value);
            break;

        case ACT_WAIT:
            waitActive = true;
            waitUntil = millis() + action.durationMs;
            break;
    }
}

bool ActionEngine::enqueueFromJson(const String& jsonText, String& message, int& addedCount) {
    addedCount = 0;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonText);
    if (err) {
        message = "JSON parse error: " + String(err.c_str());
        return false;
    }

    // Transactional parse: no partial enqueue on error.
    ActionItem staged[MAX_QUEUE];
    int stagedCount = 0;
    String localError;

    auto stageOne = [&](const ActionItem& action) -> bool {
        if (stagedCount >= MAX_QUEUE) {
            localError = "Too many actions in one request.";
            return false;
        }
        staged[stagedCount++] = action;
        return true;
    };

    if (doc.is<JsonArrayConst>()) {
        JsonArrayConst arr = doc.as<JsonArrayConst>();
        for (JsonVariantConst v : arr) {
            ActionItem action;
            if (!parseActionObject(v, action, localError)) {
                message = localError;
                return false;
            }
            if (!stageOne(action)) {
                message = localError;
                return false;
            }
        }
    } else if (doc.is<JsonObjectConst>()) {
        JsonObjectConst root = doc.as<JsonObjectConst>();

        if (root["sequence"].is<JsonArrayConst>()) {
            JsonArrayConst arr = root["sequence"].as<JsonArrayConst>();
            for (JsonVariantConst v : arr) {
                ActionItem action;
                if (!parseActionObject(v, action, localError)) {
                    message = localError;
                    return false;
                }
                if (!stageOne(action)) {
                    message = localError;
                    return false;
                }
            }
        } else if (root["action"].is<const char*>()) {
            ActionItem action;
            if (!parseActionObject(root, action, localError)) {
                message = localError;
                return false;
            }
            if (!stageOne(action)) {
                message = localError;
                return false;
            }
        } else {
            message = "No executable action found (expected 'action' or 'sequence').";
            return false;
        }
    } else {
        message = "JSON root must be object or array.";
        return false;
    }

    if (queueCount + stagedCount > MAX_QUEUE) {
        message = "Action queue full.";
        return false;
    }

    for (int i = 0; i < stagedCount; i++) {
        queue[queueTail] = staged[i];
        queueTail = (queueTail + 1) % MAX_QUEUE;
        queueCount++;
    }

    addedCount = stagedCount;
    message = "ok";
    return true;
}

void ActionEngine::clearQueue() {
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;
    waitActive = false;
    waitUntil = 0;
    blinkActive = false;
    rgb.off();
}

int ActionEngine::queuedCount() const {
    return queueCount;
}

bool ActionEngine::isBusy() const {
    return queueCount > 0 || waitActive || blinkActive || rgb.isBlinking();
}

String ActionEngine::status() const {
    String s = "queue=" + String(queueCount);

    if (waitActive) {
        long remain = (long)(waitUntil - millis());
        if (remain < 0) {
            remain = 0;
        }
        s += ", wait=" + String(remain) + "ms";
    }

    if (blinkActive || rgb.isBlinking()) {
        s += ", blinking";
    }

    return s;
}

bool ActionEngine::enqueueAction(const ActionItem& action, String& error) {
    if (queueCount >= MAX_QUEUE) {
        error = "Action queue full.";
        return false;
    }

    queue[queueTail] = action;
    queueTail = (queueTail + 1) % MAX_QUEUE;
    queueCount++;
    return true;
}

bool ActionEngine::dequeueAction(ActionItem& action) {
    if (queueCount <= 0) {
        return false;
    }

    action = queue[queueHead];
    queueHead = (queueHead + 1) % MAX_QUEUE;
    queueCount--;
    return true;
}

bool ActionEngine::parseActionObject(const JsonVariantConst& obj, ActionItem& action, String& error) {
    if (!obj.is<JsonObjectConst>()) {
        error = "Each action must be an object.";
        return false;
    }

    JsonObjectConst o = obj.as<JsonObjectConst>();
    const char* actionName = o["action"] | "";

    if (strlen(actionName) == 0) {
        error = "Action missing 'action' field.";
        return false;
    }

    if (strcmp(actionName, "set_rgb") == 0) {
        uint8_t r, g, b;
        if (!parseColor(o["color"], r, g, b, error)) {
            return false;
        }

        action.type = ACT_SET_RGB;
        action.r = r;
        action.g = g;
        action.b = b;
        return true;
    }

    if (strcmp(actionName, "blink_rgb") == 0) {
        uint8_t r, g, b;
        if (!parseColor(o["color"], r, g, b, error)) {
            return false;
        }

        int times = 0;
        if (!toIntInRange(o["times"], 1, 20, times)) {
            error = "blink_rgb.times must be 1..20";
            return false;
        }

        int durationMs = 0;
        if (!toIntInRange(o["duration_ms"], 100, 60000, durationMs)) {
            error = "blink_rgb.duration_ms must be 100..60000";
            return false;
        }

        action.type = ACT_BLINK_RGB;
        action.r = r;
        action.g = g;
        action.b = b;
        action.times = (uint8_t)times;
        action.durationMs = (uint16_t)durationMs;
        return true;
    }

    if (strcmp(actionName, "set_gpio") == 0) {
        int pin = 0;
        if (!toIntInRange(o["pin"], 0, 48, pin)) {
            error = "set_gpio.pin must be 0..48";
            return false;
        }

        if (!gpio.isValidPin(pin)) {
            error = "set_gpio.pin is not allowed.";
            return false;
        }

        uint8_t value = 0;
        if (!parseValue01(o["value"], value, error)) {
            return false;
        }

        action.type = ACT_SET_GPIO;
        action.pin = (int8_t)pin;
        action.value = value;
        return true;
    }

    if (strcmp(actionName, "wait") == 0) {
        int durationMs = 0;
        if (!toIntInRange(o["duration_ms"], 1, 60000, durationMs)) {
            error = "wait.duration_ms must be 1..60000";
            return false;
        }

        action.type = ACT_WAIT;
        action.durationMs = (uint16_t)durationMs;
        return true;
    }

    error = "Unsupported action: " + String(actionName);
    return false;
}

bool ActionEngine::parseColor(const JsonVariantConst& node, uint8_t& r, uint8_t& g, uint8_t& b, String& error) {
    int rv = 0;
    int gv = 0;
    int bv = 0;

    if (node.is<JsonArrayConst>()) {
        JsonArrayConst arr = node.as<JsonArrayConst>();
        if (arr.size() != 3) {
            error = "color array must have 3 elements.";
            return false;
        }

        if (!toIntInRange(arr[0], 0, 255, rv) ||
            !toIntInRange(arr[1], 0, 255, gv) ||
            !toIntInRange(arr[2], 0, 255, bv)) {
            error = "color values must be 0..255";
            return false;
        }
    } else if (node.is<JsonObjectConst>()) {
        JsonObjectConst obj = node.as<JsonObjectConst>();
        if (!toIntInRange(obj["r"], 0, 255, rv) ||
            !toIntInRange(obj["g"], 0, 255, gv) ||
            !toIntInRange(obj["b"], 0, 255, bv)) {
            error = "color.r/g/b must be 0..255";
            return false;
        }
    } else {
        error = "color must be array [r,g,b] or object {r,g,b}.";
        return false;
    }

    r = (uint8_t)rv;
    g = (uint8_t)gv;
    b = (uint8_t)bv;
    return true;
}

bool ActionEngine::parseValue01(const JsonVariantConst& node, uint8_t& valueOut, String& error) {
    if (node.is<bool>()) {
        valueOut = node.as<bool>() ? 1 : 0;
        return true;
    }

    int v = 0;
    if (!toIntInRange(node, 0, 1, v)) {
        error = "value must be 0/1 or boolean";
        return false;
    }

    valueOut = (uint8_t)v;
    return true;
}

bool ActionEngine::toIntInRange(const JsonVariantConst& node, int minV, int maxV, int& outValue) {
    if (node.isNull()) {
        return false;
    }

    int v = node.as<int>();
    if (v < minV || v > maxV) {
        return false;
    }

    outValue = v;
    return true;
}
