/*
  XpressNet dla ESP32

  Pokazuje działanie biblioteki: start/receive, funkcje zwrotne notify
  oraz okresowe zapytanie o położenie jednej zwrotnicy.
*/

#include <XpressNet.h>
XpressNetClass XpressNet;

//--------------------------------------------------------------
// Adres XpressNet: 1-31, musi być unikalny. Niektóre adresy są zajęte domyślnie,
// np. 2 przez LH90/LH100 prosto z pudełka albo 30 przez interfejsy PC, takie jak XnTCP.
#define XNetAddress 30    //adres w XpressNet
#define XNetRxPin 8       //RS485 RO
#define XNetTxPin 9       //RS485 DI
#define XNetSRPin 12      //RS485 DE/RE - przełączanie nadawanie/odbiór

#define LedPin 15         //dioda stanu

#define DemoTurnoutAdr 1  //adres przykładowej zwrotnicy
#define interval 8500

unsigned long previousMillisA = 0;

void setup() {
  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, HIGH);

  Serial.begin(115200);
  Serial.println("XNet Test");

  XpressNet.start(XNetAddress, XNetRxPin, XNetTxPin, XNetSRPin);    //inicjalizacja magistrali XpressNet
}

void loop() {
  XpressNet.receive();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisA > interval) {
    previousMillisA = currentMillis;
    // zapytaj o aktualne położenie przykładowej zwrotnicy
    XpressNet.getTrntInfo(0, DemoTurnoutAdr);
  }
}

//--------------------------------------------------------------------------------------------
void notifyXNetStatus (uint8_t State)
{
  digitalWrite(LedPin, State);
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  Serial.print("Weiche: ");
  Serial.print(word(Adr_High, Adr_Low));
  Serial.print(", Position: ");
  Serial.println(Pos, BIN);
}

//--------------------------------------------------------------------------------------------
void notifyFeedback(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  Serial.print("Rueckmelder: ");
  Serial.print(word(Adr_High, Adr_Low));
  Serial.print(", Position: ");
  Serial.println(Pos, BIN);
}
