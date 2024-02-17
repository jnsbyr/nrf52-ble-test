/*****************************************************************************
 *
 * nRF52 Bluetooth Long Range Mode Test
 *
 * file:     nrf52-ble-test.ino
 * encoding: UTF-8
 * created:  29.04.2023
 *
 *****************************************************************************
 *
 * MIT License
 *
 * Copyright (C) 2024 Jens B.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 *
 *****************************************************************************/

/*
 * power consumption
 *
 * - XIAO nRF52840 with stock bootloader (Adafruit_nRF52_Bootloader)
 *
 * - Seeeduino nRF52 Arduino Core 1.1.8
 *     - Adafruit nRF52 Arduino Core 1.2.0
 *     - FreeRTOS 10.0.1 (delay = vTaskDelay = tickless idle)
 *     - Nordic Soft Device 140 7.3.0 (initially disabled, use via Bluefruit)
 *     - Nordic nrfx 2.1.0
 *     - Nordic nRF5 SDK: not available
 *
 * - nRF52840 datasheet:
 *     - system off, no RAM retention:   0.40 µA (IOFF_RAMOFF_RESET)
 *     - system off, full RAM retention: 1.86 µA (IOFF_RAMON_RESET)
 *     - system on, wake on any event:   2.35 µA (ION_RAMON_EVENT)
 *     - system on, wake on RTC:         3.16 µA (ION_RAMON_RTC)
 *     - system on, HFXO, DC/DC, RAM:    2.8 mA  (ICPU2)
 *     - system on, HFXO, DC/DC, flash:  3.3 mA  (ICPU0)
 *
 * - hardware setup:
 *     - USB disconnected
 *     - VBAT 3.0 V
 *     - LEDs off
 *     - no pins connected
 *
 * - measurement:
 *     - tickless idle:                   21.5 µA (+19.1 µA)
 *     - tickless idle, ext. flash sleep:  3.5 µA (+1.1 µA)      12 µW @ 3.3 V
 *     - system off, ext. flash sleep:     1.8 µA (+1.4 µA)
 *     - constant latency:                 0.59 mA
 *
 * - notes:
 *     - USB charging: default is enabled with 50 mA
 *     - clock setting: default is USE_LFXO?
 *     - power setting: default is low power?
 *     - DC/DC settings: default is enable?
 *     - GPIO interrupt: high accuracy mode (12 .. 15 µA, default), low power mode (https://forum.seeedstudio.com/t/getting-lower-power-consumption-on-seeed-xiao-nrf52840/270129/4)
 *     - U2 (batter charger) reverse current?
 *     - U6 (power regulator) reverse current?
 *     - U7 (flash) sleep current?
 *     - disable USB completely: #undef USE_TINYUSB?
 */

#include <Arduino.h>

#include <bluefruit.h>

//comment in for XIOA power saving
//#include <Adafruit_FlashTransport.h>

//comment in for extra debug output
//#define SERIAL_DEBUG

const uint32_t LED_BLINK_PERIOD = 3000; // [ms]
const uint32_t SERIAL_SPEED = 115200; // [baud]
const uint32_t PIN_CHARGING = 23;
const float ADC_TO_VOLT = 3.6f/1023; // [V/digit]

#ifdef ADAFRUIT_FLASHTRANSPORT_H_
Adafruit_FlashTransport_QSPI flashTransport;
#endif

// put external 2M QSPI flash chip to sleep (saves 18 µA)
void sleepExternalFlash()
{
#ifdef ADAFRUIT_FLASHTRANSPORT_H_
  flashTransport.begin();
  flashTransport.runCommand(0xB9);
  flashTransport.end();
#endif
}

bool isSoftDeviceEnabled()
{
  uint8_t check;
  sd_softdevice_is_enabled(&check);
  return check;
}

/*
float lastBatteryVoltage = 0.0f;
uint16_t stableChargingVoltageCount = 0;
bool charging = false;
bool isCharging()
{
  return charging;
}
*/

float readBatteryVoltage()
{
  digitalWrite(VBAT_ENABLE, LOW);
  uint32_t adc = analogRead(PIN_VBAT);
  digitalWrite(VBAT_ENABLE, HIGH);
  float adcVoltage = adc*ADC_TO_VOLT;
  float batteryVoltage = adcVoltage * (1510.0f/510.0f); // resistor divider
/*
  bool stable = abs(batteryVoltage - lastBatteryVoltage) < 0.2f;
  if (stable)
  {
    if (stableChargingVoltageCount < 3)
    {
      stableChargingVoltageCount++;
    }
    else
    {
      charging = batteryVoltage <= 4.2f; // https://forum.seeedstudio.com/t/xiao-nrf52840-battery-voltage-reading-circuit-increases-sleep-current/273177
    }
  }
  else
  {
    stableChargingVoltageCount = 0;
  }
*/
  return batteryVoltage;
}

void bleAdvertisingStopped()
{
#ifdef SERIAL_DEBUG
  Serial.println("BLE adverting stopped");
#endif
}

uint16_t bleAdvertisingDuration = 3; // [s]
void setupBLEAdvertising()
{
  //Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED);
  //Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_SCANNABLE_UNDIRECTED);
  Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED);
  //Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED);
  Bluefruit.Advertising.setIntervalMS(BLE_GAP_ADV_INTERVAL_MIN + 10, BLE_GAP_ADV_INTERVAL_MAX);
  Bluefruit.Advertising.setFastTimeout(bleAdvertisingDuration); // [s]
  //Bluefruit.Advertising.setMaxEvents(10);
  //Bluefruit.Advertising.setPhy(BLE_GAP_PHY_CODED);
  //bleAdvertising.setFilter(BLE_GAP_ADV_FP_FILTER_CONNREQ); // untested
  Bluefruit.Advertising.setStopCallback(bleAdvertisingStopped);
  Bluefruit.Advertising.restartOnDisconnect(Bluefruit.Advertising.isConnectable());

  Bluefruit.Advertising.clearData();
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);

  if (Bluefruit.Advertising.isScannable())
  {
    Bluefruit.ScanResponse.addName();
    Bluefruit.ScanResponse.addAppearance(BLE_APPEARANCE_GENERIC_THERMOMETER);
  }
}

struct ATTR_PACKED
{
  uint16_t uuid = 0x2A6E; // temperature characteristic
  int16_t temperature = 0; // °C
} temperatureServiceData;

struct ATTR_PACKED
{
  uint16_t uuid = 0x2A6F; // humidity characteristic
  uint16_t humidity = 0;   // %RH
} humidityServiceData;

struct ATTR_PACKED
{
  uint16_t uuid = 0x2B18; // voltage characteristic
  uint16_t voltage = 0; // V
} voltageServiceData;

void updateBLEAdvertising()
{
  Bluefruit.Advertising.clearData();
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  //Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_GENERIC_THERMOMETER);
  Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, &temperatureServiceData, sizeof(temperatureServiceData));
  Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, &voltageServiceData, sizeof(voltageServiceData));
}

void setup()
{
  digitalWrite(VBAT_ENABLE, HIGH); // disable battery voltage divider to protect pin

  sleepExternalFlash();

  analogReadResolution(10); // 0..1023
  analogReference(AR_INTERNAL); // 0.6 V ref * 6 = 3.6 V

  //uint32_t t0 = micros();
  Bluefruit.begin(); // 401 ms, 3.5 µA
  Bluefruit.setTxPower(0);
  Bluefruit.setName("XIAO");
  //uint32_t t1 = micros();
  //sd_softdevice_disable(); // 0 µs
  setupBLEAdvertising();
  //uint32_t t2 = micros();

#ifdef SERIAL_DEBUG
  // wait for serial
  delay(5000);
  Serial.begin(SERIAL_SPEED);
  while(!Serial);

  Serial.print("BLE init: ");
  Serial.print(t1 - t0);
  Serial.println(" µs");

  //Serial.print("SD disable: ");
  //Serial.print(t2 - t1);
  //Serial.println(" µs");
#endif
}

void loop()
{
  float cpuTemp = readCPUTemperature();
  float vddVoltage = 3.6f*analogReadVDD()/1023;
  if (isSoftDeviceEnabled())
  {
    delay(500);
    if (!Bluefruit.Advertising.isRunning())
    {
      delay(LED_BLINK_PERIOD/2);
      digitalWrite(LED_GREEN, LOW);
      delay(100);
      temperatureServiceData.temperature = round(cpuTemp*100);
      voltageServiceData.voltage = round(vddVoltage*100);
      updateBLEAdvertising();
      Bluefruit.Advertising.start(bleAdvertisingDuration);
      digitalWrite(LED_GREEN, HIGH);
    }
    else
    {
Serial.println("*");
    }
  }
  else
  {
    digitalToggle(LED_GREEN);
    delay(LED_BLINK_PERIOD/2);
  }

#ifdef SERIAL_DEBUG
  Serial.print("CPU: ");
  Serial.print(cpuTemp);
  Serial.println(" °C");

  float batteryVoltage = readBatteryVoltage();
  Serial.print("BAT: ");
  Serial.print(batteryVoltage);
  Serial.println(" V");

  Serial.print("VDD: ");
  Serial.print(vddVoltage);
  Serial.println(" V");
#endif
}
