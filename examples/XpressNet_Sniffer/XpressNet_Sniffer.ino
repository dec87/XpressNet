/*
  Simple XpressNet sniffer for ESP32
*/

#include <XpressNet.h>

// -----------------------------------------------------------
// XpressNet settings
// -----------------------------------------------------------
// My XpressNet address
#define XNetAddress 30

#define XNetRxPin 8       //RS485 RO
#define XNetTxPin 9       //RS485 DI
#define XNetSRPin 12      //RS485 DE/RE send/receive pin

#define LedPin 15         //Status LED

XpressNetClass XpressNet;

void setup()
{
  // debug output
  Serial.begin(115200);
  Serial.println();
  Serial.println("XpressNet Sniffer");
  pinMode(LedPin, OUTPUT);

  XpressNet.start(XNetAddress, XNetRxPin, XNetTxPin, XNetSRPin);
}

void loop()
{
  XpressNet.receive();
}

void notifyXNetStatus (uint8_t State)
{
  digitalWrite(LedPin, State);
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  Serial.print("Turnout: ");
  Serial.print(word(Adr_High, Adr_Low));
  Serial.print(", Position: ");
  Serial.println(Pos, BIN);
}

//--------------------------------------------------------------------------------------------
void notifyFeedback(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  Serial.print("Feedback: ");
  Serial.print(word(Adr_High, Adr_Low));
  Serial.print(", Position: ");
  Serial.println(Pos, BIN);
}
