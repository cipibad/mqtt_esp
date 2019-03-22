#!/bin/sh

clientid=`grep CONFIG_MQTT_CLIENT_ID sdkconfig | cut -d '"' -f 2`
scp ./build/mqtt_ssl.bin 192.168.201.2:/var/www/sw.iot.cipex.ro/${clientid}.bin
