#ifndef CONFIG_H
#define CONFIG_H

// Device Info
#define DEVICE_NAME "cy_robot"
#define DEVICE_VERSION "1.0.0"

// WiFi credentials are in secrets.h

#define WIFI_TIMEOUT_MS 20000
#define WIFI_RECONNECT_INTERVAL_MS 5000

// QQ Bot credentials are in secrets.h
// Sandbox for dev/testing; switch to "https://api.sgroup.qq.com" after publishing
#define QQ_API_BASE "https://sandbox.api.sgroup.qq.com"
#define QQ_INTENTS 33554432  // GROUP_AND_C2C_EVENT (1 << 25)

// DeepSeek LLM (API key in secrets.h)
#define DEEPSEEK_API_ENDPOINT "https://api.deepseek.com/chat/completions"
#define DEEPSEEK_MODEL "deepseek-chat"
#define DEEPSEEK_MAX_TOKENS 800
#define DEEPSEEK_TIMEOUT_MS 15000

// GPIO (pin definitions in pins.h)
#define GPIO_POLL_INTERVAL_MS 100

// System
#define STATUS_REPORT_INTERVAL_MS 60000  // Report every 1 minute
#define WATCHDOG_TIMEOUT_MS 30000

// FreeRTOS task configuration
#define NETWORK_TASK_STACK   8192   // bytes
#define NETWORK_TASK_PRIO    2
#define NETWORK_TASK_CORE    0

#define HARDWARE_TASK_STACK  4096   // bytes
#define HARDWARE_TASK_PRIO   3
#define HARDWARE_TASK_CORE   1

#endif // CONFIG_H
