#include <WiFi.h>
#include "time.h"
#include <LiquidCrystal_I2C.h>
#define Blink_interval  1000
#define Blink_interval_heart  300
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


void printLocalTime()
{
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
