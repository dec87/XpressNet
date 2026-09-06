/*
*****************************************************************************
*		XpressNet.h - library for a client XpressNet protocoll
*		Copyright (c) 2015-2022 Philipp Gahtow  All right reserved.
*
**	 Free for Private usage only!
*****************************************************************************
* IMPORTANT:
*
* 	Please contact Lenz Inc. for more details.
*****************************************************************************
*  see for changes in XpressNet.h!
*/

// include this library's description file
#include "XpressNet.h"

#if defined(__AVR__)
#include <avr/interrupt.h>
XpressNetClass *XpressNetClass::active_object = 0; // Static
#endif

#if defined(ESP8266) || defined(ESP32) // ESP8266 and ESP32 Support
#include <SoftwareSerial.h>
SoftwareSerial XNetSerial;
#endif

#define interval 10500 // interval for Status LED (milliseconds)

// Constructor /////////////////////////////////////////////////////////////////
// Function that handles the creation and setup of instances

XpressNetClass::XpressNetClass()
{
	// initialize this instance's variables
	XNetclearSendBuf();
	XNetRun = false; // XNet ist inactive;
	ReadData = false; // keine Serial Daten Speichern
}

//******************************************Serial*******************************************
#if defined(ESP8266) || defined(ESP32)
void XpressNetClass::start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl) // Initialisierung Serial
#else
void XpressNetClass::start(byte XAdr, uint8_t XControl) // Initialisierung Serial
#endif
{
	ledState = LOW;		 // Status LED, used to set the LED
	previousMillis = 0;	 // Reset Time Count
	if (notifyXNetStatus)
		notifyXNetStatus(ledState);

	MY_ADDRESS = XAdr;
	MAX485_CONTROL = XControl;
	// LISTEN_MODE
	pinMode(MAX485_CONTROL, OUTPUT);
	digitalWrite(MAX485_CONTROL, LOW);

	myRequestAck = callByteParity(MY_ADDRESS | 0x00) | 0x100;
	myCallByteInquiry = callByteParity(MY_ADDRESS | 0x40) | 0x100;
	myDirectedOps = callByteParity(MY_ADDRESS | 0x60) | 0x100;

#if defined(ESP8266) || defined(ESP32)
	XNetSerial.begin(62500, SWSERIAL_8S1, XNetSerial_RX_Port, XNetSerial_TX_Port, false, 95); // One Wire Half Duplex Serial, parity mode SPACE
	if (!XNetSerial)
	{ // If the object did not initialize, then its configuration is invalid
		Serial.println("Invalid SoftwareSerial pin configuration, check config");
		while (1)
		{ // Don't continue with invalid configuration
			delay(1000);
		}
	}
	// high speed half duplex, turn off interrupts during tx
	XNetSerial.enableIntTx(false);
#else
	// Set up on 62500 Baud
	cli(); // disable interrupts while initializing the USART
#if defined(SERIAL_PORT)
	UBRRH = 0;
	UBRRL = 0x0F;
	UCSRA = 0;
	UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE) | (1 << UCSZ2);
	UCSRC = (1 << UCSZ1) | (1 << UCSZ0);
#elif defined(SERIAL_PORT_0)
	UBRR0H = 0;
	UBRR0L = 0x0F;
	UCSR0A = 0;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0) | (1 << UCSZ02);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
#elif defined(SERIAL_PORT_1)
	UBRR1H = 0;
	UBRR1L = 0x0F;
	UCSR1A = 0;
	UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1) | (1 << UCSZ12);
	UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
#endif
	sei(); // Enable the Global Interrupt Enable flag so that interrupts can be processed
	/*
	 *  Enable reception (RXEN = 1).
	 *  Enable transmission (TXEN0 = 1).
	 *	Enable Receive Interrupt (RXCIE = 1).
	 *  Set 8-bit character mode (UCSZ00, UCSZ01, and UCSZ02 together control this,
	 *  But UCSZ00, UCSZ01 are in Register UCSR0C).
	 */

	active_object = this; // hold Object to call it back in ISR
#endif
}

// Public Methods //////////////////////////////////////////////////////////////
// Functions available in Wiring sketches, this library, and other libraries

//*******************************************************************************************
// Daten ermitteln und Auswerten
void XpressNetClass::receive(void)
{
#if defined(ESP8266) || defined(ESP32)
	XNetget();
#endif
	/*
	XNetMsg[XNetlength] = 0x00;
	XNetMsg[XNetmsg] = 0x00;
	XNetMsg[XNetcommand] = 0x00;	savedData[1]
	XNetMsg[XNetdata1] = 0x00;	savedData[2]
	*/
	if (DataReady == true)
	{ // Serial Daten dekodieren
		DataReady = false;
		//	  previousMillis = millis();   // will store last time LED was updated
		// Daten, setzte LED = ON!
		if (ledState == LOW)
		{ // LED -> aktivieren!
			ledState = HIGH;
			if (notifyXNetStatus)
				notifyXNetStatus(ledState);
		}
		if (XNetMsg[XNetmsg] == 0x60)
		{ // GENERAL_BROADCAST
			if ((XNetMsg[XNetcom] & 0xF0) == 0x40)
			{
				// R�ckmeldung (Weichen-/R�ckmelder-Status)
				byte len = (XNetMsg[XNetcom] & 0x0F) / 2; // each Adr and Data
				for (byte i = 1; i <= len; i++)
				{
					sendSchaltinfo(false, XNetMsg[XNetcom + (i * 2) - 1] /*Adr*/, XNetMsg[XNetcom + (i * 2)] /*Data*/);
				}
			}
		}
		// else if (XNetMsg[XNetmsg] == myDirectedOps && XNetMsg[XNetlength] >= 3) {
		// change by Andr� Schenk
		else if (XNetMsg[XNetlength] >= 3)
		{
			// Serial.print("RX: ");
			// Serial.print(XNetMsg[XNetcom], HEX);
			// Serial.print("-");
			// Serial.print(XNetMsg[XNetdata1], HEX);
			// Serial.print(" ");
			// Serial.print(XNetMsg[XNetdata2], HEX);
			// Serial.print(" ");
			// Serial.print(XNetMsg[XNetdata3], HEX);
			// Serial.print(" ");
			// Serial.print(XNetMsg[XNetdata4], HEX);
			// Serial.print("..");
			// Serial.println();
			switch (XNetMsg[XNetcom])
			{
			// add by Norberto Redondo Melchor:
			case 0x52: // Some other device asked for an accessory change
					   // if (XNetMsg[XNetlength] >= 3) {
					   // change by Andr� Schenk
				if (XNetMsg[XNetdata2] >= 0x80)
				{
					// Pos = 0000A00P A = turnout output (active 0/inactive 1); P = Turn v1 or --0
					byte A_bit = (XNetMsg[XNetdata2] >> 3) & B0001;
					unsigned int Adr = (XNetMsg[XNetdata1] << 2) | ((XNetMsg[XNetdata2] & B0110) >> 1); // Dir afectada
					byte Pos = (XNetMsg[XNetdata2] & B0001) + 1;
					if (!A_bit)
					{ // Accessory activation request
						if (notifyTrnt)
							notifyTrnt(highByte(Adr), lowByte(Adr), Pos);
					}
					else
					{ // Accessory deactivation request
						Pos = Pos | B1000;
						if (notifyTrnt)
							notifyTrnt(highByte(Adr), lowByte(Adr), Pos);
					}
				}
				break;
			case 0x62: // Statusantwort der Zentrale - einmaliges Anmelden am Bus
				if (XNetMsg[XNetdata1] == 0x22 && XNetMsg[XNetlength] >= 5)
				{
					if (XNetRun == false)
					{ // Softwareversion anfragen
						unsigned char commandVersionSequence[] = {0x21, 0x21, 0x00};
						XNetSendadd(commandVersionSequence, 3);
						XNetRun = true;
					}
					// in powerup mode - wait until complete
					if (XNetMsg[XNetdata2] != 0 && (XNetMsg[XNetdata2] & 0x40) == 0x40)
					{
						// put us in a state where we do the status request again...
						XNetRun = false;
					}
				}
				break;
			case 0x42: // Antwort Schaltinformation
				if (XNetMsg[XNetlength] >= 4)
				{
					sendSchaltinfo(true, XNetMsg[XNetdata1], XNetMsg[XNetdata2]);
				}
				break;
			} // switch myDirectedOps ENDE
		}
		//	if (ReadData == false)	//Nachricht komplett empfangen, dann hier l�schen!
		XNetclear(); // alte verarbeitete Nachricht l�schen
	} // Daten vorhanden ENDE
	else
	{ // keine Daten empfangen, setzte LED = Blink
		previousMillis++;
		if (previousMillis > interval)
		{					 // WARTEN
			XNetRun = false; // Keine Zentrale vorhanden
			// save the last time you blinked the LED
			previousMillis = 0;
			// if the LED is off turn it on and off (Blink):
			ledState = !ledState;
			if (notifyXNetStatus)
				notifyXNetStatus(ledState);
		}
	}
}

//--------------------------------------------------------------------------------------------
// Aufbereiten der Schaltinformation
void XpressNetClass::sendSchaltinfo(bool schaltinfo, byte data1, byte data2)
{
	int Adr = data1 * 4;
	byte nibble = bitRead(data2, 4);
	byte Pos1 = data2 & B11;
	byte Pos2 = (data2 >> 2) & B11;

	if (nibble == 1)
		Adr = Adr + 2;

	if (schaltinfo)
	{
		if (notifyTrnt)
			notifyTrnt(highByte(Adr), lowByte(Adr), Pos1);
		if (notifyTrnt)
			notifyTrnt(highByte(Adr + 1), lowByte(Adr + 1), Pos2);
	}
	else
	{
		if (notifyFeedback)
			notifyFeedback(highByte(Adr), lowByte(Adr), Pos1);
		if (notifyFeedback)
			notifyFeedback(highByte(Adr + 1), lowByte(Adr + 1), Pos2);
	}
}

//--------------------------------------------------------------------------------------------
// Ermitteln der Schaltstellung einer Weiche
bool XpressNetClass::getTrntInfo(byte FAdr_High, byte FAdr_Low)
{
	int Adr = word(FAdr_High, FAdr_Low);
	byte nibble = 0; // 0 = Weiche 0 und 1; 1 = Weiche 2 und 3
	if ((Adr & 0x03) >= 2)
		nibble = 1;
	unsigned char getTrntPos[] = {0x42, 0x00, 0x80, 0x00};
	getTrntPos[1] = Adr >> 2;
	getTrntPos[2] += nibble;
	getXOR(getTrntPos, 4);
	return XNetSendadd(getTrntPos, 4);
}

//--------------------------------------------------------------------------------------------
// Schalten einer Weiche
bool XpressNetClass::setTrntPos(byte FAdr_High, byte FAdr_Low, byte Pos)
// Pos = 0000A00P	A=Weichenausgang (Aktive/Inaktive); P=Weiche nach links oder nach rechts
{
	int Adr = word(FAdr_High, FAdr_Low);
	byte AdrL = ((Pos & 0x0F) | B110) & (((Adr & 0x03) << 1) | B1001); // 1000ABBP -> A00P = Pos | BB = Adr & 0x03 (LSB Weichenadr.)

	Adr = Adr >> 2;
	bitWrite(AdrL, 7, 1);
	unsigned char setTrnt[] = {0x52, 0x00, AdrL, 0x00}; // old: 0x52, Adr, AdrL, 0x00
	// setTrnt[1] =  (Adr >> 2) & 0xFF;
	// change by Andr� Schenk:
	setTrnt[1] = Adr;
	getXOR(setTrnt, 4);

	// getTrntInfo(FAdr_High, FAdr_Low);  //Schaltstellung abfragen
	if (notifyTrnt)
		notifyTrnt(FAdr_High, FAdr_Low, (Pos & B1) + 1);

	return XNetSendadd(setTrnt, 4);
}

// Private Methods ///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions only available to other functions in this library *******************************************************

//--------------------------------------------------------------------------------------------
// calculate the XOR
void XpressNetClass::getXOR(unsigned char *data, byte length)
{
	byte XOR = 0x00;
	for (int i = 0; i < (length - 1); i++)
	{
		XOR = XOR ^ *data;
		data++;
	}
	*data = XOR;
}

//--------------------------------------------------------------------------------------------
// calculate the parity bit in the call byte for this guy
byte XpressNetClass::callByteParity(byte me)
{
	byte parity = (1 == 0);
	byte vv;
	me &= 0x7f;
	vv = me;

	while (vv)
	{
		parity = !parity;
		vv &= (vv - 1);
	}
	if (parity)
		me |= 0x80;
	return me;
}

//--------------------------------------------------------------------------------------------
int XpressNetClass::USART_Receive(void)
{
#if defined(ESP8266) || defined(ESP32)
	if (XNetSerial.available())
	{ // If anything comes in Serial
		uint16_t data = XNetSerial.read();
		if (XNetSerial.readParity())
		{ // detect parity bit set on the last message (MARK parity)
			data = data | 0x100;
		}
		return data;
	}
	else
		return -1;
#else
	unsigned char status, resh, resl;
// Wait for data to be received
#if defined(SERIAL_PORT)
	status = UCSRA;
	while (!(status & (1 << RXC)))
	{
		return -1;
	} // status = UCSRA;}

	// Get status and 9th bit, then data
	resh = UCSRB;
	resl = UDR;

	// If error, return -1
	if (status & ((1 << FE) | (1 << DOR) | (1 << PE)))
	{
		return -1;
	}

#elif defined(SERIAL_PORT_0)
	status = UCSR0A;
	while (!(status & (1 << RXC0)))
	{
		return -1;
	} // status = UCSR0A;}

	// Get status and 9th bit, then data
	resh = UCSR0B;
	resl = UDR0;

	// If error, return -1
	if (status & ((1 << FE0) | (1 << DOR0) | (1 << UPE0)))
	{
		return -1;
	}

#elif defined(SERIAL_PORT_1)
	status = UCSR1A;
	while (!(status & (1 << RXC1)))
	{
		return -1;
	} // status = UCSR1A;}

	// Get status and 9th bit, then data
	resh = UCSR1B;
	resl = UDR1;

	// If error, return -1
	if (status & ((1 << FE1) | (1 << DOR1) | (1 << UPE1)))
	{
		return -1;
	}
#endif

	// Filter the 9th bit, then return
	resh = (resh >> 1) & 0x01;
	return ((resh << 8) | resl);
#endif
}

//--------------------------------------------------------------------------------------------
void XpressNetClass::USART_Transmit(unsigned char data8)
{
#if defined(ESP8266) || defined(ESP32)
	XNetSerial.write(data8); // we didn't want to MARK = SWSERIAL_PARITY_MARK
#else
// wait for empty transmit buffer
#if defined(SERIAL_PORT)
	while (!(UCSRA & (1 << UDRE)))
	{
	}
	// put the data into buffer, and send
	UDR = data8;
#elif defined(SERIAL_PORT_0)
	while (!(UCSR0A & (1 << UDRE0)))
	{
	}
	// put the data into buffer, and send
	UDR0 = data8;
#elif defined(SERIAL_PORT_1)
	while (!(UCSR1A & (1 << UDRE1)))
	{
	}
	// put the data into buffer, and send
	UDR1 = data8;
#endif
#endif
}

//--------------------------------------------------------------------------------------------
// L�schen des letzten gesendeten Befehls
void XpressNetClass::XNetclear()
{
	XNetMsg[XNetlength] = 0x00;
	XNetMsg[XNetmsg] = 0x00;
	XNetMsg[XNetcom] = 0x00;
	XNetMsg[XNetdata1] = 0x00;
	XNetMsg[XNetdata2] = 0x00;
	XNetMsg[XNetdata3] = 0x00;
	XNetMsg[XNetdata4] = 0x00;
	XNetMsg[XNetdata5] = 0x00;
}

#if defined(__AVR__)
//--------------------------------------------------------------------------------------------
// Interrupt routine for reading via Serial
#if defined(SERIAL_PORT)
ISR(USART_RX_vect)
{
	XpressNetClass::handle_interrupt(); // weiterreichen an die Funktion
}

#elif defined(SERIAL_PORT_0)
ISR(USART_RX_vect)
{
	XpressNetClass::handle_interrupt(); // weiterreichen an die Funktion
}

#elif defined(SERIAL_PORT_1)
ISR(USART1_RX_vect)
{
	XpressNetClass::handle_interrupt(); // weiterreichen an die Funktion
}
#endif

// Interrupt handling
/* static */
inline void XpressNetClass::handle_interrupt()
{
	if (active_object)
	{
		active_object->XNetget(); // Daten Einlesen und Speichern
	}
}
#endif

//--------------------------------------------------------------------------------------------
// Serial einlesen:
void XpressNetClass::XNetget()
{
	unsigned int rxdata = USART_Receive();

	if ((int)rxdata != -1)
	{						// Daten wurden korrekt empfangen?
		previousMillis = 0; // Reset Time Count
		// This IS a Call Byte
		if (rxdata >= 0x100)
		{					  // Neue Nachricht beginnen
			ReadData = false; // keine Speichern der Serial Daten
			if (XNetMsgTemp[XNetlength] > 1)
			{
				// kopieren der Nachricht!
				XNetMsg[XNetlength] = XNetMsgTemp[XNetlength];
				XNetMsg[XNetmsg] = XNetMsgTemp[XNetmsg];
				XNetMsg[XNetcom] = XNetMsgTemp[XNetcom];
				XNetMsg[XNetdata1] = XNetMsgTemp[XNetdata1];
				XNetMsg[XNetdata2] = XNetMsgTemp[XNetdata2];
				XNetMsg[XNetdata3] = XNetMsgTemp[XNetdata3];
				XNetMsg[XNetdata4] = XNetMsgTemp[XNetdata4];
				XNetMsg[XNetdata5] = XNetMsgTemp[XNetdata5];
				DataReady = true;
			}
			if (rxdata == myRequestAck)
			{
				unsigned char requestAckAck[] = {0x20, 0x20};
				XNetsend(requestAckAck, 2);
				return; // Daten wurden verarbeitet
			}
			else if (rxdata == myCallByteInquiry)
			{
				unsigned char commandStatusSequence[] = {0x21, 0x24, 0x05};
				if (XNetRun == false)
				{
					XNetsend(commandStatusSequence, 3);
				}
				else
					XNetsend();
				return; // Daten wurden verarbeitet
			}
			else /*if (rxdata == GENERAL_BROADCAST || rxdata == myDirectedOps) */
			{	 // Datenempfang aktivieren
				ReadData = true;
				XNetMsgTemp[XNetlength] = 0; // alte Nachricht l�schen
			}
		}
		if (ReadData == true)
		{														  // Data is for our own address
			XNetMsgTemp[XNetlength]++;							  // Let's make room for it...
			XNetMsgTemp[XNetMsgTemp[XNetlength]] = rxdata & 0xFF; //...and store it
		}
	}
}

//--------------------------------------------------------------------------------------------
void XpressNetClass::XNetclearSendBuf() // Buffer leeren
{
	for (byte i = 0; i < XSendMax; i++)
	{
		XNetSend[i].length = 0x00; // L�nge zur�cksetzten
		for (byte j = 0; j < XSendMaxData; j++)
		{
			XNetSend[i].data[j] = 0x00; // Daten l�schen
		}
	}
}

//--------------------------------------------------------------------------------------------
boolean XpressNetClass::XNetSendadd(unsigned char *dataString, byte byteCount)
{
	for (byte i = 0; i < XSendMax; i++)
	{
		if (XNetSend[i].length == 0)
		{									// Daten hier Eintragen:
			XNetSend[i].length = byteCount; // Datenlaenge
			for (byte b = 0; b < byteCount; b++)
			{
				XNetSend[i].data[b] = *dataString;
				dataString++;
			}
			return true; // leeren Platz gefunden -> ENDE
		}
	}
	return false; // Kein Platz im Sendbuffer frei!
}

//--------------------------------------------------------------------------------------------
// Byte via Serial senden
void XpressNetClass::XNetsend(void)
{
	if (XNetSend[0].length != 0)
	{ // && XNetSend[0].length < XSendMaxData) {
		if (XNetSend[0].data[0] != 0)
			XNetsend(XNetSend[0].data, XNetSend[0].length);
		for (int i = 0; i < (XSendMax - 1); i++)
		{
			XNetSend[i].length = XNetSend[i + 1].length;
			for (int j = 0; j < XSendMaxData; j++)
			{
				XNetSend[i].data[j] = XNetSend[i + 1].data[j]; // Daten kopieren
			}
		}
		// letzten Leeren
		XNetSend[XSendMax - 1].length = 0x00;
		for (int j = 0; j < XSendMaxData; j++)
		{
			XNetSend[XSendMax - 1].data[j] = 0x00; // Daten l�schen
		}
	}
	else
		XNetSend[0].length = 0;
}

//--------------------------------------------------------------------------------------------
// send along a bunch of bytes to the Command Station
void XpressNetClass::XNetsend(unsigned char *dataString, byte byteCount)
{
	unsigned int i;
	digitalWrite(MAX485_CONTROL, HIGH);
#if defined(ESP8266) || defined(ESP32)
	XNetSerial.enableTx(true);
#endif

	for (i = 0; i < byteCount; i++)
	{
		USART_Transmit(*dataString);
		dataString++;
	}

#if defined(__AVR__)
	WAIT_FOR_XMIT_COMPLETE;
#else
	delayMicroseconds(10);
#endif
	digitalWrite(MAX485_CONTROL, LOW);
#if defined(ESP8266) || defined(ESP32)
	XNetSerial.enableTx(false);
#endif
}