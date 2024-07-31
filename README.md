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
        - [ ] TODO: Change write to always instead of when connected_to_GS (keep for testing)
    - [x] Receive commands
    - [x] Parse Comands
    - [ ] Center signal to desired frequency before downsample
    - [x] Downsample data (factor of 2048: 8M -> 3.9062k)
    - [ ] ~~Transmit  downsampled data to MCC~~ Done with Resilio Sync

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

---
- [ ] OBC-P4.c
    - Interfaces with GNSS Receiver
        - [ ] Receives data from Receiver 
        - [ ] Stores data locally
        - [ ] ~~Transmits data to MCC~~ Done with Resilio Sync
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


#### Throttle Uplink and Downlink

```
(OBC-downlink-115kbit) $ sudo tc qdisc add dev <eth-iface> root tbf rate 115kbit burst 16kbit latency 50ms
(MCC-uplink-10kbit)    $ sudo tc qdisc add dev <eth-iface> root tbf rate 10kbit burst 10kbit latency 50ms
```

#### GPS

On startup:
- Set /dev/ttyUSB0 baudrate to 115200: 
        ```sudo stty -F /dev/ttyUSB0 115200```
- Start gpsd daemon:
        ```sudo gpsd /dev/ttyUSB0 -n -N -D 3 -G```


#### OBC USB per-port power switching 

```uhubctl``` can be used to control power of individual USB ports of the Raspberry Pi. Useful to control SDR USB port.

```sudo uhubctl -l 1-1 -p 2 -a off```
```-l```: hub location
```-p```: port number
```-a```: action, ```on```/```off```

