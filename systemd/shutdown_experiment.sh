#!/bin/bash

systemctl stop obc-p1.service
systemctl disable obc-p1.service

systemctl stop start_gpsd.service
systemctl disable start_gpsd.service

umount /dev/sda2

sleep 20

shutdown now