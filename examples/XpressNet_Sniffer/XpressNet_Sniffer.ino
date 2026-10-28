/*
  Prosty podsłuch magistrali XpressNet dla ESP32
*/

#include <XpressNet.h>

// -----------------------------------------------------------
// Ustawienia XpressNet
// -----------------------------------------------------------
// mój adres XpressNet
#define XNetAddress 30

#define XNetRxPin 8       //RS485 RO
#define XNetTxPin 9       //RS485 DI
#define XNetSRPin 12      //RS485 DE/RE - przełączanie nadawanie/odbiór

#define LedPin 15         //dioda stanu

XpressNetClass XpressNet;

void setup()
{
  // wyjście diagnostyczne
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
