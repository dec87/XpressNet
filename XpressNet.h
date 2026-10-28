/*
  XpressNet.h - biblioteka protokołu XpressNet
  Copyright (c) 2013-2024 Philipp Gahtow. Wszelkie prawa zastrzeżone.
  Tylko do użytku prywatnego!

  Wersja 3.0.0 (28.10.2026)

  Uwaga:
  Działa tylko z XpressNet w wersji 3.0 lub nowszej!
  *********************************************************************
  21.07.2015 Philipp Gahtow - zmiana adresowania komend zwrotnic
							- mniejsze zużycie pamięci w funkcji setPower
  29.09.2015 Philipp Gahtow - poprawka komend F13-F20 dla Multimaus
  17.11.2015 Philipp Gahtow - poprawka AdrL w setTrntPos
  02.02.2017 Philipp Gahtow - obsługa zmiany akcesoriów 0x52 (autor: Norberto Redondo Melchor)
							- poprawka konwersji adresu w setTrntPos
							- poprawka zawężających konwersji w tablicach
  28.04.2017 Philipp Gahtow - mniejsze zużycie RAM w callByteParity
  23.07.2017 Philipp Gahtow - zmiany dla WLANMaus (autor: André Schenk)
  20.10.2017 Philipp Gahtow - rozgłoszenie informacji o stanie zwrotnic ("Schaltinformation")
  18.04.2020 Philipp Gahtow - poprawka długich adresów od 100 w górę
  26.08.2021 Philipp Gahtow - komendy E4 i odbiór wszystkich komunikatów w osobnym buforze
  19.11.2021 Philipp Gahtow - funkcje lokomotywy w pakietach; poprawka adresu i funkcji w odpowiedziach E4
  31.01.2022 Philipp Gahtow - obsługa ESP8266 i ESP32 przez Software Serial (https://github.com/plerup/espsoftwareserial)
  27.01.2024 Philipp Gahtow - obsługa Leonardo
  28.10.2026 Damian Celmer  - niezawodna transmisja: receive() odczytuje cały bufor odbiorczy,
							  odpowiedź na call byte tylko w otwartym oknie odpytania,
							  wysłana komenda zostaje w kolejce do przyjęcia przez centralkę
							  i jest powtarzana po "błąd transmisji"/"zajęta" lub prośbie o potwierdzenie,
							  bufor odbiorczy z kontrolą zakresu (nie nadpisuje już kolejki wysyłania),
							  kontrola długości i XOR odebranych komunikatów, brak ponownego
							  przekazania poprzedniego komunikatu, rozgłoszenia 0x4X kierowane wg typu
							  (zwrotnica -> notifyTrnt, czujnik -> notifyFeedback), bit aktywacji
							  w 0x52 czytany zgodnie ze specyfikacją, limit czasu magistrali w ms
  28.10.2026 Damian Celmer  - tylko ESP32: usunięta obsługa AVR i ESP8266
*/

// zabezpieczenie przed wielokrotnym dołączeniem
#ifndef XpressNet_h
#define XpressNet_h

#if !defined(ESP32)
#error "Ta biblioteka XpressNet obsługuje tylko ESP32."
#endif

#include <Arduino.h>

//--------------------------------------------------------------------------------------------

// Call byte rozgłoszenia ogólnego (ustawiony 9. bit)
#define GENERAL_BROADCAST 0x160

// Indeksy pól w odebranym komunikacie
#define XNetlength	0		//długość
#define XNetmsg		1		//call byte
#define XNetcom		2		//nagłówek (rodzaj komendy)
#define XNetdata1	3		//bajt danych 1
#define XNetdata2	4		//bajt danych 2

// Bufor odbiorczy: [0] = długość, potem call byte, nagłówek, do 15 bajtów danych i bajt XOR.
#define XNetMsgSize 19

#define XSendMax 16			//maksymalna liczba komend w kolejce wysyłania
#define XSendMaxData 8		//maksymalna liczba bajtów jednej komendy

// Ile razy komenda jest powtarzana po "błąd transmisji"/"centralka zajęta"
// lub prośbie o potwierdzenie, zanim zostanie odrzucona.
#ifndef XSendMaxRetry
#define XSendMaxRetry 5
#endif

// Odpowiedź na call byte tego urządzenia jest wysyłana tylko wtedy, gdy został on
// odczytany najpóźniej po tylu mikrosekundach od chwili, gdy bufor odbiorczy był
// ostatnio pusty. Późniejsza odpowiedź zderzyłaby się z kolejnym call bytem centralki,
// więc odpytanie jest pomijane, a komenda czeka na następne.
#ifndef XNetMaxResponseDelayUs
#define XNetMaxResponseDelayUs 60
#endif

// Brak bajtów na magistrali przez tyle ms = brak centralki (dioda stanu miga).
#ifndef XNetTimeoutMs
#define XNetTimeoutMs 1000
#endif

typedef struct	//komenda w kolejce wysyłania
{
	uint8_t length;			//długość danych
	byte data[XSendMaxData];	//dane do wysłania
} XSend;

// Interfejs biblioteki
// Ta wersja obsługuje tylko przełączanie zwrotnic/akcesoriów i odczyt ich położenia.
class XpressNetClass
{
  // interfejs publiczny
  public:
    XpressNetClass(void);	//konstruktor
	void start(byte XAdr, uint8_t XNetSerial_RX_Port, uint8_t XNetSerial_TX_Port, uint8_t XControl);  //inicjalizacja portu
	void receive(void);				//odbiór i obsługa komunikatów XpressNet - wywoływać w loop()

	bool getTrntInfo (byte FAdr_High, byte FAdr_Low);		//zapytanie o położenie zwrotnicy
	bool setTrntPos (byte FAdr_High, byte FAdr_Low, byte Pos);		//przełączenie zwrotnicy

  // interfejs prywatny
  private:
	  //zmienne:
	boolean XNetRun;	//XpressNet aktywny (centralka odpowiedziała na zapytanie o status)
	byte MY_ADDRESS;	//adres XpressNet: 1-31, musi być unikalny
	byte MAX485_CONTROL; //pin przełączania nadawanie/odbiór RS485
	unsigned int myDirectedOps;		// call byte komunikatów skierowanych do nas
	unsigned int myCallByteInquiry;	// call byte odpytania nas (okno na odpowiedź)
	unsigned int myRequestAck;		// call byte prośby o potwierdzenie
	uint8_t XNetMsg[XNetMsgSize];		//odebrany i sprawdzony komunikat
	bool DataReady;				//komunikat gotowy do obsługi
	uint8_t XNetMsgTemp[XNetMsgSize];	//odbierany komunikat (długość, call byte, nagłówek, dane ...)
	bool ReadData;				//zapisywanie odbieranych bajtów włączone
	XSend XNetSend[XSendMax];		//kolejka wysyłania

	// stan komendy na początku kolejki wysyłania
	bool XNetSendPending;		//komenda wysłana, wynik jeszcze nieznany
	bool XNetSendFailed;		//centralka zgłosiła błąd/zajętość -> wyślij ponownie
	uint8_t XNetSendRetries;	//liczba powtórzeń bieżącej komendy

	//dioda stanu:
	int ledState;             // stan diody
	unsigned long lastByteMillis;	// czas ostatniego odebranego bajtu / przełączenia diody
	unsigned long lastIdleMicros;	// ostatnia chwila, gdy bufor odbiorczy był pusty

		//funkcje:
	void XNetprocessByte(unsigned int rxdata, bool inTime);	//obsługa jednego odebranego bajtu
	void XNetmessageComplete(void);	//sprawdzenie komunikatu (długość/XOR) i przekazanie go dalej
	void XNethandleMessage(const uint8_t *msg);	//obsługa sprawdzonego komunikatu
	void reportAccessory(byte data1, byte data2);  //przekazanie informacji o zwrotnicach/czujnikach
	void getXOR (unsigned char *data, byte length); // obliczenie XOR
	byte callByteParity (byte me);	// obliczenie bitu parzystości

	boolean XNetSendadd(unsigned char *dataString, byte byteCount);	//dodanie komendy do kolejki
	void XNetSendremoveFirst(void);	//usunięcie pierwszej komendy z kolejki
	void XNetsend(bool inTime); //odpowiedź na call byte: wysłanie komendy z kolejki
	void XNetsend(unsigned char *dataString, byte byteCount);	//wysłanie bajtów na magistralę
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
