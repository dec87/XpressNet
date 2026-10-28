/*
  XpressNet for ESP32

  Showing the funktion of the Library for a Switch control
  The circuit:
 * Status LED and resistor attached from LedPin to ground
 * LED and resistor attached from SwitchLedPin to ground (switch straight)
 * pushbuttons from ButtonSPin / ButtonTurnPin to ground (internal pull-up)
*/

#include <XpressNet.h>
XpressNetClass XpressNet;

#define LedPin 15         //Status LED
#define SwitchLedPin 16   //Signalisiert den Weichenzustand
#define ButtonSPin 10     //Taster Weiche gerade
#define ButtonTurnPin 11  //Taster Weiche abzweigend

byte lastButtonSState = HIGH;    // variable for reading the Button status
byte lastButtonTurnState = HIGH;    // variable for reading the Button status

unsigned long previousMillis = 0;        // will store last time the position was requested
const long interval = 1000;           // interval at which to request (milliseconds)
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

  pinMode(SwitchLedPin, OUTPUT);
  digitalWrite(SwitchLedPin, LOW);

  pinMode(ButtonSPin, INPUT_PULLUP);
  pinMode(ButtonTurnPin, INPUT_PULLUP);

  XpressNet.start(XNetAddress, XNetRxPin, XNetTxPin, XNetSRPin);    //Initialisierung XNet-Bus
}

//--------------------------------------------------------------------------------------------
void loop() {
  XpressNet.receive();  //permanent update the library

  CheckButton();  //check if Button push

  //Ask for switch state
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    XpressNet.getTrntInfo(0, SwitchAdr);
  }
}

//--------------------------------------------------------------------------------------------
void CheckButton()
{
  //Gerade
  if (digitalRead(ButtonSPin) == LOW && lastButtonSState == HIGH) { //down
    lastButtonSState = LOW;  //save state
    digitalWrite(SwitchLedPin, HIGH);

    XpressNet.setTrntPos(0, SwitchAdr, B1000); //switch 18 gerade (output active, P = 0)
  }
  if (digitalRead(ButtonSPin) == HIGH && lastButtonSState == LOW) { //up
    lastButtonSState = HIGH; //save state
  }
  //Abzweigen
  if (digitalRead(ButtonTurnPin) == LOW && lastButtonTurnState == HIGH) { //down
    lastButtonTurnState = LOW;  //save state
    digitalWrite(SwitchLedPin, LOW);

    XpressNet.setTrntPos(0, SwitchAdr, B1001); //switch 18 turn (output active, P = 1)
  }
  if (digitalRead(ButtonTurnPin) == HIGH && lastButtonTurnState == LOW) { //up
    lastButtonTurnState = HIGH; //save state
  }
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos)
{
  if (Adr_Low == SwitchAdr) {
    if (Pos == B10)
      digitalWrite(SwitchLedPin, LOW);
    if (Pos == B01)
      digitalWrite(SwitchLedPin, HIGH);
  }
}

//--------------------------------------------------------------------------------------------
void notifyXNetStatus (uint8_t State)
{
  digitalWrite(LedPin, State);
}
