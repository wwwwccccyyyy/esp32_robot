/*
 * cy_robot ESP32 - Main
 * QQ Bot + DeepSeek LLM + GPIO Control
 *
 * Hardware: ESP32-S3 N16R8 (16MB Flash + 8MB PSRAM)
 */

#include <Arduino.h>
#include "secrets.h"
#include "config.h"
#include "pins.h"
#include "net/wifi_manager.h"
#include "net/web_portal.h"
#include "llm/deepseek_client.h"
#include "hw/gpio_manager.h"
#include "hw/rgb_led.h"
#include "engine/action_engine.h"
#include "bot/command_handler.h"
#include "bot/qq_bot.h"

// Module instances
WiFiManager wifiManager;
WebPortal webPortal;
DeepSeekClient deepseek;
GpioManager gpioManager;
RgbLed rgbLed;
ActionEngine actionEngine(rgbLed, gpioManager);
CommandHandler cmdHandler(deepseek, actionEngine);
QQBot qqBot(cmdHandler);

unsigned long lastStatusReport = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n");
    Serial.println("=============================");
    Serial.println("  cy_robot ESP32-S3");
    Serial.println("  QQ Bot + DeepSeek Assistant");
    Serial.println("=============================");
    Serial.println();

    // Initialize RGB LED
    rgbLed.begin();
    rgbLed.setColor(0, 0, 50); // Dim blue = booting

    // Initialize GPIO
    gpioManager.begin();

    // Initialize DeepSeek Client
    Serial.println("Initializing DeepSeek Client...");
    deepseek.begin();
    deepseek.setSystemPrompt(
        "You are cy_robot, a warm and helpful assistant on an ESP32 device. "
        "Be concise, natural, and friendly. "
        "When the developer prompt explicitly asks for strict JSON planning, "
        "you must output valid JSON only."
    );

    // Connect WiFi
    Serial.println("Connecting to WiFi...");
    rgbLed.setColor(50, 50, 0); // Yellow = connecting
    wifiManager.begin();

    if (wifiManager.isConnected()) {
        Serial.println("WiFi Connected!");
        Serial.print("IP: ");
        Serial.println(wifiManager.getIP());

        rgbLed.setColor(0, 50, 0); // Green = connected

        // Test DeepSeek
        Serial.println("Testing DeepSeek API...");
        if (deepseek.isReady()) {
            Serial.println("DeepSeek API Ready");
        } else {
            Serial.println("DeepSeek not configured - check secrets.h");
        }

        // Initialize QQ Bot
        Serial.println("Starting QQ Bot...");
        qqBot.begin();

        rgbLed.setColor(0, 0, 0); // Off = ready
    } else {
        rgbLed.setColor(50, 0, 0); // Red = AP mode
        Serial.println("AP Mode active - connect to 'cy_robot-Setup' to configure");
        webPortal.begin();
    }

    Serial.println("\nSetup complete!\n");
}

void loop() {
    // Run action queue and timed effects.
    actionEngine.loop();

    // Handle WiFi
    if (!wifiManager.isConnected()) {
        wifiManager.reconnect();
        delay(100);
        return;
    }

    // QQ Bot WebSocket loop (heartbeat + messages)
    qqBot.loop();

    // Tick action engine again in case network work took time.
    actionEngine.loop();

    // Status report every minute
    unsigned long now = millis();
    if (now - lastStatusReport >= STATUS_REPORT_INTERVAL_MS) {
        Serial.println("Status - Heap: " + String(ESP.getFreeHeap() / 1024) + "KB, RSSI: " + String(WiFi.RSSI()) + "dBm");
        lastStatusReport = now;
    }
}
