/*
 * ------------------------------------------------
 *             MFRC522      Arduino      NodeMCU
 *             Reader/PCD   Uno/101
 * Signal      Pin          Pin
 * ------------------------------------------------
 * RST/Reset   RST          0             D3
 * SPI SS      SDA(SS)      2             D4
 * SPI MOSI    MOSI         13            D7
 * SPI MISO    MISO         12            D6
 * SPI SCK     SCK          14            D5
 * ------------------------------------------------
 */

/* Card UID = file_name.dat
 * In file struct:
 * Telegram chat_id
 * Sector number
 * Sector random
 */

/* Szükséges modulok
 * Valós random szám generátor [X]
 * Kártya olvasó/író           []
 * Kártya adat validáló        []
 * Telegram üzenet küldő       [X]
 * Telegram user premisson     []
 * OTA modul                   []

 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "ESP8266TrueRandom.h"
#include <SD.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#define BOTtoken "424218094:AAFxTF_GKfTA-o9kRuPnEF-SpPsPFCff2mI"

const int timezone = 1;
bool debug = true;
bool nfcModuleHealty = true;
constexpr uint8_t RST_PIN = 4;
constexpr uint8_t SDA_PIN = 2;
MFRC522 mfrc522(SDA_PIN, RST_PIN);
int wifiStatus;
const char* ssid     = "................";
const char* password = "...............";
String ip_address, subnet_mask, gateway, channel;

File data; //SD Card modul

byte block;
byte sector_key;
byte sector_random;
String cardUid = "";

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
void setup() {
        SPI.begin();      // SPI inicializálás
        mfrc522.PCD_Init(); // MFRC522 kártya inicializálás
        WiFi.begin(ssid, password);

        if (debug) {
          Serial.begin(9600);
          mfrc522.PCD_DumpVersionToSerial();
        }

        configTime(timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");//ctime(&now)-val lehet lekérni
        while (!time(nullptr)) {
                delay(500);
        }

}
bool isNfcModuleIsWorking(bool healty){
        //Menet közbeni NFC szenzor ellenőrzés
        if(mfrc522.PCD_PerformSelfTest()) {
                return true;
        }
        else{
                return false;
        }
}
void isAllRandomData(){
        byte uuidNumber[16];
        String card_data;

        //Random generált adat
        for (int i = 0; i < 10; i++) {
          ESP8266TrueRandom.uuid(uuidNumber);
          card_data += ESP8266TrueRandom.uuidToString(uuidNumber).substring(3,7);
        }
        card_data.toUpperCase();
        card_data += "FF";
        //FIXME: uuidStr átalakítása byte formátumuvá


        //Random generált blokk
        ESP8266TrueRandom.uuid(uuidNumber);
        block = ESP8266TrueRandom.random(1,17);
}


/*
String isNFCRead(){
  MFRC522::StatusCode status;
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authentikáció() sikertelen: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else Serial.println(F("PCD_Authentikáció() sikeres: "));






  mfrc522.PICC_HaltA(); // PICC lezárása
  mfrc522.PCD_StopCrypto1();  // PCD titkosítás lezárása

}
*/

/*
String isNFCWrite(){
  MFRC522::StatusCode status;
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authentikáció() sikertelen: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else Serial.println(F("PCD_Authentikáció() sikeres: "));


  status = mfrc522.MIFARE_Write(block, adatok, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else Serial.println(F("MIFARE_Write() success: "));


   mfrc522.PICC_HaltA(); // PICC lezárása
   mfrc522.PCD_StopCrypto1();  // PCD titkosítás lezárása

}
*/

/*
String isSDCardRead(String cardNumber){
      //Kártya adatok beolvasása az SD kártyáról
      cardNumber += ".dat";
      data = SD.open(cardNumber);

      if (data){



      }
      myFile.close();
}
*/

/*
String isSDCardWrite(String cardNumber){
      //Kártya adatok eltárolása az SD kártyára
      cardNumber += ".dat"
      data = SD.open(cardNumber, FILE_WRITE);

      if (data){



      }
      myFile.close();
}
*/
void wifiConnection(){
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
        }

        //Network inforamation
        ip_address = WiFi.localIP().toString();
        subnet_mask = WiFi.subnetMask().toString();
        gateway = WiFi.gatewayIP().toString();
        channel = WiFi.channel();
        //Network inforamation
}
void handleNewMessages(int numNewMessages) {
        for(int i=0; i<numNewMessages; i++) {
                String chat_id = String(bot.messages[i].chat_id);
                String text = bot.messages[i].text;

                bot.sendChatAction(chat_id, "typing");
                Serial.println("Chat Id: "+chat_id + " Message:"+ text);
                if (text=="/help") {
                    String welcome = "";
                    welcome = welcome + "/help : Az összes parancs kiíratása \n";
                    welcome = welcome + "/status : Az eszköz állapotának lekérdezése\n";
                    welcome = welcome + "/wifi : A jelenlegi WiFi jelszó lekérdezése\n";
                    welcome = welcome + "/open : Az ajtó kinyitása 3mp-ig\n";
                    welcome = welcome + "/lockoverride : Az ajtó kártyás beléptetésének felülbírálása";

                    bot.sendMessage(chat_id, welcome, "Markdown");
                }

                else if (text == "/status"){
                  String status = "";
                  status= status +"\n\nConnected wifi name: "+ ssid;
                  status= status +"\nWifi Channel: " + channel;
                  status= status +"\nLocal IP address: " + ip_address;
                  status= status +"\nSubnet mask: " + subnet_mask;
                  status= status +"\nDefault gateway: " + gateway;
                  bot.sendMessage(chat_id, status, "Markdown");
                }
                else if (text == "/wifi"){
                  String wifi = "";
                  wifi = wifi + "\nIgen, van wifi! Ezért működöm én is!";
                  wifi = wifi + "\nTe is szeretnéd tudni a wifi jelszót?";
                  wifi = wifi + "\n Írd be, hogy /wifipass, talán ez segít!";
                  bot.sendMessage(chat_id, wifi, "Markdown");
                }
                else if (text == "/wifipass"){
                  String wifi = "";
                  wifi = wifi + "\nTe aztán valóban szeretnéd azt a wifi jelszót!";
                  wifi = wifi + "\nBiztosan kell neked?? Ugyan mi olyan fontos, hogy internetre kapcsolódjál?";
                  wifi = wifi + "\nDe te tudod:/wifiaccess";
                  bot.sendMessage(chat_id, wifi, "Markdown");
                }
                else if (text == "/wifiaccess"){
                    String wifi = "Wifi pass:12345678";
                    wifi = wifi + "\n Na itt van! Használd egészséggel!";
                    bot.sendMessage(chat_id, wifi, "Markdown");
                }
                else if (text == "/open"){
                  String open = "";
                  open = open + "\nAz ajtó ideiglenesen kinyitva!"
                  bot.sendMessage(chat_id, open, "Markdown");
                }
                else if (text == "/lockoverride"){
                  String lock="";
                  lock = lock + "\nAz ajtó bezárva tartása!!"
                  lock = lock + "\nFIGYELEM: A JELENLEGI FUNKCIÓ AKTIVÁLÁSA UTÁN NEM LEHET KÁRTYÁVAL KINYITNI AZ AJTÓT!!!"
                  lock = lock + "\nAZ /open PARANCS FELÜLÍRJA A LOCKOVERRIDE-OT!"
                  bot.sendMessage(chat_id, , "Markdown");
                }
                else{
                  String error = "Ilyen parancs nem létezik!";
                  bot.sendMessage(chat_id, error, "Markdown");
                }

        }
}

void isRelay(){


}
////////////////////////////////////////////////////////////////////////////////
void loop() {

        if (WiFi.status() !=  WL_CONNECTED){//Újra csatlakozás
          if (debug) Serial.println("Wifi disconnected!");
          wifiConnection();
          if (debug) Serial.println("Wifi connected!");
        }

        if (millis() > Bot_lasttime + Bot_mtbs)  {
                        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
                        while(numNewMessages) {
                                handleNewMessages(numNewMessages);
                                numNewMessages = bot.getUpdates(bot.last_message_received + 1);
                        }
                        Bot_lasttime = millis();
        }

        //Új kártya detektálása
        if ( ! mfrc522.PICC_IsNewCardPresent()) return;

        //Kártya érzékelése
        if ( ! mfrc522.PICC_ReadCardSerial()) return;

        for (byte i = 0; i < 4; i++) cardUid += String(mfrc522.uid.uidByte[i]); //UID beolvasása









    sector_key = 0;
    block = 0;
    cardUid="";
}
