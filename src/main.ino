#include <Arduino.h>
#include <esp32notifications.h>
#include <functional>
#include "defaults.h"
#include "utils.h"
#include "NetworkManager.h"

String deviceName = String("ancs2mqtt-") + getChipId();

NetworkManager nwManager(deviceName.c_str(), WIFI_INITIAL_AP_PASSWORD, STATUS_PIN, CONFIG_PIN);

BLENotifications notifications;

void onBLEStateChanged(BLENotifications::State state) {
  switch(state) {
    case BLENotifications::StateConnected:
      Serial.println("ancs2mqtt: stateConnected - connected to a phone or tablet");
      break;

    case BLENotifications::StateDisconnected:
      Serial.println("ancs2mqtt: stateDisconnected - disconnected from a phone or tablet");
      notifications.startAdvertising();
      break;
  }
}

void onNotificationArrived(const ArduinoNotification* notification, const Notification* rawNotificationData) {
  DynamicJsonDocument doc(MQTT_BUFFER_SIZE);

  doc["title"] = notification->title;
  doc["msg"]   = notification->message;
  doc["cat"]   = notifications.getNotificationCategoryDescription(notification->category);

  String payload;
  serializeJson(doc, payload);

  Serial.print("ancs2mqtt: received notification ");
  Serial.println(payload);

  nwManager.publish(notification->type, payload);
}

// A notification was cleared
void onNotificationRemoved(const ArduinoNotification* notification, const Notification* rawNotificationData) {
  Serial.print("ancs2mqtt: removed notification ");
  Serial.println(notification->title);
  Serial.println(notification->message);
  Serial.println(notification->type);
}

void setup() {
  Serial.begin(115200);
  Serial.println("ancs2mqtt: starting up...");

  // Start network manager.
  nwManager.start([]() -> void {
    Serial.println("ancs2mqtt: initializing BLE");
    // Set up the BLENotification library once the network manager has started.
    notifications.begin(nwManager.getName());
    notifications.setConnectionStateChangedCallback(onBLEStateChanged);
    notifications.setNotificationCallback(onNotificationArrived);
    notifications.setRemovedCallback(onNotificationRemoved);
  });
}

void loop() {
  nwManager.loop();
}
