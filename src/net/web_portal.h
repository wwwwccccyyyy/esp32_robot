#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <ESPAsyncWebServer.h>

class WebPortal {
public:
    void begin();

private:
    AsyncWebServer* server = nullptr;
};

#endif // WEB_PORTAL_H
