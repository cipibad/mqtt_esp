#!/bin/sh

clientid=$(grep CONFIG_CLIENT_ID sdkconfig | cut -d '"' -f 2)
flash_size=$(grep CONFIG_ESPTOOLPY_FLASHSIZE= sdkconfig | cut -d '"' -f 2)
rpi_address=192.168.200.254
if [ "$flash_size" = "1MB" ]; then
    scp ./build/mqtt_ssl.ota.bin pi@${rpi_address}:docker/volume/sw/var/www/sw.iot.cipex.ro/${clientid}.bin
    if [ $? -eq 0 ]; then
        echo "Copied 1MB OTA FW to ${clientid}.bin"
    else
        echo "ERROR: Cannot copy 1MB OTA FW to ${clientid}.bin"
    fi
else
    scp ./build/mqtt_ssl.bin pi@${rpi_address}:docker/volume/sw/var/www/sw.iot.cipex.ro/${clientid}.bin
    if [ $? -eq 0 ]; then
        echo "Copied 1MB+ OTA FW to ${clientid}.bin"
    else
        echo "ERROR: Cannot copy 1MB+ OTA FW to ${clientid}.bin"
    fi
fi
