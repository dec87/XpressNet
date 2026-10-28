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
#include <SoftwareSerial.h>

static SoftwareSerial XNetSerial;

// Constructor /////////////////////////////////////////////////////////////////
// Function that handles the creation and setup of instances

XpressNetClass::XpressNetClass()
{
	// initialize this instance's variables
	memset(XNetSend, 0, sizeof(XNetSend));
	memset(XNetMsg, 0, sizeof(XNetMsg));
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
void XpressNetClass::start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl) // Initialisierung Serial
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

	XNetSerial.begin(62500, SWSERIAL_8S1, XNetSerial_RX_Port, XNetSerial_TX_Port, false, 95); // parity mode SPACE = 9th bit 0 on send
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
}

// Public Methods //////////////////////////////////////////////////////////////
// Functions available in Wiring sketches, this library, and other libraries

//*******************************************************************************************
// Daten ermitteln und Auswerten - call as often as possible from loop()
void XpressNetClass::receive(void)
{
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
		{ // 9th bit set (MARK parity) = Call Byte
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

	// no data on the bus: blink the status LED
	if ((unsigned long)(millis() - lastByteMillis) > XNetTimeoutMs)
	{
		XNetRun = false; // Keine Zentrale vorhanden
		lastByteMillis = millis();
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
	unsigned char setTrnt[] = {0x52, 0x00, AdrL, 0x00};
	setTrnt[1] = Adr; // change by André Schenk
	getXOR(setTrnt, 4);

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
		// GENERAL_BROADCAST, directed message or another device: Datenempfang aktivieren
		ReadData = true;
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
boolean XpressNetClass::XNetSendadd(unsigned char *dataString, byte byteCount)
{
	if (byteCount == 0 || byteCount > XSendMaxData)
		return false;
	for (byte i = 0; i < XSendMax; i++)
	{
		if (XNetSend[i].length == 0)
		{									// Daten hier Eintragen:
			memcpy(XNetSend[i].data, dataString, byteCount);
			XNetSend[i].length = byteCount; // Datenlaenge
			return true; // leeren Platz gefunden -> ENDE
		}
	}
	return false; // Kein Platz im Sendbuffer frei!
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
	XNetSerial.enableTx(true);
	// one call for all bytes: no gaps between them; returns after the last stop bit
	XNetSerial.write(dataString, byteCount);
	delayMicroseconds(10);
	digitalWrite(MAX485_CONTROL, LOW);
	XNetSerial.enableTx(false);
}
