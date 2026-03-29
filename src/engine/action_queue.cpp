#include "action_queue.h"
#include <string.h>

#ifdef _FREERTOS_QUEUE_H

void ActionQueue::begin() {
    if (!handle) {
        handle = xQueueCreate(MAX_QUEUE, sizeof(ActionItem));
    }
}

bool ActionQueue::send(const ActionItem& item) {
    if (!handle) return false;
    return xQueueSend(handle, &item, 0) == pdTRUE;
}

bool ActionQueue::receive(ActionItem& item, uint32_t waitTicks) {
    if (!handle) return false;
    return xQueueReceive(handle, &item, waitTicks) == pdTRUE;
}

void ActionQueue::clear() {
    if (handle) xQueueReset(handle);
}

int ActionQueue::count() const {
    if (!handle) return 0;
    return (int)uxQueueMessagesWaiting(handle);
}

#else
// Single-thread ring buffer fallback (for unit tests / non-FreeRTOS builds).

void ActionQueue::begin() {
    head = tail = cnt = 0;
}

bool ActionQueue::send(const ActionItem& item) {
    if (cnt >= MAX_QUEUE) return false;
    buf[tail] = item;
    tail = (tail + 1) % MAX_QUEUE;
    cnt++;
    return true;
}

bool ActionQueue::receive(ActionItem& item, uint32_t) {
    if (cnt <= 0) return false;
    item = buf[head];
    head = (head + 1) % MAX_QUEUE;
    cnt--;
    return true;
}

void ActionQueue::clear() {
    head = tail = cnt = 0;
}

int ActionQueue::count() const {
    return cnt;
}

#endif
