#ifndef QQ_BOT_H
#define QQ_BOT_H

#include <Arduino.h>
#include <WebSocketsClient.h>

class CommandRouter;

class QQBot {
public:
    explicit QQBot(CommandRouter& handler);
    void begin();
    void loop();
    bool isReady();

private:
    CommandRouter& cmdRouter;
    WebSocketsClient ws;

    String accessToken;
    unsigned long tokenExpiry = 0;
    String sessionId;
    int lastSeq = -1;
    unsigned long heartbeatInterval = 41250;
    unsigned long lastHeartbeat = 0;
    bool wsConnected = false;
    bool identified = false;

    bool refreshToken();
    String getGatewayUrl();
    void sendIdentify();
    void sendHeartbeat();
    void onWsEvent(WStype_t type, uint8_t* payload, size_t length);
    void handlePayload(const char* payload);
    void sendGroupMessage(const String& groupOpenId, const String& msgId, const String& content);
    void sendDirectMessage(const String& userOpenId, const String& msgId, const String& content);
};

#endif // QQ_BOT_H
