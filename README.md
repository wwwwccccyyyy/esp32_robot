# cy_robot (ESP32-S3 QQ Bot + DeepSeek)

This project is a derivative work based on an open-source ESP32 bot project, then customized and extended for my own use cases.

Main changes in this repo:
- Migrated bot channel to QQ Bot events.
- Switched LLM integration to DeepSeek API.
- Added GPIO and RGB action-tag execution.
- Kept PlatformIO + ESP32-S3 deployment workflow.

If needed, please add the original upstream repository URL here:
- Original project: `<ORIGINAL_REPO_URL>`

## Features

- QQ bot message handling (group + C2C).
- DeepSeek chat completion integration.
- Action tags from AI response:
  - `[RGB:r,g,b]`
  - `[GPIO:pin,value]`
- Wi-Fi connect with AP fallback setup portal.
- ESP32-S3 build via PlatformIO.

## Hardware

- Board: ESP32-S3 (configured for N16R8 in `platformio.ini`).
- Wi-Fi: 2.4 GHz.
- USB: Serial upload/monitor.

## Quick Start

1. Install PlatformIO (VS Code extension or CLI).
2. Copy `include/secrets.h.example` to `include/secrets.h`.
3. Fill in credentials in `include/secrets.h`:

```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define QQ_APP_ID "YOUR_QQ_APP_ID"
#define QQ_APP_SECRET "YOUR_QQ_APP_SECRET"
#define DEEPSEEK_API_KEY "YOUR_DEEPSEEK_API_KEY"
```

4. Build and upload:

```bash
pio run --target upload
pio device monitor
```

## Commands

- `/start` - Show help.
- `/status` - Show device status.
- Normal text - Forward to DeepSeek and return response.

## Project Layout

```text
esp32_robot/
  include/
    config.h
    pins.h
    secrets.h.example
  src/
    main.cpp
    bot/
      command_handler.*
      qq_bot.*
    llm/
      deepseek_client.*
    net/
      wifi_manager.*
      web_portal.*
    hw/
      gpio_manager.*
      rgb_led.*
  platformio.ini
  README.md
```

## Security Notes

- Never commit `include/secrets.h`.
- Rotate keys immediately if they are ever exposed.
- `QQ_API_BASE` is currently sandbox by default; switch to production base URL when publishing.

## Acknowledgements

- Original upstream open-source project (to be linked).
- [ArduinoJson](https://arduinojson.org/)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [WebSockets](https://github.com/Links2004/arduinoWebSockets)
