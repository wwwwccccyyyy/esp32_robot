#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

class DeepSeekClient;
class GpioManager;
class RgbLed;

class CommandHandler;

// Command entry: name + handler function pointer + description
struct CmdEntry {
    const char* name;
    const char* desc;
    String (CommandHandler::*handler)();
};

// Action tag: prefix + executor
typedef void (CommandHandler::*TagExecutor)(const String& params);
struct ActionTagEntry {
    const char* prefix;   // e.g. "RGB:"
    TagExecutor executor;
};

class CommandHandler {
public:
    CommandHandler(DeepSeekClient& llm, GpioManager& gpio, RgbLed& rgb);
    String handle(const String& text);

private:
    DeepSeekClient& llm;
    GpioManager& gpio;
    RgbLed& rgb;

    unsigned long lastAiCall = 0;
    static const unsigned long AI_COOLDOWN_MS = 5000;

    // --- Command table ---
    static const CmdEntry commands[];
    static const int commandCount;
    String buildHelpMessage();

    // Command handlers (add new ones here + register in commands[])
    String cmdStart();
    String cmdStatus();

    // --- Action tag table ---
    static const ActionTagEntry actionTags[];
    static const int actionTagCount;
    String executeActions(const String& aiResponse);
    String extractAndExecuteTags(String reply);

    // Tag executors (add new ones here + register in actionTags[])
    void execRgb(const String& params);
    void execGpio(const String& params);
};

#endif // COMMAND_HANDLER_H
