/*
  XpressNet for Arduino

  Remember the last commanded Turnout positions using the XpressNet Interface
  (adapted: the trimmed library only supports Turnout switching / position
  reading, so this example now remembers Turnout state instead of Loco state)

  - Store the Turnout position in EEPROM
  - Restore/resend the position on startup
  
*/

#include <XpressNet.h>
XpressNetClass XpressNet;

#include <EEPROM.h>

// XpressNet address: must be in range of 1-31; must be unique. Note that some IDs
// are currently used by default, like 2 for a LH90 or LH100 out of the box, or 30
// for PC interface devices like the XnTCP.
#define XNetAddress 30 //Adresse im XpressNet

#define MAXTRNT 300 //  (300 * 3 = 900) + Overhead -> max EEPROM 1024 Byte 
#define refreshTime 10 //ms to wait between each packet

#define ledPIN 13   //XpressNet Status
#define deletePIN 12 //Remove all Information!

//Debugging:
//#define DEBUG Serial   

uint16_t TrntAdr[MAXTRNT];

void setup() {
  #if defined(DEBUG)
  DEBUG.begin(115200);
  DEBUG.println("XPressNet Turnout store");
  #endif

  pinMode(ledPIN, OUTPUT);
  pinMode(deletePIN, INPUT_PULLUP);
  
  EEPROMReadStore();

  XpressNet.start(XNetAddress, 9);    //initialisierung XNet Serial und Send/Receive-PIN as SLAVE
}

void loop() {
  
 XpressNet.receive(); // XpressNet work function: read and write to X-net bus.

 if (digitalRead(deletePIN) == LOW) {
    clearAllInfo();
 }

}

void EEPROMReadStore() {
  uint16_t adr = 0xFFFF;
  for (uint16_t Slot = 0; Slot < MAXTRNT; Slot++) {
    adr = EEPROM.read(Slot*3) | (EEPROM.read(Slot*3+1) << 8);
    if (adr != 0xFFFF && adr <= 9999) {
      for (uint16_t x = 0; x < Slot; x++) {
        //doppelte Einträge?
        if (TrntAdr[x] == adr) {
          #if defined(DEBUG)  
          DEBUG.print("löschen ");
          #endif
          for (byte del = 0; del < 3; del++) {
            EEPROM.update(Slot*3+del, 0xFF);
          }
        }
      }
      TrntAdr[Slot] = adr;
      byte Pos = EEPROM.read(Slot*3+2);
      #if defined(DEBUG)
      DEBUG.print("Weiche: ");
      DEBUG.print(adr);
      DEBUG.print(" Position: ");
      DEBUG.println(Pos, BIN);
      #endif
      XpressNet.setTrntPos(highByte(adr), lowByte(adr), Pos);
      delay(refreshTime);
    }
    else {
      //leeren:
      TrntAdr[Slot] = 0xFFFF;
      for (byte del = 0; del < 3; del++) {
        EEPROM.update(Slot*3+del, 0xFF);
      }
    }
  }
}

uint16_t getSlot(uint16_t adr) {
  for (uint16_t Slot = 0; Slot < MAXTRNT; Slot++) {
    if (TrntAdr[Slot] == adr)
      return Slot; //vorhanden
  }
  //neu anlegen:
  for (uint16_t Slot = 0; Slot < MAXTRNT; Slot++) {
    if (TrntAdr[Slot] == 0xFFFF) {
      TrntAdr[Slot] = adr;
      EEPROM.update(Slot*3,adr & 0xFF);
      EEPROM.update(Slot*3+1, adr >> 8);
      EEPROM.update(Slot*3+2, 0);
      #if defined(DEBUG)
      DEBUG.print("Neue Weiche ");
      DEBUG.println(adr);
      #endif
      return Slot;
    }
  }
  return 0;
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos)
{
  #if defined(DEBUG)
  DEBUG.print("TurnOut: ");
  DEBUG.print(word(Adr_High, Adr_Low));
  DEBUG.print(", Position: ");
  DEBUG.println(Pos, BIN);
  #endif

  uint16_t Slot = getSlot(word(Adr_High,Adr_Low));
  EEPROM.update(Slot*3, Adr_Low);
  EEPROM.update(Slot*3+1, Adr_High);
  EEPROM.update(Slot*3+2, Pos);
}

//--------------------------------------------------------------------------------------------
void notifyXNetStatus(uint8_t LedState)
{
  digitalWrite(ledPIN, LedState);
}

//--------------------------------------------------------------------------------------------
void clearAllInfo() {
  for (uint16_t Slot = 0; Slot < MAXTRNT; Slot++) {
    TrntAdr[Slot] = 0xFFFF;
    EEPROM.update(Slot*3, 0xFF);
    EEPROM.update(Slot*3+1, 0xFF);
    EEPROM.update(Slot*3+2, 0xFF);
  }
  #if defined(DEBUG)
  DEBUG.println("CLEAR ALL!");
  #endif
}
