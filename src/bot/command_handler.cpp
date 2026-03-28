#include "command_handler.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "config.h"
#include "engine/action_engine.h"
#include "llm/deepseek_client.h"

namespace {

String toLowerCopy(String s) {
    s.toLowerCase();
    return s;
}

bool startsWithAny(const String& text, const char* const* prefixes, int n) {
    for (int i = 0; i < n; i++) {
        if (text.startsWith(prefixes[i])) {
            return true;
        }
    }
    return false;
}

bool containsAny(const String& text, const char* const* patterns, int n) {
    for (int i = 0; i < n; i++) {
        if (text.indexOf(patterns[i]) >= 0) {
            return true;
        }
    }
    return false;
}

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

bool looksLikeLlmError(const String& text) {
    return text.startsWith("HTTP Error:") ||
           text.startsWith("Connection") ||
           text.startsWith("API request failed") ||
           text.startsWith("Parse error:") ||
           text.startsWith("API Key not configured");
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

    // Local fast chitchat path: avoid remote dependency for simple greetings.
    bool localHandled = false;
    String localReply = tryLocalChitchat(input, localHandled);
    if (localHandled) {
        return localReply;
    }

    unsigned long now = millis();
    if (now - lastAiCall < AI_COOLDOWN_MS) {
        unsigned long remaining = (AI_COOLDOWN_MS - (now - lastAiCall)) / 1000 + 1;
        return "我还在整理上一条，等我 " + String(remaining) + "s 再来一条。";
    }
    lastAiCall = now;

    // Path 2: chat-only requests use conversational prompt.
    if (!likelyHardwareIntent(input)) {
        String chatPrompt;
        chatPrompt += "You are cy_robot on an ESP32 device.\\n";
        chatPrompt += "Reply warmly, concise, in the same language as the user.\\n";
        chatPrompt += "Do not output action tags or JSON.\\n";
        chatPrompt += "User:\\n";
        chatPrompt += input;

        String chatReply = cleanReply(llm.chat(chatPrompt));
        if (chatReply.length() == 0 || looksLikeLlmError(chatReply)) {
            return "我听到了，网络有点慢，我们可以先聊，也可以直接发控制命令。";
        }
        return chatReply;
    }

    // Path 3: hardware intent -> planner JSON.
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
    plannerPrompt += "If user asks no hardware action, set sequence to [].\\n";
    plannerPrompt += "User request:\\n";
    plannerPrompt += input;

    String aiRaw = cleanReply(llm.chat(plannerPrompt));
    if (aiRaw.length() == 0 || looksLikeLlmError(aiRaw)) {
        return "命令我收到了，但云端暂时没回我。你可以重试一次，或直接发 JSON 指令。";
    }

    String jsonPayload = extractJsonPayload(aiRaw);
    if (jsonPayload.length() == 0) {
        // Keep natural conversation if planner format drifts.
        return aiRaw;
    }

    JsonDocument doc;
    DeserializationError parseError = deserializeJson(doc, jsonPayload);
    if (parseError) {
        return aiRaw;
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

bool CommandHandler::likelyHardwareIntent(const String& text) const {
    String lower = toLowerCopy(text);

    static const char* const enKeywords[] = {
        "led", "light", "blink", "flash", "rgb", "gpio", "pin", "turn on", "turn off", "set", "color"
    };
    static const char* const zhKeywords[] = {
        "灯", "闪", "颜色", "红", "绿", "蓝", "亮", "灭", "引脚", "脚", "开", "关", "秒", "毫秒"
    };

    return containsAny(lower, enKeywords, sizeof(enKeywords) / sizeof(enKeywords[0])) ||
           containsAny(text, zhKeywords, sizeof(zhKeywords) / sizeof(zhKeywords[0]));
}

String CommandHandler::tryLocalChitchat(const String& text, bool& handled) const {
    handled = false;

    String input = text;
    input.trim();
    String lower = toLowerCopy(input);

    static const char* const greetPrefixes[] = {
        "hi", "hello", "hey", "你好", "您好", "在吗", "在嗎", "哈喽", "哈囉"
    };

    if (startsWithAny(lower, greetPrefixes, sizeof(greetPrefixes) / sizeof(greetPrefixes[0])) ||
        startsWithAny(input, greetPrefixes, sizeof(greetPrefixes) / sizeof(greetPrefixes[0]))) {
        handled = true;
        return "你好呀，我在线。想聊聊天，还是要我控制硬件？";
    }

    if (lower == "thanks" || lower == "thank you" || input == "谢谢" || input == "謝謝") {
        handled = true;
        return "不客气，我们继续。";
    }

    return "";
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
