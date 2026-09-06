/*


  Simple XpressNet sniffer for Arduino MEGA or ESP8266/ESP32


*/

#include <XpressNet.h>

// -----------------------------------------------------------
// XpressNet settings
// -----------------------------------------------------------
// My XpressNet address
#define XNetAddress 30

#if defined(ESP8266) || defined(ESP32)
#define XNetPort D7			//Serial TX/RX 
#define XNet_TXRX D8		//send/receive pin
#else
#define XNet_TXRX 9		//send/receive pin	
#endif

XpressNetClass XpressNet;

void setup()
{
  // debug output
  Serial.begin(115200);
  Serial.println();
  Serial.println("XpressNet Sniffer");
  pinMode(LED_BUILTIN, OUTPUT);

  #if defined(ESP8266) || defined(ESP32)
	XpressNet.start(XNetAddress, XNetPort, XNet_TXRX);
  #else
	XpressNet.start(XNetAddress, XNet_TXRX);
  #endif

}

void loop()
{
  XpressNet.receive();
}

void notifyXNetStatus (uint8_t State)
{
  digitalWrite(LED_BUILTIN, State);
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
