#include "qq_bot.h"
#include "command_router.h"
#include "config.h"
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

QQBot::QQBot(CommandRouter& handler) : cmdRouter(handler) {}

void QQBot::begin() {
    if (!refreshToken()) {
        Serial.println("Failed to get QQ access token");
        return;
    }

    String gatewayUrl = getGatewayUrl();
    if (gatewayUrl.length() == 0) {
        Serial.println("Failed to get QQ gateway URL");
        return;
    }

    // Parse wss://host/path
    String host = gatewayUrl;
    String path = "/";
    if (host.startsWith("wss://")) host = host.substring(6);
    int pathIdx = host.indexOf('/');
    if (pathIdx > 0) {
        path = host.substring(pathIdx);
        host = host.substring(0, pathIdx);
    }

    Serial.println("QQ WebSocket: " + host + path);
    ws.beginSSL(host.c_str(), 443, path.c_str());
    ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->onWsEvent(type, payload, length);
    });
    ws.setReconnectInterval(5000);
}

void QQBot::loop() {
    ws.loop();

    // Heartbeat
    if (identified && heartbeatInterval > 0) {
        unsigned long now = millis();
        if (now - lastHeartbeat >= heartbeatInterval) {
            sendHeartbeat();
            lastHeartbeat = now;
        }
    }

    // Refresh token 5 minutes before expiry (overflow-safe)
    if (tokenExpiry > 0 && (long)(millis() - (tokenExpiry - 300000)) >= 0) {
        refreshToken();
    }
}

bool QQBot::isReady() {
    return wsConnected && identified;
}

// -- Authentication --

bool QQBot::refreshToken() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, "https://bots.qq.com/app/getAppAccessToken");
    http.addHeader("Content-Type", "application/json");

    String body = "{\"appId\":\"" + String(QQ_APP_ID) +
                  "\",\"clientSecret\":\"" + String(QQ_APP_SECRET) + "\"}";
    int code = http.POST(body);

    if (code == 200) {
        JsonDocument doc;
        deserializeJson(doc, http.getString());
        accessToken = doc["access_token"].as<String>();
        int expiresIn = doc["expires_in"] | 7200;
        tokenExpiry = millis() + (unsigned long)expiresIn * 1000;
        Serial.println("QQ token OK, expires in " + String(expiresIn) + "s");
        http.end();
        return true;
    }

    Serial.println("QQ token failed: HTTP " + String(code));
    http.end();
    return false;
}

String QQBot::getGatewayUrl() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String(QQ_API_BASE) + "/gateway");
    http.addHeader("Authorization", "QQBot " + accessToken);
    http.addHeader("X-Union-Appid", QQ_APP_ID);

    int code = http.GET();
    String url = "";

    if (code == 200) {
        JsonDocument doc;
        deserializeJson(doc, http.getString());
        url = doc["url"].as<String>();
        Serial.println("Gateway: " + url);
    } else {
        Serial.println("Gateway failed: HTTP " + String(code));
    }

    http.end();
    return url;
}

// -- WebSocket --

void QQBot::sendIdentify() {
    JsonDocument doc;
    doc["op"] = 2;
    JsonObject d = doc["d"].to<JsonObject>();
    d["token"] = "QQBot " + accessToken;
    d["intents"] = QQ_INTENTS;
    JsonArray shard = d["shard"].to<JsonArray>();
    shard.add(0);
    shard.add(1);

    String payload;
    serializeJson(doc, payload);
    ws.sendTXT(payload);
    Serial.println("Sent Identify");
}

void QQBot::sendHeartbeat() {
    String payload;
    if (lastSeq >= 0) {
        payload = "{\"op\":1,\"d\":" + String(lastSeq) + "}";
    } else {
        payload = "{\"op\":1,\"d\":null}";
    }
    ws.sendTXT(payload);
}

void QQBot::onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("QQ WS disconnected");
            wsConnected = false;
            identified = false;
            break;
        case WStype_CONNECTED:
            Serial.println("QQ WS connected");
            wsConnected = true;
            break;
        case WStype_TEXT:
            handlePayload((const char*)payload);
            break;
        default:
            break;
    }
}

void QQBot::handlePayload(const char* payload) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        Serial.println("QQ JSON parse error");
        return;
    }

    int op = doc["op"] | -1;

    switch (op) {
        case 10: {
            // Hello -> send Identify
            heartbeatInterval = doc["d"]["heartbeat_interval"] | 41250;
            lastHeartbeat = millis();
            Serial.println("Heartbeat interval: " + String(heartbeatInterval) + "ms");
            sendIdentify();
            break;
        }
        case 11:
            // Heartbeat ACK
            break;
        case 0: {
            // Dispatch event
            int seq = doc["s"] | -1;
            if (seq >= 0) lastSeq = seq;

            const char* t = doc["t"];
            if (!t) break;
            String eventType = t;

            if (eventType == "READY") {
                sessionId = doc["d"]["session_id"].as<String>();
                identified = true;
                Serial.println("QQ Bot ready! Session: " + sessionId);
            }
            else if (eventType == "RESUMED") {
                identified = true;
                Serial.println("QQ Bot resumed");
            }
            else if (eventType == "GROUP_AT_MESSAGE_CREATE") {
                String groupOpenId = doc["d"]["group_openid"].as<String>();
                String msgId = doc["d"]["id"].as<String>();
                String content = doc["d"]["content"].as<String>();
                content.trim();
                Serial.println("Group: " + content);

                String response = cmdRouter.handle(content);
                sendGroupMessage(groupOpenId, msgId, response);
            }
            else if (eventType == "C2C_MESSAGE_CREATE") {
                String userOpenId = doc["d"]["author"]["user_openid"].as<String>();
                String msgId = doc["d"]["id"].as<String>();
                String content = doc["d"]["content"].as<String>();
                content.trim();
                Serial.println("DM: " + content);

                String response = cmdRouter.handle(content);
                sendDirectMessage(userOpenId, msgId, response);
            }
            break;
        }
        case 7:
            // Server requests reconnect
            Serial.println("QQ reconnect requested");
            ws.disconnect();
            break;
        case 9:
            // Invalid session -> re-identify
            Serial.println("QQ invalid session");
            identified = false;
            sendIdentify();
            break;
    }
}

// -- Send Messages --

void QQBot::sendGroupMessage(const String& groupOpenId, const String& msgId, const String& content) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String(QQ_API_BASE) + "/v2/groups/" + groupOpenId + "/messages");
    http.addHeader("Authorization", "QQBot " + accessToken);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["content"] = content;
    doc["msg_type"] = 0;
    doc["msg_id"] = msgId;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    if (code != 200 && code != 204) {
        Serial.println("Group send failed: " + String(code));
    }
    http.end();
}

void QQBot::sendDirectMessage(const String& userOpenId, const String& msgId, const String& content) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String(QQ_API_BASE) + "/v2/users/" + userOpenId + "/messages");
    http.addHeader("Authorization", "QQBot " + accessToken);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["content"] = content;
    doc["msg_type"] = 0;
    doc["msg_id"] = msgId;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    if (code != 200 && code != 204) {
        Serial.println("DM send failed: " + String(code));
    }
    http.end();
}
