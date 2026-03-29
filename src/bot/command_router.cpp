#include "command_router.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "config.h"
#include "bot/planner.h"
#include "engine/action_queue.h"
#include "device/device_registry.h"
#include "device/device.h"

namespace {

bool parseActionObject(const JsonVariantConst& obj, ActionItem& item, String& error);

bool parseColor(const JsonVariantConst& node, uint8_t& r, uint8_t& g, uint8_t& b, String& error) {
    if (node.is<JsonArrayConst>()) {
        JsonArrayConst arr = node.as<JsonArrayConst>();
        if (arr.size() != 3) { error = "color array must have 3 elements"; return false; }
        int rv = arr[0] | -1, gv = arr[1] | -1, bv = arr[2] | -1;
        if (rv < 0 || rv > 255 || gv < 0 || gv > 255 || bv < 0 || bv > 255) {
            error = "color values must be 0..255"; return false;
        }
        r = rv; g = gv; b = bv; return true;
    }
    if (node.is<JsonObjectConst>()) {
        JsonObjectConst o = node.as<JsonObjectConst>();
        int rv = o["r"] | -1, gv = o["g"] | -1, bv = o["b"] | -1;
        if (rv < 0 || rv > 255 || gv < 0 || gv > 255 || bv < 0 || bv > 255) {
            error = "color.r/g/b must be 0..255"; return false;
        }
        r = rv; g = gv; b = bv; return true;
    }
    error = "color must be array or object"; return false;
}

bool toIntInRange(const JsonVariantConst& node, int lo, int hi, int& out) {
    if (node.isNull()) return false;
    int v = node.as<int>();
    if (v < lo || v > hi) return false;
    out = v; return true;
}

bool parseValue01(const JsonVariantConst& node, uint8_t& val, String& error) {
    if (node.is<bool>()) { val = node.as<bool>() ? 1 : 0; return true; }
    int v = 0;
    if (!toIntInRange(node, 0, 1, v)) { error = "value must be 0/1 or boolean"; return false; }
    val = v; return true;
}

bool parseActionObject(const JsonVariantConst& obj, ActionItem& item, String& error) {
    if (!obj.is<JsonObjectConst>()) { error = "Each action must be an object"; return false; }
    JsonObjectConst o = obj.as<JsonObjectConst>();
    const char* name = o["action"] | "";
    if (strlen(name) == 0) { error = "Missing 'action' field"; return false; }

    memset(&item, 0, sizeof(item));
    strncpy(item.actionName, name, sizeof(item.actionName) - 1);

    if (strcmp(name, "set_rgb") == 0) return parseColor(o["color"], item.r, item.g, item.b, error);
    if (strcmp(name, "blink_rgb") == 0) {
        if (!parseColor(o["color"], item.r, item.g, item.b, error)) return false;
        int t = 0, d = 0;
        if (!toIntInRange(o["times"], 1, 20, t)) { error = "times must be 1..20"; return false; }
        if (!toIntInRange(o["duration_ms"], 100, 60000, d)) { error = "duration_ms must be 100..60000"; return false; }
        item.times = t; item.durationMs = d; return true;
    }
    if (strcmp(name, "set_gpio") == 0) {
        int p = 0;
        if (!toIntInRange(o["pin"], 0, 48, p)) { error = "pin must be 0..48"; return false; }
        item.pin = p;
        return parseValue01(o["value"], item.value, error);
    }
    if (strcmp(name, "wait") == 0) {
        int d = 0;
        if (!toIntInRange(o["duration_ms"], 1, 60000, d)) { error = "duration_ms must be 1..60000"; return false; }
        item.durationMs = d; return true;
    }
    error = "Unsupported action: " + String(name); return false;
}

bool enqueueJson(const String& payload, ActionQueue& queue, String& error, int& addedCount) {
    addedCount = 0;
    JsonDocument doc;
    if (deserializeJson(doc, payload)) { error = "JSON parse error"; return false; }

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
        for (JsonVariantConst v : doc.as<JsonArrayConst>())
            if (!stageOne(v)) return false;
    } else if (doc.is<JsonObjectConst>()) {
        JsonObjectConst root = doc.as<JsonObjectConst>();
        if (root["sequence"].is<JsonArrayConst>()) {
            for (JsonVariantConst v : root["sequence"].as<JsonArrayConst>())
                if (!stageOne(v)) return false;
        } else if (root["action"].is<const char*>()) {
            if (!stageOne(root)) return false;
        } else { error = "No executable action found"; return false; }
    } else { error = "JSON root must be object or array"; return false; }

    for (int i = 0; i < stagedCount; i++) {
        if (!queue.send(staged[i])) { error = "Queue full"; return false; }
    }
    addedCount = stagedCount;
    return true;
}

} // namespace

CommandRouter::CommandRouter(Planner& planner, ActionQueue& queue, DeviceRegistry& registry)
    : planner(planner), queue(queue), registry(registry) {}

String CommandRouter::handle(const String& text) {
    String input = text;
    input.trim();

    if (input.length() == 0) return "我在呢，你可以直接说想做什么。";

    // Slash commands
    if (input.startsWith("/")) return handleSlashCommand(input);

    // Direct JSON
    if (input.startsWith("{") || input.startsWith("[")) {
        String response;
        if (handleDirectJson(input, response)) return response;
        return "JSON rejected: " + response;
    }

    // Delegate to planner
    return planner.plan(input);
}

bool CommandRouter::handleDirectJson(const String& text, String& response) {
    int added = 0;
    String error;
    if (enqueueJson(text, queue, error, added)) {
        if (added <= 0) { response = "JSON accepted, no actions scheduled."; }
        else { response = "Queued " + String(added) + " action(s). queue=" + String(queue.count()); }
        return true;
    }
    response = error;
    return false;
}

String CommandRouter::handleSlashCommand(const String& text) {
    if (text == "/start" || text == "/help") return buildHelpMessage();
    if (text == "/status") return buildStatusMessage();
    if (text == "/queue") return "Queue: " + String(queue.count()) + " pending";
    if (text == "/clear") {
        queue.clear();
        return "Queue cleared.";
    }
    return "Unknown command. Use /help.";
}

String CommandRouter::buildHelpMessage() {
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

String CommandRouter::buildStatusMessage() {
    String msg = DEVICE_NAME " Status\\n\\n";
    msg += "Uptime: " + String(millis() / 1000) + "s\\n";
    msg += "Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB\\n";
    msg += "WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\\n";
    msg += "IP: " + WiFi.localIP().toString() + "\\n";
    msg += "Version: " + String(DEVICE_VERSION) + "\\n";
    msg += "Queue: " + String(queue.count()) + " pending";
    return msg;
}
