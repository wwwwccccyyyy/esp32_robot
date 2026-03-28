#include "command_handler.h"
#include "llm/deepseek_client.h"
#include "hw/gpio_manager.h"
#include "hw/rgb_led.h"
#include "config.h"
#include <WiFi.h>

// ============================================================
// Command table — add new commands here
// ============================================================
const CmdEntry CommandHandler::commands[] = {
    { "/start",  "Show help",    &CommandHandler::cmdStart  },
    { "/status", "Device info",  &CommandHandler::cmdStatus },
    // { "/reboot", "Restart device", &CommandHandler::cmdReboot },
};
const int CommandHandler::commandCount = sizeof(commands) / sizeof(commands[0]);

// ============================================================
// Action tag table — add new AI action tags here
// ============================================================
const ActionTagEntry CommandHandler::actionTags[] = {
    { "RGB:",  &CommandHandler::execRgb  },
    { "GPIO:", &CommandHandler::execGpio },
    // { "PWM:",  &CommandHandler::execPwm },
    // { "SERVO:", &CommandHandler::execServo },
};
const int CommandHandler::actionTagCount = sizeof(actionTags) / sizeof(actionTags[0]);

// ============================================================
// Core dispatch
// ============================================================

CommandHandler::CommandHandler(DeepSeekClient& llm, GpioManager& gpio, RgbLed& rgb)
    : llm(llm), gpio(gpio), rgb(rgb) {}

String CommandHandler::handle(const String& text) {
    // Slash commands
    if (text.startsWith("/")) {
        for (int i = 0; i < commandCount; i++) {
            if (text == commands[i].name) {
                return (this->*commands[i].handler)();
            }
        }
        return "Unknown command. Type /start for help.";
    }

    // Rate limiting
    unsigned long now = millis();
    if (now - lastAiCall < AI_COOLDOWN_MS) {
        unsigned long remaining = (AI_COOLDOWN_MS - (now - lastAiCall)) / 1000 + 1;
        return "Please wait " + String(remaining) + "s before next message.";
    }
    lastAiCall = now;

    // Build context and call AI
    String context = "[Device state: LED=" + rgb.getStatus() +
                     " Heap=" + String(ESP.getFreeHeap() / 1024) + "KB" +
                     " Uptime=" + String(millis() / 1000) + "s]\n" + text;

    String aiResponse = llm.chat(context);
    return executeActions(aiResponse);
}

// ============================================================
// Command handlers
// ============================================================

String CommandHandler::cmdStart() {
    return buildHelpMessage();
}

String CommandHandler::cmdStatus() {
    String msg = DEVICE_NAME " Status\n\n";
    msg += "Uptime: " + String(millis() / 1000) + "s\n";
    msg += "Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB\n";
    msg += "WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    msg += "IP: " + WiFi.localIP().toString() + "\n";
    msg += "Version: " + String(DEVICE_VERSION);
    return msg;
}

String CommandHandler::buildHelpMessage() {
    String msg = DEVICE_NAME " is ready!\n\n";
    msg += "Just tell me what you want in natural language!\n\n";
    msg += "Commands:\n";
    for (int i = 0; i < commandCount; i++) {
        msg += String(commands[i].name) + " - " + commands[i].desc + "\n";
    }
    return msg;
}

// ============================================================
// Action tag processing — generic loop
// ============================================================

String CommandHandler::executeActions(const String& aiResponse) {
    String reply = aiResponse;

    for (int t = 0; t < actionTagCount; t++) {
        String prefix = "[" + String(actionTags[t].prefix);
        int prefixLen = prefix.length();

        int pos = reply.indexOf(prefix);
        while (pos >= 0) {
            int end = reply.indexOf(']', pos);
            if (end < 0) break;

            String params = reply.substring(pos + prefixLen, end);
            (this->*actionTags[t].executor)(params);

            // Remove tag from reply
            reply = reply.substring(0, pos) + reply.substring(end + 1);
            pos = reply.indexOf(prefix);
        }
    }

    reply.trim();
    return reply;
}

// ============================================================
// Tag executors
// ============================================================

void CommandHandler::execRgb(const String& params) {
    // params: "r,g,b"
    int c1 = params.indexOf(',');
    int c2 = params.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > 0) {
        int r = constrain(params.substring(0, c1).toInt(), 0, 255);
        int g = constrain(params.substring(c1 + 1, c2).toInt(), 0, 255);
        int b = constrain(params.substring(c2 + 1).toInt(), 0, 255);
        rgb.setColor(r, g, b);
        Serial.println("Action: RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
    }
}

void CommandHandler::execGpio(const String& params) {
    // params: "pin,value"
    int comma = params.indexOf(',');
    if (comma > 0) {
        int pin = params.substring(0, comma).toInt();
        int val = params.substring(comma + 1).toInt();
        gpio.setPin(pin, val);
        Serial.println("Action: GPIO " + String(pin) + "=" + String(val));
    }
}
