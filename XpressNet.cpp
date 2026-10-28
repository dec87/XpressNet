/*
*****************************************************************************
*		XpressNet.cpp - biblioteka klienta protokołu XpressNet
*		Copyright (c) 2015-2022 Philipp Gahtow. Wszelkie prawa zastrzeżone.
*
**	 Tylko do użytku prywatnego!
*****************************************************************************
* WAŻNE:
*
* 	Szczegóły protokołu: Lenz Inc.
*****************************************************************************
*  historia zmian: zob. XpressNet.h
*/

#include "XpressNet.h"
#include <SoftwareSerial.h>

static SoftwareSerial XNetSerial;

// Konstruktor /////////////////////////////////////////////////////////////////

XpressNetClass::XpressNetClass()
{
	memset(XNetSend, 0, sizeof(XNetSend));
	memset(XNetMsg, 0, sizeof(XNetMsg));
	memset(XNetMsgTemp, 0, sizeof(XNetMsgTemp));
	XNetRun = false; // XpressNet nieaktywny
	ReadData = false; // nie zapisuj odbieranych bajtów
	DataReady = false;
	XNetSendPending = false;
	XNetSendFailed = false;
	XNetSendRetries = 0;
	ledState = LOW;
	lastByteMillis = 0;
	lastIdleMicros = 0;
}

//******************************************Port szeregowy*******************************************
void XpressNetClass::start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl)
{
	ledState = LOW;		 // dioda stanu
	lastByteMillis = millis();
	lastIdleMicros = micros();
	if (notifyXNetStatus)
		notifyXNetStatus(ledState);

	MY_ADDRESS = XAdr;
	MAX485_CONTROL = XControl;
	// tryb odbioru
	pinMode(MAX485_CONTROL, OUTPUT);
	digitalWrite(MAX485_CONTROL, LOW);

	myRequestAck = callByteParity(MY_ADDRESS | 0x00) | 0x100;
	myCallByteInquiry = callByteParity(MY_ADDRESS | 0x40) | 0x100;
	myDirectedOps = callByteParity(MY_ADDRESS | 0x60) | 0x100;

	XNetSerial.begin(62500, SWSERIAL_8S1, XNetSerial_RX_Port, XNetSerial_TX_Port, false, 95); // parzystość SPACE = 9. bit równy 0 przy nadawaniu
	if (!XNetSerial)
	{ // nieprawidłowa konfiguracja pinów
		Serial.println("Invalid SoftwareSerial pin configuration, check config");
		while (1)
		{ // nie kontynuuj z błędną konfiguracją
			delay(1000);
		}
	}
	// szybka transmisja półdupleksowa: przerwania wyłączone podczas nadawania
	XNetSerial.enableIntTx(false);
}

// Metody publiczne //////////////////////////////////////////////////////////////

//*******************************************************************************************
// Odbiór i obsługa danych - wywoływać jak najczęściej w loop()
void XpressNetClass::receive(void)
{
	// Odczytaj wszystko, co czeka w buforze, a nie tylko jeden bajt: na call byte
	// tego urządzenia trzeba odpowiedzieć od razu, a nie po starszych bajtach.
	while (true)
	{
		if (!XNetSerial.available())
		{
			lastIdleMicros = micros();
			break;
		}
		unsigned int rxdata = XNetSerial.read();
		if (XNetSerial.readParity())
		{ // ustawiony 9. bit (parzystość MARK) = call byte
			rxdata |= 0x100;
		}
		// Bajt został odebrany w całości po chwili, gdy bufor był ostatnio pusty.
		// Okno odpowiedzi centralki jest jeszcze otwarte tylko wtedy, gdy było to
		// bardzo niedawno i po tym bajcie nic więcej nie przyszło.
		bool inTime = (XNetSerial.available() == 0) &&
					  ((unsigned long)(micros() - lastIdleMicros) <= XNetMaxResponseDelayUs);
		XNetprocessByte(rxdata, inTime);
		if (DataReady)
		{
			DataReady = false;
			XNethandleMessage(XNetMsg);
		}
	}

	// brak danych na magistrali: dioda stanu miga
	if ((unsigned long)(millis() - lastByteMillis) > XNetTimeoutMs)
	{
		XNetRun = false; // brak centralki
		lastByteMillis = millis();
		ledState = !ledState;
		if (notifyXNetStatus)
			notifyXNetStatus(ledState);
	}
}

//--------------------------------------------------------------------------------------------
// Obsługa kompletnego i sprawdzonego komunikatu
void XpressNetClass::XNethandleMessage(const uint8_t *msg)
{
	// są dane: dioda stanu świeci
	if (ledState == LOW)
	{
		ledState = HIGH;
		if (notifyXNetStatus)
			notifyXNetStatus(ledState);
	}

	byte callByte = msg[XNetmsg];
	byte header = msg[XNetcom];

	if (callByte == (GENERAL_BROADCAST & 0xFF))
	{ // rozgłoszenie ogólne
		if ((header & 0xF0) == 0x40)
		{
			// rozgłoszenie stanów: pary adres + dane (zwrotnice i czujniki)
			byte pairs = (header & 0x0F) / 2;
			for (byte i = 0; i < pairs; i++)
			{
				reportAccessory(msg[XNetdata1 + (i * 2)], msg[XNetdata2 + (i * 2)]);
			}
		}
		return;
	}

	// Komunikaty skierowane - także do innych urządzeń (zmiana: André Schenk)
	switch (header)
	{
	// autor: Norberto Redondo Melchor
	case 0x52: // inne urządzenie przełącza akcesorium: 1000CBBD
		if ((msg[XNetdata2] & 0xF0) == 0x80)
		{
			byte data2 = msg[XNetdata2];
			unsigned int Adr = (msg[XNetdata1] << 2) | ((data2 & B0110) >> 1);
			byte Pos = (data2 & B0001) + 1;
			// C (bit 3) = 1: włączenie wyjścia, C = 0: wyłączenie wyjścia (zgłaszane jako Pos | B1000)
			if (!(data2 & B1000))
				Pos = Pos | B1000;
			if (notifyTrnt)
				notifyTrnt(highByte(Adr), lowByte(Adr), Pos);
		}
		break;
	case 0x62: // odpowiedź centralki na zapytanie o status - jednorazowe zgłoszenie na magistrali
		if (msg[XNetdata1] == 0x22)
		{
			if (XNetRun == false)
			{ // zapytaj o wersję oprogramowania
				unsigned char commandVersionSequence[] = {0x21, 0x21, 0x00};
				XNetSendadd(commandVersionSequence, 3);
				XNetRun = true;
			}
			// centralka w trakcie uruchamiania - poczekaj, aż skończy
			if (msg[XNetdata2] != 0 && (msg[XNetdata2] & 0x40) == 0x40)
			{
				// przy następnym odpytaniu ponownie zapytaj o status
				XNetRun = false;
			}
		}
		break;
	case 0x42: // odpowiedź z informacją o stanie zwrotnic
		reportAccessory(msg[XNetdata1], msg[XNetdata2]);
		break;
	}
}

//--------------------------------------------------------------------------------------------
// Przekazanie informacji o zwrotnicach/czujnikach: data2 = ITTNZZZZ
void XpressNetClass::reportAccessory(byte data1, byte data2)
{
	byte type = (data2 >> 5) & 0x03; // 00/01 = dekoder akcesoriów, 10 = moduł czujników, 11 = zarezerwowane
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
// Zapytanie o położenie zwrotnicy
bool XpressNetClass::getTrntInfo(byte FAdr_High, byte FAdr_Low)
{
	int Adr = word(FAdr_High, FAdr_Low);
	byte nibble = 0; // 0 = zwrotnice 0 i 1; 1 = zwrotnice 2 i 3
	if ((Adr & 0x03) >= 2)
		nibble = 1;
	unsigned char getTrntPos[] = {0x42, 0x00, 0x80, 0x00};
	getTrntPos[1] = Adr >> 2;
	getTrntPos[2] += nibble;
	getXOR(getTrntPos, 4);
	return XNetSendadd(getTrntPos, 4);
}

//--------------------------------------------------------------------------------------------
// Przełączenie zwrotnicy
bool XpressNetClass::setTrntPos(byte FAdr_High, byte FAdr_Low, byte Pos)
// Pos = 0000A00P	A = wyjście włączone/wyłączone; P = kierunek zwrotnicy
{
	int Adr = word(FAdr_High, FAdr_Low);
	byte AdrL = ((Pos & 0x0F) | B110) & (((Adr & 0x03) << 1) | B1001); // 1000ABBP: A00P = Pos, BB = Adr & 0x03 (najmłodsze bity adresu)

	Adr = Adr >> 2;
	bitWrite(AdrL, 7, 1);
	unsigned char setTrnt[] = {0x52, 0x00, AdrL, 0x00};
	setTrnt[1] = Adr; // zmiana: André Schenk
	getXOR(setTrnt, 4);

	if (notifyTrnt)
		notifyTrnt(FAdr_High, FAdr_Low, (Pos & B1) + 1);

	return XNetSendadd(setTrnt, 4);
}

// Metody prywatne ///////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------
// obliczenie XOR (zapisywany w ostatnim bajcie)
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
// obliczenie bitu parzystości w call bycie
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
// Obsługa jednego odebranego bajtu.
// inTime = false: call byte odczytany za późno, by odpowiedzieć bez kolizji.
void XpressNetClass::XNetprocessByte(unsigned int rxdata, bool inTime)
{
	lastByteMillis = millis(); // reset licznika czasu

	// to jest call byte
	if (rxdata >= 0x100)
	{					  // początek nowego komunikatu
		ReadData = false; // nie zapisuj odbieranych bajtów
		XNetmessageComplete();			// poprzedni komunikat jest już kompletny
		XNetMsgTemp[XNetlength] = 0;	// nigdy nie przekazuj go drugi raz

		if (rxdata == myRequestAck)
		{
			// centralka nie odebrała poprawnie naszej ostatniej transmisji
			if (XNetSendPending)
				XNetSendFailed = true;
			if (inTime)
			{
				unsigned char requestAckAck[] = {0x20, 0x20};
				XNetsend(requestAckAck, 2);
			}
			return; // bajt obsłużony
		}
		else if (rxdata == myCallByteInquiry)
		{
			XNetsend(inTime);
			return; // bajt obsłużony
		}
		// rozgłoszenie, komunikat do nas lub do innego urządzenia: włącz zapisywanie
		ReadData = true;
	}
	if (ReadData == true)
	{
		if (XNetMsgTemp[XNetlength] >= (XNetMsgSize - 1))
		{ // dłuższy niż jakikolwiek poprawny komunikat: odrzuć, nie pisz poza bufor
			ReadData = false;
			XNetMsgTemp[XNetlength] = 0;
			return;
		}
		XNetMsgTemp[XNetlength]++;							  // zrób miejsce...
		XNetMsgTemp[XNetMsgTemp[XNetlength]] = rxdata & 0xFF; //...i zapisz bajt
	}
}

//--------------------------------------------------------------------------------------------
// Sprawdzenie zebranego komunikatu (długość z nagłówka, XOR) i przekazanie go dalej.
void XpressNetClass::XNetmessageComplete(void)
{
	byte len = XNetMsgTemp[XNetlength]; // call byte + nagłówek + dane + XOR
	if (len < 3)
		return;
	byte header = XNetMsgTemp[XNetcom];
	if (len != (header & 0x0F) + 3)
		return; // niekompletny lub za długi
	byte XOR = 0x00;
	for (byte i = XNetcom; i <= len; i++)
		XOR ^= XNetMsgTemp[i];
	if (XOR != 0x00)
		return; // błąd transmisji

	// Odpowiedź centralki na naszą ostatnią transmisję?
	if (XNetSendPending && XNetMsgTemp[XNetmsg] == (myDirectedOps & 0xFF) && header == 0x61)
	{
		// 0x80 = błąd transmisji, 0x81 = centralka zajęta -> wyślij ponownie
		// (0x82 = komenda nieobsługiwana -> odrzuć)
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
		{									// wolne miejsce - zapisz komendę
			memcpy(XNetSend[i].data, dataString, byteCount);
			XNetSend[i].length = byteCount;
			return true;
		}
	}
	return false; // brak miejsca w kolejce wysyłania
}

//--------------------------------------------------------------------------------------------
void XpressNetClass::XNetSendremoveFirst(void)
{
	memmove(&XNetSend[0], &XNetSend[1], sizeof(XSend) * (XSendMax - 1));
	memset(&XNetSend[XSendMax - 1], 0, sizeof(XSend));
}

//--------------------------------------------------------------------------------------------
// Odpowiedź na nasz call byte: wysłanie kolejnej komendy z kolejki.
// Komenda zostaje w kolejce, dopóki centralka nie przyjmie jej bez "błąd transmisji"/"zajęta"
// (taka odpowiedź zawsze przychodzi przed naszym następnym call bytem).
void XpressNetClass::XNetsend(bool inTime)
{
	if (XNetSendPending)
	{
		if (XNetSendFailed && XNetSendRetries < XSendMaxRetry)
		{
			XNetSendRetries++; // zostaw w kolejce, wyślij ponownie
		}
		else
		{
			XNetSendremoveFirst(); // przyjęta (albo za dużo powtórzeń)
			XNetSendRetries = 0;
		}
		XNetSendPending = false;
		XNetSendFailed = false;
	}

	if (!inTime)
		return; // za późno, odpowiedz przy następnym call bycie

	if (XNetRun == false)
	{
		unsigned char commandStatusSequence[] = {0x21, 0x24, 0x05};
		XNetsend(commandStatusSequence, 3);
		return;
	}

	while (XNetSend[0].length != 0 && XNetSend[0].data[0] == 0)
		XNetSendremoveFirst(); // nieprawidłowy wpis

	if (XNetSend[0].length != 0)
	{
		XNetsend(XNetSend[0].data, XNetSend[0].length);
		XNetSendPending = true;
	}
}

//--------------------------------------------------------------------------------------------
// wysłanie bajtów do centralki
void XpressNetClass::XNetsend(unsigned char *dataString, byte byteCount)
{
	digitalWrite(MAX485_CONTROL, HIGH);
	XNetSerial.enableTx(true);
	// jedno wywołanie dla wszystkich bajtów: bez przerw między nimi; wraca po ostatnim bicie stopu
	XNetSerial.write(dataString, byteCount);
	delayMicroseconds(10);
	digitalWrite(MAX485_CONTROL, LOW);
	XNetSerial.enableTx(false);
}
