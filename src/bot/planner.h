#ifndef PLANNER_H
#define PLANNER_H

#include <Arduino.h>

class DeepSeekClient;
class DeviceRegistry;
class ActionQueue;

class Planner {
public:
    Planner(DeepSeekClient& llm, DeviceRegistry& registry, ActionQueue& queue);
    String plan(const String& userText);

private:
    DeepSeekClient& llm;
    DeviceRegistry& registry;
    ActionQueue& queue;

    unsigned long lastAiCall = 0;
    static const unsigned long AI_COOLDOWN_MS = 3000;

    bool likelyHardwareIntent(const String& text) const;
    String tryLocalChitchat(const String& text, bool& handled) const;
    String extractJsonPayload(const String& raw) const;
    bool enqueueFromJson(const String& jsonPayload, String& error, int& addedCount);
    String buildPlannerPrompt(const String& userInput) const;
};

#endif // PLANNER_H
