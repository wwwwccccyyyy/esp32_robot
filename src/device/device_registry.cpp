#include "device_registry.h"
#include "device.h"
#include "engine/action_queue.h"
#include <string.h>

bool DeviceRegistry::add(Device* dev) {
    if (!dev || count >= MAX_DEVICES) return false;
    devices[count++] = dev;
    return true;
}

Device* DeviceRegistry::findByAction(const char* actionName) const {
    const char* names[8];
    for (int i = 0; i < count; i++) {
        int n = devices[i]->actionNames(names, 8);
        for (int j = 0; j < n; j++) {
            if (strcmp(names[j], actionName) == 0) {
                return devices[i];
            }
        }
    }
    return nullptr;
}

Device* DeviceRegistry::deviceAt(int i) const {
    if (i < 0 || i >= count) return nullptr;
    return devices[i];
}

int DeviceRegistry::deviceCount() const {
    return count;
}

void DeviceRegistry::buildPromptFragment(String& out) const {
    out += "Actions:\n";
    for (int i = 0; i < count; i++) {
        out += devices[i]->actionSchema();
    }
    out += "  wait: {\"action\":\"wait\",\"duration_ms\":MS}  1-60000\n";
}
