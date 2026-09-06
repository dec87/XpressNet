/*
  XpressNet.h - library for XpressNet protocoll
  Copyright (c) 2013-2024 Philipp Gahtow  All right reserved.
  for Private use only!

  Version 2.7.2 (27.07.2024)

  Notice:
  Works until now, only with XPressNet Version 3.0 or higher!
  *********************************************************************
  21.07.2015 Philipp Gahtow - change adressing of switch commands
							- optimize memory use of setPower Function
  29.09.2015 Philipp Gahtow - fix F13 to F20 command for Multimaus
  17.11.2015 Philipp Gahtow - fix in setTrntPos for AdrL
  02.02.2017 Philipp Gahtow - add accessory change 0x52 (by Norberto Redondo Melchor)
							- fix in setTrntPos Adr convert
							- fix narrow conversations in arrays
  28.04.2017 Philipp Gahtow - optimize ram usage in callByteParity							
  23.07.2017 Philipp Gahtow - add changes for WLANMaus made by André Schenk
  20.10.2017 Philipp Gahtow - add BC for "Schaltinformation"
  18.04.2020 Philipp Gahtow - fix long Address from 100 on!
  26.08.2021 Philipp Gahtow - add E4 commands and detect all messages with seperate stack
  19.11.2021 Philipp Gahtow - add new function to set LokFunc in packets; fix E4 commands return fault loco adr and func;
  31.01.2022 Philipp Gahtow - add Support with use of Software Serial for ESP8266 and ESP32 (https://github.com/plerup/espsoftwareserial)
  27.01.2024 Philipp Gahtow - add Support Leonardo
*/

// ensure this library description is only included once
#ifndef XpressNet_h
#define XpressNet_h

// include types & constants of Wiring core API
#if defined(WIRING)
 #include <Wiring.h>
#elif ARDUINO >= 100
 #include <Arduino.h>
#else
 #include <WProgram.h>
#endif

/* From the ATMega datasheet: */
//--------------------------------------------------------------------------------------------
// Which serial port is used, if we have more than one on the chip?
// note that the 328s (the currently produced "smaller" chips) only
// have one serial port, so we force this.
//Maybe we are running on a MEGA chip with more than 1 port? If so, you
//can put the serial port to port 1, and use the port 0 for status messages
//to your PC.
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega32U4__) //Arduino MEGA, Leonardo
#define SERIAL_PORT_1
#undef SERIAL_PORT_0

#elif defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega644P__)  //Sanguino (other pins!)
#define SERIAL_PORT_1
#undef SERIAL_PORT_0

#elif defined(ESP8266) || (ESP32)		//ESP8266 and ESP32 Support
#define XNetSerial_Default	D7

#else //others Arduino UNO
#define SERIAL_PORT_0
#undef SERIAL_PORT_1
#endif

// when sending data, do NOT continue until the hardware has sent the data out

#if defined(SERIAL_PORT)
#define WAIT_FOR_XMIT_COMPLETE {while (!(UCSRA & (1<<TXC))); UCSRA = (1<<TXC); UCSRA = 0;}
#elif defined(SERIAL_PORT_0)
#define WAIT_FOR_XMIT_COMPLETE {while (!(UCSR0A & (1<<TXC0))); UCSR0A = (1<<TXC0); UCSR0A = 0;}
#elif defined(SERIAL_PORT_1)
#define WAIT_FOR_XMIT_COMPLETE {while (!(UCSR1A & (1<<TXC1))); UCSR1A = (1<<TXC1); UCSR1A = 0;}
#endif

//--------------------------------------------------------------------------------------------

// XPressnet Call Bytes.
// broadcast to everyone, we save the incoming data and process it later.
#define GENERAL_BROADCAST 0x160

//XpressNet Befehl, jedes gesendete Byte
#define XNetlength	0		//Länge
#define XNetmsg		1		//Message
#define XNetcom		2		//Kennung/Befehl
#define XNetdata1	3		//Databyte1
#define XNetdata2	4		//Databyte2
#define XNetdata3	5		//Databyte3
#define XNetdata4	6		//Databyte4
#define XNetdata5	7		//Databyte5

#define XSendMax 16			//Maximalanzahl der Datenpakete im Sendebuffer
#define XSendMaxData 8		//Anzahl Elemente im Datapaket Array XSend

typedef struct	//Antwort/Abfragespeicher
{
	uint8_t length;			//Speicher für Datenlänge
	byte data[XSendMaxData];	//zu sendende Daten
} XSend;

// library interface description
// Diese Variante unterstützt nur das Schalten von Weichen/Zubehör und das Auslesen ihrer Rückmeldeposition.
class XpressNetClass
{
  // user-accessible "public" interface
  public:
    XpressNetClass(void);	//Constuctor
	#if defined(ESP8266) || (ESP32)
	void start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl);  //Initialisierung Serial
	#else
	void start(byte XAdr, uint8_t XControl);  //Initialisierung Serial
	#endif
	void receive(void);				//Prüfe ob XNet Packet vorhanden und werte es aus.

	bool getTrntInfo (byte FAdr_High, byte FAdr_Low);		//Ermitteln der Schaltstellung einer Weiche
	bool setTrntPos (byte FAdr_High, byte FAdr_Low, byte Pos);		//Schalten einer Weiche

	// public only for easy access by interrupt handlers
	static inline void handle_interrupt();		//Serial Interrupt bearbeiten

  // library-accessible "private" interface
  private:
	  //Variables:
	boolean XNetRun;	//XpressNet ist aktiv
	byte MY_ADDRESS;	//XpressNet address: must be in range of 1-31; must be unique.
	byte MAX485_CONTROL; //Port for send or receive control
	unsigned int myDirectedOps;		// the address we look for when we are listening for ops
	unsigned int myCallByteInquiry;	// the address we look for for our Call Byte Window
	unsigned int myRequestAck;		// the address for a request acknowlegement sent
	uint8_t XNetMsg[8];		//Daten wurden empfangen
	bool DataReady;				//Daten können verarbeitet werden
	uint8_t XNetMsgTemp[8];		//Serial receive (Length, Message, Command, Data1 to Data5)
	bool ReadData;				//Empfangene Serial Daten: (Speichern = true/Komplett = false)
	static XpressNetClass *active_object;	//aktuelle aktive Object 
	void XNetget(void);			//Empfangene Daten eintragen 
	XSend XNetSend[XSendMax];		//Sendbuffer

		//Functions:
	void getXOR (unsigned char *data, byte length); // calculate the XOR
	byte callByteParity (byte me);	// calculate the parity bit
	int USART_Receive( void );	//Serial Empfangen
	void USART_Transmit (unsigned char data8); //Serial Senden
	void XNetclear(void);		//Serial Nachricht zurücksetzten

	void sendSchaltinfo(bool schaltinfo, byte data1, byte data2);  //Aufbereiten der Schaltinformation
	
	void XNetclearSendBuf();	//Sendbuffer leeren
	boolean XNetSendadd(unsigned char *dataString, byte byteCount);	//Zum Sendebuffer Hinzufügen
	void XNetsend(void); //Send Saved Data aus Sendebuffer
	void XNetsend(unsigned char *dataString, byte byteCount);	//Sende Daten aus Array

	//Status LED:
	int ledState;             // ledState used to set the LED
	long previousMillis;        // will store last time LED was updated
};

#if defined (__cplusplus)
	extern "C" {
#endif

	extern void notifyXNetStatus(uint8_t LedState ) __attribute__ ((weak));
	extern void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) __attribute__ ((weak));
	extern void notifyFeedback(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) __attribute__ ((weak));

#if defined (__cplusplus)
}
#endif


#endif

