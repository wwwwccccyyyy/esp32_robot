#include "web_portal.h"
#include <AsyncTCP.h>
#include <Preferences.h>

void WebPortal::begin() {
    if (server) return;

    server = new AsyncWebServer(80);

    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>cy_robot Setup</title>";
        html += "<style>body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff}";
        html += ".container{max-width:400px;margin:0 auto;background:#2a2a2a;padding:20px;border-radius:10px}";
        html += "input{width:100%;padding:10px;margin:5px 0;border-radius:5px;border:none;box-sizing:border-box}";
        html += "button{width:100%;padding:12px;background:#00d26a;color:#000;border:none;border-radius:5px;cursor:pointer;font-weight:bold}";
        html += "h2{color:#00d26a}</style></head><body>";
        html += "<div class='container'><h2>cy_robot Setup</h2>";
        html += "<form action='/save' method='POST'>";
        html += "<label>WiFi SSID:</label><input type='text' name='ssid' required>";
        html += "<label>WiFi Password:</label><input type='password' name='pass' required>";
        html += "<label>QQ App ID:</label><input type='text' name='qq_appid' required>";
        html += "<label>QQ App Secret:</label><input type='password' name='qq_secret' required>";
        html += "<label>DeepSeek API Key:</label><input type='text' name='apikey' required>";
        html += "<button type='submit'>Save & Restart</button></form></div></body></html>";
        request->send(200, "text/html", html);
    });

    server->on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
        Preferences prefs;
        prefs.begin("miniclaw", false);

        if (request->hasParam("ssid", true))
            prefs.putString("wifi_ssid", request->getParam("ssid", true)->value());
        if (request->hasParam("pass", true))
            prefs.putString("wifi_pass", request->getParam("pass", true)->value());
        if (request->hasParam("qq_appid", true))
            prefs.putString("qq_appid", request->getParam("qq_appid", true)->value());
        if (request->hasParam("qq_secret", true))
            prefs.putString("qq_secret", request->getParam("qq_secret", true)->value());
        if (request->hasParam("apikey", true))
            prefs.putString("ds_apikey", request->getParam("apikey", true)->value());

        prefs.end();

        request->send(200, "text/html", "<h1 style='color:#fff;background:#1a1a1a;padding:20px'>Saved! Restarting...</h1>");
        // Restart after response is sent
        delay(1000);
        ESP.restart();
    });

    server->begin();
    Serial.println("Web portal started on port 80");
}
