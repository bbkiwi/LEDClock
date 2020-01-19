//************* Wol Clock by Jon Fuge *mod by bbkiwi *****************************

//************* Declare included libraries ******************************
#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>
//#include <NeoPixelBus.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

//************* Declare structures ******************************
//Create structure for LED RGB information
struct RGB {
  byte r, g, b;
};

//Create structure for time information
struct TIME {
  byte Hour, Minute;
};

//************* Editable Options ******************************
//The colour of the "12" to give visual reference to the top
const RGB Twelve = {0, 0, 255}; // { 128, 0, 128 }; //purple
//The colour of the "quarters" 3, 6 & 9 to give visual reference
const RGB Quarters = { 64, 0, 40 }; //purple
//The colour of the "divisions" 1,2,4,5,7,8,10 & 11 to give visual reference
const RGB Divisions = { 8, 0, 5 }; //purple
//All the other pixels with no information
const RGB Background = { 1, 3, 10 };//blue

//The Hour hand
const RGB Hour = { 0, 100, 0 };//blue 100
//The Minute hand
const RGB Minute = { 64, 32, 0 };//orange medium
//The Second hand
const RGB Second = { 32, 16, 0 };//orange dim

// Make clock go forwards or backwards (dependant on hardware)
const char ClockGoBackwards = 0;

//Set brightness by time for night and day mode
const TIME WeekNight = {20, 00}; // Night time to go dim
const TIME WeekMorning = {6, 15}; //Morning time to go bright
const TIME WeekendNight = {21, 30}; // Night time to go dim
const TIME WeekendMorning = {9, 30}; //Morning time to go bright

const int day_brightness = 50;
const int night_brightness = 16;

//Set your timezone in hours difference rom GMT
const int hours_Offset_From_GMT = 12; 

//Set your wifi details so the board can connect and get the time from the internet
const char* ssid     = "Test Station";
const char* password = "testpassword";
IPAddress staticIP(192,168,1,229);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

const int TopOfClock = 20; // to make given pixel the top
byte SetClock;


WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);
unsigned long int update_interval_secs = 3601; 
NTPClient timeClient(ntpUDP, "nz.pool.ntp.org", hours_Offset_From_GMT * 3600, update_interval_secs * 1000);
// Which pin on the ESP8266 is connected to the NeoPixels?
#define PIN            3 // This is the D9 pin

//************* Declare user functions ******************************
void Draw_Clock(time_t t, byte Phase);
int ClockCorrect(int Pixel);
void SetBrightness(time_t t);
void SetClockFromNTP ();
bool IsDst();

//************* Declare NeoPixel ******************************
//NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> pixels(60);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);

//************* Setup function for Wol_Clock ******************************
void setup() {
  // NOTE no delay after Serial.begin and next three WiFi cmds
  // see  https://github.com/esp8266/Arduino/issues/3489
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.config(staticIP, gateway, subnet);
  WiFi.begin(ssid, password); // Try to connect to WiFi
  
  pixels.begin(); // This initializes the NeoPixel library.
  Draw_Clock(0, 1); // Just draw a blank clock
  Draw_Clock(0, 2); // Draw the clock background
  Serial.println("ClockLED1");
  Serial.print("Connecting");
  while ( WiFi.status() != WL_CONNECTED ) 
  {
    Serial.print(".");
    delay ( 500 ); // keep waiting until we successfully connect to the WiFi
  }
//  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
//    Serial.println("Connection Failed! Rebooting...");
//    delay(5000);
//    ESP.restart();
//  }
  Serial.println();
  Serial.print("WiFi Connected, IP:");
  Serial.println(WiFi.localIP());
  Draw_Clock(0, 3); // Add the quater hour indicators

  SetClockFromNTP(); // get the time from the NTP server with timezone correction

    // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
 });
  ArduinoOTA.begin();
  Serial.println("Ready");
}

void SetClockFromNTP ()
{
  timeClient.update(); // get the time from the NTP server if last update was longer ago than update_interval
  setTime(timeClient.getEpochTime()); // Set the system time from the clock
  if (IsDst())
    adjustTime((hours_Offset_From_GMT + 1) * 3600); // offset the system time with the user defined timezone (3600 seconds in an hour)
  else
    adjustTime(hours_Offset_From_GMT * 3600); // offset the system time with the user defined timezone (3600 seconds in an hour)
}

bool IsDst()
{
  if (month() < 3 || month() > 10)  return true; 
  if (month() > 3 && month() < 10)  return false; 

  int previousSunday = day() - weekday();

  if (month() == 3) return previousSunday >= 24;
  if (month() == 10) return previousSunday < 24;

  return false; // this line never gonna happend
}

time_t prevDisplay = 0; // when the digital clock was displayed

//************* Main program loop for Wol_Clock ******************************
bool ota_flag = true;
uint16_t time_elapsed = 0;
void loop() {
  if(ota_flag)
  {
      Serial.println("50 seconds to do OTA, then clock will start");
      while(time_elapsed < 50000)
    {
      ArduinoOTA.handle();
      time_elapsed = millis();
      delay(10);
//      Serial.println(time_elapsed);  
//      Serial.println("OTA OK");  
    }
    ota_flag = false;
  }
  
  time_t t = now(); // Get the current time
  if (now() != prevDisplay) { //update the display only if time has changed
    prevDisplay = now();
    digitalClockDisplay();
  }
  Draw_Clock(t, 4); // Draw the whole clock face with hours minutes and seconds

  SetClockFromNTP();
  
//  // Bug could be at start of hour but internal clock is fast, so clock will be set back
//  //  before noon. Then will hit the hour again and call SetClockFromNTP again. 
//  //  Even if internal clock is only out by a few sec, the minute may still be zero
//  //  So SetClockFromNTP still may get called
//  if (minute(t) == 0) // at the start of each hour, update the time from the time server
//    if (SetClock == 1)
//    {
//      SetClockFromNTP(); // get the time from the NTP server with timezone correction
//      SetClock = 0;
//    }
//    else
//    {
//      delay(200); // Just wait for 0.1 seconds
//      SetClock = 1;
//    }
}


void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

//************* Functions to draw the clock ******************************
void Draw_Clock(time_t t, byte Phase)
{
  if (Phase <= 0)
    for (int i = 0; i < 60; i++)
      pixels.setPixelColor(ClockCorrect(i), pixels.Color(0, 0, 0)); // for Phase = 0 or less, all pixels are black

  if (Phase >= 1)
    for (int i = 0; i < 60; i++)
      pixels.setPixelColor(ClockCorrect(i), pixels.Color(Background.r, Background.g, Background.b)); // for Phase = 1 or more, draw minutes with Background colour

  if (Phase >= 2)
    for (int i = 0; i < 60; i = i + 5)
      pixels.setPixelColor(ClockCorrect(i), pixels.Color(Divisions.r, Divisions.g, Divisions.b)); // for Phase = 2 or more, draw 5 minute divisions

  if (Phase >= 3) {
    for (int i = 0; i < 60; i = i + 15)
      pixels.setPixelColor(ClockCorrect(i), pixels.Color(Quarters.r, Quarters.g, Quarters.b)); // for Phase = 3 or more, draw 15 minute divisions
    pixels.setPixelColor(ClockCorrect(0), pixels.Color(Twelve.r, Twelve.g, Twelve.b)); // for Phase = 3 and above, draw 12 o'clock indicator
  }

  if (Phase >= 4) {
    pixels.setPixelColor(ClockCorrect(second(t)), pixels.Color(Second.r, Second.g, Second.b)); // draw the second hand first
    if (second() % 2)
      pixels.setPixelColor(ClockCorrect(minute(t)), pixels.Color(Minute.r, Minute.g, Minute.b)); // to help identification, minute hand flshes between normal and half intensity
    else
      pixels.setPixelColor(ClockCorrect(minute(t)), pixels.Color(Minute.r / 2, Minute.g / 2, Minute.b / 2)); // lower intensity minute hand

    pixels.setPixelColor(ClockCorrect(((hour(t) % 12) * 5) + minute(t) / 12), pixels.Color(Hour.r, Hour.g, Hour.b)); // draw the hour hand last
  }

  SetBrightness(t); // Set the clock brightness dependant on the time
  pixels.show(); // show all the pixels
}

//************* Function to set the clock brightness ******************************
void SetBrightness(time_t t)
{
  int NowHour = hour(t);
  int NowMinute = minute(t);

  if ((weekday() >= 2) && (weekday() <= 6))
    if ((NowHour > WeekNight.Hour) || ((NowHour == WeekNight.Hour) && (NowMinute >= WeekNight.Minute)) || ((NowHour == WeekMorning.Hour) && (NowMinute <= WeekMorning.Minute)) || (NowHour < WeekMorning.Hour))
      pixels.setBrightness(night_brightness);
    else
      pixels.setBrightness(day_brightness);
  else if ((NowHour > WeekendNight.Hour) || ((NowHour == WeekendNight.Hour) && (NowMinute >= WeekendNight.Minute)) || ((NowHour == WeekendMorning.Hour) && (NowMinute <= WeekendMorning.Minute)) || (NowHour < WeekendMorning.Hour))
    pixels.setBrightness(night_brightness);
  else
    pixels.setBrightness(day_brightness);
}

//************* This function reverses the pixel order ******************************
//              and ajusts top of clock
int ClockCorrect(int Pixel)
{
  Pixel = (Pixel + TopOfClock) % 60;
  if (ClockGoBackwards == 1)
    return ((60 - Pixel +30) % 60); // my first attempt at clock driving had it going backwards :)
  else
    return (Pixel);
}
