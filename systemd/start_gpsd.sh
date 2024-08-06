#!/bin/bash

sleep 2

stty -F /dev/ttyUSB0 115200

gpsd /dev/ttyUSB0 -G -n -D 3
while true; do
    output=$(ls /dev/ | grep ttyUSB0)
    if [[ "$output" != "ttyUSB0" ]]; then
        break
    fi
    sleep 1
done
