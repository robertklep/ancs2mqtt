#pragma once
#include <Arduino.h>
#include <MQTT.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include <string>
#include <functional>
#include "esp32notifications.h"
#include "defaults.h"

DNSServer   dnsServer;
WebServer   httpServer(80);
WiFiClient  wifiClient;
MQTTClient  mqttClient;

class NetworkManager {
  bool    needReset       = false;
  bool    needMqttConnect = false;

  String  name;
  String  topicPrefix;

  uint8_t statusPin;
  char    mqttHostValue[PARAMETER_STRING_LENGTH];
  char    mqttPortValue[PARAMETER_STRING_LENGTH]   = "1883";
  char    mqttUserNameValue[PARAMETER_STRING_LENGTH];
  char    mqttUserPasswordValue[PARAMETER_STRING_LENGTH];

  IotWebConf*                 iotWebConf;
  IotWebConfParameterGroup    mqttGroup             = IotWebConfParameterGroup("mqtt", "MQTT configuration");
  IotWebConfTextParameter     mqttHostParam         = IotWebConfTextParameter("MQTT host",         "mqttHost", mqttHostValue,         PARAMETER_STRING_LENGTH);
  IotWebConfTextParameter     mqttPortParam         = IotWebConfTextParameter("MQTT port",         "mqttPort", mqttPortValue,         PARAMETER_STRING_LENGTH);
  IotWebConfTextParameter     mqttUserNameParam     = IotWebConfTextParameter("MQTT user",         "mqttUser", mqttUserNameValue,     PARAMETER_STRING_LENGTH);
  IotWebConfPasswordParameter mqttUserPasswordParam = IotWebConfPasswordParameter("MQTT password", "mqttPass", mqttUserPasswordValue, PARAMETER_STRING_LENGTH);

  void wifiConnected();
  void configSaved();
  bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
  void handleRoot();

  public:
    NetworkManager(String name, const char* initialApPassword, uint8_t statusPin) : name(name), statusPin(statusPin) {
      iotWebConf = new IotWebConf(name.c_str(), &dnsServer, &httpServer, initialApPassword, CONFIG_VERSION);
      topicPrefix = "ancs2mqtt/";
    }

    void setTopicPrefix(String prefix) {
      topicPrefix = prefix;
    }

    void start() {
      // Add parameters to group, and group to config.
      mqttGroup.addItem(&mqttHostParam);
      mqttGroup.addItem(&mqttPortParam);
      mqttGroup.addItem(&mqttUserNameParam);
      mqttGroup.addItem(&mqttUserPasswordParam);
      iotWebConf->addParameterGroup(&mqttGroup);

      // Status indicator pin
      iotWebConf->setStatusPin(statusPin);
      //iotWebConf.setConfigPin(CONFIG_PIN);

      // Set callbacks
      using namespace std::placeholders;
      iotWebConf->setWifiConnectionCallback(std::bind(&NetworkManager::wifiConnected, this));
      iotWebConf->setConfigSavedCallback(std::bind(&NetworkManager::configSaved, this));
      iotWebConf->setFormValidator(std::bind(&NetworkManager::formValidator, this, _1));

      // Initialize the configuration.
      bool validConfig = iotWebConf->init();
      if (! validConfig) {
        mqttHostValue[0]         = '\0';
        strcpy(mqttPortValue, "1883");
        mqttUserNameValue[0]     = '\0';
        mqttUserPasswordValue[0] = '\0';
      }

      // Set up URL handlers.
      httpServer.on("/", std::bind(&NetworkManager::handleRoot, this));
      httpServer.on("/config", [this]{ iotWebConf->handleConfig(); });
      httpServer.onNotFound([this](){ iotWebConf->handleNotFound(); });

      // Start MQTT client
      mqttClient.begin(wifiClient);
    }

    bool publish(const char* topic, const char* payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(topicPrefix + topic, payload, retained, qos);
    }

    bool publish(const String topic, const char* payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(topicPrefix + topic, payload, retained, qos);
    }

    bool publish(const char* topic, const String payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(topicPrefix + topic, payload, retained, qos);
    }

    bool publish(const String topic, const String payload, bool retained = false, int qos = 0) {
      return mqttClient.publish(topicPrefix + topic, payload, retained, qos);
    }

    void loop() {
      iotWebConf->doLoop();
      mqttClient.loop();

      if (needMqttConnect) {
        if (connectMqtt()) {
          needMqttConnect = false;
        }
      } else if (iotWebConf->getState() == iotwebconf::OnLine && ! mqttClient.connected()) {
        Serial.println("MQTT reconnect");
        connectMqtt();
      }

      if (needReset) {
        Serial.println("Rebooting after 1 second.");
        iotWebConf->delay(1000);
        ESP.restart();
      }
    }

    bool connectMqtt() {
      static unsigned lastMqttConnectionAttempt = 0;
      unsigned long   now = millis();

      // Only try once per second.
      if (1000 > now - lastMqttConnectionAttempt) {
        return false;
      }
      Serial.print("Connecting to MQTT server...");
      if (! connectMqttOptions()) {
        Serial.println("...failed!");
        lastMqttConnectionAttempt = now;
        return false;
      }
      Serial.println("...connected!");

      // Announce ourselves.
      iotwebconf::WifiAuthInfo authInfo = iotWebConf->getWifiAuthInfo();
      publish("bridge/info/name", name);
      publish("bridge/info/ssid", authInfo.ssid);
      publish("bridge/info/ip",   localIP());

      return true;
    }

    bool connectMqttOptions() {
      mqttClient.setHost(mqttHostValue, String(mqttPortValue).toInt());
      if (mqttUserPasswordValue[0] != '\0') {
        return mqttClient.connect(iotWebConf->getThingName(), mqttUserNameValue, mqttUserPasswordValue);
      } else if (mqttUserNameValue[0] != '\0') {
        return mqttClient.connect(iotWebConf->getThingName(), mqttUserNameValue);
      } else {
        return mqttClient.connect(iotWebConf->getThingName());
      }
    }

    String localIP() {
      IPAddress ipAddress = WiFi.localIP();
      String ipString     = String(ipAddress[0]);

      for (int octet = 1; octet < 4; octet++) {
        ipString += '.' + String(ipAddress[octet]);
      }
      return ipString;
    }
};

void NetworkManager::wifiConnected() {
  needMqttConnect = true;
}

void NetworkManager::configSaved() {
  Serial.println("Configuration was updated.");
  needReset = true;
}

bool NetworkManager::formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper) {
  Serial.println("Validating form.");
  String host = webRequestWrapper->arg(mqttHostParam.getId());
  String port = webRequestWrapper->arg(mqttPortParam.getId());

  if (host.length() < 3) {
    mqttHostParam.errorMessage = "Please provide at least 3 characters.";
    return false;
  }

  if (! port.toInt()) {
    mqttPortParam.errorMessage = "Invalid port number.";
    return false;
  }

  return true;
}


void NetworkManager::handleRoot() {
  if (iotWebConf->handleCaptivePortal()) {
    // Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>ancs2mqtt</title><style>body { font-family: Helvetica, sans-serif; font-size: 14px }</style></head><body><h1>ancs2mqtt</h1>";
  s += "<ul>";
  s += "<li>MQTT host: ";
  s += mqttHostValue;
  s += "<li>MQTT port: ";
  s += mqttPortValue;
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  httpServer.send(200, "text/html", s);
}
