#ifndef ACTION_QUEUE_H
#define ACTION_QUEUE_H

#include <Arduino.h>

// Pure POD — safe for FreeRTOS xQueueSend/Receive (memcpy).
struct ActionItem {
    char actionName[16];  // "set_rgb", "blink_rgb", "set_gpio", "wait"
    uint8_t r, g, b;
    uint8_t times;
    uint16_t durationMs;
    int8_t pin;
    uint8_t value;
};

#if __has_include(<freertos/FreeRTOS.h>)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

class ActionQueue {
public:
    static const int MAX_QUEUE = 16;

    void begin();
    bool send(const ActionItem& item);
    bool receive(ActionItem& item, uint32_t waitTicks = 0);
    void clear();
    int count() const;

private:
#ifdef _FREERTOS_QUEUE_H
    QueueHandle_t handle = nullptr;
#else
    // Fallback single-thread ring buffer
    ActionItem buf[MAX_QUEUE];
    int head = 0;
    int tail = 0;
    int cnt = 0;
#endif
};

#endif // ACTION_QUEUE_H
