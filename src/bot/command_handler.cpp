#include "command_handler.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "config.h"
#include "engine/action_engine.h"
#include "llm/deepseek_client.h"

namespace {

String cleanReply(String text) {
    text.trim();

    if (text.startsWith("```")) {
        int firstNewline = text.indexOf('\n');
        int lastFence = text.lastIndexOf("```");
        if (firstNewline >= 0 && lastFence > firstNewline) {
            text = text.substring(firstNewline + 1, lastFence);
            text.trim();
        }
    }

    if (text.length() > 280) {
        text = text.substring(0, 280) + "...";
    }

    return text;
}

} // namespace

CommandHandler::CommandHandler(DeepSeekClient& llm, ActionEngine& actionEngine)
    : llm(llm), actionEngine(actionEngine) {}

String CommandHandler::handle(const String& text) {
    String input = text;
    input.trim();

    if (input.length() == 0) {
        return "我在呢，你可以直接说想做什么。";
    }

    if (input.startsWith("/")) {
        return handleSlashCommand(input);
    }

    // Path 1: direct JSON command from user.
    if (looksLikeJson(input)) {
        String response;
        int added = 0;
        if (enqueueJsonPlan(input, false, response, added)) {
            if (added <= 0) {
                return "JSON accepted, no actions scheduled.";
            }
            return "Queued " + String(added) + " action(s). " + actionEngine.status();
        }
        return "JSON rejected: " + response;
    }

    // Path 2: natural language -> LLM planner JSON.
    unsigned long now = millis();
    if (now - lastAiCall < AI_COOLDOWN_MS) {
        unsigned long remaining = (AI_COOLDOWN_MS - (now - lastAiCall)) / 1000 + 1;
        return "我还在整理上一条，等我 " + String(remaining) + "s 再来一条。";
    }
    lastAiCall = now;

    String plannerPrompt;
    plannerPrompt += "You are planning actions for an ESP32 device.\\n";
    plannerPrompt += "Return ONLY a single JSON object, no markdown.\\n";
    plannerPrompt += "Schema: {\"reply\":\"warm short message in user's language\",\"sequence\":[...]}\\n";
    plannerPrompt += "Allowed actions: set_rgb, blink_rgb, set_gpio, wait.\\n";
    plannerPrompt += "Allowed GPIO pins: 48,4,5,6,7,15,16,17,18,8,3.\\n";
    plannerPrompt += "Action templates:\\n";
    plannerPrompt += "{\"action\":\"set_rgb\",\"color\":[r,g,b]}\\n";
    plannerPrompt += "{\"action\":\"blink_rgb\",\"color\":[r,g,b],\"times\":3,\"duration_ms\":1000}\\n";
    plannerPrompt += "{\"action\":\"set_gpio\",\"pin\":4,\"value\":1}\\n";
    plannerPrompt += "{\"action\":\"wait\",\"duration_ms\":500}\\n";
    plannerPrompt += "If user is chatting and no hardware action is needed, set sequence to [].\\n";
    plannerPrompt += "User request:\\n";
    plannerPrompt += input;

    String aiRaw = llm.chat(plannerPrompt);
    aiRaw = cleanReply(aiRaw);

    String jsonPayload = extractJsonPayload(aiRaw);
    if (jsonPayload.length() == 0) {
        // Fallback: keep conversation fun even when planner format fails.
        if (aiRaw.length() > 0) {
            return aiRaw;
        }
        return "我理解得还不够准确，你可以再具体一点，比如：1秒红灯闪3次。";
    }

    JsonDocument doc;
    DeserializationError parseError = deserializeJson(doc, jsonPayload);
    if (parseError) {
        if (aiRaw.length() > 0) {
            return aiRaw;
        }
        return "Planner JSON parse failed: " + String(parseError.c_str());
    }

    String reply = "收到，我来处理。";
    if (doc["reply"].is<const char*>()) {
        String parsedReply = cleanReply(doc["reply"].as<String>());
        if (parsedReply.length() > 0) {
            reply = parsedReply;
        }
    }

    bool hasExecutable = false;
    if (doc.is<JsonObjectConst>()) {
        JsonObjectConst obj = doc.as<JsonObjectConst>();
        if (obj["action"].is<const char*>()) {
            hasExecutable = true;
        } else if (obj["sequence"].is<JsonArrayConst>()) {
            hasExecutable = obj["sequence"].as<JsonArrayConst>().size() > 0;
        }
    } else if (doc.is<JsonArrayConst>()) {
        hasExecutable = doc.as<JsonArrayConst>().size() > 0;
    }

    if (!hasExecutable) {
        return reply;
    }

    String enqueueResult;
    int added = 0;
    if (!enqueueJsonPlan(jsonPayload, false, enqueueResult, added)) {
        return "Plan rejected: " + enqueueResult;
    }

    if (added <= 0) {
        return reply;
    }

    return reply + " | queued " + String(added) + " action(s). " + actionEngine.status();
}

String CommandHandler::handleSlashCommand(const String& text) {
    if (text == "/start" || text == "/help") {
        return buildHelpMessage();
    }

    if (text == "/status") {
        return buildStatusMessage();
    }

    if (text == "/queue") {
        return "Queue status: " + actionEngine.status();
    }

    if (text == "/clear") {
        actionEngine.clearQueue();
        return "Queue cleared. LED off.";
    }

    return "Unknown command. Use /help.";
}

String CommandHandler::buildHelpMessage() {
    String msg = DEVICE_NAME " is ready!\\n\\n";
    msg += "Commands:\\n";
    msg += "/help - show help\\n";
    msg += "/status - device status\\n";
    msg += "/queue - queue status\\n";
    msg += "/clear - clear queue and stop effects\\n\\n";
    msg += "You can chat naturally, or send direct JSON.\\n";
    msg += "Example JSON:\\n";
    msg += "{\"sequence\":[{\"action\":\"blink_rgb\",\"color\":[255,0,0],\"times\":3,\"duration_ms\":1000}]}";
    return msg;
}

String CommandHandler::buildStatusMessage() {
    String msg = DEVICE_NAME " Status\\n\\n";
    msg += "Uptime: " + String(millis() / 1000) + "s\\n";
    msg += "Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB\\n";
    msg += "WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\\n";
    msg += "IP: " + WiFi.localIP().toString() + "\\n";
    msg += "Version: " + String(DEVICE_VERSION) + "\\n";
    msg += "Actions: " + actionEngine.status();
    return msg;
}

bool CommandHandler::looksLikeJson(const String& text) const {
    return text.startsWith("{") || text.startsWith("[");
}

String CommandHandler::extractJsonPayload(const String& raw) const {
    String s = raw;
    s.trim();

    if (looksLikeJson(s)) {
        return s;
    }

    int objStart = s.indexOf('{');
    int objEnd = s.lastIndexOf('}');
    int arrStart = s.indexOf('[');
    int arrEnd = s.lastIndexOf(']');

    bool hasObj = (objStart >= 0 && objEnd > objStart);
    bool hasArr = (arrStart >= 0 && arrEnd > arrStart);

    if (!hasObj && !hasArr) {
        return "";
    }

    if (hasObj && (!hasArr || objStart <= arrStart)) {
        return s.substring(objStart, objEnd + 1);
    }

    return s.substring(arrStart, arrEnd + 1);
}

bool CommandHandler::enqueueJsonPlan(const String& raw, bool allowExtract, String& response, int& addedCount) {
    String payload = raw;
    if (allowExtract) {
        payload = extractJsonPayload(raw);
        if (payload.length() == 0) {
            response = "No JSON payload found.";
            return false;
        }
    }

    return actionEngine.enqueueFromJson(payload, response, addedCount);
}
