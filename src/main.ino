#include <Arduino.h>
#include "NetworkManager.h"
#include "defaults.h"

#define STATUS_PIN LED_BUILTIN

uint32_t getChipId() {
  uint64_t macAddress = ESP.getEfuseMac();
  uint32_t id         = 0;

  for(int i = 0; i < 17; i = i + 8) {
    id |= ((macAddress >> (40 - i)) & 0xff) << i;
  }
  return id;
}

NetworkManager nwManager(String("ancs2mqtt-") + String(getChipId(), HEX), WIFI_INITIAL_AP_PASSWORD, STATUS_PIN);

void setup() {
  Serial.begin(115200);
  Serial.println("ancs2mqtt starting up...");
  nwManager.start();
}

void loop() {
  nwManager.loop();
}
