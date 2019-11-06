#include <WiFi.h>
#include "time.h"
#include <LiquidCrystal_I2C.h>
#include <PN532_HSU.h>
#include <PN532.h>

#define Blink_interval  1000
#define Blink_interval_heart  300
#define RXD2 16
#define TXD2 17

PN532_HSU pn532hsu(Serial2);
PN532 nfc(pn532hsu);

unsigned long previousMillis = 0, previousMillis_heart = 0;
bool BlankOnOff = false, BlankOnOff_heart = false;

//deklarasi wifi
const char* ssid       = "od3ng";
const char* password   = "0d3n9bro";

const char* ntpServer = "asia.pool.ntp.org";
const long  gmtOffset_sec = 7 * 60 * 60; // UTC+7
const int   daylightOffset_sec = 3600;

//display
const String title = "Smart Parking Polinema";

//deklarasi LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Make custom characters:
byte Heart[] = {
  B00000,
  B01010,
  B11111,
  B11111,
  B01110,
  B00100,
  B00000,
  B00000
};

void clearMessage(String message) {
  lcd.clear();
  lcd.home();
  lcd.print(message);
}

//setup reader
void setupReader() {
  clearMessage("Init Reader...");
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Hello!");
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  // configure board to read RFID tags
  nfc.SAMConfig();

  Serial.println("Waiting for an ISO14443A Card ...");
  clearMessage("Init Berhasil...");
  delay(1000);
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  scrollText(0, title, 400, 16);

  lcd.setCursor(0, 1);
  char timeStringBuff[50]; //50 chars should be enough
  strftime(timeStringBuff, sizeof(timeStringBuff), "%D %H:%M", &timeinfo);
  Serial.println(timeStringBuff);
  String asString(timeStringBuff);

  lcd.print(asString);
  //  lcd.print(asString.substring(0, 11));
  //  BlinkText(":", 11, 1);
  //  Serial.println(asString.substring(0, 11));
  //  lcd.setCursor(12, 1);
  //  Serial.println(asString.substring(12, 14));
  //  lcd.print(asString.substring(12, 14));
  BlinkText();
}

void BlinkText() {
  unsigned long currentMillis = millis();
  unsigned long currentMillisDiff = currentMillis - previousMillis_heart;
  Serial.println(currentMillisDiff);
  if (currentMillisDiff > Blink_interval_heart)   {
    previousMillis_heart = currentMillis;
    if (BlankOnOff_heart) {
      lcd.setCursor(15, 1);
      lcd.print(" ");
    }
    else {
      lcd.createChar(0, Heart);
      lcd.setCursor(15, 1);
      lcd.write(0);
    } //End Else
    BlankOnOff_heart = !BlankOnOff_heart;
  }
}

void BlinkText(char *msgtxt, int col, int row) {
  unsigned long currentMillis = millis();
  lcd.setCursor(col, row);
  //  if (currentMillis - previousMillis > Blink_interval)   {
  if (true)   {
    previousMillis = currentMillis;
    if (BlankOnOff) {
      for (int i = 0; i < strlen(msgtxt); i++) {
        lcd.print(" ");
      } //End For
    } // End If
    else {
      lcd.print(msgtxt);
    } //End Else
    BlankOnOff = !BlankOnOff;
  } //End If
} // End BlinkText

void displayHeart() {
  lcd.createChar(0, Heart);
  lcd.setCursor(15, 1);
  lcd.write(0);
}

void setup()
{

  //setup lcd
  lcd.init();
  lcd.backlight();

  Serial.begin(115200);

  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  WiFi.begin(ssid, password);
  delay(100);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  setupReader();

  lcd.clear();

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  printLocalTime();

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void loop()
{
  readCard();
  delay(500);
  printLocalTime();
}

// Function to scroll text
// The function acepts the following arguments:
// row: row number where the text will be displayed
// message: message to scroll
// delayTime: delay between each character shifting
// lcdColumns: number of columns of your LCD
void scrollText(int row, String message, int delayTime, int lcdColumns) {
  for (int i = 0; i < lcdColumns; i++) {
    message = " " + message;
  }
  message = message + " ";
  for (int pos = 0; pos < message.length(); pos++) {
    lcd.setCursor(0, row);
    lcd.print(message.substring(pos, pos + lcdColumns));
    delay(delayTime);
  }
}

void readCard() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: "); Serial.print(uidLength, DEC); Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");

    if (uidLength == 4)
    {
      // We probably have a Mifare Classic card ...
      Serial.println("Seems to be a Mifare Classic card (4 byte UID)");

      // Now we need to try to authenticate it for read/write access
      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
      Serial.println("Trying to authenticate block 4 with default KEYA value");
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

      // Start with block 4 (the first block of sector 1) since sector 0
      // contains the manufacturer data and it's probably better just
      // to leave it alone unless you know what you're doing
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 1, 0, keya);

      if (success)
      {
        Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
        uint8_t data[16];

        // If you want to write something to block 4 to test with, uncomment
        // the following line and this text should be read back in a minute
        // data = { 'a', 'd', 'a', 'f', 'r', 'u', 'i', 't', '.', 'c', 'o', 'm', 0, 0, 0, 0};
        // success = nfc.mifareclassic_WriteDataBlock (4, data);

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(1, data);

        if (success)
        {
          // Data seems to have been read ... spit it out
          Serial.println("Reading Block 4:");
          nfc.PrintHexChar(data, 16);
          String str = (char*)data;
          Serial.println("Nama");
          Serial.println(str);
          clearMessage(str);
          Serial.println("");

          // Wait a bit before reading the card again
          delay(1000);
        }
        else
        {
          Serial.println("Ooops ... unable to read the requested block.  Try another key?");
        }
      }
      else
      {
        Serial.println("Ooops ... authentication failed: Try another key?");
      }
    }

    if (uidLength == 7)
    {
      // We probably have a Mifare Ultralight card ...
      Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");

      // Try to read the first general-purpose user page (#4)
      Serial.println("Reading page 4");
      uint8_t data[32];
      success = nfc.mifareultralight_ReadPage (4, data);
      if (success)
      {
        // Data seems to have been read ... spit it out
        nfc.PrintHexChar(data, 4);
        Serial.println("");

        // Wait a bit before reading the card again
        delay(1000);
      }
      else
      {
        Serial.println("Ooops ... unable to read the requested page!?");
      }
    }
  }
}
