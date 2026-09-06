# XpressNet
Arduino XpressNet Slave Library

Works with AVR Arduino UNO/NANO on Hardware Serial and on Arduino MEGA on Serial1.
Support ESP8266 and ESP32 with Software Serial.

## Scope

This library implements a minimal XpressNet client focused only on **accessory
decoders (turnouts/Weichen)**: switching a turnout and reading back its position
(including position broadcasts from feedback/Rückmeldemodule). Locomotive
control, CV programming and track power control are **not** supported.

## API

```cpp
XpressNetClass XpressNet;

// Arduino UNO/MEGA (Hardware Serial):
XpressNet.start(byte XAdr, uint8_t XControl);
// ESP8266/ESP32 (Software Serial):
XpressNet.start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl);

XpressNet.receive();                                       // call this in loop()

XpressNet.getTrntInfo(byte FAdr_High, byte FAdr_Low);       // request turnout position
XpressNet.setTrntPos(byte FAdr_High, byte FAdr_Low, byte Pos); // switch a turnout
```

Implement these weak callbacks in your sketch to receive events:

```cpp
void notifyXNetStatus(uint8_t State);                                 // bus/connection status LED
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos);       // turnout position changed
void notifyFeedback(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos);   // feedback module report
```

## Examples

- `Xpressnet_Lib` - minimal setup showing start/receive and the notify callbacks
- `Xpressnet_Lib_Weiche` - switch a turnout with two buttons and poll its position
- `Xpressnet_Lib_StopButton` - toggle a turnout with a single push-button
- `XpressNet_Lib_Lok_Speicher` - remember the last commanded turnout positions in EEPROM and restore them after a reset
- `XpressNet_Sniffer` - log turnout/feedback traffic seen on the bus

See the `examples` folder to find out how to use the library.

This is a fork of [https://github.com/Digital-MoBa/XpressNet](https://github.com/Digital-MoBa/XpressNet),
simplified, fixed and adapted to handle only turnout switching.
