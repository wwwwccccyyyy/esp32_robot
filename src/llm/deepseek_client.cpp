#include "deepseek_client.h"
#include "secrets.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void DeepSeekClient::begin() {
    apiKey = DEEPSEEK_API_KEY;
    Serial.println("DeepSeek Client initialized");
}

bool DeepSeekClient::isReady() {
    return apiKey.length() > 10;
}

void DeepSeekClient::setSystemPrompt(const String& prompt) {
    systemPrompt = prompt;
}

String DeepSeekClient::chat(const String& message) {
    if (!isReady()) {
        return "API Key not configured";
    }

    for (int attempt = 0; attempt <= maxRetries; attempt++) {
        if (attempt > 0) {
            Serial.println("Retry " + String(attempt) + "/" + String(maxRetries));
            delay(1000 * attempt); // Simple backoff: 1s, 2s
        }

        String result = doRequest(message);
        if (!result.startsWith("HTTP Error:") && !result.startsWith("Connection")) {
            return result;
        }

        Serial.println("Attempt " + String(attempt + 1) + " failed: " + result);
    }

    return "API request failed after retries";
}

String DeepSeekClient::doRequest(const String& message) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(DEEPSEEK_TIMEOUT_MS / 1000);

    HTTPClient http;
    http.useHTTP10(true);
    http.begin(client, DEEPSEEK_API_ENDPOINT);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + apiKey);
    http.setTimeout(DEEPSEEK_TIMEOUT_MS);

    // Build JSON request (OpenAI-compatible format)
    JsonDocument doc;
    doc["model"] = DEEPSEEK_MODEL;
    doc["max_tokens"] = DEEPSEEK_MAX_TOKENS;
    doc["stream"] = false;
    doc["temperature"] = 0.7;

    JsonArray messages = doc["messages"].to<JsonArray>();

    if (systemPrompt.length() > 0) {
        JsonObject sysMsg = messages.add<JsonObject>();
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
    }

    JsonObject userMsg = messages.add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["content"] = message;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.println("Sending to DeepSeek: " + message);
    Serial.println("Free heap: " + String(ESP.getFreeHeap()));
    int httpCode = http.POST(requestBody);
    requestBody = ""; // Free memory immediately

    String response = "";
    if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("Response length: " + String(payload.length()));

        JsonDocument filter;
        filter["choices"][0]["message"]["content"] = true;

        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, payload, DeserializationOption::Filter(filter));
        payload = ""; // Free memory

        if (!error) {
            const char* content = respDoc["choices"][0]["message"]["content"];
            if (content) {
                response = String(content);
                response.trim();
                if (response.length() > 800) {
                    response = response.substring(0, 800) + "...";
                }
            } else {
                response = "Empty response from AI";
            }
        } else {
            response = "Parse error: " + String(error.c_str());
            Serial.println("JSON parse error: " + String(error.c_str()));
        }
    } else if (httpCode < 0) {
        response = "Connection failed: " + String(httpCode);
        Serial.println("Connection failed: " + String(httpCode));
    } else {
        response = "HTTP Error: " + String(httpCode);
        Serial.println("HTTP Error: " + String(httpCode));
        Serial.println("Response: " + http.getString().substring(0, 200));
    }

    http.end();
    return response.length() > 0 ? response : "No response";
}
