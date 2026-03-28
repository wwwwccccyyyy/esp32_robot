#ifndef DEEPSEEK_CLIENT_H
#define DEEPSEEK_CLIENT_H

#include <Arduino.h>

class DeepSeekClient {
public:
    void begin();
    void setSystemPrompt(const String& prompt);
    String chat(const String& message);
    bool isReady();

private:
    String apiKey;
    String systemPrompt;
    int maxRetries = 2;

    String doRequest(const String& message);
};

#endif // DEEPSEEK_CLIENT_H
