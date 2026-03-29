#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>

struct ActionItem;

class Device {
public:
    virtual ~Device() = default;
    virtual const char* name() const = 0;
    virtual int actionNames(const char* out[], int max) const = 0;
    virtual const char* actionSchema() const = 0;
    virtual bool execute(const ActionItem& item, String& error) = 0;
    virtual void loop() = 0;
    virtual bool isBusy() const = 0;
};

#endif // DEVICE_H
