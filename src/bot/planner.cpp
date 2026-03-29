#include "planner.h"

#include <ArduinoJson.h>
#include "llm/deepseek_client.h"
#include "device/device_registry.h"
#include "device/device.h"
#include "engine/action_queue.h"

namespace {

String toLowerCopy(String s) {
    s.toLowerCase();
    return s;
}

bool startsWithAny(const String& text, const char* const* prefixes, int n) {
    for (int i = 0; i < n; i++) {
        if (text.startsWith(prefixes[i])) return true;
    }
    return false;
}

bool containsAny(const String& text, const char* const* patterns, int n) {
    for (int i = 0; i < n; i++) {
        if (text.indexOf(patterns[i]) >= 0) return true;
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
    return text;
}

String cleanChatReply(String text) {
    text = cleanReply(text);
    if (text.length() > 280) text = text.substring(0, 280) + "...";
    return text;
}

bool looksLikeLlmError(const String& text) {
    return text.startsWith("HTTP Error:") ||
           text.startsWith("Connection") ||
           text.startsWith("API request failed") ||
           text.startsWith("Parse error:") ||
           text.startsWith("API Key not configured");
}

bool parseColor(const JsonVariantConst& node, uint8_t& r, uint8_t& g, uint8_t& b, String& error) {
    if (node.is<JsonArrayConst>()) {
        JsonArrayConst arr = node.as<JsonArrayConst>();
        if (arr.size() != 3) { error = "color array must have 3 elements"; return false; }
        int rv = arr[0] | -1, gv = arr[1] | -1, bv = arr[2] | -1;
        if (rv < 0 || rv > 255 || gv < 0 || gv > 255 || bv < 0 || bv > 255) {
            error = "color values must be 0..255"; return false;
        }
        r = rv; g = gv; b = bv;
        return true;
    }
    if (node.is<JsonObjectConst>()) {
        JsonObjectConst obj = node.as<JsonObjectConst>();
        int rv = obj["r"] | -1, gv = obj["g"] | -1, bv = obj["b"] | -1;
        if (rv < 0 || rv > 255 || gv < 0 || gv > 255 || bv < 0 || bv > 255) {
            error = "color.r/g/b must be 0..255"; return false;
        }
        r = rv; g = gv; b = bv;
        return true;
    }
    error = "color must be array [r,g,b] or object {r,g,b}";
    return false;
}

bool toIntInRange(const JsonVariantConst& node, int minV, int maxV, int& out) {
    if (node.isNull()) return false;
    int v = node.as<int>();
    if (v < minV || v > maxV) return false;
    out = v;
    return true;
}

bool parseValue01(const JsonVariantConst& node, uint8_t& val, String& error) {
    if (node.is<bool>()) { val = node.as<bool>() ? 1 : 0; return true; }
    int v = 0;
    if (!toIntInRange(node, 0, 1, v)) { error = "value must be 0/1 or boolean"; return false; }
    val = (uint8_t)v;
    return true;
}

bool parseActionObject(const JsonVariantConst& obj, ActionItem& item, String& error) {
    if (!obj.is<JsonObjectConst>()) { error = "Each action must be an object"; return false; }
    JsonObjectConst o = obj.as<JsonObjectConst>();
    const char* actionName = o["action"] | "";
    if (strlen(actionName) == 0) { error = "Action missing 'action' field"; return false; }

    memset(&item, 0, sizeof(item));
    strncpy(item.actionName, actionName, sizeof(item.actionName) - 1);

    if (strcmp(actionName, "set_rgb") == 0) {
        return parseColor(o["color"], item.r, item.g, item.b, error);
    }
    if (strcmp(actionName, "blink_rgb") == 0) {
        if (!parseColor(o["color"], item.r, item.g, item.b, error)) return false;
        int times = 0;
        if (!toIntInRange(o["times"], 1, 20, times)) { error = "blink_rgb.times must be 1..20"; return false; }
        int dur = 0;
        if (!toIntInRange(o["duration_ms"], 100, 60000, dur)) { error = "blink_rgb.duration_ms must be 100..60000"; return false; }
        item.times = (uint8_t)times;
        item.durationMs = (uint16_t)dur;
        return true;
    }
    if (strcmp(actionName, "set_gpio") == 0) {
        int pin = 0;
        if (!toIntInRange(o["pin"], 0, 48, pin)) { error = "set_gpio.pin must be 0..48"; return false; }
        item.pin = (int8_t)pin;
        if (!parseValue01(o["value"], item.value, error)) return false;
        return true;
    }
    if (strcmp(actionName, "wait") == 0) {
        int dur = 0;
        if (!toIntInRange(o["duration_ms"], 1, 60000, dur)) { error = "wait.duration_ms must be 1..60000"; return false; }
        item.durationMs = (uint16_t)dur;
        return true;
    }
    error = "Unsupported action: " + String(actionName);
    return false;
}

} // namespace

Planner::Planner(DeepSeekClient& llm, DeviceRegistry& registry, ActionQueue& queue)
    : llm(llm), registry(registry), queue(queue) {}

String Planner::plan(const String& userText) {
    String input = userText;
    input.trim();

    if (input.length() == 0) {
        return "我在呢，你可以直接说想做什么。";
    }

    // Local chitchat
    bool localHandled = false;
    String localReply = tryLocalChitchat(input, localHandled);
    if (localHandled) return localReply;

    // Cooldown
    unsigned long now = millis();
    if (now - lastAiCall < AI_COOLDOWN_MS) {
        unsigned long remaining = (AI_COOLDOWN_MS - (now - lastAiCall)) / 1000 + 1;
        return "我还在整理上一条，等我 " + String(remaining) + "s 再来一条。";
    }
    lastAiCall = now;

    // Chat-only (no hardware intent)
    if (!likelyHardwareIntent(input)) {
        String chatPrompt;
        chatPrompt += "You are cy_robot on an ESP32 device.\\n";
        chatPrompt += "Reply warmly, concise, in the same language as the user.\\n";
        chatPrompt += "Do not output action tags or JSON.\\n";
        chatPrompt += "User:\\n";
        chatPrompt += input;

        String chatReply = cleanChatReply(llm.chat(chatPrompt));
        if (chatReply.length() == 0 || looksLikeLlmError(chatReply)) {
            return "我听到了，网络有点慢，我们可以先聊，也可以直接发控制命令。";
        }
        return chatReply;
    }

    // Hardware planner
    String plannerPrompt = buildPlannerPrompt(input);
    String aiRaw = cleanReply(llm.chat(plannerPrompt, 0.15, true));
    Serial.println("[Planner] raw response: " + aiRaw);
    if (aiRaw.length() == 0 || looksLikeLlmError(aiRaw)) {
        return "命令我收到了，但云端暂时没回我。你可以重试一次，或直接发 JSON 指令。";
    }

    String jsonPayload = extractJsonPayload(aiRaw);
    if (jsonPayload.length() == 0) {
        Serial.println("[Planner] no JSON found in response");
        return aiRaw;
    }
    Serial.println("[Planner] extracted JSON: " + jsonPayload);

    JsonDocument doc;
    DeserializationError parseError = deserializeJson(doc, jsonPayload);
    if (parseError) {
        Serial.println("[Planner] JSON parse error: " + String(parseError.c_str()));
        return aiRaw;
    }

    String reply = "收到，我来处理。";
    if (doc["reply"].is<const char*>()) {
        String parsedReply = cleanReply(doc["reply"].as<String>());
        if (parsedReply.length() > 0) reply = parsedReply;
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

    if (!hasExecutable) return reply;

    String enqueueResult;
    int added = 0;
    if (!enqueueFromJson(jsonPayload, enqueueResult, added)) {
        Serial.println("[Planner] enqueue failed: " + enqueueResult);
        return "Plan rejected: " + enqueueResult;
    }

    if (added <= 0) return reply;
    return reply + " | queued " + String(added) + " action(s). queue=" + String(queue.count());
}

String Planner::buildPlannerPrompt(const String& userInput) const {
    String p;
    p += "You are an ESP32 hardware controller. Output ONLY valid JSON, no text.\n";
    p += "Schema: {\"reply\":\"short message in user language\",\"sequence\":[actions]}\n";
    registry.buildPromptFragment(p);
    p += "Rules:\n";
    p += "- duration_ms is TOTAL blink time, not per-blink\n";
    p += "- To turn off LED: set_rgb with [0,0,0]\n";
    p += "- If no hardware action needed: sequence=[]\n";
    p += "Examples:\n";
    p += "User: 红灯闪3下\n";
    p += "{\"reply\":\"好的，红灯闪3下\",\"sequence\":[{\"action\":\"blink_rgb\",\"color\":[255,0,0],\"times\":3,\"duration_ms\":1500}]}\n";
    p += "User: turn on blue light\n";
    p += "{\"reply\":\"Blue light on\",\"sequence\":[{\"action\":\"set_rgb\",\"color\":[0,0,255]}]}\n";
    p += "User: 先亮绿灯2秒再亮红灯\n";
    p += "{\"reply\":\"好的\",\"sequence\":[{\"action\":\"set_rgb\",\"color\":[0,255,0]},{\"action\":\"wait\",\"duration_ms\":2000},{\"action\":\"set_rgb\",\"color\":[255,0,0]}]}\n";
    p += "User: 关灯\n";
    p += "{\"reply\":\"已关灯\",\"sequence\":[{\"action\":\"set_rgb\",\"color\":[0,0,0]}]}\n";
    p += "User request:\n";
    p += userInput;
    return p;
}

bool Planner::likelyHardwareIntent(const String& text) const {
    String lower = toLowerCopy(text);

    static const char* const enKeywords[] = {
        "led", "light", "blink", "flash", "rgb", "gpio", "pin",
        "turn on", "turn off", "set", "color", "bright", "dim",
        "red", "green", "blue", "white", "yellow", "purple", "orange", "pink", "cyan",
        "on", "off", "glow", "fade", "stop"
    };
    static const char* const zhKeywords[] = {
        "灯", "闪", "颜色", "红", "绿", "蓝", "亮", "灭", "引脚", "脚", "开", "关", "秒", "毫秒",
        "白", "黄", "紫", "橙", "粉", "青", "停", "暗", "明",
        "呼吸", "变色", "彩虹", "光", "熄", "点亮", "照", "色"
    };

    return containsAny(lower, enKeywords, sizeof(enKeywords) / sizeof(enKeywords[0])) ||
           containsAny(text, zhKeywords, sizeof(zhKeywords) / sizeof(zhKeywords[0]));
}

String Planner::tryLocalChitchat(const String& text, bool& handled) const {
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

String Planner::extractJsonPayload(const String& raw) const {
    String s = raw;
    s.trim();

    if (s.startsWith("{") || s.startsWith("[")) return s;

    int start = -1;
    char openChar = 0, closeChar = 0;
    for (int i = 0; i < (int)s.length(); i++) {
        if (s[i] == '{' || s[i] == '[') {
            start = i;
            openChar = s[i];
            closeChar = (s[i] == '{') ? '}' : ']';
            break;
        }
    }
    if (start < 0) return "";

    int depth = 0;
    bool inString = false, escape = false;
    for (int i = start; i < (int)s.length(); i++) {
        char c = s[i];
        if (escape) { escape = false; continue; }
        if (c == '\\' && inString) { escape = true; continue; }
        if (c == '"') { inString = !inString; continue; }
        if (inString) continue;
        if (c == openChar) depth++;
        else if (c == closeChar) {
            depth--;
            if (depth == 0) return s.substring(start, i + 1);
        }
    }
    return "";
}

bool Planner::enqueueFromJson(const String& jsonPayload, String& error, int& addedCount) {
    addedCount = 0;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonPayload);
    if (err) { error = "JSON parse error: " + String(err.c_str()); return false; }

    // Transactional staging
    ActionItem staged[ActionQueue::MAX_QUEUE];
    int stagedCount = 0;

    auto stageOne = [&](const JsonVariantConst& v) -> bool {
        if (stagedCount >= ActionQueue::MAX_QUEUE) { error = "Too many actions"; return false; }
        ActionItem item;
        if (!parseActionObject(v, item, error)) return false;
        staged[stagedCount++] = item;
        return true;
    };

    if (doc.is<JsonArrayConst>()) {
        for (JsonVariantConst v : doc.as<JsonArrayConst>()) {
            if (!stageOne(v)) return false;
        }
    } else if (doc.is<JsonObjectConst>()) {
        JsonObjectConst root = doc.as<JsonObjectConst>();
        if (root["sequence"].is<JsonArrayConst>()) {
            for (JsonVariantConst v : root["sequence"].as<JsonArrayConst>()) {
                if (!stageOne(v)) return false;
            }
        } else if (root["action"].is<const char*>()) {
            if (!stageOne(root)) return false;
        } else {
            error = "No executable action found";
            return false;
        }
    } else {
        error = "JSON root must be object or array";
        return false;
    }

    // Commit
    for (int i = 0; i < stagedCount; i++) {
        if (!queue.send(staged[i])) { error = "Action queue full"; return false; }
    }
    addedCount = stagedCount;
    error = "ok";
    return true;
}
