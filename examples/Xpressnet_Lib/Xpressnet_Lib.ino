/*
  XpressNet for ESP32

  Showing the funktion of the Library: start/receive, the notify callbacks
  and a periodic position request of one turnout.
*/

#include <XpressNet.h>
XpressNetClass XpressNet;

//--------------------------------------------------------------
// XpressNet address: must be in range of 1-31; must be unique. Note that some IDs
// are currently used by default, like 2 for a LH90 or LH100 out of the box, or 30
// for PC interface devices like the XnTCP.
#define XNetAddress 30    //Adresse im XpressNet
#define XNetRxPin 8       //RS485 RO
#define XNetTxPin 9       //RS485 DI
#define XNetSRPin 12      //RS485 DE/RE Send/Receive-PIN

#define LedPin 15         //Status LED

#define DemoTurnoutAdr 1  //Adresse der Beispielweiche
#define interval 8500

unsigned long previousMillisA = 0;

void setup() {
  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, HIGH);

  Serial.begin(115200);
  Serial.println("XNet Test");

  XpressNet.start(XNetAddress, XNetRxPin, XNetTxPin, XNetSRPin);    //Initialisierung XNet-Bus
}

void loop() {
  XpressNet.receive();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisA > interval) {
    previousMillisA = currentMillis;
    // Aktuelle Schaltstellung der Beispielweiche abfragen:
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
