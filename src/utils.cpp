#include <Arduino.h>
#include <stdint.h>

String getChipId() {
  uint64_t macAddress = ESP.getEfuseMac();
  uint32_t id         = 0;

  for (int i = 0; i < 17; i = i + 8) {
    id |= ((macAddress >> (40 - i)) & 0xff) << i;
  }
  return String(id, HEX);
}
