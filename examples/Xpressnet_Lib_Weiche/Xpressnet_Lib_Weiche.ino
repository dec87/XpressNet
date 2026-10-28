/*
  XpressNet dla ESP32

  Pokazuje sterowanie zwrotnicą dwoma przyciskami.
  Układ:
 * dioda stanu z rezystorem od LedPin do masy
 * dioda z rezystorem od SwitchLedPin do masy (świeci = zwrotnica na wprost)
 * przyciski od ButtonSPin / ButtonTurnPin do masy (wewnętrzny pull-up)
*/

#include <XpressNet.h>
XpressNetClass XpressNet;

#define LedPin 15         //dioda stanu
#define SwitchLedPin 16   //sygnalizuje położenie zwrotnicy
#define ButtonSPin 10     //przycisk: zwrotnica na wprost
#define ButtonTurnPin 11  //przycisk: zwrotnica na bok

byte lastButtonSState = HIGH;    // poprzedni stan przycisku
byte lastButtonTurnState = HIGH;    // poprzedni stan przycisku

unsigned long previousMillis = 0;        // czas ostatniego zapytania o położenie
const long interval = 1000;           // odstęp między zapytaniami (ms)
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

  pinMode(SwitchLedPin, OUTPUT);
  digitalWrite(SwitchLedPin, LOW);

  pinMode(ButtonSPin, INPUT_PULLUP);
  pinMode(ButtonTurnPin, INPUT_PULLUP);

  XpressNet.start(XNetAddress, XNetRxPin, XNetTxPin, XNetSRPin);    //inicjalizacja magistrali XpressNet
}

//--------------------------------------------------------------------------------------------
void loop() {
  XpressNet.receive();  //ciągła obsługa biblioteki

  CheckButton();  //sprawdź, czy przycisk został wciśnięty

  //zapytaj o położenie zwrotnicy
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    XpressNet.getTrntInfo(0, SwitchAdr);
  }
}

//--------------------------------------------------------------------------------------------
void CheckButton()
{
  //na wprost
  if (digitalRead(ButtonSPin) == LOW && lastButtonSState == HIGH) { //wciśnięty
    lastButtonSState = LOW;  //zapamiętaj stan
    digitalWrite(SwitchLedPin, HIGH);

    XpressNet.setTrntPos(0, SwitchAdr, B1000); //zwrotnica 18 na wprost (wyjście włączone, P = 0)
  }
  if (digitalRead(ButtonSPin) == HIGH && lastButtonSState == LOW) { //puszczony
    lastButtonSState = HIGH; //zapamiętaj stan
  }
  //na bok
  if (digitalRead(ButtonTurnPin) == LOW && lastButtonTurnState == HIGH) { //wciśnięty
    lastButtonTurnState = LOW;  //zapamiętaj stan
    digitalWrite(SwitchLedPin, LOW);

    XpressNet.setTrntPos(0, SwitchAdr, B1001); //zwrotnica 18 na bok (wyjście włączone, P = 1)
  }
  if (digitalRead(ButtonTurnPin) == HIGH && lastButtonTurnState == LOW) { //puszczony
    lastButtonTurnState = HIGH; //zapamiętaj stan
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
