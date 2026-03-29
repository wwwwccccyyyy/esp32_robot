/*
 * cy_robot ESP32 - Main
 * QQ Bot + DeepSeek LLM + GPIO Control
 * FreeRTOS dual-core: Core 0 = network/LLM, Core 1 = hardware
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

#include "engine/action_queue.h"
#include "device/device.h"
#include "device/device_registry.h"
#include "device/rgb_led_device.h"
#include "device/gpio_device.h"

#include "bot/planner.h"
#include "bot/command_router.h"
#include "bot/qq_bot.h"

// --- Module instances ---
WiFiManager wifiManager;
WebPortal webPortal;
DeepSeekClient deepseek;
GpioManager gpioManager;
RgbLed rgbLed;

// Device layer
RgbLedDevice rgbLedDevice(rgbLed);
GpioDevice gpioDevice(gpioManager);
DeviceRegistry registry;
ActionQueue actionQueue;

// Bot layer
Planner planner(deepseek, registry, actionQueue);
CommandRouter cmdRouter(planner, actionQueue, registry);
QQBot qqBot(cmdRouter);

// --- FreeRTOS tasks ---

// Core 0: QQBot WebSocket loop (includes LLM calls via Planner)
void networkTask(void* param) {
    for (;;) {
        if (!wifiManager.isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        qqBot.loop();
        vTaskDelay(pdMS_TO_TICKS(1));  // yield
    }
}

// Core 1: consume ActionQueue, tick all device loops
void hardwareTask(void* param) {
    for (;;) {
        // Tick all device state machines (blink animations etc.)
        for (int i = 0; i < registry.deviceCount(); i++) {
            registry.deviceAt(i)->loop();
        }

        ActionItem item;
        if (actionQueue.receive(item, pdMS_TO_TICKS(10))) {
            if (strcmp(item.actionName, "wait") == 0) {
                vTaskDelay(pdMS_TO_TICKS(item.durationMs));
            } else {
                Device* dev = registry.findByAction(item.actionName);
                if (dev) {
                    String err;
                    if (!dev->execute(item, err)) {
                        Serial.println("[HW] execute error: " + err);
                    }
                } else {
                    Serial.println("[HW] no device for action: " + String(item.actionName));
                }
            }
        }
    }
}

// --- Arduino setup/loop (runs on Core 1 by default) ---

unsigned long lastStatusReport = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n");
    Serial.println("=============================");
    Serial.println("  cy_robot ESP32-S3");
    Serial.println("  QQ Bot + DeepSeek Assistant");
    Serial.println("  FreeRTOS Dual-Core");
    Serial.println("=============================");
    Serial.println();

    // Initialize RGB LED
    rgbLed.begin();
    rgbLed.setColor(0, 0, 50); // Dim blue = booting

    // Initialize GPIO
    gpioManager.begin();

    // Register devices
    registry.add(&rgbLedDevice);
    registry.add(&gpioDevice);

    // Initialize action queue
    actionQueue.begin();

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

    // Launch FreeRTOS tasks
    xTaskCreatePinnedToCore(
        networkTask, "network",
        NETWORK_TASK_STACK, nullptr,
        NETWORK_TASK_PRIO, nullptr,
        NETWORK_TASK_CORE
    );

    xTaskCreatePinnedToCore(
        hardwareTask, "hardware",
        HARDWARE_TASK_STACK, nullptr,
        HARDWARE_TASK_PRIO, nullptr,
        HARDWARE_TASK_CORE
    );

    Serial.println("\nSetup complete! Tasks launched.\n");
}

void loop() {
    // WiFi reconnect
    if (!wifiManager.isConnected()) {
        wifiManager.reconnect();
        delay(100);
        return;
    }

    // Status report every minute
    unsigned long now = millis();
    if (now - lastStatusReport >= STATUS_REPORT_INTERVAL_MS) {
        Serial.println("Status - Heap: " + String(ESP.getFreeHeap() / 1024) +
                       "KB, RSSI: " + String(WiFi.RSSI()) + "dBm" +
                       ", Queue: " + String(actionQueue.count()));
        lastStatusReport = now;
    }

    delay(100); // Low priority idle
}
