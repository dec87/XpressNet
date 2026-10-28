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
// On AVR the receive side runs in the USART interrupt: guard data shared with loop().
#define XNET_LOCK()   \
	uint8_t xnetSREG = SREG; \
	cli()
#define XNET_UNLOCK() SREG = xnetSREG
#else
// On ESP8266/ESP32 everything runs from receive() in loop(): nothing to guard.
#define XNET_LOCK()
#define XNET_UNLOCK()
#endif

#if defined(ESP8266) || defined(ESP32) // ESP8266 and ESP32 Support
#include <SoftwareSerial.h>
SoftwareSerial XNetSerial;
#endif

// Constructor /////////////////////////////////////////////////////////////////
// Function that handles the creation and setup of instances

XpressNetClass::XpressNetClass()
{
	// initialize this instance's variables
	XNetclearSendBuf();
	XNetclear();
	memset(XNetMsgTemp, 0, sizeof(XNetMsgTemp));
	XNetRun = false; // XNet ist inactive;
	ReadData = false; // keine Serial Daten Speichern
	DataReady = false;
	XNetSendPending = false;
	XNetSendFailed = false;
	XNetSendRetries = 0;
	ledState = LOW;
	lastByteMillis = 0;
	lastIdleMicros = 0;
}

//******************************************Serial*******************************************
#if defined(ESP8266) || defined(ESP32)
void XpressNetClass::start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl) // Initialisierung Serial
#else
void XpressNetClass::start(byte XAdr, uint8_t XControl) // Initialisierung Serial
#endif
{
	ledState = LOW;		 // Status LED, used to set the LED
	lastByteMillis = millis();
	lastIdleMicros = micros();
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
// Daten ermitteln und Auswerten - call as often as possible from loop()
void XpressNetClass::receive(void)
{
#if defined(ESP8266) || defined(ESP32)
	// Read everything that is waiting, not just one byte: the call byte for this
	// device must be answered right away, not after older bytes in the buffer.
	while (true)
	{
		if (!XNetSerial.available())
		{
			lastIdleMicros = micros();
			break;
		}
		unsigned int rxdata = XNetSerial.read();
		if (XNetSerial.readParity())
		{ // detect parity bit set on the last message (MARK parity) = Call Byte
			rxdata |= 0x100;
		}
		// The byte completed some time after the buffer was last seen empty.
		// Only if that was very recent and nothing else arrived after it is the
		// answer window of the command station still open.
		bool inTime = (XNetSerial.available() == 0) &&
					  ((unsigned long)(micros() - lastIdleMicros) <= XNetMaxResponseDelayUs);
		XNetprocessByte(rxdata, inTime);
		if (DataReady)
		{
			DataReady = false;
			XNethandleMessage(XNetMsg);
		}
	}
#else
	if (DataReady)
	{ // copy the message, the interrupt may already receive the next one
		uint8_t msg[XNetMsgSize];
		{
			XNET_LOCK();
			memcpy(msg, XNetMsg, XNetMsgSize);
			DataReady = false;
			XNET_UNLOCK();
		}
		XNethandleMessage(msg);
	}
#endif

	// no data on the bus: blink the status LED
	unsigned long lastByte;
	{
		XNET_LOCK();
		lastByte = lastByteMillis;
		XNET_UNLOCK();
	}
	if ((unsigned long)(millis() - lastByte) > XNetTimeoutMs)
	{
		XNetRun = false; // Keine Zentrale vorhanden
		{
			XNET_LOCK();
			lastByteMillis = millis();
			XNET_UNLOCK();
		}
		ledState = !ledState;
		if (notifyXNetStatus)
			notifyXNetStatus(ledState);
	}
}

//--------------------------------------------------------------------------------------------
// Auswerten einer vollständigen und geprüften Nachricht
void XpressNetClass::XNethandleMessage(const uint8_t *msg)
{
	// Daten, setzte LED = ON!
	if (ledState == LOW)
	{
		ledState = HIGH;
		if (notifyXNetStatus)
			notifyXNetStatus(ledState);
	}

	byte callByte = msg[XNetmsg];
	byte header = msg[XNetcom];

	if (callByte == (GENERAL_BROADCAST & 0xFF))
	{ // GENERAL_BROADCAST
		if ((header & 0xF0) == 0x40)
		{
			// Feedback broadcast: pairs of address + data (turnouts and feedback modules)
			byte pairs = (header & 0x0F) / 2;
			for (byte i = 0; i < pairs; i++)
			{
				reportAccessory(msg[XNetdata1 + (i * 2)], msg[XNetdata2 + (i * 2)]);
			}
		}
		return;
	}

	// Directed messages - also those for other devices (change by André Schenk)
	switch (header)
	{
	// add by Norberto Redondo Melchor:
	case 0x52: // Some other device asked for an accessory change: 1000CBBD
		if ((msg[XNetdata2] & 0xF0) == 0x80)
		{
			byte data2 = msg[XNetdata2];
			unsigned int Adr = (msg[XNetdata1] << 2) | ((data2 & B0110) >> 1);
			byte Pos = (data2 & B0001) + 1;
			// C (bit 3) = 1: activate output, C = 0: deactivate output (reported as Pos | B1000)
			if (!(data2 & B1000))
				Pos = Pos | B1000;
			if (notifyTrnt)
				notifyTrnt(highByte(Adr), lowByte(Adr), Pos);
		}
		break;
	case 0x62: // Statusantwort der Zentrale - einmaliges Anmelden am Bus
		if (msg[XNetdata1] == 0x22)
		{
			if (XNetRun == false)
			{ // Softwareversion anfragen
				unsigned char commandVersionSequence[] = {0x21, 0x21, 0x00};
				XNetSendadd(commandVersionSequence, 3);
				XNetRun = true;
			}
			// in powerup mode - wait until complete
			if (msg[XNetdata2] != 0 && (msg[XNetdata2] & 0x40) == 0x40)
			{
				// put us in a state where we do the status request again...
				XNetRun = false;
			}
		}
		break;
	case 0x42: // Antwort Schaltinformation
		reportAccessory(msg[XNetdata1], msg[XNetdata2]);
		break;
	}
}

//--------------------------------------------------------------------------------------------
// Aufbereiten der Schalt-/Rückmeldeinformation: data2 = ITTNZZZZ
void XpressNetClass::reportAccessory(byte data1, byte data2)
{
	byte type = (data2 >> 5) & 0x03; // 00/01 = accessory decoder, 10 = feedback module, 11 = reserved
	if (type == 0x03)
		return;

	int Adr = data1 * 4;
	byte nibble = bitRead(data2, 4);
	byte Pos1 = data2 & B11;
	byte Pos2 = (data2 >> 2) & B11;

	if (nibble == 1)
		Adr = Adr + 2;

	if (type == 0x02)
	{
		if (notifyFeedback)
		{
			notifyFeedback(highByte(Adr), lowByte(Adr), Pos1);
			notifyFeedback(highByte(Adr + 1), lowByte(Adr + 1), Pos2);
		}
	}
	else
	{
		if (notifyTrnt)
		{
			notifyTrnt(highByte(Adr), lowByte(Adr), Pos1);
			notifyTrnt(highByte(Adr + 1), lowByte(Adr + 1), Pos2);
		}
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
	// change by André Schenk:
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
// Löschen der letzten empfangenen Nachricht
void XpressNetClass::XNetclear()
{
	memset(XNetMsg, 0, sizeof(XNetMsg));
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
// Serial einlesen (AVR: called from the receive interrupt, always in time)
void XpressNetClass::XNetget()
{
	int rxdata = USART_Receive();
	if (rxdata != -1)
		XNetprocessByte((unsigned int)rxdata, true);
}

//--------------------------------------------------------------------------------------------
// Ein empfangenes Byte auswerten.
// inTime = false: the call byte was read too late to answer it without a collision.
void XpressNetClass::XNetprocessByte(unsigned int rxdata, bool inTime)
{
	lastByteMillis = millis(); // Reset Time Count

	// This IS a Call Byte
	if (rxdata >= 0x100)
	{					  // Neue Nachricht beginnen
		ReadData = false; // keine Speichern der Serial Daten
		XNetmessageComplete();			// previous message is complete now
		XNetMsgTemp[XNetlength] = 0;	// never deliver it a second time

		if (rxdata == myRequestAck)
		{
			// The command station did not get our last transmission right
			if (XNetSendPending)
				XNetSendFailed = true;
			if (inTime)
			{
				unsigned char requestAckAck[] = {0x20, 0x20};
				XNetsend(requestAckAck, 2);
			}
			return; // Daten wurden verarbeitet
		}
		else if (rxdata == myCallByteInquiry)
		{
			XNetsend(inTime);
			return; // Daten wurden verarbeitet
		}
		else /*if (rxdata == GENERAL_BROADCAST || rxdata == myDirectedOps) */
		{	 // Datenempfang aktivieren
			ReadData = true;
		}
	}
	if (ReadData == true)
	{
		if (XNetMsgTemp[XNetlength] >= (XNetMsgSize - 1))
		{ // longer than any valid message: discard it, do not write past the buffer
			ReadData = false;
			XNetMsgTemp[XNetlength] = 0;
			return;
		}
		XNetMsgTemp[XNetlength]++;							  // Let's make room for it...
		XNetMsgTemp[XNetMsgTemp[XNetlength]] = rxdata & 0xFF; //...and store it
	}
}

//--------------------------------------------------------------------------------------------
// Check the collected message (length from header, XOR) and hand it over.
void XpressNetClass::XNetmessageComplete(void)
{
	byte len = XNetMsgTemp[XNetlength]; // call byte + header + data + XOR
	if (len < 3)
		return;
	byte header = XNetMsgTemp[XNetcom];
	if (len != (header & 0x0F) + 3)
		return; // incomplete or too long
	byte XOR = 0x00;
	for (byte i = XNetcom; i <= len; i++)
		XOR ^= XNetMsgTemp[i];
	if (XOR != 0x00)
		return; // transmission error

	// Answer of the command station to our last transmission?
	if (XNetSendPending && XNetMsgTemp[XNetmsg] == (myDirectedOps & 0xFF) && header == 0x61)
	{
		// 0x80 = transfer error, 0x81 = command station busy -> send again
		// (0x82 = instruction not supported -> drop)
		if (XNetMsgTemp[XNetdata1] == 0x80 || XNetMsgTemp[XNetdata1] == 0x81)
			XNetSendFailed = true;
	}

	memcpy(XNetMsg, XNetMsgTemp, XNetMsgSize);
	DataReady = true;
}

//--------------------------------------------------------------------------------------------
void XpressNetClass::XNetclearSendBuf() // Buffer leeren
{
	memset(XNetSend, 0, sizeof(XNetSend));
}

//--------------------------------------------------------------------------------------------
boolean XpressNetClass::XNetSendadd(unsigned char *dataString, byte byteCount)
{
	if (byteCount == 0 || byteCount > XSendMaxData)
		return false;
	boolean added = false;
	XNET_LOCK();
	for (byte i = 0; i < XSendMax; i++)
	{
		if (XNetSend[i].length == 0)
		{									// Daten hier Eintragen:
			memcpy(XNetSend[i].data, dataString, byteCount);
			XNetSend[i].length = byteCount; // Datenlaenge
			added = true; // leeren Platz gefunden -> ENDE
			break;
		}
	}
	XNET_UNLOCK();
	return added; // false = Kein Platz im Sendbuffer frei!
}

//--------------------------------------------------------------------------------------------
void XpressNetClass::XNetSendremoveFirst(void)
{
	memmove(&XNetSend[0], &XNetSend[1], sizeof(XSend) * (XSendMax - 1));
	memset(&XNetSend[XSendMax - 1], 0, sizeof(XSend));
}

//--------------------------------------------------------------------------------------------
// Answer to our call byte: send the next command from the send buffer.
// A command stays in the buffer until the command station took it without
// "transfer error"/"busy" (that answer always comes before our next call byte).
void XpressNetClass::XNetsend(bool inTime)
{
	if (XNetSendPending)
	{
		if (XNetSendFailed && XNetSendRetries < XSendMaxRetry)
		{
			XNetSendRetries++; // keep it, send again
		}
		else
		{
			XNetSendremoveFirst(); // accepted (or too many retries)
			XNetSendRetries = 0;
		}
		XNetSendPending = false;
		XNetSendFailed = false;
	}

	if (!inTime)
		return; // too late, answer at the next call byte

	if (XNetRun == false)
	{
		unsigned char commandStatusSequence[] = {0x21, 0x24, 0x05};
		XNetsend(commandStatusSequence, 3);
		return;
	}

	while (XNetSend[0].length != 0 && XNetSend[0].data[0] == 0)
		XNetSendremoveFirst(); // invalid entry

	if (XNetSend[0].length != 0)
	{
		XNetsend(XNetSend[0].data, XNetSend[0].length);
		XNetSendPending = true;
	}
}

//--------------------------------------------------------------------------------------------
// send along a bunch of bytes to the Command Station
void XpressNetClass::XNetsend(unsigned char *dataString, byte byteCount)
{
	digitalWrite(MAX485_CONTROL, HIGH);
#if defined(ESP8266) || defined(ESP32)
	XNetSerial.enableTx(true);
	// one call for all bytes: no gaps between them; returns after the last stop bit
	XNetSerial.write(dataString, byteCount);
#else
	for (byte i = 0; i < byteCount; i++)
	{
		USART_Transmit(*dataString);
		dataString++;
	}
#endif

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
