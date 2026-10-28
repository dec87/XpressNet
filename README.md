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

`notifyTrnt` reports `Pos` = 1 or 2 for a known position, 0 for a turnout that was
not switched yet and 3 for an invalid state. When another device sends a
deactivate command (0x52 with C = 0), `Pos` has bit 3 set (`Pos | B1000`).

## Reliable transfer

- Call `receive()` as often as possible. It reads all waiting bytes at once.
- On ESP8266/ESP32 (Software Serial) the call byte of this device is answered only
  while the answer window of the command station is still open. When `loop()` was
  busy and the call byte is read too late, that poll is skipped (an answer would
  collide with the next call byte) and the command is sent at the next poll.
- A sent command stays in the send buffer until the command station accepts it.
  After "transfer error" (0x61 0x80), "command station busy" (0x61 0x81) or a
  request acknowledgement it is sent again, up to `XSendMaxRetry` times.
- Received messages are checked (length from the header, XOR) before they are
  passed on; too long messages are dropped instead of overwriting memory.

Tuning (define before including the library, e.g. in `build_flags`):

| Define | Default | Meaning |
|---|---|---|
| `XSendMaxRetry` | 5 | resends of one command before it is dropped |
| `XNetMaxResponseDelayUs` | 60 | max. delay (µs) for answering our call byte (Software Serial) |
| `XNetTimeoutMs` | 1000 | no bus data for this long = no command station, status LED blinks |

## Examples

- `Xpressnet_Lib` - minimal setup showing start/receive and the notify callbacks
- `Xpressnet_Lib_Weiche` - switch a turnout with two buttons and poll its position
- `Xpressnet_Lib_StopButton` - toggle a turnout with a single push-button
- `XpressNet_Lib_Lok_Speicher` - remember the last commanded turnout positions in EEPROM and restore them after a reset
- `XpressNet_Sniffer` - log turnout/feedback traffic seen on the bus

See the `examples` folder to find out how to use the library.

This is a fork of [https://github.com/Digital-MoBa/XpressNet](https://github.com/Digital-MoBa/XpressNet),
simplified, fixed and adapted to handle only turnout switching.
