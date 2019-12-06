#include<iostream>
#include <WiFi.h>
#include <time.h>
#include <TimeLib.h>
#include <LiquidCrystal_I2C.h>
#include <PN532_HSU.h>
#include <PN532.h>
#include <SPI.h>
#include <LedMatrix.h>

#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

using namespace std;

#define Blink_interval  1000
#define Blink_interval_heart  100
#define RXD2 16
#define TXD2 17

#define NUMBER_OF_DEVICES 4 //number of led matrix connect in series
#define CS_PIN 15
#define CLK_PIN 14
#define MISO_PIN 2 //we do not use this pin just fill to match constructor
#define MOSI_PIN 12

LedMatrix ledMatrix = LedMatrix(NUMBER_OF_DEVICES, CLK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

PN532_HSU pn532hsu(Serial2);
PN532 nfc(pn532hsu);

unsigned long previousMillis = 0, previousMillis_heart = 0;
bool BlankOnOff = false, BlankOnOff_heart = false;

//deklarasi wifi
const char* ssid = "Noureen Cell";
const char* password = "lepengdados";

const char* ntp_server = "asia.pool.ntp.org";
const char* mqtt_server = "192.168.8.101";

unsigned int add7Hour = 7 * 3600;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntp_server);

//display
const String title = "Smart Parking";

//deklarasi LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

//deklarasi task
TaskHandle_t Task2;

String serialNumber;
unsigned long unixTime;

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

void displayBerhasil(String msg, String msg2) {
  lcd.clear();
  lcd.home();
  lcd.print(msg);
  lcd.setCursor(0, 1);
  lcd.print(msg2);
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
  nfc.setPassiveActivationRetries(0x01);
  nfc.SAMConfig();

  Serial.println("Waiting for an ISO14443A Card ...");
  clearMessage("Init Berhasil...");
  delay(1000);
}

void setupRunningText() {
  clearMessage("Init LED...");
  ledMatrix.init();
  ledMatrix.setText("Politeknik Negeri Malang");
  ledMatrix.setAlternateDisplayOrientation(1);
  clearMessage("Init Berhasil...");
  delay(1000);
}

void printLocalTime() {

  lcd.print(title);

  lcd.setCursor(0, 1);
  unixTime += add7Hour;
  time_t ti = unixTime;

  char timeStringBuff[50] = {}; //50 chars should be enough
  strftime(timeStringBuff, sizeof(timeStringBuff), "%D %H:%M ", localtime(&ti));
  String asString(timeStringBuff);
  //  Serial.println(asString);
  lcd.print(asString);
  BlinkText();
}

void BlinkText() {
  unsigned long currentMillis = millis();
  unsigned long currentMillisDiff = currentMillis - previousMillis_heart;
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

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/parkir/sum");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {

  //setup lcd
  lcd.init();
  lcd.backlight();

  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  setupReader();

  setupRunningText();

  lcd.clear();

  unixTime = timeClient.getEpochTime();
  printLocalTime();

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
    Task2code,   /* Task function. */
    "Task2",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task2,      /* Task handle to keep track of created task */
    1);          /* pin task to core 1 */
  delay(500);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  Serial.println();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  //  timeClient.setTimeOffset(7 * 3600);
  timeClient.setUpdateInterval(5 * 60 * 1000); //ms
}

//Task2code: blinks an LED every 700 ms
void Task2code( void * pvParameters ) {
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    //running-text
    ledMatrix.clear();
    ledMatrix.scrollTextLeft();
    ledMatrix.drawText();
    ledMatrix.commit();
    delay(100);
  }
}

void loop() {
  unixTime = timeClient.getEpochTime();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  readCard();
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
    char strSerial[8] = "";
    array_to_string(uid, 4, strSerial);
    String cardSerial = String(strSerial);
    Serial.printf("Serialnya memory %s", serialNumber);
    Serial.println();
    Serial.printf("Serialnya %s", cardSerial);
    Serial.println();
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: "); Serial.print(uidLength, DEC); Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");

    String str = (char*)uid;
    if (!serialNumber.equals(cardSerial)) {
      Serial.println("Serial different");
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
        success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);

        if (success)
        {
          Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
          uint8_t temp[48];

          uint8_t index16 = 0;
          for (int i = 4; i < 7; i++) {
            uint8_t data[16];
            success = nfc.mifareclassic_ReadDataBlock(i, data);
            copy_n(begin(data), 16, begin(temp) + index16);
            index16 += 16;
          }

          if (success)
          {
            Serial.println(unixTime);
            String hexDate =  String(unixTime, HEX);
            hexDate.toUpperCase();
            Serial.println(hexDate);
            char arrDate[hexDate.length() + 1];
            hexDate.toCharArray(arrDate, hexDate.length() + 1);
            uint8_t tglByte[4];
            uint8_t tglByteTemp[4] = {};
            hex2ByteArray(arrDate, 4, tglByte);
            copy(begin(tglByte), end(tglByte), tglByteTemp);
            nfc.PrintHexChar(tglByte, 4);
            nfc.PrintHexChar(tglByteTemp, 4);
            // Data seems to have been read ... spit it out
            Serial.println("Reading Block 4:");
            nfc.PrintHexChar(temp, 48);


            uint8_t tempWrite[48] = {};
            uint8_t tempWrite16[16] = {};
            copy(begin(temp), end(temp), tempWrite);
            copy_n(begin(tglByte), 4, begin(tempWrite) + 10);
            copy_n(begin(tempWrite), 16, begin(tempWrite16));
            nfc.PrintHexChar(tempWrite, 48);
            nfc.PrintHexChar(tempWrite16, 16);
            Serial.println("");


            uint8_t nopol[10] = {};
            uint8_t tanggal[4] = {};
            uint8_t nip[18] = {};
            uint8_t expired[4] = {};
            uint8_t stMasuk = {};
            uint8_t kodeGate = {};
            uint8_t stKartu = {};

            //          copy nopol
            copy_n(begin(temp), 10, begin(nopol));
            //copy tanggal
            copy_n(begin(temp) + 10, 4, begin(tanggal));
            stMasuk = temp[14];
            kodeGate = temp[15];
            //          copy nip
            copy_n(begin(temp) + 16, 18, begin(nip));
            //copy expired
            copy_n(begin(temp) + 34, 4, begin(expired));
            stKartu = temp[38];

            Serial.print("Nopol: ");
            nfc.PrintHexChar(nopol, 10);
            String strNopol = (char*)nopol;
            String noKend = hex2Ascii(nopol, 10);
            Serial.println(noKend);
            Serial.print("Tanggal: " );
            nfc.PrintHexChar(tanggal, 4);
            Serial.println();
            Serial.print("Nip: " );
            nfc.PrintHexChar(nip, 18);
            String strNip = (char*)nip;
            displayBerhasil(strNip, strNopol);
            delay(1500);
            Serial.println();
            Serial.print("Expired: " );
            nfc.PrintHexChar(expired, 4);
            Serial.println();
            Serial.print("Status Masuk: " );
            Serial.println(stMasuk);
            Serial.print("Status Kartu: " );
            Serial.println(stKartu);
            Serial.print("Kode Gate: " );
            Serial.println(kodeGate);
            Serial.print("Tanggal long: " );
            char str[8] = "";
            array_to_string(tanggal, 4, str);
            String tglTrx = String(str);
            Serial.println(tglTrx);

            uint64_t result = getUInt64fromHex(str);
            unsigned long longTanggal = result;
            Serial.println(String(longTanggal));
            unsigned int add7Hour = 7 * 3600;
            result += add7Hour;

            char buff[32];
            sprintf(buff, "%02d.%02d.%02d %02d:%02d:%02d", day(result), month(result), year(result), hour(result), minute(result), second(result));
            Serial.println(buff);

            char strExpired[8] = "";
            array_to_string(expired, 4, strExpired);
            String tglExpired = String(strExpired);
            Serial.println(tglExpired);
            uint64_t resultExpired = getUInt64fromHex(strExpired);

            resultExpired += add7Hour;

            char buffExpired[32];
            sprintf(buffExpired, "%02d.%02d.%02d %02d:%02d:%02d", day(result), month(result), year(result), hour(result), minute(result), second(result));

            printf(" bignum = %lld\n", result);
            String tgl = uint64ToString(result);
            Serial.println(tgl);
            time_t ti = tgl.toInt();
            char timeStringBuff[50]; //50 chars should be enough
            strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", localtime(&ti));
            Serial.println(timeStringBuff);

            //          unsigned long offset_days = 7;    // 7 hours
            //          unsigned long t_unix_date1, t_unix_date2;
            //          t_unix_date1 = result;
            //          Serial.print("t_unix_date1: ");
            //          Serial.println(t_unix_date1);
            //          //          offset_days = offset_days * 86400;    // convert number of days to seconds
            //          offset_days = 7 * 3600;
            //          t_unix_date2 = result + offset_days;
            //          Serial.print("t_unix_date2: ");
            //          Serial.println(t_unix_date2);
            //          printf("Date1: %4d-%02d-%02d %02d:%02d:%02d\n", year(t_unix_date1), month(t_unix_date1), day(t_unix_date1), hour(t_unix_date1), minute(t_unix_date1), second(t_unix_date1));
            //          printf("Date2: %4d-%02d-%02d %02d:%02d:%02d\n", year(t_unix_date2), month(t_unix_date2), day(t_unix_date2), hour(t_unix_date2), minute(t_unix_date2), second(t_unix_date2));

            if (!client.connected()) {
              reconnect();
            }
            client.loop();

            char strUid[8] = "";
            array_to_string(uid, 4, strUid);

            String stringSend = "";
            String cardUid = String(strUid);
            stringSend += cardUid;
            stringSend += ";";

            stringSend += noKend;
            stringSend += ";";
            stringSend += hexDate;
            stringSend += ";";

            String cardNip = hex2Ascii(nip, 18);
            stringSend += cardNip;
            stringSend += ";";

            stringSend += tglExpired;
            stringSend += ";";

            String cardStMasuk = String(stMasuk);
            stringSend += cardStMasuk;
            stringSend += ";";

            String cardStKartu = String(stKartu);
            stringSend += cardStKartu;
            stringSend += ";";

            String cardKodeGate = String(kodeGate);
            stringSend += cardKodeGate;

            success = nfc.mifareclassic_WriteDataBlock (4, tempWrite16);
            if (success) {
              Serial.println(stringSend);
              char publish[stringSend.length() + 1];
              stringSend.toCharArray(publish, stringSend.length() + 1);
              client.publish("parkir/card", publish);

              // Wait a bit before reading the card again
              delay(500);
            } else {
              Serial.println("Write block 4 gagal...");
            }


            // Wait a bit before reading the card again
            delay(1500);
            lcd.clear();
            lcd.setCursor(0, 0);
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
    serialNumber = cardSerial;
  } else {
    serialNumber = "";
  }
  printLocalTime();
}

String uint64ToString(uint64_t input) {
  String result = "";
  uint8_t base = 10;

  do {
    char c = input % base;
    input /= base;

    if (c < 10)
      c += '0';
    else
      c += 'A' - 10;
    result = c + result;
  } while (input);
  return result;
}

void array_to_string(byte array[], unsigned int len, char buffer[])
{
  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
}

uint64_t getUInt64fromHex(char const *str)
{
  uint64_t accumulator = 0;
  for (size_t i = 0 ; isxdigit((unsigned char)str[i]) ; ++i)
  {
    char c = str[i];
    accumulator *= 16;
    if (isdigit(c)) /* '0' .. '9'*/
      accumulator += c - '0';
    else if (isupper(c)) /* 'A' .. 'F'*/
      accumulator += c - 'A' + 10;
    else /* 'a' .. 'f'*/
      accumulator += c - 'a' + 10;

  }

  return accumulator;
}

String hex2Ascii(uint8_t data[], uint8_t len)
{
  char dt[len] = "";
  byte index = 0;
  for (uint8_t i = 0; i < len; i++) {
    char c = data[i] & 0xFF;
    if (c <= 0x1f || c > 0x7f) {
      dt[i] = '.';
    } else {
      dt[i] = c;
    }
    index++;
    dt[index] = '\0';
  }
  return String(dt);
}

void hex2ByteArray(char data[], uint8_t len, uint8_t result[]) {
  char temp[3];
  for (int i = 0; i < len; i++) {
    uint8_t extract;
    char a = data[2 * i];
    char b = data[2 * i + 1];
    extract = convertCharToHex(a) << 4 | convertCharToHex(b);
    result[i] = extract;
  }
}

char convertCharToHex(char ch)
{
  char returnType;
  switch (ch)
  {
    case '0':
      returnType = 0;
      break;
    case  '1' :
      returnType = 1;
      break;
    case  '2':
      returnType = 2;
      break;
    case  '3':
      returnType = 3;
      break;
    case  '4' :
      returnType = 4;
      break;
    case  '5':
      returnType = 5;
      break;
    case  '6':
      returnType = 6;
      break;
    case  '7':
      returnType = 7;
      break;
    case  '8':
      returnType = 8;
      break;
    case  '9':
      returnType = 9;
      break;
    case  'A':
      returnType = 10;
      break;
    case  'B':
      returnType = 11;
      break;
    case  'C':
      returnType = 12;
      break;
    case  'D':
      returnType = 13;
      break;
    case  'E':
      returnType = 14;
      break;
    case  'F' :
      returnType = 15;
      break;
    default:
      returnType = 0;
      break;
  }
  return returnType;
}
