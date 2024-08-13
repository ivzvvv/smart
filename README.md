# SMART Experiment Software Architecture

![Alt Text](images/SW_arch.png)

#### OBC - On-Board Computer

- Raspberry Pi 4
- OS: Raspbian (Debian)

OS config:
- [ ] Read-only filesystem except SSD and conf files (last section of [this document](docs/ro_rootfs.pdf))

Programs:

---
- [ ] OBC-P1.c
    - [x] Receives samples from SDR
    - [x] Change SDR configuration
    - [x] Writes to filesystem
        - [X] Raw samples always - 1 file 32 MB per second (8 mega I/Q samples)
        - [X] Downsampled samples when connected_to_GS - 1 file 15.624 KB (3906 I/Q samples)
    - [x] Receive commands
    - [x] Parse Comands
    - [x] Center signal to desired frequency before downsample
    - [x] Downsample data (factor of 2048: 8M -> 3.9062k)
    - [x] ~~Transmit  downsampled data to MCC~~ Done with Resilio Sync

#### Synchronizing data with GS

[Resilio Sync](https://www.resilio.com/) will be used to syncronize the folder of the OBC containing the downsampled data.
Sync folder is ```/home/rslsync/``` in OBC and MCC.

##### Command format:

    :[command]:[value]:

    e.g.

    Change central frequency to 140MHz
    :F:140000000:

    Send heartbeat (no value needed)
    :H:
Available commands:

| command | name | value |
|-|-|-|
| H | Heartbeat | no value |
| F | Tuner central frequency| in Hz |
| S | Sampling Frequency | in Hz |
| U | Shutdown SDR USB port | no value |
| R | Shutdown and restart SDR USB port | no value |
| Z | (TODO) End of flight experiment shutdown | no value |

---
- [ ] OBC-P4.c
    - Interfaces with GNSS Receiver
        - [x] Receives data from Receiver 
        - [ ] ~~Stores data locally~~ OBC telnet client
        - [x] ~~Transmits data to MCC~~ GS client connects to gpsd on-board
---


#### MCC - Mission Control Computer

- Laptop 
- Running Ubuntu 22.04 LTS

Programs:
- [ ] MCC-P1
    - Sends commands periodically, create new connection every time.
- [ ] MCC-P2
- [ ] MCC-P3
- [ ] MCC-P4

MCC programs will be implemented in matlab as a standalone app. 


| Unit | Port |Program|    Use         | 
|------|------|-------|----------------|
|OBC   | 9090 | OBC-P1|Receive commands to change SDR tuner configuration|
|OBC   |22    |  ssh  |  Open ssh connection     |
|MCC   | 9191 | MCC-P1|Receive I/Q data from SS  |

---

#### Throttle Uplink and Downlink

```
(OBC-downlink-115kbit) $ sudo tc qdisc add dev <eth-iface> root tbf rate 115kbit burst 16kbit latency 50ms
(MCC-uplink-10kbit)    $ sudo tc qdisc add dev <eth-iface> root tbf rate 10kbit burst 10kbit latency 50ms
```

---

#### GPS

On startup:
- Set /dev/ttyUSB0 baudrate to 115200: 
        ```sudo stty -F /dev/ttyUSB0 115200```
- Start gpsd daemon:
        ```sudo gpsd /dev/ttyUSB0 -n -N -D 3 -G```

The ```start_gpsd.sh``` script is used as a systemd service to set the baudrate and start the gpsd daemon. After starting the daemon, if ```/dev/ttyUSB0``` is no longer detected, the service stops. After every 5 seconds the service is reloaded. 
To configure the service, two files are needed:
- Copy ```/systemd/start_gpsd.service``` to ```/etc/systemd/system/start_gpsd.service```
- ```start_gpsd.sh```

Ensure ```ExecStart``` variable in ```start_gpsd.service``` corresponds to the path to ```start_gpsd.sh```. To enable the service:
```sudo systemctl enable start_gpsd.service```

TODO: Add on-board ```./telnet-client 127.0.0.1 2947``` executed after the gpsd daemon to save the gpsd data locally.

---

#### OBC USB per-port power switching 

```uhubctl``` can be used to control power of individual USB ports of the Raspberry Pi. Useful to control SDR USB port.

```sudo uhubctl -l 1-1 -p 2 -a off```
```-l```: hub location
```-p```: port number
```-a```: action, ```on```/```off```

**IMPORTANT NOTE: During setup on campaign week, annotate the USB port of the SDR and don't change it.**

---

#### OBC-P1 service

The ```OBC-P1``` binary is used as a systemd service just like the ```start_gpsd.sh``` script. Required files:
- Copy ```/systemd/obc-p1.service``` to ```/etc/systemd/system/obc-p1.service```
- ```OBC-P1``` compiled binary

Ensure ```ExecStart``` variable in ```obc-p1.service``` corresponds to the path to the ```OBC-P1``` binary. To enable the service:
```sudo systemctl enable obc-p1.service```

Stops when SDR is disconnected. Restarts every 5 seconds. 

---

#### restart-sdr-usb service

The ```restart-sdr-usb.sh``` script is used as a systemd service to power off and after 5 seconds power on the SDR usb port. Required files:
- Copy ```/systemd/restart-sdr-usb.service``` to ```/etc/systemd/system/restart-sdr-usb.service```
- ```restart-sdr-usb.sh``` script

Ensure ```ExecStart``` variable in ```restart-sdr-usb.service``` corresponds to the path to the ```restart-sdr-usb.sh``` script. **DO NOT ENABLE THIS SERVICE. IT IS SUPOSED TO BE STARTED BY OBC-P1 WHEN THE :R: COMMAND IS RECEIVED. AFTER POWERING ON AND OFF THE SERVICE QUITS.**

---

#### Mount SSD

```/etc/fstab``` has the following entry:

```PARTUUID=22eb6c44-2e0d-4a4a-be2a-e5fddf32b1c9    /mnt/OBC-SMART	    ntfs    defaults    0   2```

This automounts the external SSD to the ```/mnt/OBC-SMART``` folder on startup. 

---

#### End of experiment Shutdown Procedures

- Stop and disable ```obc-p1.service```
- Stop and disable ```start_gpsd.service```
- Unmount SSD
- Shutdown Pi
