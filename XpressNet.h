/*
  XpressNet.h - library for XpressNet protocoll
  Copyright (c) 2013-2024 Philipp Gahtow  All right reserved.
  for Private use only!

  Version 3.0.0 (28.10.2026)

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
  28.10.2026 Damian Celmer  - reliable transfer: drain the whole RX buffer each receive() call,
							  answer the call byte only while the poll window is still open,
							  keep a sent command until the command station accepts it and
							  resend it after "transfer error"/"busy" or a request acknowledgement,
							  bounded RX buffer (fixes overwriting the send queue), XOR/length check
							  of received messages, no duplicate delivery of the previous message,
							  broadcast 0x4X routed by type (turnout -> notifyTrnt, feedback ->
							  notifyFeedback), 0x52 activate bit read according to the spec,
							  bus timeout measured in milliseconds
  28.10.2026 Damian Celmer  - ESP32 only: AVR and ESP8266 support removed
*/

// ensure this library description is only included once
#ifndef XpressNet_h
#define XpressNet_h

#if !defined(ESP32)
#error "This XpressNet library supports only ESP32."
#endif

#include <Arduino.h>

//--------------------------------------------------------------------------------------------

// Call byte of a general broadcast (9th bit set)
#define GENERAL_BROADCAST 0x160

// Index of the fields in a received message
#define XNetlength	0		//Länge
#define XNetmsg		1		//Call Byte
#define XNetcom		2		//Header (Kennung/Befehl)
#define XNetdata1	3		//Databyte1
#define XNetdata2	4		//Databyte2

// Receive buffer: [0] = length, then call byte, header, up to 15 data bytes and the XOR byte.
#define XNetMsgSize 19

#define XSendMax 16			//Maximalanzahl der Datenpakete im Sendebuffer
#define XSendMaxData 8		//Anzahl Elemente im Datapaket Array XSend

// How often a command is resent after "transfer error"/"command station busy"
// or a request acknowledgement before it is dropped.
#ifndef XSendMaxRetry
#define XSendMaxRetry 5
#endif

// The call byte for this device is answered only if it was read at most this many
// microseconds after the receive buffer was last seen empty. A later answer would
// collide with the next call byte of the command station, so the poll is skipped
// instead and the command waits for the next one.
#ifndef XNetMaxResponseDelayUs
#define XNetMaxResponseDelayUs 60
#endif

// No byte on the bus for this long = no command station (status LED blinks).
#ifndef XNetTimeoutMs
#define XNetTimeoutMs 1000
#endif

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
	void start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl);  //Initialisierung Serial
	void receive(void);				//Prüfe ob XNet Packet vorhanden und werte es aus.

	bool getTrntInfo (byte FAdr_High, byte FAdr_Low);		//Ermitteln der Schaltstellung einer Weiche
	bool setTrntPos (byte FAdr_High, byte FAdr_Low, byte Pos);		//Schalten einer Weiche

  // library-accessible "private" interface
  private:
	  //Variables:
	boolean XNetRun;	//XpressNet ist aktiv
	byte MY_ADDRESS;	//XpressNet address: must be in range of 1-31; must be unique.
	byte MAX485_CONTROL; //Port for send or receive control
	unsigned int myDirectedOps;		// the address we look for when we are listening for ops
	unsigned int myCallByteInquiry;	// the address we look for for our Call Byte Window
	unsigned int myRequestAck;		// the address for a request acknowlegement sent
	uint8_t XNetMsg[XNetMsgSize];		//Daten wurden empfangen (geprüfte Nachricht)
	bool DataReady;				//Daten können verarbeitet werden
	uint8_t XNetMsgTemp[XNetMsgSize];	//Serial receive (Length, Message, Command, Data1 ...)
	bool ReadData;				//Empfangene Serial Daten: (Speichern = true/Komplett = false)
	XSend XNetSend[XSendMax];		//Sendbuffer

	// state of the command at the head of the send buffer
	bool XNetSendPending;		//Befehl gesendet, Ergebnis noch offen
	bool XNetSendFailed;		//Zentrale meldet Fehler/Busy -> erneut senden
	uint8_t XNetSendRetries;	//Anzahl Wiederholungen des aktuellen Befehls

	//Status LED:
	int ledState;             // ledState used to set the LED
	unsigned long lastByteMillis;	// time of the last received byte / LED toggle
	unsigned long lastIdleMicros;	// last time the receive buffer was seen empty

		//Functions:
	void XNetprocessByte(unsigned int rxdata, bool inTime);	//ein empfangenes Byte auswerten
	void XNetmessageComplete(void);	//Nachricht prüfen (Länge/XOR) und übergeben
	void XNethandleMessage(const uint8_t *msg);	//geprüfte Nachricht auswerten
	void reportAccessory(byte data1, byte data2);  //Aufbereiten der Schalt-/Rückmeldeinformation
	void getXOR (unsigned char *data, byte length); // calculate the XOR
	byte callByteParity (byte me);	// calculate the parity bit

	boolean XNetSendadd(unsigned char *dataString, byte byteCount);	//Zum Sendebuffer Hinzufügen
	void XNetSendremoveFirst(void);	//ersten Eintrag aus dem Sendebuffer entfernen
	void XNetsend(bool inTime); //Antwort auf Call Byte: Send Saved Data aus Sendebuffer
	void XNetsend(unsigned char *dataString, byte byteCount);	//Sende Daten aus Array
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
