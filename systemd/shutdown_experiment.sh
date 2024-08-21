#!/bin/bash

if [ "$EXPERIMENT_ENDED" == "0" ]; then
    echo "export EXPERIMENT_ENDED=1" >> ~/.profile 

    systemctl stop obc-p1.service
    systemctl disable obc-p1.service

    systemctl stop start_gpsd.service
    systemctl disable start_gpsd.service

    umount /dev/sda2

    sleep 5

    shutdown now
elif [ "$EXPERIMENT_ENDED" == "1" ]; then
    umount /dev/sda2
    sleep 10

    shutdown now
fi