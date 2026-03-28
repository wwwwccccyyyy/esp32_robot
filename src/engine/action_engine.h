#ifndef ACTION_ENGINE_H
#define ACTION_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>

class RgbLed;
class GpioManager;

class ActionEngine {
public:
    ActionEngine(RgbLed& rgb, GpioManager& gpio);

    void loop();
    bool enqueueFromJson(const String& jsonText, String& message, int& addedCount);
    void clearQueue();

    int queuedCount() const;
    bool isBusy() const;
    String status() const;

private:
    enum ActionType : uint8_t {
        ACT_SET_RGB = 0,
        ACT_BLINK_RGB,
        ACT_SET_GPIO,
        ACT_WAIT
    };

    struct ActionItem {
        ActionType type = ACT_WAIT;
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t times = 0;
        uint16_t durationMs = 0;
        int8_t pin = -1;
        uint8_t value = 0;
    };

    static const int MAX_QUEUE = 16;
    ActionItem queue[MAX_QUEUE];
    int queueHead = 0;
    int queueTail = 0;
    int queueCount = 0;

    bool waitActive = false;
    unsigned long waitUntil = 0;
    bool blinkActive = false;

    RgbLed& rgb;
    GpioManager& gpio;

    bool enqueueAction(const ActionItem& action, String& error);
    bool dequeueAction(ActionItem& action);

    bool parseActionObject(const JsonVariantConst& obj, ActionItem& action, String& error);
    bool parseColor(const JsonVariantConst& node, uint8_t& r, uint8_t& g, uint8_t& b, String& error);
    bool parseValue01(const JsonVariantConst& node, uint8_t& valueOut, String& error);

    static bool toIntInRange(const JsonVariantConst& node, int minV, int maxV, int& outValue);
};

#endif // ACTION_ENGINE_H
