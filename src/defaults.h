#pragma once

//#define IOTWEBCONF_DEBUG_TO_SERIAL

#define WIFI_INITIAL_AP_PASSWORD "ancs2mqtt"
#define MQTT_BUFFER_SIZE         1024

// LED pin to reflect current status
#define STATUS_PIN LED_BUILTIN

// Digital pin to pull down to start AP mode
// using the initial AP password (defined above)
#define CONFIG_PIN D2
