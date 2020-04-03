# mqtt_esp
stable and secure esp8266/esp32 idf based iot firmware that can be used by any home automation system supporting MQTT to control relays/sensors and generally all what SonOff and similar devices offer.

Firmware can be manually build and installed on devkit boards and also on Sonoff devices lister bellow.
It will securely connect to configured MQTT bridge from where can be used by any MQTT client: [Node-Red](https://nodered.org/), [MQTT Dash](https://play.google.com/store/apps/details?id=net.routix.mqttdash&hl=ro), [Home Assistant](https://www.home-assistant.io/hassio/)/Google Assistant/Alexa/etc.

MQTT Interface is generally described here, while source code will be a better information source: https://github.com/cipibad/mqtt_esp/wiki/mqtt-interface

Supported Sonoff devices: 
  * Sonoff Basic: https://sonoff.tech/product/wifi-diy-smart-switches/basicr2
  * Sonoff S20: https://www.itead.cc/smart-socket.html
  * Sonoff T1: https://www.itead.cc/sonoff-t1-eu.html
  * Sonoff 4CH: https://www.itead.cc/smart-home/sonoff-4ch.html

