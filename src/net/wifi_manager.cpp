#include "wifi_manager.h"
#include "secrets.h"
#include "config.h"
#include <WiFi.h>

void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi [");
    Serial.print(WIFI_SSID);
    Serial.print("]");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
        if (attempts % 10 == 0) {
            Serial.println("\nRetrying...");
            WiFi.disconnect();
            delay(100);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        apMode = false;
    } else {
        Serial.print("\nWiFi Failed! Status: ");
        Serial.println(WiFi.status());
        Serial.println("Starting AP Mode...");
        setupAP();
    }
}

void WiFiManager::setupAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("cy_robot-Setup", "cyrobot123");
    apMode = true;

    Serial.println("AP Mode Started");
    Serial.print("SSID: cy_robot-Setup\nPassword: cyrobot123\n");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isAPMode() {
    return apMode;
}

void WiFiManager::reconnect() {
    if (!isConnected()) {
        WiFi.reconnect();
    }
}

String WiFiManager::getIP() {
    if (apMode) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

int WiFiManager::getRSSI() {
    return WiFi.RSSI();
}
