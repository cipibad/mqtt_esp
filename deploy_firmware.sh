#!/bin/sh

clientid=`grep CONFIG_CLIENT_ID sdkconfig | cut -d '"' -f 2`
flash_size=`grep CONFIG_ESPTOOLPY_FLASHSIZE= sdkconfig | cut -d '"' -f 2`

if [ "$flash_size" = "1MB" ]; then
    scp ./build/mqtt_ssl.ota.bin pi@192.168.200.3:docker/volume/sw/var/www/sw.iot.cipex.ro/${clientid}.bin
    echo "Copied 1MB OTA FW to ${clientid}.bin"
else
    scp ./build/mqtt_ssl.bin pi@192.168.200.3:docker/volume/sw/var/www/sw.iot.cipex.ro/${clientid}.bin
    echo "Copied 1MB+ OTA FW to ${clientid}.bin"
fi
