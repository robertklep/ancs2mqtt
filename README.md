# ancs2mqtt

ESP32-based ANCS ([Apple Notification Center Service](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Introduction/Introduction.html)) consumer to publish iOS notifications from your iDevice over MQTT.

## Synopsis

This project implements an ESP32-based Notification Consumer that will pair with your iDevice and publish notifications it receives to an MQTT broker of your choice.

The connection between your iDevice and the ESP32 is maintained over BLE, which means that both devices can't be more than a few meters apart. It might be possible to have multiple ESP32's running this firmware around your home, but I have yet to test that myself.

## Prerequisites

* [PlatformIO](https://platformio.org/): this is required to build the firmware.
* An ESP32 board. I use WeMos D1 MINI ESP32 boards that can be bought for a few €€€ on sites like AliExpress.
  Other ESP32-based boards should work too as long as they are supported by PlatformIO. Make sure to change the `platformio.ini` file to reflect the different board.
* An MQTT broker. If you don't know what this is, this project may not be for you 🥴

## HOWTO

Heads up: this isn't a plug-and-play project. You should be comfortable working with (Linux/macOS) command lines. At some point I will try and provide prebaked firmware files, but you'd still need to know a bit about flashing ESP32 firmware.

### Building and uploading the firmware

I don't use the PlatformIO IDE so the following steps use the PlatformIO CLI tool (`platformio/pio`).

* Obtain the project source code, either by downloading a ZIP file from Github or by cloning the repository:
  ```
  $ git clone https://github.com/robertklep/ancs2mqtt
  ```
* (Edit the `platformio.ini` file if you're using a different board)
* Build the code (in the directory containing the `platformio.ini` file):
  ```
  $ pio run
  ```
* If that worked: great! If not, and you're certain it's my fault, [create an issue](https://github.com/robertklep/ancs2mqtt/issues).
* If you have a successful build, upload it to your ESP32 device:
  ```
  $ pio run --target=upload
  ```
  (you may have to explicitly pass the serial port device: `--upload-port /dev/cu.usbserial*`)
* If that worked: we're almost there! If not, see above.
* Connect to the device using the serial monitor to see if it boots up properly:
  ```
  $ pio device monitor --baud 115200 --raw
  ```
  (to explicitly pass the serial port device: `--port /dev/cu.usbserial*`);
* See the following steps.

### Configuration

#### WiFi/MQTT (do this first)

When the device is first started, you should notice the status LED blinking. This means that the device is running in WiFi AP mode and is waiting for you to connect to and configure it.

* Open the WiFi configuration of your iDevice/laptop/etc and look for a network called `ancs2mqtt-XXXXXX` (`XXXXXX` will vary).
* Connect to the network using the default password: `ancs2mqtt`
* Once connected, you should be shown a "captive portal" page. If not, open a browser and navigate to `192.168.4.1`
* Configure the settings:
  * **System Configuration**:
    * _"Thing name"_ (optional): unique name for your ESP32 device. The default name should be good.
    * _"AP password"_ (required): will replace the default AP password.
    * _"WiFi SSID"_ (required): your WiFi network name.
    * _"WiFi password"_ (required): your WiFi password.
  * **MQTT configuration**:
    * _"iOS device identifier"_ (required): a unique name to identify the iOS device you will be pairing this ESP32 with. For instance "jacks-iphone". This will be used as part of the MQTT topic that notifications will be published too (see below, _"MQTT Topics"_, for more information).
    * _"MQTT host"_ (required): hostname/IP address of your MQTT broker
    * _"MQTT port"_ (optional): MQTT broker port (default 1883)
    * _"MQTT user"_ (optional): MQTT username to authenticate to the broker with
    * _"MQTT password"_ (optional): MQTT password to authenticate to the broker with
  * **Reset configuration**:
    * _"Reset configuration"_: check this box to perform a full reset of the configuration. The device will reset and start AP mode using the default password.

After configuration, press "Apply" and wait for the device to reboot. If everything went well, the device will eventually announce itself to the MQTT broker on topic `ancs2mqtt/bridge/info`. If the device doesn't appear to come up, you'll have to connect a serial monitor (see _"Building and uploading the firmware"_) to see what's going on.

If you want to change any parts of the configuration afterwards, you can navigate with your browser to the ESP32's IP address (log in using username `admin` and the AP password you configured in the System Configuration).

#### iOS (do this when your device successfully announced itself over MQTT)

Once WiFi and MQTT are successfully configured, you can pair the ESP32 with your iDevice:
* On your iDevice, open the Bluetooth settings
* The ESP32 device should be visible as the configured "Thing name" (see the previous step). Connect with it.
* When asked if you want to pair with the device, and you want to allow the device to be sent notifications, answer both affirmatively.

When pairing is successful, the device will start publishing notifications to MQTT. iOS seems to keep a history of older notifications that it will send over immediately.

### MQTT Topics

The following MQTT topics are used to publish data to. Data will be in JSON format.

* `ancs2mqtt/bridge/info`: published once it has connected successfully to WiFi and MQTT.
* `ancs2mqtt/IOS_DEVICE_NAME/APP_IDENTIFIER`: published when a new notification is received (`IOS_DEVICE_NAME` is the configured _"iOS device identifier"_, `APP_IDENTIFIER` is the iOS app identifier for the app that is responsible for the notification).

## Oh no I forgot my AP password!

If you temporarily connect the GPIO2/D2 pin to GND and then reset, the ESP32 will start up in AP mode using the default AP password (`ancs2mqtt`).
