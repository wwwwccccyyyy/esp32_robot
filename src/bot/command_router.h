#ifndef COMMAND_ROUTER_H
#define COMMAND_ROUTER_H

#include <Arduino.h>

class Planner;
class ActionQueue;
class DeviceRegistry;

class CommandRouter {
public:
    CommandRouter(Planner& planner, ActionQueue& queue, DeviceRegistry& registry);
    String handle(const String& text);

private:
    Planner& planner;
    ActionQueue& queue;
    DeviceRegistry& registry;

    String handleSlashCommand(const String& text);
    String buildHelpMessage();
    String buildStatusMessage();
    bool handleDirectJson(const String& text, String& response);
};

#endif // COMMAND_ROUTER_H
