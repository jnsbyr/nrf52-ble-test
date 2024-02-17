# Bluetooth Long Range Test

Ever wondered what Bluetooth "Long Range" Mode (coded PHY) is about? This test project for the nRF52 MCU will show the pros and cons.

### Table of contents

[1. Motivation](#motivation)  
[2. Bluetooth Basics](#bluetooth-basics)  
[2.1 Bluetooth Communication Model](#bluetooth-communication-model)  
[2.2 Bluetooth Implementations](#bluetooth-implementations)  
[3. Source Code Extras](#source-code-extras)  
[4. BLE Range Tests](#ble-range-tests)  
[5. Conclusions](#conclusions)  
[6. Licenses and Credits](#licenses-and-credits)  

## Motivation

After evaluating a 433 MHZ RF solution for a [low power DHT](https://github.com/jnsbyr/samd21-solardht) I was looking for an alternative that:

- uses a standard wireless protocol
- supports bidirectional operation
- allows indoor use through walls and floors, preferably without meshing
- requires less than 200 µW

There are several popular candidates:

- Z-Wave (sub-GHz, 1999)
- EnOcean (sub-GHz, 2001)
- Zigbee (mostly 2.4 GHz, 2003)
- Bluetooth Low Energy (2.4 GHz, 2010)
- Thread (2.4 GHz, 2014)

My go with **Z-Wave** a while back was unsatisfactory, but probably not representative: device functionality was good, but the gateway dongle (ZME UZB) repeatedly forgot devices. Also device firmware updates required a manufacturer specific gateway, so fixing bugs and adding features was not possible in my setup.

I like **EnOcean**, especially the energy harvesting sensors and modules. Only setting up encryption when needed can be a little tedious and not all commercially available devices support it. There are ready to use transceiver modules available.

Never tried **Zigbee**. At the time I looked into it the concepts sounded promising but made the impression of work in progress. Also using the 2.5 GHz ISM band is not ideal, because it puts constraints on indoor range, but Zigbee uses a mesh topology that helps to compensate. **Zigbee Pro** also supports sub-GHz ISM bands, but compatible devices tend to be pricy. Zigbee seems to be a valid option, especially if you plan to deploy several devices that can create a robust mesh.

For **Bluetooth Low Energy** (BLE) the same restrictions apply as for Zigbee regarding the frequency and topology and that is why I did not consider it previously. Bluetooth 4.0 added meshing and Bluetooth 5.0 introduced a "long range" mode.

**Thread** uses the same frequency and topology as Zigbee and Bluetooth Mesh. Looking at a comparison published by STMicroelectronics Thread seems to have a few performance advantages compared to Zigbee and Bluetooth, but if they are relevant depends on the use case. Thread is based on IPv6 as does Zigbee IP while Bluetooth Mesh networking is not using IP.

As Bluetooth is supported by a lot of devices (e.g. smartphones), I wanted to find out if the long range mode improves indoor operation without the need to setup a mesh.

## Bluetooth Basics

To make your own BLE range test you need at least two devices that support the long range mode. That's where things unexpectedly started to become difficult for me, mainly because the long range mode is one of the many optional Bluetooth 5.0 features. Additionally Bluetooth - as most technologies - comes along with its own terminology and platform specific tools. So the simple question "Does the Rasberry Pi on my desk support long range mode?" had no immediate answer.

Let's start with some of the fundamental concepts of Bluetooth before coming back to this question.

### Bluetooth Communication Model

Bluetooth specification describes a lot of cans and only a few musts, often allowing more than one way to exchange the same amount and type of payload data. The following paragraphs are intended to provide the gist of some of the major concepts of Bluetooth Low Energy.

There is *advertising*: connectionless sending, either 1:1 (directed) or 1:n (undirected, broadcast). Advertising can be performed with or without *advertising data*. It can be marked as *scannable* when the sender offers to supply additional data if receiving a *scan request*. It can be marked as *connectable* if the sender offers to support connected mode. And it can be marked as *extended* if the advertising data size can be higher than 31 bytes. Extended advertising requires Bluetooth 5.0 and it comes with several variants like primary and secondary advertising. The *advertising data* and the *scan reply data* have the same format specifications. They can contain zero or more *characterisics*, either predefined (e.g. 16 bit temperature in °C) or user defined (*manufacturer data*).

The concepts of advertising and scan request/response aleady provide various ways of typically unsecured communication between two or more participants while the *connected mode* adds device authentication and data encryption for 1:1 communication. Advertising is not required to use connected mode.

All that has been described so far has nothing to do with long range mode. That is someting related to the way a Bluetooth *packet* (transmission unit) is send over the air. Each packet consists at least of *preamble*, *access address*, *protocol data unit (PDU)* and *CRC*. Classic Bluetooth uses a symbol rate of 1 Mbits/s. Bluetooth 5.0 adds the option for higher throughput with a symbol rate of 2 M and for a coded variant of the 1 M transmission, adding data redundancy and error correction, resulting in an effective data rate of 500 k (S=2) or 125 k (S=8). This coded 1 M *PHY* is what allows a longer transmission range. The different parts of a Bluetooth packet are encoded slightly different depending on the PHY type. It is possible to switch between the PHY types and it is supported to use 1 M and coded PHY exclusively.

Everything from advertising over scan request/response and PHY selection is considered as parts of the Bluetooth Generic Access Profile (*GAP*).

I will skip the details regarding connected mode, Bluetooth Generic Attribute Profiles (*GATT*) and Bluetooth services as they are not required for a range test and switch to the platform considerations.

### Bluetooth Implementations

The Bluetooth 5.0 specifications are from 2016, but still not every device around has the required support, lacking either on the hardware side or on the software side.

#### Linux

BlueZ is the name of the Bluetooth implementation for Linux. The current version 5.72 supports BLE and even provides a partial implementation of Bluetooth Mesh. The command line tools for BlueZ are called *bluetoothctl*, *btmgmt* and *btmon* and 
```shell
# btmgmt -i 0 phy
```
will tell you if the default Bluetooth device supports BLE (LE1MTX/RX) and coded PHY (LECODEDTX/RX) and if these PHY modes are active or not.
```shell
# btmgmt -i 0 phy LE1MTX LE1MRX LECODEDTX LECODEDRX 
```
will activate the 1M and coded PHY.

Using these commands I was able to determine that the on-board Bluetooth device of the Rasperry Pi 4B on my desk had BLE support but no support for coded PHY. So I added a Bluetooth USB dongle with a RTL8761B chip. This devices supports 1 M and coded PHY, but activating coded PHY mode failed. Assuming that the BlueZ version 5.66 coming with Rasbian 12 may be the cause, I installed BlueZ 5.72 from source and now activating coded PHY worked.

By combining bluetoothctl with btmon it is possible to perform various Bluetooth operations like advertise, scan, pair and connect and to monitor incoming packets. Using bluetoothctl is not very intuitive, so look for online examples. If you want to do more, then python combined with dbus events seem to be the tools of choice.

#### Smartphone

To determine if a specific smartphone model supports coded PHY one can look for feedback from other users or perform a test, e.g. with the nRF Connect app from Nordic Semiconductors. This app is a very useful and easy to use  tool for analysing Bluetooth communications.

Scan for devices and try to connect to an available device with a specific PHY. If coded PHY is selectable your smartphone has the required support. 

#### IoT Devices

All major players offering Bluetooth components seem to support most of the Bluetooth 5.0 features including coded PHY. 

I selected the Seed Studio XIAO nRF52840 because of its small form factor and the low power requirements of the nRF52840. Nordic Semiconductors provides two different APIs to implement Bluetooth applications:

- nRF Connect SDK
- SoftDevice

The XIAO is geared for the SoftDevice when using the Arduino platform. The SoftDevice can either be used directly or wrapped by the Adafruit Bluefruit library. Using the SoftDevice directly is not easy. Apart from the API reference only a few examples can be found. Some of these examples use a mix of both APIs, but the nRF Connect SDK API is not included in the Seed Studio SDK. On the other hand the Adafruit Bluefruit library ensures quick results when making the first steps but currently does not support coded PHY operations. 

## Source Code Extras

This project uses an enhanced version of the [Adafruit Bluefruit nRF52 Libraries 0.21.0](https://github.com/jnsbyr/Adafruit_nRF52_Arduino/tree/feature/extended-advertising/libraries/Bluefruit52Lib/) to be able to use advanced advertising and coded PHY. I first tried subclassing BLEAdvertising, but that did not work as expected because Bluefruit uses an internal instance of BLEAdvertising for the BLE event handling that cannot be overridden. So I extracted the Bluefruit library source from the Seed Studio SDK and modified BLEAdvertising directly. Now it was possible to test advertising with the XIAO board, the Pi and the smartphone. Note that the Adafruit GitHub repository and the Seed Studio GitHub repository contain independent copies of the Bluefruit library with the same version number but slightly different code. 

The code also contains a few snippets that may be useful for other projects:

- read internal temperature of MCU using *readCPUTemperature()* - seems to be quite accurate, especially immediately after idle wakeup
- read MCU supply voltage using *analogReadVDD()*
- read battery voltage using *readBatteryVoltage()*
- Bluetooth characteristic definitions for temperature, humidity and voltage
- XIAO nRF52840 low power optimization using *sleepExternalFlash()*

## BLE Range Tests

The setup:

- XIAO nRF52840, transmitting periodic advertising with 0 dB via 1 M PHY or coded PHY
- Rasberry Pi with RTL8761B dongle, receiving
- Smartphone, receiving

The results when changing advertising PHY from 1 M to coded:

- RTL8761B RSSI value worsens by around 4 dB within same room  
- Smartphone RSSI values improves by more than 6 dB within same room 
- Smartphone indoor reception range through at least 1 brick/mortar wall/floor does not change noticeably

## Conclusions

- The result with the RTL8761B as receiver contradicts other published tests and might be caused by a firmware issue.
- The results with the smartphone as receiver show that coded PHY improves transmission in line of sight situations, but does not improve indoor transmission through solid walls and floors.
- Indoor transmissions at 2.4 GHz through solid walls and floors with Bluetooth, ZigBee or Thread require robust meshing or gateways. 
- In some situations a point-to-point transmission can be improved by using external antennas instead of on-board antennas or by increasing the transmit power. 
- Using meshing and/or gateways only for the purpose of extending the range of a single device is not efficient in respect to energy and costs. Consider using a sub-GHz wireless technology instead.

## Licenses and Credits

### Documentation and Photos

Copyright (c) 2024 [Jens B.](https://github.com/jnsbyr)

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

### Source Code

Copyright (c) 2024 [Jens B.](https://github.com/jnsbyr)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/mit/)

The source code was edited and build using the [Arduino IDE](https://www.arduino.cc/en/software/), [Arduino CLI](https://github.com/arduino/arduino-cli) and [Microsoft Visual Studio Code](https://code.visualstudio.com).

The source code depends on:

#### Arduino Core for Seeed nRF52 Boards (Seeeduino nRF52 1.1.8)

Copyright (c) 2015 Arduino LLC.  All right reserved.
Copyright (c) 2016 Sandeep Mistry All right reserved.
Copyright (c) 2017 Adafruit Industries. All rights reserved.

[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL%202.1%20only-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)

#### Adafruit Bluefruit nRF52 Libraries (based on 0.21.0)

Copyright (c) 2016 Adafruit Industries

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Bluefruit52Lib/LICENSE)

#### Adafruit nRFCrypto (0.0.5, indirect dependency)

Copyright (c) 2020 Adafruit Industries

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/adafruit/Adafruit_nRFCrypto/blob/master/LICENSE)

#### Adafruit TinyUSB Library (1.7.0, indirect dependency)

Copyright (c) 2019 Ha Thach for Adafruit Industries

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/master/LICENSE))

#### Adafruit Little File System Libraries (0.11.0, indirect dependency)

Copyright (c) 2017, Arm Limited. All rights reserved.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Adafruit_LittleFS/src/littlefs/LICENSE.md)

#### Adafruit Internal File System on Bluefruit nRF52 (0.11.0, indirect dependency)

Copyright (c) 2015 Arduino LLC.  All right reserved.
Copyright (c) 2016 Sandeep Mistry All right reserved.
Copyright (c) 2017 Adafruit Industries. All rights reserved.

[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL%202.1%20only-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)
