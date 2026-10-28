/*
  XpressNet for ESP32

  Showing the funktion of the Library for a single push-button Turnout toggle

  The circuit:
 * Status LED attached from LedPin to ground, turnout LED from TurnoutLedPin to ground
 * pushbutton from ButtonPin to ground (internal pull-up)
*/

#include <XpressNet.h>
XpressNetClass XpressNet;

#define LedPin 15         //Status LED
#define TurnoutLedPin 16  //Signalisiert die letzte Weichenstellung
#define ButtonPin 10      //Taster zum Umschalten der Weiche

byte lastButtonState = HIGH;    // variable for reading the Button status
byte turnoutState = LOW;        // toggelt bei jedem Tastendruck

//--------------------------------------------------------------
// XpressNet address: must be in range of 1-31; must be unique. Note that some IDs
// are currently used by default, like 2 for a LH90 or LH100 out of the box, or 30
// for PC interface devices like the XnTCP.
#define XNetAddress 30    //Adresse im XpressNet
#define XNetRxPin 8       //RS485 RO
#define XNetTxPin 9       //RS485 DI
#define XNetSRPin 12      //RS485 DE/RE Send/Receive-PIN

#define SwitchAdr 18      //Adresse der Beispielweiche

//--------------------------------------------------------------------------------------------
void setup() {
  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, HIGH);

  pinMode(TurnoutLedPin, OUTPUT);
  digitalWrite(TurnoutLedPin, LOW);

  pinMode(ButtonPin, INPUT_PULLUP);

  XpressNet.start(XNetAddress, XNetRxPin, XNetTxPin, XNetSRPin);    //Initialisierung XNet-Bus
}

//--------------------------------------------------------------------------------------------
void loop() {
  XpressNet.receive();  //permanent update the library

  CheckButton();  //check if Button push
}

//--------------------------------------------------------------------------------------------
void CheckButton()
{
  if (digitalRead(ButtonPin) == LOW && lastButtonState == HIGH) { //down
    lastButtonState = LOW;  //save state
    turnoutState = !turnoutState;
    XpressNet.setTrntPos(0, SwitchAdr, turnoutState ? B1001 : B1000); // output active, P = turnoutState
  }
  if (digitalRead(ButtonPin) == HIGH && lastButtonState == LOW) { //up
    lastButtonState = HIGH; //save state
  }
}

//--------------------------------------------------------------------------------------------
void notifyXNetStatus (uint8_t State)
{
  digitalWrite(LedPin, State);
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos)
{
  if (Adr_Low == SwitchAdr) {
    if (Pos == B10)
      digitalWrite(TurnoutLedPin, LOW);
    if (Pos == B01)
      digitalWrite(TurnoutLedPin, HIGH);
  }
}
