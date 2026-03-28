#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

class WiFiManager {
public:
    void begin();
    bool isConnected();
    bool isAPMode();
    void reconnect();
    String getIP();
    int getRSSI();

private:
    bool apMode = false;
    void setupAP();
};

#endif // WIFI_MANAGER_H
