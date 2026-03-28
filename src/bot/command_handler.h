#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

class DeepSeekClient;
class ActionEngine;

class CommandHandler {
public:
    CommandHandler(DeepSeekClient& llm, ActionEngine& actionEngine);
    String handle(const String& text);

private:
    DeepSeekClient& llm;
    ActionEngine& actionEngine;

    unsigned long lastAiCall = 0;
    static const unsigned long AI_COOLDOWN_MS = 3000;

    String handleSlashCommand(const String& text);
    String buildHelpMessage();
    String buildStatusMessage();

    bool looksLikeJson(const String& text) const;
    String extractJsonPayload(const String& raw) const;
    bool enqueueJsonPlan(const String& raw, bool allowExtract, String& response, int& addedCount);
};

#endif // COMMAND_HANDLER_H
