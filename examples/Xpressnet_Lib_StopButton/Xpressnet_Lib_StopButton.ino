/*
  XpressNet dla ESP32

  Pokazuje przełączanie zwrotnicy jednym przyciskiem (każde wciśnięcie zmienia kierunek).

  Układ:
 * dioda stanu od LedPin do masy, dioda zwrotnicy od TurnoutLedPin do masy
 * przycisk od ButtonPin do masy (wewnętrzny pull-up)
*/

#include <XpressNet.h>
XpressNetClass XpressNet;

#define LedPin 15         //dioda stanu
#define TurnoutLedPin 16  //sygnalizuje ostatnie położenie zwrotnicy
#define ButtonPin 10      //przycisk przełączania zwrotnicy

byte lastButtonState = HIGH;    // poprzedni stan przycisku
byte turnoutState = LOW;        // zmienia się przy każdym wciśnięciu

//--------------------------------------------------------------
// Adres XpressNet: 1-31, musi być unikalny. Niektóre adresy są zajęte domyślnie,
// np. 2 przez LH90/LH100 prosto z pudełka albo 30 przez interfejsy PC, takie jak XnTCP.
#define XNetAddress 30    //adres w XpressNet
#define XNetRxPin 8       //RS485 RO
#define XNetTxPin 9       //RS485 DI
#define XNetSRPin 12      //RS485 DE/RE - przełączanie nadawanie/odbiór

#define SwitchAdr 18      //adres przykładowej zwrotnicy

//--------------------------------------------------------------------------------------------
void setup() {
  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, HIGH);

  pinMode(TurnoutLedPin, OUTPUT);
  digitalWrite(TurnoutLedPin, LOW);

  pinMode(ButtonPin, INPUT_PULLUP);

  XpressNet.start(XNetAddress, XNetRxPin, XNetTxPin, XNetSRPin);    //inicjalizacja magistrali XpressNet
}

//--------------------------------------------------------------------------------------------
void loop() {
  XpressNet.receive();  //ciągła obsługa biblioteki

  CheckButton();  //sprawdź, czy przycisk został wciśnięty
}

//--------------------------------------------------------------------------------------------
void CheckButton()
{
  if (digitalRead(ButtonPin) == LOW && lastButtonState == HIGH) { //wciśnięty
    lastButtonState = LOW;  //zapamiętaj stan
    turnoutState = !turnoutState;
    XpressNet.setTrntPos(0, SwitchAdr, turnoutState ? B1001 : B1000); // wyjście włączone, P = turnoutState
  }
  if (digitalRead(ButtonPin) == HIGH && lastButtonState == LOW) { //puszczony
    lastButtonState = HIGH; //zapamiętaj stan
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
