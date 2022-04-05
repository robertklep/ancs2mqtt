#pragma once
#include <Arduino.h>
#include <MQTT.h>
#include <EEPROM.h>
#include <string>
#include <functional>
#include "defaults.h"
#include <ArduinoJson.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include "esp32notifications.h"

DNSServer   dnsServer;
WebServer   httpServer(80);
WiFiClient  wifiClient;
MQTTClient  mqttClient(MQTT_BUFFER_SIZE);

using CallbackFn = std::function<void()>;

class NetworkManager {
  bool needReset       = false;
  bool needMqttConnect = false;

  CallbackFn onStartedCallback;

  const char*  name;
  String       topicPrefix = "ancs2mqtt";

  uint8_t statusPin;
  uint8_t configPin;

  // Parameter configuration.
  //
  // NOTE: after making changes to parameters that may affect their storage
  //       layout in EEPROM (changing length, identifier, order, etc), you
  //       need to update the CONFIG_VERSION define so WiFiManager will spot
  //       the changes and reset the stored configuration.
  //
  //       Make sure CONFIG_VERSION is (at most) 4 bytes.

#define CONFIG_VERSION    "0100"
#define TEXT_PARAM_LENGTH 32

  char    iosDeviceIdentifierValue[TEXT_PARAM_LENGTH];
  char    mqttHostValue[TEXT_PARAM_LENGTH];
  char    mqttPortValue[6];
  char    mqttUserNameValue[TEXT_PARAM_LENGTH];
  char    mqttUserPasswordValue[TEXT_PARAM_LENGTH];
  char    resetConfigValue[9];

  IotWebConf*                 iotWebConf;
  IotWebConfParameterGroup    mqttGroup                = IotWebConfParameterGroup("mqtt", "MQTT configuration");
  IotWebConfTextParameter     iosDeviceIdentifierParam = IotWebConfTextParameter("iOS device identifier", "iosd",   iosDeviceIdentifierValue, TEXT_PARAM_LENGTH, nullptr, "Identifier for your iOS device");
  IotWebConfTextParameter     mqttHostParam            = IotWebConfTextParameter("MQTT host",             "mqth",   mqttHostValue,            TEXT_PARAM_LENGTH, nullptr, "MQTT hostname or IP address");
  IotWebConfTextParameter     mqttPortParam            = IotWebConfTextParameter("MQTT port",             "mqtp",   mqttPortValue,            6,                 "1883",  "MQTT port (typically 1883)");
  IotWebConfTextParameter     mqttUserNameParam        = IotWebConfTextParameter("MQTT user",             "mqtu",   mqttUserNameValue,        TEXT_PARAM_LENGTH, nullptr, "MQTT username (optional)");
  IotWebConfPasswordParameter mqttUserPasswordParam    = IotWebConfPasswordParameter("MQTT password",     "mqtP",   mqttUserPasswordValue,    TEXT_PARAM_LENGTH, nullptr, "MQTT password (optional)");
  IotWebConfParameterGroup    resetGroup               = IotWebConfParameterGroup("reset", "Reset configuration");
  IotWebConfCheckboxParameter resetConfigParam         = IotWebConfCheckboxParameter("Reset configuration", "rset", resetConfigValue,         9,                 false);

  void wifiConnected();
  void configSaved();
  bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
  void handleRoot();

  public:
    NetworkManager(const char* name, const char* initialApPassword, uint8_t statusPin = LED_BUILTIN, uint8_t configPin = D2) : name(name), statusPin(statusPin), configPin(configPin) {
      iotWebConf = new IotWebConf(name, &dnsServer, &httpServer, initialApPassword, CONFIG_VERSION);
    }

    void setTopicPrefix(String prefix) {
      topicPrefix = prefix;
    }

    void start(CallbackFn onStarted = nullptr) {
      onStartedCallback = onStarted;

      // Add parameters to group, and group to config.
      mqttGroup.addItem(&iosDeviceIdentifierParam);
      mqttGroup.addItem(&mqttHostParam);
      mqttGroup.addItem(&mqttPortParam);
      mqttGroup.addItem(&mqttUserNameParam);
      mqttGroup.addItem(&mqttUserPasswordParam);
      iotWebConf->addParameterGroup(&mqttGroup);

      resetGroup.addItem(&resetConfigParam);
      iotWebConf->addParameterGroup(&resetGroup);

      // Status indicator pin
      iotWebConf->setStatusPin(statusPin);
      // iotWebConf->setConfigPin(configPin);

      // Set callbacks
      using namespace std::placeholders;
      iotWebConf->setWifiConnectionCallback(std::bind(&NetworkManager::wifiConnected, this));
      iotWebConf->setConfigSavedCallback(std::bind(&NetworkManager::configSaved, this));
      iotWebConf->setFormValidator(std::bind(&NetworkManager::formValidator, this, _1));

      // Initialize the configuration.
      bool validConfig = iotWebConf->init();
      if (! validConfig) {
        /*
        mqttHostValue[0]            = '\0';
        mqttPortValue[0]            = '\0';
        mqttUserNameValue[0]        = '\0';
        mqttUserPasswordValue[0]    = '\0';
        iosDeviceIdentifierValue[0] = '\0';
        resetConfigValue[0]         = '\0';
        */
      } else {
        if (String(resetConfigValue).equals("selected")) {
          Serial.println("ancs2mqtt: clearing EEPROM and starting over.");
          clearEEPROM();
          publishForBridge("state", "Disconnected", true);
          ESP.restart();
          return;
        }
      }

      // Set very short AP timeout (basically skip it).
      iotWebConf->setApTimeoutMs(1);

      // Set up URL handlers.
      httpServer.on("/",    [this]{ iotWebConf->handleConfig(); });
      httpServer.onNotFound([this](){ iotWebConf->handleNotFound(); });

      // Start MQTT client
      mqttClient.begin(wifiClient);
    }

    String bridgeTopic(const char* topic) {
      return bridgeTopic(String(topic));
    }

    String bridgeTopic(const String topic) {
      return topicPrefix + String("/bridge/") + topic;
    }

    String deviceTopic(const char* topic) {
      return deviceTopic(String(topic));
    }

    String deviceTopic(const String topic) {
      return topicPrefix + String("/") + String(iosDeviceIdentifierValue) + String("/") + topic;
    }

    bool publish(const char* topic, const char* payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(deviceTopic(topic), payload, retained, qos);
    }

    bool publish(const String topic, const char* payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(deviceTopic(topic), payload, retained, qos);
    }

    bool publish(const char* topic, const String payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(deviceTopic(topic), payload, retained, qos);
    }

    bool publish(const String topic, const String payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(deviceTopic(topic), payload, retained, qos);
    }

    bool publishForDevice(const char* topic, const char* payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(deviceTopic(topic), payload, retained, qos);
    }

    bool publishForBridge(const char* topic, const char* payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(bridgeTopic(topic), payload, retained, qos);
    }

    void loop() {
      iotWebConf->doLoop();
      mqttClient.loop();

      if (needMqttConnect) {
        if (mqttConnect()) {
          needMqttConnect = false;
        }
      } else if (iotWebConf->getState() == iotwebconf::OnLine && ! mqttClient.connected()) {
        Serial.println("ancs2mqtt: MQTT reconnect");
        needMqttConnect = true;
      }

      if (needReset) {
        Serial.println("ancs2mqtt: rebooting after 1 second.");
        iotWebConf->delay(1000);
        publishForBridge("state", "Disconnected", true);
        ESP.restart();
      }
    }

    bool mqttConnect() {
      static bool     firstTime   = true;
      static unsigned lastAttempt = 0;
      unsigned long   now         = millis();

      // Only try once per second.
      if (now - lastAttempt < 1000) {
        return false;
      }
      Serial.printf("ancs2mqtt: connecting to MQTT server %s:%s ", mqttHostValue, mqttPortValue);
      if (! mqttConnectOptions()) {
        Serial.println("...failed!");
        lastAttempt = now;
        return false;
      }
      Serial.println("...connected!");

      // Announce ourselves to the broker.
      mqttAnnounce();

      // Call the start callback (if any).
      if (firstTime) {
        firstTime = false;
        if (onStartedCallback != nullptr) {
          onStartedCallback();
        }
      }

      return true;
    }

    bool mqttConnectOptions() {
      mqttClient.setHost(mqttHostValue, String(mqttPortValue).toInt());

      // Set Last Will and Testament topic
      String lwtTopic = bridgeTopic("LWT");
      mqttClient.setWill(lwtTopic.c_str(), "Offline", true, 0);

      if (mqttUserPasswordValue[0] != '\0') {
        return mqttClient.connect(iotWebConf->getThingName(), mqttUserNameValue, mqttUserPasswordValue);
      } else if (mqttUserNameValue[0] != '\0') {
        return mqttClient.connect(iotWebConf->getThingName(), mqttUserNameValue);
      } else {
        return mqttClient.connect(iotWebConf->getThingName());
      }
    }

    // Announce ourselves.
    void mqttAnnounce() {
      DynamicJsonDocument doc(255);
      iotwebconf::WifiAuthInfo authInfo = iotWebConf->getWifiAuthInfo();

      doc["version"]      = CONFIG_VERSION;
      doc["name"]         = name;
      doc["ssid"]         = authInfo.ssid;
      doc["ip"]           = localIP();
      doc["device-topic"] = deviceTopic("+");

      String payload;
      serializeJson(doc, payload);

      publishForBridge("LWT", "Online", true, 0);
      publishForBridge("info", payload.c_str());
    }

    String localIP() {
      IPAddress ipAddress = WiFi.localIP();
      String ipString     = String(ipAddress[0]);

      for (int octet = 1; octet < 4; octet++) {
        ipString += '.' + String(ipAddress[octet]);
      }
      return ipString;
    }

    void clearEEPROM() {
      EEPROM.begin(512);
      for (int i = 0; i < 512; i++) {
        EEPROM.write(i, 0);
      }
      EEPROM.end();
    }

    char* getName() {
      return iotWebConf->getThingName();
    }
};

void NetworkManager::wifiConnected() {
  needMqttConnect = true;
}

void NetworkManager::configSaved() {
  Serial.println("ancs2mqtt: configuration was updated.");
  needReset = true;
}

bool NetworkManager::formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper) {
  Serial.println("ancs2mqtt: validating form.");
  String host  = webRequestWrapper->arg(mqttHostParam.getId());
  String port  = webRequestWrapper->arg(mqttPortParam.getId());
  String ident = webRequestWrapper->arg(iosDeviceIdentifierParam.getId());

  if (host.length() < 3) {
    mqttHostParam.errorMessage = "Please provide at least 3 characters.";
    return false;
  }

  if (! port.toInt()) {
    mqttPortParam.errorMessage = "Invalid port number.";
    return false;
  }

  if (ident.length() < 1 || ident.indexOf("/") != -1) {
    iosDeviceIdentifierParam.errorMessage = "Please provide at least one character, and no slashes ('/').";
    return false;
  }

  return true;
}
