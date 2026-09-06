/*
  XpressNet for Arduino
  
  Showing the funktion of the Library
  
  XpressNet use on Arduino Mega Serial 1; on Arduino UNO Serial0
  
*/

//----------------------------------------------------------------------------
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  #define SerialDEBUG 1  //For Serial Port Debugging (Arduino Mega only)
#endif  

//----------------------------------------------------------------------------

#include <XpressNet.h> 
XpressNetClass XpressNet;

//Debug Ausgabe
#ifdef __AVR_ATmega8__
#include <SoftwareSerial.h>
SoftwareSerial Debug(5, 6); // RX, TX
#else
#define Debug Serial
#endif

// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int led = 13;

//--------------------------------------------------------------
// XpressNet address: must be in range of 1-31; must be unique. Note that some IDs
// are currently used by default, like 2 for a LH90 or LH100 out of the box, or 30
// for PC interface devices like the XnTCP.
#define XNetAddress 30    //Adresse im XpressNet
#define XNetSRPin 9       //Max485 Busdriver Send/Receive-PIN

long previousMillisA = 0;        // will store last time LED was updated
#define interval 8500 

#define DemoTurnoutAdr 1  //Adresse der Beispielweiche

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);
 digitalWrite(led, HIGH); 
 delay(1000);
  
  #if SerialDEBUG
  Debug.begin(115200);
  Debug.println("XNet Test");
  #endif
 
  XpressNet.start(XNetAddress, XNetSRPin);    //Initialisierung XNet-Bus

}

// the loop routine runs over and over again forever:
void loop() {
  
  XpressNet.receive();
  
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillisA > interval) {
    previousMillisA = currentMillis;
    // Aktuelle Schaltstellung der Beispielweiche abfragen:
    XpressNet.getTrntInfo(0, DemoTurnoutAdr);
  }
  
}

void notifyXNetDebug(String s) 
{
  #if SerialDEBUG
  Debug.println(s);
  #endif
}

//--------------------------------------------------------------------------------------------
void notifyXNetStatus (uint8_t State)
{
  digitalWrite(led, State);
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  #if SerialDEBUG
  Debug.print("Weiche: "); 
  Debug.print(word(Adr_High, Adr_Low)); 
  Debug.print(", Position: "); 
  Debug.println(Pos, BIN); 
  #endif
}

//--------------------------------------------------------------------------------------------
void notifyFeedback(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  #if SerialDEBUG
  Debug.print("Rueckmelder: "); 
  Debug.print(word(Adr_High, Adr_Low)); 
  Debug.print(", Position: "); 
  Debug.println(Pos, BIN); 
  #endif
}
