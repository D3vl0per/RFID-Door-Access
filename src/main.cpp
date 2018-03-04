/*
 * ------------------------------------------------
 *             MFRC522      Arduino      NodeMCU
 *             Reader/PCD   Uno/101
 * Signal      Pin          Pin
 * ------------------------------------------------
 * RST/Reset   RST          0             D3
 * SPI SS      SDA(SS)      2             D4
 * SPI SCK     SCK          14            D5
 * SPI MISO    MISO         12            D6
 * SPI MOSI    MOSI         13            D7
 * ------------------------------------------------
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
#include <time.h>
#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#define BOTtoken ".................................."

#define DEADBOLT 15 //BUG: Ha olyan GPIO porta állítjuk, melyet a kártya olvasó comportja használ, akkor !!crash!!


const int timezone = 1;
bool debug = true;
bool nfcModuleHealty = true;
bool lockOverride = false;
bool OTAInProgress = false;
constexpr uint8_t RST_PIN = 4;
constexpr uint8_t SDA_PIN = 2;
MFRC522 mfrc522(SDA_PIN, RST_PIN);
int wifiStatus;


const char* ssid     = "";
const char* password = "";



String ip_address, subnet_mask, gateway, channel;

//HACK: Beolvasásnál kell majd lefixálni a tömbök méretét!!
String owner[] = {};
String dev[] = {};
String member[] = {};



byte block;
byte sector_key;
byte sector_random;
String cardUid = "";

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
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
        //HACK: uuidStr átalakítása byte formátumuvá


        //Random generált blokk
        ESP8266TrueRandom.uuid(uuidNumber);
        block = ESP8266TrueRandom.random(1,17);
}
/*
   String isNFCRead(){
   MFRC522::MIFARE_Key key;
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
   MFRC522::MIFARE_Key key;

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
void isMessage2AllUser(String text, String chat_id){
        for (int i = 0; i < sizeof(dev); i++) {
                bot.sendMessage(dev[i], text, "Markdown");
        }
        for (int i = 0; i < sizeof(member); i++) {
                bot.sendMessage(member[i], text, "Markdown");
        }
        for (int i = 0; i < sizeof(owner); i++) {
                if (owner[i]!=chat_id) {
                        bot.sendMessage(owner[i], text, "Markdown");
                }
        }
}
void isGuestPrivilege(String text, String chat_id){
        if (debug){
                Serial.print(" Privilege level: Guest\n");
                Serial.println("--------------------------------------------------------------");
        }
        if (text=="/help") {
                String help = "";
                help = help + "/help : Az összes parancs kiíratása \n";
                help = help + "/knock : Kopogás \n";

                bot.sendMessage(chat_id, help, "Markdown");
        }
        else if (text=="/knock") {
                time_t now = time(nullptr);
                String knock = "";
                knock = knock + "\nKopogtál!";

                bot.sendMessage(chat_id, knock, "Markdown");
                String msgtoowner="";
                msgtoowner = msgtoowner + "A" + chat_id + "telegram azonosítójú személy kopogtatott";
                msgtoowner = msgtoowner + "\nKérés érkezett: " +  ctime(&now);
                for (int i = 0; i < sizeof(owner); i++) {
                        bot.sendMessage(owner[i], msgtoowner, "Markdown");
                }
        }
        else if (text == "/start") {
                String start = "\nKedves Vendég! Önnnek nincsen hozzáférése az ajtó vezérléshez!";
                bot.sendMessage(chat_id, start, "Markdown");
        }
        else {
                String asd = "Ilyen paranacs vagy nem létezik, vagy nincsen megfelelő jogosultsága a futtatáshoz!";
                bot.sendMessage(chat_id, asd, "Markdown");
        }

}
void isMemberPrivilege(String text, String chat_id){
        if (debug){
                Serial.print(" Privilege level: Member\n");
                Serial.println("--------------------------------------------------------------");
        }
        if (text=="/help") {
                String help = "";
                help = help + "/help : Az összes parancs kiíratása \n";
                help = help + "/status : Az eszköz állapotának lekérdezése\n";
                help = help + "/wifi : A jelenlegi WiFi jelszó lekérdezése\n";
                help = help + "/open : Az ajtó kinyitása 3mp-ig\n";
                bot.sendMessage(chat_id, help, "Markdown");
        }
        else if (text == "/status") {
                String status = "";
                time_t now = time(nullptr);
                status = status + ctime(&now) + "\n\n";
                status = status +"\n\nKapcsolódott wifi: "+ ssid;
                if (lockOverride) status = status + "lockOverride: AKTÍV";
                else status = status + "lockOverride: AKTÍV";
                status = status + "\nPrivilégium szint: Tag";
                bot.sendMessage(chat_id, status, "Markdown");
        }
        else if (text == "/wifi") {
                String wifi = "";
                wifi += "A hálózat neve: HackerSpace";
                wifi += "\nA wifi jelszó: példajelszó";
                wifi += "\n\n U.i.: Ne DOS-old a netet :-D! \nKöszi!";
                bot.sendMessage(chat_id, wifi, "Markdown");
        }
        else if (text == "/open") {
                String open = "";
                open = open + "\nAz ajtó ideiglenesen kinyitva!";
                bot.sendMessage(chat_id, open, "Markdown");
        }
        else{
                String asd = "Ilyen paranacs vagy nem létezik, vagy nincsen megfelelő jogosultsága a futtatáshoz!";
                bot.sendMessage(chat_id, asd, "Markdown");
        }
}
void isDeveloperPrivilege(String text, String chat_id){
        if (debug){
                Serial.print(" Privilege level: Developer\n");
                Serial.println("--------------------------------------------------------------");
        }
        if (text=="/help") {
                String help = "";
                help = help + "/help : Az összes parancs kiíratása \n";
                help = help + "/status : Az eszköz állapotának lekérdezése\n";
                help = help + "/wifi : A jelenlegi WiFi jelszó lekérdezése\n";
                help = help + "/open : Az ajtó kinyitása 3mp-ig\n";
                bot.sendMessage(chat_id, help, "Markdown");
                help = " ";
                help += "\n--------------------------------------------------------------------";
                help += "\n                   CSAK FEJLESZTŐKNEK!!!";
                help += "\n--------------------------------------------------------------------";
                help += "\n/reset : NodeMCU resetelése ";
                help += "\n/ota : NodeMCU frissítési üzemmódba állítása";
                help += "\n!!!Frissítési mód aktiválása után kártyás és telegrammos beléptetés nem lehetésges!!!";
                help += "\n--------------------------------------------------------------------";
                bot.sendMessage(chat_id, help, "Markdown");
        }
        else if (text == "/status") {
                String status = "";
                ip_address = WiFi.localIP().toString();
                subnet_mask = WiFi.subnetMask().toString();
                gateway = WiFi.gatewayIP().toString();
                channel = WiFi.channel();

                time_t now = time(nullptr);
                status = status + ctime(&now) + "\n\n";
                status = status +"\nKapcsolódott wifi: "+ ssid;
                status = status +"\nWifi csatorna: " + channel;
                status = status +"\nAz eszköz IP címe: " + ip_address;
                status = status +"\nAlhálózati maszk: " + subnet_mask;
                status = status +"\nRouter IP: " + gateway;
                if (lockOverride) status = status + "\nlockOverride: AKTÍV";
                else status = status + "\nlockOverride: AKTÍV";
                status = status + "\nPrivilégium szint: Fejlesztő";
                bot.sendMessage(chat_id, status, "Markdown");
                status = "";
                status += "\n-------------------------------------------------------------------";
                status += "\n       INFORMÁCIÓ A FEJLESZTŐKNEK";//HACK:Modilos felületre méretezve XD!
                status += "\n-------------------------------------------------------------------";
                status += "\nKód verzió: 0.1 Hyper Alpha";
                status = status + "\nKód mérete: " + ESP.getSketchSize();
                status = status + "\nSzabad hely: " + ESP.getFreeSketchSpace();
                status = status + "\nÚj firmware feltöltése OTA-n keresztül: ";
                if (ESP.getSketchSize() > ESP.getFreeSketchSpace()) status += "NEM LEHETSÉGES! :-(";
                else status += "LEHETSÉGES :-)";
                status = status + "\nBoot verzió: " + ESP.getBootVersion()+"v";
                status = status + "\nCore verzió: " + ESP.getCoreVersion()+"v";
                status = status + "\nSDK verzió: " + ESP.getSdkVersion();
                status = status + "\nA kódról készített MD5 kivonat: \n|" + ESP.getSketchMD5()+ "|";
                status += "\n-----------------------------------------------------------------------";
                bot.sendMessage(chat_id, status, "Markdown");
        }
        else if (text == "/open") {
                String open = "";
                open = open + "\nAz ajtó ideiglenesen kinyitva!";
                bot.sendMessage(chat_id, open, "Markdown");
        }
        else if (text == "/restart") {
                String restart = "FIGYELEM: EZ EGY VESZÉLYES PARANCS!";
                bot.sendMessage(chat_id, restart, "Markdown");
                ESP.restart();
        }

        else if (text == "/reset") {
                String reset = "FIGYELEM: EZ EGY VESZÉLYES PARANCS!";
                bot.sendMessage(chat_id, reset, "Markdown");
                ESP.reset();
        }
        else if (text == "/read") {
                EEPROM.begin(512);

                bot.sendMessage(chat_id, String(EEPROM.read(0)-48), "Markdown");
                EEPROM.write(0, '0');
                EEPROM.write(1, '0');
                EEPROM.commit();
        }
        else if (text == "/ota") {
                EEPROM.begin(512);
                if (String(EEPROM.read(1)-48)=="0") {
                        String ota = "OTA mod aktiválva!";
                        ota = ota + "\nA jelnlegi IP cím: " + WiFi.localIP();
                        bot.sendMessage(chat_id, ota, "Markdown");

                        EEPROM.begin(512);
                        EEPROM.write(1, '1');//NOTE:Boot loop ellen
                        EEPROM.write(0, '1');//NOTE:Update kapcsoló
                        EEPROM.commit();

                        delay(1000);
                        ESP.reset();
                }
                else   {
                        EEPROM.begin(512);
                        EEPROM.write(1, '0');//NOTE:Boot loop ellen
                        EEPROM.commit();
                }
        }

        else{
                String asd = "Ilyen paranacs vagy nem létezik, vagy nincsen megfelelő jogosultsága a futtatáshoz!";
                bot.sendMessage(chat_id, asd, "Markdown");
        }
}
void isOwnerPrivilege(String text, String chat_id){
        if (debug){
                Serial.print(" Privilege level: Owner\n");
                Serial.println("--------------------------------------------------------------");
        }

        if (text=="/help") {
                String help = "";
                help = help + "/help : Az összes parancs kiíratása \n";
                help = help + "/status : Az eszköz állapotának lekérdezése\n";
                help = help + "/wifi : A jelenlegi WiFi jelszó lekérdezése\n";
                help = help + "/open : Az ajtó kinyitása 3mp-ig\n";
                help = help + "/lockOverride : Az ajtó kártyás beléptetésének felülbírálása";
                bot.sendMessage(chat_id, help, "Markdown");
        }
        else if (text == "/status") {
                String status = "";
                time_t now = time(nullptr);
                status = status + ctime(&now) + "\n\n";
                status = status +"\n\nKapcsolódott wifi: "+ ssid;
                status = status +"\nWifi csatorna: " + channel;
                status = status +"\nAz eszköz IP címe: " + ip_address;
                status = status +"\nAlhálózati maszk: " + subnet_mask;
                status = status +"\nRouter IP: " + gateway;
                if (lockOverride) status = status + "lockOverride: AKTÍV";
                else status = status + "lockOverride: NEM AKTÍV";
                status = status + "\nPrivilégium szint: Tulajdonos";
                bot.sendMessage(chat_id, status, "Markdown");
        }
        else if (text == "/wifi") {
                String wifi = "";
                wifi = wifi + "\nIgen, van wifi! Ezért működöm én is!";
                wifi = wifi + "\nTe is szeretnéd tudni a wifi jelszót?";
                wifi = wifi + "\n Írd be, hogy /wifipass, talán ez segít!";
                bot.sendMessage(chat_id, wifi, "Markdown");
        }
        else if (text == "/wifipass") {
                String wifi = "";
                wifi = wifi + "\nTe aztán valóban szeretnéd azt a wifi jelszót!";
                wifi = wifi + "\nBiztosan kell neked?? Ugyan mi olyan fontos, hogy internetre kapcsolódjál?";
                wifi = wifi + "\nDe te tudod:/wifiaccess";
                bot.sendMessage(chat_id, wifi, "Markdown");
        }
        else if (text == "/wifiaccess") {
                String wifi = "Wifi pass:12345678";
                wifi = wifi + "\n Na itt van! Használd egészséggel!";
                bot.sendMessage(chat_id, wifi, "Markdown");
        }
        else if (text == "/open") {
                String open = "";
                open = open + "\nAz ajtó ideiglenesen kinyitva!";
                bot.sendMessage(chat_id, open, "Markdown");
        }
        else if (text == "/lockOverride") {
                String lock="";
                lock = lock + "\nAz ajtó bezárva tartása!!";
                lock = lock + "\nFIGYELEM: A JELENLEGI FUNKCIÓ AKTIVÁLÁSA UTÁN NEM LEHET KÁRTYÁVAL KINYITNI AZ AJTÓT!!!";
                lock = lock + "\nAZ /open PARANCS FELÜLÍRJA A lockOverride-OT!";
                lockOverride = true;
                bot.sendMessage(chat_id, lock, "Markdown");
        }
        else if (text.substring(0,7) == "/msg2all") {
                String text = "";
                isMessage2AllUser(text,chat_id);
                bot.sendMessage(chat_id, "Üzenet elküldve!", "Markdown");
        }

        else if (text == "/restart") {
                String restart = "FIGYELEM: EZ EGY VESZÉLYES PARANCS!";
                bot.sendMessage(chat_id, restart, "Markdown");
                ESP.restart();
        }
        else if (text == "/reset") {
                String reset = "FIGYELEM: EZ EGY VESZÉLYES PARANCS!";
                bot.sendMessage(chat_id, reset, "Markdown");
                ESP.reset();
        }
        else{
                String asd = "Ilyen paranacs nem létezik!";
                bot.sendMessage(chat_id, asd, "Markdown");
        }
}
void handleNewMessages(int numNewMessages) {
        for(int i=0; i<numNewMessages; i++) {
                String chat_id = String(bot.messages[i].chat_id);
                String text = bot.messages[i].text;
                bot.sendChatAction(chat_id, "typing");

                if (debug){
                        time_t now = time(nullptr);
                        Serial.println("\n\n--------------------------------------------------------------");
                        Serial.print("| "+ String(ctime(&now)));
                        Serial.print("| Chat Id: " + chat_id + " Message: " + text);
                }


                bool sw = true;
                if (sw == true) {
                        for (int i = 0; i < sizeof(dev); i++) {
                                if (chat_id == dev[i]) {
                                        isDeveloperPrivilege(text, chat_id);
                                        sw = false;
                                }
                        }
                }
                if (sw == true) {
                        for (int i = 0; i < sizeof(owner); i++) {
                                if (chat_id == owner[i]) {
                                        isOwnerPrivilege(text, chat_id);
                                        sw = false;
                                }
                        }
                }
                if (sw == true) {
                        for (int i = 0; i < sizeof(member); i++) {
                                if (chat_id == member[i]) {
                                        isMemberPrivilege(text, chat_id);
                                        sw = false;
                                }
                        }
                }
                if (sw == true) {

                        isGuestPrivilege(text, chat_id);


                }

        }
}
////////////////////////////////////////////////////////////////////////////////
void setup() {

        SPI.begin();         // SPI inicializálás
        mfrc522.PCD_Init();    // MFRC522 kártya inicializálás
        WiFi.begin(ssid, password);

        wifiConnection();
        if (debug) {
                Serial.begin(115200);
                mfrc522.PCD_DumpVersionToSerial();
        }
        configTime(timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");//ctime(&now)-val lehet lekérni
        while (!time(nullptr)) {
                delay(500);
        }
        pinMode(DEADBOLT, INPUT);
        if(String(EEPROM.read(0)-48)=="1"){
                bot.sendMessage(dev[0], "OTA loop entered", "Markdown");
                EEPROM.begin(512);
                EEPROM.write(1, '0');
                EEPROM.commit();
                bot.sendMessage(dev[0], "EEPROM changed", "Markdown");
                delay(3000);
                   ArduinoOTA.setHostname("myesp8266");
                   ArduinoOTA.setPort(8266);

                   ArduinoOTA.onStart([]() {
                        String type;
                        if (ArduinoOTA.getCommand() == U_FLASH)
                                type = "sketch";
                        else // U_SPIFFS
                                type = "filesystem";

                        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                        Serial.println("Start updating " + type);
                   });
                   ArduinoOTA.onEnd([]() {
                        Serial.println("\nEnd");
                   });
                   ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
                        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                   });
                   ArduinoOTA.onError([](ota_error_t error) {
                        Serial.printf("Error[%u]: ", error);
                        if (debug) {


                        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                        else if (error == OTA_END_ERROR) Serial.println("End Failed");}
                });

                   ArduinoOTA.begin();
                   bot.sendMessage(dev[0], "Arduino Begin succesfull", "Markdown");
                   OTAInProgress = true;
   }

}
void loop() {
        if(OTAInProgress==true){
                ArduinoOTA.handle();
        }
        else{
        if (WiFi.status() !=  WL_CONNECTED) {//Újra csatlakozás
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
        if ( !mfrc522.PICC_IsNewCardPresent()) return;

        //Kártya érzékelése
        if ( !mfrc522.PICC_ReadCardSerial()) return;


        for (byte i = 0; i < 4; i++) cardUid += String(mfrc522.uid.uidByte[i]);    //UID beolvasása









        sector_key = 0;
        block = 0;
        cardUid="";
}
}
