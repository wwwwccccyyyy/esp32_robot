#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

#include <Arduino.h>

class Device;

class DeviceRegistry {
public:
    static const int MAX_DEVICES = 8;

    bool add(Device* dev);
    Device* findByAction(const char* actionName) const;
    Device* deviceAt(int i) const;
    int deviceCount() const;

    // Build the action schema prompt fragment from all registered devices.
    void buildPromptFragment(String& out) const;

private:
    Device* devices[MAX_DEVICES] = {};
    int count = 0;
};

#endif // DEVICE_REGISTRY_H
