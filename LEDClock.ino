/*********
   Using for Bedroom, Iris, and GBT clocks Feb 2024
   Only Bedroom has mic and speaker
  Using tttapa examples with clock code by Jon Fuge *mod by bbkiwi
  //https://github.com/PaulStoffregen/Time

   2 Jan 2023
   Now C:\Users\Bill\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.0.2
       and ../libraries/ESP8266mDNS/src/ESP8266mDNS.h has been changed
*/

//NOTE if want to upload sketch data SPIFFS via OTA can NOT set OTA password
/************* Declare included libraries ******************************/
#include <NTPClient.h>
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>
#include <string.h>
#include "pitches.h"
#include "sunset.h"

//  MUST have this file which defines homeSSID and homePW
#include "localwificonfig.h"

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

//#define BEDROOM_CLOCK
//#define IRIS_CLOCK
#define TEST_CLOCK
//#define GBT_CLOCK

//#if defined BEDROOM_CLOCK || defined TEST_CLOCK
#if defined BEDROOM_CLOCK
#define MUSIC
#define HASMIC
#endif

/* Cass Bay */
#define LATITUDE        -43.601131
#define LONGITUDE       172.689831
#define CST_OFFSET      12
#define DST_OFFSET      13

SunSet sun;

ESP8266WiFiMulti wifiMulti;       // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);       // create a web server on port 80
WebSocketsServer webSocket(81);    // create a websocket server on port 81
uint8_t websocketId_num = 0;

File fsUploadFile;                                    // a File variable to temporarily store the received file

const char *ssid = "LED Clock Access Point"; // The name of the Wi-Fi network that will be created
const char *password = "ledclock";   // The password required to connect to it, leave blank for an open network

// OTA and mDns must have same name

#ifdef BEDROOM_CLOCK
const char *OTAandMdnsName = "LEDClock";           // A name and a password for the OTA and mDns service
#endif
#ifdef IRIS_CLOCK
const char *OTAandMdnsName = "IrisLEDClock";           // A name and a password for the OTA and mDns service
#endif
#ifdef TEST_CLOCK
const char *OTAandMdnsName = "TestLEDClock";           // A name and a password for the OTA and mDns service
#endif
#ifdef GBT_CLOCK
const char *OTAandMdnsName = "TestLEDClock";           // A name and a password for the OTA and mDns service
#endif

const char *OTAPassword = "ledclock";

// must be longer than longest message
char buf[400];


time_t currentTime;

//************* Declare structures ******************************
//Create structure for LED RGB information
struct RGB {
  byte r, g, b;
};

//Create structure for time information
struct TIME {
  byte Hour, Minute;
};

#define NUM_DISP_OPTIONS 5
RGB SliderColor;
// 5 Options for display index 0 normal day, 4 night,
//The colour of the "12" to give visual reference to the top
RGB Twelve[NUM_DISP_OPTIONS] = {{0, 0, 60}, {0, 0, 0}, {0, 0, 60}, {0, 0, 60}, {0, 0, 0}, };
//The colour of the "quarters" 3, 6 & 9 to give visual reference
RGB Quarters[NUM_DISP_OPTIONS] = {{0, 0, 60}, {0, 0, 0}, {0, 0, 60}, {0, 0, 60}, {0, 0, 0}, };
//The colour of the "divisions" 1,2,4,5,7,8,10 & 11 to give visual reference
RGB Divisions[NUM_DISP_OPTIONS] = {{ 0, 0, 6 }, { 0, 0, 0 }, { 0, 0, 6 }, { 0, 0, 6 }, { 0, 0, 0 } };
//All the other pixels with no information
RGB Background[NUM_DISP_OPTIONS] = {{ 0, 0, 0 }, {255, 0, 0}};

//The Hour hand
RGB Hour[NUM_DISP_OPTIONS] = {{ 0, 255, 0 }, { 0, 0, 255 }, { 0, 255, 0 }, { 0, 255, 0 }, {100, 0, 0}}; //green
//The Minute hand
RGB Minute[NUM_DISP_OPTIONS] = {{ 255, 255, 0 }, { 0, 0, 255 }, { 255, 255, 0 }, { 255, 255, 0 }, {0, 0, 0} }; //yellow
//The Second hand
RGB Second[NUM_DISP_OPTIONS] = {{ 0, 0, 255 }, { 0, 0, 0 }, { 0, 0, 255 }, { 0, 0, 255 }, { 0, 0, 0 }};

// Make clock go forwards or backwards (dependant on hardware)
#ifdef BEDROOM_CLOCK
bool ClockGoBackwards = true;
#endif
#if defined IRIS_CLOCK || defined TEST_CLOCK || defined GBT_CLOCK
bool ClockGoBackwards = false;
#endif


int day_disp_ind = 0;
bool minute_blink[NUM_DISP_OPTIONS] = {true, true};
int minute_width[NUM_DISP_OPTIONS] = {2, 4, 2, 2, -1}; //-1 means don't show
int hour_width[NUM_DISP_OPTIONS] = {3, 5, 3, 3, 0};
int second_width[NUM_DISP_OPTIONS] = {0, -1, 0, 0, -1};

//Set brightness by time for night and day mode
//TODO set via gui or have auto set
TIME WeekNight = {18, 00}; // Night time to go dim
TIME WeekMorning = {7, 15}; //Morning time to go bright
TIME WeekendNight = {18, 00}; // Night time to go dim
TIME WeekendMorning = {7, 15}; //Morning time to go bright
TIME Sunrise;
TIME Sunset;
TIME CivilSunrise;
TIME CivilSunset;
TIME NauticalSunrise;
TIME NauticalSunset;
TIME AstroSunrise;
TIME AstroSunset;


byte day_brightness = 127;
byte night_brightness = 16;
// Can force night mode during day
//     and day mode during night
// force_night gets set to false when becomes night
// force_day get set to false wehn become day
bool force_night = false;
bool force_day = false;

//Set your timezone in hours difference rom GMT
int hours_Offset_From_GMT = 12;


String daysOfWeek[8] = {"dummy", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String monthNames[13] = {"dummy", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
bool ota_flag = true;
bool sound_alarm_flag = false;
int light_alarm_num = 0;
int light_alarm_parm1 = 0;
int light_alarm_parm2 = 0;
int light_alarm_parm3 = 0;
int light_alarm_parm4 = 0;
int light_alarm_parm5 = 0;
int light_alarm_parm6 = 0;
bool led_color_alarm_flag = false;
uint32_t led_color_alarm_rgb;
//tmElements_t alarmTime;
tmElements_t calcTime = {0};

//typedef enum {
//  ONCE,
//  DAILY,
//  WEEKLY
//} alarmType;

struct ALARM {
  bool alarmSet = false;
  int alarmType; // if neg only display during daylight mode
  int parm1;
  int parm2;
  int parm3;
  int parm4;
  int parm5;
  int parm6;
  uint16_t duration;
  uint32_t repeat;
  tmElements_t alarmTime;
};

#define NUM_ALARMS 5
ALARM alarmInfo[NUM_ALARMS];
//alarmInfo.alarmSet = false;


//bool alarmSet = false;

uint16_t time_elapsed = 0;

#ifdef TEST_CLOCK
int TopOfClock = 0; // to make given pixel the top
#else
int TopOfClock = 15; // to make given pixel the top
#endif


#ifdef MUSIC
// notes in the melody for the sound alarm:
int melody[] = { // Shave and a hair cut (3x) two bits terminate with -1 = STOP
  NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, REST,
  NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, REST,
  NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, REST, NOTE_B4, NOTE_C5, STOP
};
int whole_note_duration = 1000; // milliseconds
// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 8, 8, 4, 4, 4, 4, 8, 8, 4, 4, 4, 4, 4
};
#endif

WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);
unsigned long int update_interval_secs = 3601;
NTPClient timeClient(ntpUDP, "nz.pool.ntp.org", hours_Offset_From_GMT * 3600, update_interval_secs * 1000);

// Which pin on the ESP8266 is connected to the NeoPixels?
#ifdef BEDROOM_CLOCK
#define NEOPIXEL_PIN 3      // For Bedroom clock This is the D9 pin
#endif
#if defined IRIS_CLOCK || defined TEST_CLOCK || defined GBT_CLOCK
#define NEOPIXEL_PIN 4      // This is the D2 pin
#endif

#define PIEZO_PIN 5         // This is D1
#define analogInPin  A0     // ESP8266 Analog Pin ADC0 = A0

//************* Declare user functions ******************************
void Draw_Clock(time_t t, byte Phase);
int ClockCorrect(int Pixel);
void SetBrightness(time_t t);
bool SetClockFromNTP();
bool IsDst();
bool IsDay();
void limited_delay(int d);


//************* Declare NeoPixel ******************************
//Using 1M WS2812B 5050 RGB Non-Waterproof 60 LED Strip
// use NEO_KHZ800 but maybe 400 makes wifi more stable???
#define NUM_LEDS 60
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
bool ClockInitialized = false;
time_t nextCalcTime;
time_t nextAlarmTime;

const int ESP_BUILTIN_LED = 2;

void setup() {
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");
  sun.setPosition(LATITUDE, LONGITUDE, DST_OFFSET);
  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  startOTA();                  // Start the OTA service
  startSPIFFS();               // Start the SPIFFS and list all contents

  // Initialize alarmTime(s) to default (now)
  for (int alarm_ind = 0; alarm_ind < NUM_ALARMS; alarm_ind++) {
    breakTime(now(), alarmInfo[alarm_ind].alarmTime);
    sprintf(buf, "Default alarmInfo[%d] set=%d, type=%d, parm1=%d, parm2=%d, parm3=%d,  parm4=%d, parm5=%d, parm6=%d, duration=%d, repeat=%d\n %d:%02d:%02d %s %d %s %d", alarm_ind, alarmInfo[alarm_ind].alarmSet, alarmInfo[alarm_ind].alarmType,
            alarmInfo[alarm_ind].parm1, alarmInfo[alarm_ind].parm2, alarmInfo[alarm_ind].parm3,
            alarmInfo[alarm_ind].parm4, alarmInfo[alarm_ind].parm5, alarmInfo[alarm_ind].parm6,
            alarmInfo[alarm_ind].duration, alarmInfo[alarm_ind].repeat,
            hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)),
            second(makeTime(alarmInfo[alarm_ind].alarmTime)), daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), day(makeTime(alarmInfo[alarm_ind].alarmTime)),
            monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), year(makeTime(alarmInfo[alarm_ind].alarmTime)));
    //Serial.println();
    Serial.println(buf);
  }

  if (!loadConfig()) {
    Serial.println("Failed to load config will use defaults");
    // use default parameters
    // attempt to save in configuration file
    if (!saveConfig()) {
      Serial.println("Failed to save config");
    } else {
      Serial.println("default Config saved");
    }
  } else {
    // will have loaded the saved parameters
    Serial.println("Config loaded");
    for (int alarm_ind = 0; alarm_ind < NUM_ALARMS; alarm_ind++) {
      sprintf(buf, "Loaded alarmInfo[%d] set=%d, type=%d, parm1=%d, parm2=%d, parm3=%d,  parm4=%d, parm5=%d, parm6=%d, duration=%d, repeat=%d\n %d:%02d:%02d %s %d %s %d", alarm_ind, alarmInfo[alarm_ind].alarmSet, alarmInfo[alarm_ind].alarmType,
              alarmInfo[alarm_ind].parm1, alarmInfo[alarm_ind].parm2, alarmInfo[alarm_ind].parm3,
              alarmInfo[alarm_ind].parm4, alarmInfo[alarm_ind].parm5, alarmInfo[alarm_ind].parm6,
              alarmInfo[alarm_ind].duration, alarmInfo[alarm_ind].repeat,
              hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)),
              second(makeTime(alarmInfo[alarm_ind].alarmTime)), daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), day(makeTime(alarmInfo[alarm_ind].alarmTime)),
              monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), year(makeTime(alarmInfo[alarm_ind].alarmTime)));
      //Serial.println();
      Serial.println(buf);
    }
  }
  startWebSocket();            // Start a WebSocket server
  startMDNS();                 // Start the mDNS responder
  startServer();               // Start a HTTP server with a file read handler and an upload handler
  strip.begin(); // This initializes the NeoPixel library.
  colorAll(strip.Color(127, 0, 0), 1000, now());
  Draw_Clock(0, 3); // Add the quater hour indicators
  ClockInitialized = SetClockFromNTP(); //// sync first time, updates system clock and adjust it for daylight savings
  calcSun();
  randomSeed(now());
  //pinMode(ESP_BUILTIN_LED, OUTPUT);
}

/*___________________LOOP__________________________________________________________*/
time_t prevDisplay = 0; // when the digital clock was displayed

void loop() {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events
  MDNS.update();                              // must have above as well

  if (light_alarm_num)  {
    show_alarm_pattern(light_alarm_num, 10000, light_alarm_parm1, light_alarm_parm2, light_alarm_parm3, light_alarm_parm4, light_alarm_parm5, light_alarm_parm6);
    light_alarm_num = 0;
  }

  if (led_color_alarm_flag)  {
    colorAll(led_color_alarm_rgb, 1000, now());
    led_color_alarm_flag = false;
  }

#ifdef MUSIC
  if (sound_alarm_flag) {
    Serial.println("sound flag on\n");
    playsong(melody, noteDurations, whole_note_duration, PIEZO_PIN);
    //BUG? if use below generates tone, but then clock produces spurious leds
    // lighting up on ring (obvious in night mode)
    // something to do with timer2 changed???
    //tone(PIEZO_PIN, 300, 1000);
    // This code does seem to cause the problem
    //  will try implementing playsong to use noTone as
    // below which is not causeing spurious leds lighting
    //tone(PIEZO_PIN, 300);
    //delay(1000);
    //noTone(PIEZO_PIN);
    sound_alarm_flag = false;
  }
#endif

  // Check for alarms
  // Note if multiple alarms scheduled for same time they will go consecutively
  //
  for (int alarm_ind = 0; alarm_ind < NUM_ALARMS; alarm_ind++) {
    if (alarmInfo[alarm_ind].alarmSet && prevDisplay >= makeTime(alarmInfo[alarm_ind].alarmTime))
    {
      if (prevDisplay - 10 < makeTime(alarmInfo[alarm_ind].alarmTime)) {
        // only show the alarm if close to set time
        // this prevents alarm from going off on a restart where configured alarm is in past
        currentTime = now();
        //if (IsDay(currentTime) || alarmInfo[alarm_ind].alarmType > 0) {
        if (force_day || ( not force_night and IsDay(currentTime)) || alarmInfo[alarm_ind].alarmType > 0) {
          show_alarm_pattern(abs(alarmInfo[alarm_ind].alarmType), alarmInfo[alarm_ind].duration,
                             alarmInfo[alarm_ind].parm1, alarmInfo[alarm_ind].parm2, alarmInfo[alarm_ind].parm3,
                             alarmInfo[alarm_ind].parm4, alarmInfo[alarm_ind].parm5, alarmInfo[alarm_ind].parm6);
          // redraw clock now to restore clock leds (thus not leaving alarm display on past its duration)
          Draw_Clock(now(), 4); // Draw the whole clock face with hours minutes and seconds
        } else {
          sprintf(buf, "Display supressed at night or via pause");
          Serial.println();
          Serial.println(buf);
          webSocket.sendTXT(websocketId_num, buf);
        }
        sprintf(buf, "Alarm type %d (%d, %d, %d, %d, %d, %d) at %d:%02d:%02d %s %d %s %d", alarmInfo[alarm_ind].alarmType,
                alarmInfo[alarm_ind].parm1, alarmInfo[alarm_ind].parm2, alarmInfo[alarm_ind].parm3,
                alarmInfo[alarm_ind].parm4, alarmInfo[alarm_ind].parm5, alarmInfo[alarm_ind].parm6,
                hour(currentTime), minute(currentTime), second(currentTime), daysOfWeek[weekday(currentTime)].c_str(), day(currentTime),  monthNames[month(currentTime)].c_str(), year(currentTime));
        Serial.println();
        Serial.println(buf);
        webSocket.sendTXT(websocketId_num, buf);
      }

      if (alarmInfo[alarm_ind].repeat <= 0)
      {
        alarmInfo[alarm_ind].alarmSet = false;
      } else { // update next repeat
        nextAlarmTime = makeTime(alarmInfo[alarm_ind].alarmTime);
        currentTime = now();
        while (nextAlarmTime <= currentTime) nextAlarmTime += alarmInfo[alarm_ind].repeat;
        breakTime(nextAlarmTime, alarmInfo[alarm_ind].alarmTime);
        sprintf(buf, "Next Alarm [%d] %d:%02d:%02d %s %d %s %d", alarm_ind, hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)),
                second(makeTime(alarmInfo[alarm_ind].alarmTime)), daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), day(makeTime(alarmInfo[alarm_ind].alarmTime)),
                monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), year(makeTime(alarmInfo[alarm_ind].alarmTime)));
        Serial.println();
        Serial.println(buf);
        webSocket.sendTXT(websocketId_num, buf);
      }
    }
  }

#ifdef HASMIC
  // read the analog in value coming from microphone
  int sensorValue = analogRead(analogInPin);
  if (sensorValue > 900 ) {
    Serial.println(sensorValue);
    light_alarm_num = random(1, 40);
    time_elapsed = 0;
  }
#endif

  time_t t = now(); // Get the current time seconds
  if (now() != prevDisplay) { //update the display only if time has changed
    prevDisplay = now();
    if (second() == 0)
      digitalClockDisplay();
    else
      Serial.print('-');
    // Reset forcing
    force_day = force_day and not IsDay(t);
    force_night = force_night and IsDay(t);
    Draw_Clock(t, 4); // Draw the whole clock face with hours minutes and seconds
    ClockInitialized |= SetClockFromNTP(); // sync initially then every update_interval_secs seconds, updates system clock and adjust it for daylight savings
  }

  // Check if new day and recalculate sunSet etc.
  if (prevDisplay >= makeTime(calcTime))
  {
    calcSun(); // computes sun rise and sun set, updates calcTime
  }
  delay(10); // needed to keep wifi going
}

void limited_delay(int d) {
  delay(min(abs(d), 10000));
}

void show_alarm_pattern(byte light_alarm_num, uint16_t duration, int parm1, int parm2, int parm3, int parm4, int parm5, int parm6) {
  sprintf(buf, "Pattern %d, duration = %d, p1 = %d, p2 = %d, p3 = %d, p4 = %d, p5 = %d, p6 = %d", light_alarm_num, duration,  parm1, parm2, parm3,  parm4, parm5, parm6);
  Serial.println(buf);

  int isecond = second(now());
  int iminute = (60 * minute(now()) + isecond + 30) / 60; // round to nearest minute
  int ihour = ((hour(now()) % 12) * 5) + (iminute + 6) / 12; // round to nearest LED

  switch (light_alarm_num) {
    case 1:  // rainbow 10 wait
      showlights(duration, -1, -1, -1, -1, -1, -1, 10, -1, now());
      break;
    case 2: // rainbow 5 wait
      showlights(duration, -1, -1, -1, -1, -1, -1, 5, -1, now());
      break;
    case 3: // rainbow 1 wait
      showlights(duration, -1, -1, -1, -1, -1, -1, 1, -1, now());
      break;
    case 4: // ranbow2 0 wait
      showlights(duration, -1, -1, -1, -1, -1, -1, 0, -1, now());
      break;
    case 5: // 3 worms fast 1 wait
      moveworms(parm1, parm2, now(), duration);
      break;
    case 6: // 3  worms slow 5 wait
      moveworms(5, 3, now(), duration);
      break;

    //cellularAutomata(int wait, uint8_t rule, long pixelhue, time_t t, uint16_t duration)
    case 7:
      cellularAutomata(250, 26, 30, 60, random(65535), now(), duration);
      break;
    case 8:
      cellularAutomata(250, 26, 26, 26, random(65535), now(), duration);
      break;
    case 9:
      cellularAutomata(250, 30, 30, 30, random(65535), now(), duration);
      break;
    case 10:
      cellularAutomata(250, 60, 60, 60, random(65535), now(), duration);
      break;
    case 11:
      // goes black cellularAutomata(250, 104, 104, 104, random(65535), now(), duration);
      cellularAutomata(250, 110, 110, 110, random(65535), now(), duration);
      break;
    case 12:
      cellularAutomata(50, 26, random(65535), now(), duration);
      break;
    case 13:
      cellularAutomata(50, 30, random(65535), now(), duration);
      break;
    case 14:
      cellularAutomata(50, 45, random(65535), now(), duration);
      break;
    case 15:
      cellularAutomata(50, 57, random(65535), now(), duration);
      break;
    case 16:
      cellularAutomata(50, 60, random(65535), now(), duration);
      break;
    case 17:
      cellularAutomata(50, 73, random(65535), now(), duration);
      break;
    case 18:
      cellularAutomata(50, 110, random(65535), now(), duration);
      break;
    case 19: // fire
      fire(now(), duration);
      break;
    // fireflys
    case 20:
      firefly(1000, 5, 0, 65535, 256, 255, 256,  255, 256, now(), duration);
      break;
    case 21:
      firefly(parm1, parm2, 0, 65535, parm3, 255, 256,  255, 256, now(), duration);
      break;
    case 22:
      firefly(1000, 5, 32000, 32001, 0, 255, 256,  255, 256, now(), duration);
      break;
    case 23:
      firefly(1000, 5, 0, 65535, 33000, 255, 256,  255, 256, now(), duration);
      break;
    case 24:
      firefly(100, 1, 0, 65535, 256, 0, 1,  1, 256, now(), duration);
      break;
    // (partial) rainbows
    case 25:
      //void rainbow(int wait, int embedding, long firsthue, int hueinc,  int ncolorloop, int ncolorfrac, int nodepix, time_t t, uint16_t duration) {
      rainbow(0, parm1, 0, parm2, 16, parm3, ihour, now(), duration); // full rainbow ring rotating
      //rainbow(0, 1, 0, 256, 1, 1, ihour, now(), duration); // full rainbow ring rotating
      break;
    case 26:
      color_wipe(parm1, parm2, parm3, parm4 == 0 ? 0 : 65336 / parm4 , parm5 == 0 ? 0 : 100 / parm5 , parm6, ihour, now(), duration); // color_wipe
      // old color_wipe(20, parm1, 0, parm2 == 0 ? 0 : 65336 / parm2 , parm3 == 0 ? 0 : 100 / parm3 , 0, ihour, now(), duration); // color_wipe
      //rainbow(0, 1, 0, 32, 1, 1, ihour, now(), duration);  // full rainbox ring rotating 8 times slower
      break;
    case 27:
      rainbow(0, 1, 0, 256, 4, 1, ihour, now(), duration); // 4 full rainbows in ring rotating
      break;
    case 28:
      rainbow(0, 1, 0, 32, 4, 1, ihour, now(), duration); // 4 full rainbows in ring rotating 8 times slower
      break;
    case 29:
      rainbow(0, 1, 32000, 32, 4, 1, ihour, now(), duration); // 4 full rainbows as above starting different place
      break;
    case 30:
      rainbow(0, 2, 0, 256, 4, 1, ihour, now(), duration); //  4 full and 4 reverse flowing from ihour led
      break;
    case 31:
      rainbow(0, 2, 0, 256, 1, 4, ihour, now(), duration); //  1/4 rainbox and its reverse flowing from ihour led
      break;
    case 32:
      rainbow(0, 2, 0, 256, 1, 4, ihour, now(), duration); //  1/4 rainbox and its reverse flowing from ihour led
      break;
    case 33:
      rainbow(0, 2, 0, 16, 1, 4, ihour, now(), duration); //   1/64 rainbox and its reverse flowing from ihour led
      break;
    case 34:
      rainbow(0, 2, 32000, 16, 1, 4, ihour, now(), duration); // start diff place 1/64 rainbox and its reverse flowing from ihour led
      break;
    case 35:
      rainbow(0, 3, 0, 256, 1, 1, ihour, now(), duration);
      break;
    case 36:
      rainbow(0, 3, 0, 256, 4, 1, ihour, now(), duration);
      break;
    case 37:
      rainbow(0, 4, 0, 256, 1, 1, ihour, now(), duration);
      break;
    case 38:
      rainbow(0, 4, 0, 256, 4, 1, ihour, now(), duration);
      break;
    case 39:
      rainbow(0, 5, 0, 256, 1, 1, ihour, now(), duration);
      break;
    // color wipes
    case 40:
      showlights(duration, 1, 1, 1, -1, -1, -1, -1, -1, now());
      break;
    case 41:
      showlights(duration, 2, 2, 2, -1, -1, -1, -1, -1, now());
      break;
    case 42:
      showlights(duration, 5, 5, 5, -1, -1, -1, -1, -1, now());
      break;
    default:
      rainbow(0, 5, 0, 256, 4, 1, ihour, now(), duration);
  }
}

#ifdef MUSIC
void playsong(int * melody, int * noteDurations, int whole_note_duration, int pin) {
  int maxnumNotes = 2000;
  uint32_t time_start;
  uint32_t time_finish_tone;
  uint32_t time_finish;
  Serial.println("sound alarm, then clock will start");
  for (int thisNote = 0; thisNote < maxnumNotes; thisNote++) {
    time_start = millis();
    if (melody[thisNote] == STOP) break; //STOP is defined to be -1
    // to calculate the note duration, take whole note durations divided by the note type.
    //e.g. quarter note = whole_note_duration / 4, eighth note = whole_note_duration/8, etc.
    int noteDuration = whole_note_duration / noteDurations[thisNote];
    time_finish_tone = time_start + noteDuration;
    if (melody[thisNote] > 30) {
      tone(pin, melody[thisNote], noteDuration);
      // alternative not giving duration as param to tone, as caused LEDs to fire afterwards
      // but seems if just give noTone at end of melody that is ok too
      //tone(pin, melody[thisNote]);
      //while (millis() < time_finish_tone);
      //noTone(pin);
    }
    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    time_finish = time_start + pauseBetweenNotes;
    while (millis() < time_finish);
  }
  //IMPORTANT to call noTone after melody is done
  //  otherwise get funny signal going to LED ring aftwerwards
  noTone(pin);
}
#endif

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/
// Taken from ConfigFile example
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  // Safety code incase somehow had too big file
  if (size > 4096) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  // Deserialize day_disp_ind
  day_disp_ind = doc["day_disp_ind"];
  // Deserialize night_brightness
  night_brightness = doc["night_brightness"];
  night_brightness = min(night_brightness, day_brightness);


  int alarm_ind = 0;
  for (JsonObject alarm : doc["alarms"].as<JsonArray>()) {
    //TODO if alarm_ind >=NUM_ALARMS abort
    alarmInfo[alarm_ind].alarmSet = alarm["alarmSet"]; // true, true, true, true, true
    alarmInfo[alarm_ind].alarmType = alarm["alarmType"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].parm1 = alarm["p1"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].parm2 = alarm["p2"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].parm3 = alarm["p3"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].parm4 = alarm["p4"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].parm5 = alarm["p5"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].parm6 = alarm["p6"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].duration = alarm["duration"]; // 10000, 10000, 10000, 10000, 10000
    alarmInfo[alarm_ind].repeat = alarm["repeat"]; // 86400, 86400, 86400, 86400, 86400

    JsonObject alarm_alarmTime = alarm["alarmTime"];
    alarmInfo[alarm_ind].alarmTime.Second = alarm_alarmTime["sec"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].alarmTime.Minute = alarm_alarmTime["min"]; // 0, 0, 0, 0, 0
    alarmInfo[alarm_ind].alarmTime.Hour = alarm_alarmTime["hour"]; // 12, 12, 12, 12, 12
    alarmInfo[alarm_ind].alarmTime.Day = alarm_alarmTime["date"]; // 7, 7, 7, 7, 7
    alarmInfo[alarm_ind].alarmTime.Month = alarm_alarmTime["month"]; // 1, 1, 1, 1, 1
    alarmInfo[alarm_ind].alarmTime.Year = alarm_alarmTime["year"]; // 53, 53, 53, 53, 53
    alarm_ind++;
  }

  // Deserialize Twelve
  JsonArray twelveArray = doc["Twelve"];
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = twelveArray[i];
    Twelve[i].r = rgbObj["r"];
    Twelve[i].g = rgbObj["g"];
    Twelve[i].b = rgbObj["b"];
  }

  // Deserialize Quarters
  JsonArray quartersArray = doc["Quarters"];
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = quartersArray[i];
    Quarters[i].r = rgbObj["r"];
    Quarters[i].g = rgbObj["g"];
    Quarters[i].b = rgbObj["b"];
  }

  // Deserialize Divisions
  JsonArray divisionsArray = doc["Divisions"];
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = divisionsArray[i];
    Divisions[i].r = rgbObj["r"];
    Divisions[i].g = rgbObj["g"];
    Divisions[i].b = rgbObj["b"];
  }

  // Deserialize Background
  JsonArray backgroundArray = doc["Background"];
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = backgroundArray[i];
    Background[i].r = rgbObj["r"];
    Background[i].g = rgbObj["g"];
    Background[i].b = rgbObj["b"];
  }

  // Deserialize Hour
  JsonArray hourArray = doc["Hour"];
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = hourArray[i];
    Hour[i].r = rgbObj["r"];
    Hour[i].g = rgbObj["g"];
    Hour[i].b = rgbObj["b"];
    hour_width[i] = rgbObj["width"];
  }

  // Deserialize Minute
  JsonArray minuteArray = doc["Minute"];
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = minuteArray[i];
    Minute[i].r = rgbObj["r"];
    Minute[i].g = rgbObj["g"];
    Minute[i].b = rgbObj["b"];
    minute_blink[i] = rgbObj["blink"];
    minute_width[i] = rgbObj["width"];
  }

  // Deserialize Second
  JsonArray secondArray = doc["Second"];
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = secondArray[i];
    Second[i].r = rgbObj["r"];
    Second[i].g = rgbObj["g"];
    Second[i].b = rgbObj["b"];
    second_width[i] = rgbObj["width"];
  }


  return true;
}

bool saveConfig() {

  DynamicJsonDocument doc(6144);

  // Serialize day_disp_ind
  doc["day_disp_ind"] = day_disp_ind;
  // Serialize night_brightness
  doc["night_brightness"] = night_brightness;

  // Serialize alarms
  JsonArray alarms = doc.createNestedArray("alarms");
  for (int alarm_ind = 0; alarm_ind < NUM_ALARMS; alarm_ind++) {
    JsonObject alarms_nested = alarms.createNestedObject();
    alarms_nested["alarmSet"] = alarmInfo[alarm_ind].alarmSet;
    alarms_nested["alarmType"] = alarmInfo[alarm_ind].alarmType;
    alarms_nested["p1"] = alarmInfo[alarm_ind].parm1;
    alarms_nested["p2"] = alarmInfo[alarm_ind].parm2;
    alarms_nested["p3"] = alarmInfo[alarm_ind].parm3;
    alarms_nested["p4"] = alarmInfo[alarm_ind].parm4;
    alarms_nested["p5"] = alarmInfo[alarm_ind].parm5;
    alarms_nested["p6"] = alarmInfo[alarm_ind].parm6;
    alarms_nested["duration"] = alarmInfo[alarm_ind].duration;
    alarms_nested["repeat"] = alarmInfo[alarm_ind].repeat;

    JsonObject alarms_nested_alarmTime = alarms_nested.createNestedObject("alarmTime");
    alarms_nested_alarmTime["sec"] = alarmInfo[alarm_ind].alarmTime.Second;
    alarms_nested_alarmTime["min"] = alarmInfo[alarm_ind].alarmTime.Minute;
    alarms_nested_alarmTime["hour"] = alarmInfo[alarm_ind].alarmTime.Hour;
    alarms_nested_alarmTime["date"] = alarmInfo[alarm_ind].alarmTime.Day;
    alarms_nested_alarmTime["month"] = alarmInfo[alarm_ind].alarmTime.Month;
    alarms_nested_alarmTime["year"] = alarmInfo[alarm_ind].alarmTime.Year;
  }

  // Serialize Twelve
  JsonArray twelveArray = doc.createNestedArray("Twelve");
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = twelveArray.createNestedObject();
    rgbObj["r"] = Twelve[i].r;
    rgbObj["g"] = Twelve[i].g;
    rgbObj["b"] = Twelve[i].b;
  }

  // Serialize Quarters
  JsonArray quartersArray = doc.createNestedArray("Quarters");
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = quartersArray.createNestedObject();
    rgbObj["r"] = Quarters[i].r;
    rgbObj["g"] = Quarters[i].g;
    rgbObj["b"] = Quarters[i].b;
  }

  // Serialize Divisions
  JsonArray divisionsArray = doc.createNestedArray("Divisions");
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = divisionsArray.createNestedObject();
    rgbObj["r"] = Divisions[i].r;
    rgbObj["g"] = Divisions[i].g;
    rgbObj["b"] = Divisions[i].b;
  }

  // Serialize Background
  JsonArray backgroundArray = doc.createNestedArray("Background");
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = backgroundArray.createNestedObject();
    rgbObj["r"] = Background[i].r;
    rgbObj["g"] = Background[i].g;
    rgbObj["b"] = Background[i].b;
  }

  // Serialize Hour
  JsonArray hourArray = doc.createNestedArray("Hour");
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = hourArray.createNestedObject();
    rgbObj["r"] = Hour[i].r;
    rgbObj["g"] = Hour[i].g;
    rgbObj["b"] = Hour[i].b;
    rgbObj["width"] = hour_width[i];
  }

  // Serialize Minute
  JsonArray minuteArray = doc.createNestedArray("Minute");
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = minuteArray.createNestedObject();
    rgbObj["r"] = Minute[i].r;
    rgbObj["g"] = Minute[i].g;
    rgbObj["b"] = Minute[i].b;
    rgbObj["blink"] = minute_blink[i];
    rgbObj["width"] = minute_width[i];
  }

  // Serialize Second
  JsonArray secondArray = doc.createNestedArray("Second");
  for (int i = 0; i < NUM_DISP_OPTIONS; ++i) {
    JsonObject rgbObj = secondArray.createNestedObject();
    rgbObj["r"] = Second[i].r;
    rgbObj["g"] = Second[i].g;
    rgbObj["b"] = Second[i].b;
    rgbObj["width"] = second_width[i];
  }

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  serializeJson(doc, configFile);
  return true;
}


void startWiFi() { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  wifiMulti.addAP(homeSSID, homePW);  // add Wi-Fi networks you want to connect to
  wifiMulti.addAP(homeSSID2, homePW2);
  //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED && WiFi.softAPgetStationNum() < 1) {  // Wait for the Wi-Fi to connect to station or a station connects to AP
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  if (WiFi.softAPgetStationNum() == 0) {     // If the ESP is connected to an AP
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());             // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
    WiFi.softAPdisconnect(false);             // Switch off soft AP mode
    Serial.print("So switching soft AP off");
  } else {                                   // If a station is connected to the ESP SoftAP
    Serial.print("Station connected to ESP8266 AP");
  }
  Serial.println("\r\n");
  delay(100);
}

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAandMdnsName);
  //Comment out if want to upload sketch data (SPIFFS) via OTA
  //ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
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
  Serial.println("OTA ready\r\n");
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(OTAandMdnsName);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(OTAandMdnsName);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler

  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  // server.on("/makeAP",[](){
  //    server.send(200,"text/plain", "Making AP http://192.168.4.1 ...");
  //    delay(1000);
  //    createAccessPoint();
  //  });
  //
  //  server.on("/APOff",[](){
  //    server.send(200,"text/plain", "Turn off AP ");
  //    delay(1000);
  //    WiFi.softAPdisconnect(true);
  //  });

  server.on("/restart", []() {
    server.send(200, "text/plain", "Restarting ...");
    delay(1000);
    ESP.restart();
  });

  //    server.on("/startota",[](){
  //      server.send(200,"text/plain", "Make OTA ready ...");
  //      delay(1000);
  //      ota_flag = true;
  //      time_elapsed = 0;
  //    });

  server.on("/lightalarm", []() {
    String numstr = server.arg("num");
    light_alarm_num = (strlen(numstr.c_str()) > 0) ? numstr.toInt() : 1;
    server.send(200, "text/plain", "Light alarm Starting ...");
    delay(1000);
    //light_alarm_num = random(1, 43);
    time_elapsed = 0;
  });

  server.on("/soundalarm", []() {
    server.send(200, "text/plain", "Sound alarm Starting ...");
    delay(1000);
    sound_alarm_flag = true;
    time_elapsed = 0;
  });

  server.on("/whattime", []() {
    sprintf(buf, "%d:%02d:%02d %s %d %s %d", hour(), minute(), second(), daysOfWeek[weekday()].c_str(), day(), monthNames[month()].c_str(), year());
    server.send(200, "text/plain", buf);
    delay(1000);
  });

  server.on("/setbright", setBright);

  server.on("/settime", settime);

  server.on("/setalarm", setalarmurl);

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.htm";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload() { // upload a new file to the SPIFFS
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
      if (SPIFFS.exists(pathWithGz))                     // version of that file must be deleted (if it exists)
        SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  Serial.println("Serial: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!SPIFFS.exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (SPIFFS.exists(path)) {
    return server.send(500, "text/plain", "FILE EXISTS");
  }
  File file = SPIFFS.open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, "text/plain", "CREATE FAILED");
  }
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    if (entry.name()[0] == '/') {
      output += &(entry.name()[1]);
    } else {
      output += entry.name();
    }
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
  //NOTE messages get queued up
  websocketId_num = num; // save so can send to websock from other places
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
#ifdef MUSIC
      sprintf(buf, "MUSIC");
      Serial.println();
      Serial.println(buf);
      webSocket.sendTXT(websocketId_num, buf);
#endif
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] payload: %s length: %d\n", num, payload, length);
      // first char of payload specifies action
      // #, V, B, t, q, d, H, M, s, D, F, G, P, L, W, A, a, S
      if (payload[0] == '#') {            // we get RGB data
        uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);   // decode rgb data
        int r = ((rgb >> 20) & 0x3FF);                     // 10 bits per color, so R: bits 20-29
        int g = ((rgb >> 10) & 0x3FF);                     // G: bits 10-19
        int b =          rgb & 0x3FF;                      // B: bits  0-9
        // r,g,b now from 0 to 1023
        sprintf(buf, "led color r = %d, g = %d, b = %d", r, g, b);
        // scale from 0 to 255
        SliderColor  = {r >> 2, g >> 2, b >> 2};
        //webSocket.sendTXT(num, buf);
        led_color_alarm_flag = true;
        led_color_alarm_rgb = strip.Color(r >> 2, g >> 2, b >> 2);
        //analogWrite(ESP_BUILTIN_LED, b); INTERFER with LED strip
        //Serial.printf("%d\n", b);
      } else if (payload[0] == 'V') {                      // browser sent V to save config file
        if (!saveConfig()) {
          Serial.println("Failed to save config");
          sprintf(buf, "Failed to save config");
          webSocket.sendTXT(num, buf);
        } else {
          Serial.println("Config saved");
          sprintf(buf, "Config saved");
          webSocket.sendTXT(num, buf);
        }
      } else if (payload[0] == 'B') {                      // browser sent B to set Background color from SliderColor
        Background[day_disp_ind] = SliderColor;
      } else if (payload[0] == 't') {                      // browser sent t to set Twelve color from SliderColor
        Twelve[day_disp_ind] = SliderColor;
      } else if (payload[0] == 'q') {                      // browser sent q to set Quarters color from SliderColor
        Quarters[day_disp_ind] = SliderColor;
      } else if (payload[0] == 'd') {                      // browser sent d to set Divisions color from SliderColor
        Divisions[day_disp_ind] = SliderColor;
      } else if (payload[0] == 'H') {                      // browser sent H to set Hour hand color from SliderColor
        hour_width[day_disp_ind] = payload[1] - '0';
        Hour[day_disp_ind] = SliderColor;
      } else if (payload[0] == 'M') {                      // browser sent M to set Minute hand color from SliderColor
        minute_width[day_disp_ind] = payload[3] - '0';
        minute_blink[day_disp_ind] = (payload[1] == '1');
        Minute[day_disp_ind] = SliderColor;
      } else if (payload[0] == 's') {                      // browser sent s to set Second hand color from SliderColor
        second_width[day_disp_ind] = payload[2] - '0';
        Second[day_disp_ind] = SliderColor;
      } else if (payload[0] == 'D') {                      // browser sent D to set disp_ind for daytime use
        day_disp_ind = payload[2] - '0';
      } else if (payload[0] == 'F') {                      // browser sent F to force_day
        force_day = true;
        force_night = false;
      } else if (payload[0] == 'G') {                      // browser sent G to  force_night
        force_night = true;
        force_day = false;
      } else if (payload[0] == 'P') {                      // the browser sends an P for pattern follow by type, parm1, ..., parm6
        //TODO why if light_alarm_num was declared byte did this blow up had to make int
        sscanf((char *) payload, "P%d %d %d %d %d %d %d", &light_alarm_num, &light_alarm_parm1, &light_alarm_parm2, &light_alarm_parm3, &light_alarm_parm4, &light_alarm_parm5, &light_alarm_parm6);
        //light_alarm_num = random(1, 40);
      } else if (payload[0] == 'L') {                      // the browser sends an L when the meLody effect is enabled
        sound_alarm_flag = true;
        //digitalWrite(ESP_BUILTIN_LED, 1);  // turn off the LED
      } else if (payload[0] == 'W') {                      // the browser sends an W for What time?
        sprintf(buf, "WHATTIME%d:%02d:%02d %s %d %s %d", hour(), minute(), second(), daysOfWeek[weekday()].c_str(), day(), monthNames[month()].c_str(), year());
        webSocket.sendTXT(num, buf);
        //digitalWrite(ESP_BUILTIN_LED, 0);  // turn on the LED


      } else if (payload[0] == 'R') {                      // the browser sends an R to reset alarm
        int alarm_ind;
        sscanf((char *) payload, "R%d", &alarm_ind);
        //PREVENT bad input
        if (alarm_ind >= 5) alarm_ind = 0;
        if (alarm_ind < 0) alarm_ind = 0;
        alarmInfo[alarm_ind].alarmSet = false;
        Serial.printf("%Reset alarm[%d]\n", alarm_ind);
        sprintf(buf, "Reset alarm[%d]", alarm_ind);
        webSocket.sendTXT(num, buf);


      } else if (payload[0] == 'A') {                      // the browser sends an A to set alarm
        char Aday[4]; //3 char
        char Amonth[4]; //3 char
        int  AmonthNum;
        int Adate;
        int Ayear;
        int Ahour;
        int Aminute;
        int alarm_ind;
        int alarmtype;
        int parm1;
        int parm2;
        int parm3;
        int parm4;
        int parm5;
        int parm6;
        int alarmrepeat;
        int alarmduration;

        //sprintf(buf, "Set Alarm for %s length: %d", payload, length);
        //webSocket.sendTXT(num, buf);
        sscanf((char *) payload, "A%d %d %d %d %d %d %d %d %d %d %d %s %s %2d %4d %2d:%2d", &alarm_ind, &alarmtype, &parm1, &parm2, &parm3,
               &parm4, &parm5, &parm6, &alarmrepeat, &alarmduration, &AmonthNum, Aday, Amonth, &Adate, &Ayear, &Ahour, &Aminute);
        //PREVENT bad input
        if (alarm_ind >= 5) alarm_ind = 0;
        if (alarm_ind < 0) alarm_ind = 0;


        Serial.printf("Set alarm[%d] for %s %s %2d %2d %4d %2d:%2d\n", alarm_ind, Aday, Amonth, Adate, AmonthNum + 1, Ayear, Ahour, Aminute);
        sprintf(buf, "Set alarm for %s %s %2d %2d %4d %2d:%2d", Aday, Amonth, Adate, AmonthNum + 1, Ayear, Ahour, Aminute);
        webSocket.sendTXT(num, buf);

        setalarm(alarm_ind, alarmtype, parm1, parm2, parm3, parm4, parm5, parm6, alarmduration, alarmrepeat, 0, Aminute, Ahour, Adate, AmonthNum + 1, Ayear);
        alarmInfo[alarm_ind].alarmSet = true;
        sprintf(buf, "Alarm[%d] set=%d, type=%d, parm1=%d, parm2=%d, parm3=%d, parm4=%d, parm5=%d, parm6=%d, duration=%d, repeat=%d\n %d:%02d:%02d %s %d %s %d", alarm_ind, alarmInfo[alarm_ind].alarmSet, alarmInfo[alarm_ind].alarmType,
                alarmInfo[alarm_ind].parm1, alarmInfo[alarm_ind].parm2, alarmInfo[alarm_ind].parm3,
                alarmInfo[alarm_ind].parm4, alarmInfo[alarm_ind].parm5, alarmInfo[alarm_ind].parm6,
                alarmInfo[alarm_ind].duration, alarmInfo[alarm_ind].repeat,
                hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)),
                second(makeTime(alarmInfo[alarm_ind].alarmTime)), daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), day(makeTime(alarmInfo[alarm_ind].alarmTime)),
                monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), year(makeTime(alarmInfo[alarm_ind].alarmTime)));
        webSocket.sendTXT(num, buf);

      } else if (payload[0] == 'a') {
        // populate by sending  Alarm info back
        int alarm_ind = payload[1] - '0';
        // send back info same order as payload when alarm defined
        sprintf(buf, "ALARMINFO,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s %s %2d %4d %2d:%2d:%2d", alarm_ind, alarmInfo[alarm_ind].alarmSet, alarmInfo[alarm_ind].alarmType,
                alarmInfo[alarm_ind].parm1, alarmInfo[alarm_ind].parm2, alarmInfo[alarm_ind].parm3,
                alarmInfo[alarm_ind].parm4, alarmInfo[alarm_ind].parm5, alarmInfo[alarm_ind].parm6,
                alarmInfo[alarm_ind].repeat, alarmInfo[alarm_ind].duration,
                daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(),
                monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(),
                day(makeTime(alarmInfo[alarm_ind].alarmTime)),
                year(makeTime(alarmInfo[alarm_ind].alarmTime)),
                hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)),
                second(makeTime(alarmInfo[alarm_ind].alarmTime)));
        webSocket.sendTXT(num, buf);

      } else if (payload[0] == 'S') {                      // the browser sends an S to compute sunsets
        Serial.printf("Compute Sunsets\n");
        calcSun();
      }
      break;
  }
}

void setalarm(int alarm_ind, int alarmtype, int p1, int p2, int p3, int p4, int p5, int p6,  uint16_t t, uint32_t r, uint8_t s, uint8_t m, uint8_t h, uint8_t d, uint8_t mth, uint16_t y) {
  alarmInfo[alarm_ind].alarmType = alarmtype;
  alarmInfo[alarm_ind].parm1 = p1;
  alarmInfo[alarm_ind].parm2 = p2;
  alarmInfo[alarm_ind].parm3 = p3;
  alarmInfo[alarm_ind].parm4 = p4;
  alarmInfo[alarm_ind].parm5 = p5;
  alarmInfo[alarm_ind].parm6 = p6;
  alarmInfo[alarm_ind].duration = t;
  alarmInfo[alarm_ind].repeat = r;
  alarmInfo[alarm_ind].alarmTime.Second = s;
  alarmInfo[alarm_ind].alarmTime.Minute = m;
  alarmInfo[alarm_ind].alarmTime.Hour = h;
  alarmInfo[alarm_ind].alarmTime.Day = d;
  alarmInfo[alarm_ind].alarmTime.Month = mth;
  //NOTE year is excess from 1970
  alarmInfo[alarm_ind].alarmTime.Year = y - 1970;
  sprintf(buf, "Alarm[%d] set=%d, type=%d, parm1=%d, parm2=%d, parm3=%d, parm4=%d, parm5=%d, parm6=%d, duration=%d, repeat=%d\n %d:%02d:%02d %s %d %s %d", alarm_ind, alarmInfo[alarm_ind].alarmSet, alarmInfo[alarm_ind].alarmType,
          alarmInfo[alarm_ind].parm1, alarmInfo[alarm_ind].parm2, alarmInfo[alarm_ind].parm3,
          alarmInfo[alarm_ind].parm4, alarmInfo[alarm_ind].parm5, alarmInfo[alarm_ind].parm6,
          alarmInfo[alarm_ind].duration, alarmInfo[alarm_ind].repeat,
          hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)),
          second(makeTime(alarmInfo[alarm_ind].alarmTime)), daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), day(makeTime(alarmInfo[alarm_ind].alarmTime)),
          monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), year(makeTime(alarmInfo[alarm_ind].alarmTime)));
  server.send(200, "text/plain", buf);
  Serial.println(buf);
}

void setalarmurl()
{

  String numstr = server.arg("number");
  int alarm_num = (strlen(numstr.c_str()) > 0) ? numstr.toInt() : 0;

  if (alarm_num >= NUM_ALARMS) return;
  if (alarm_num < 0) return;

  alarmInfo[alarm_num].alarmSet = true;
  String type = server.arg("type");
  String p1 = server.arg("parm1");
  String p2 = server.arg("parm2");
  String p3 = server.arg("parm3");
  String p4 = server.arg("parm4");
  String p5 = server.arg("parm5");
  String p6 = server.arg("parm6");
  String t = server.arg("duration"); // in ms
  String r = server.arg("repeat"); // in seconds
  String h = server.arg("hour");
  String m = server.arg("min");
  String s = server.arg("sec");
  String d = server.arg("day");
  String mth = server.arg("month");
  String y = server.arg("year");

  breakTime(now(), alarmInfo[alarm_num].alarmTime);

  setalarm(alarm_num,
           (strlen(type.c_str()) > 0) ? type.toInt() : alarmInfo[alarm_num].alarmType,
           (strlen(p1.c_str()) > 0) ? p1.toInt() : alarmInfo[alarm_num].parm1,
           (strlen(p2.c_str()) > 0) ? p2.toInt() : alarmInfo[alarm_num].parm2,
           (strlen(p3.c_str()) > 0) ? p3.toInt() : alarmInfo[alarm_num].parm3,
           (strlen(p4.c_str()) > 0) ? p4.toInt() : alarmInfo[alarm_num].parm4,
           (strlen(p5.c_str()) > 0) ? p5.toInt() : alarmInfo[alarm_num].parm5,
           (strlen(p6.c_str()) > 0) ? p6.toInt() : alarmInfo[alarm_num].parm6,
           (strlen(t.c_str()) > 0) ? t.toInt() : alarmInfo[alarm_num].duration, //10000,
           (strlen(r.c_str()) > 0) ? r.toInt() : alarmInfo[alarm_num].repeat, //SECS_PER_DAY,
           (strlen(s.c_str()) > 0) ?  s.toInt() : alarmInfo[alarm_num].alarmTime.Second,
           (strlen(m.c_str()) > 0) ?  m.toInt() : alarmInfo[alarm_num].alarmTime.Minute,
           (strlen(h.c_str()) > 0) ?  h.toInt() : alarmInfo[alarm_num].alarmTime.Hour,
           (strlen(d.c_str()) > 0) ?  d.toInt() : alarmInfo[alarm_num].alarmTime.Day,
           (strlen(mth.c_str()) > 0) ? mth.toInt() : alarmInfo[alarm_num].alarmTime.Month,
           (strlen(y.c_str()) > 0) ?  y.toInt() : alarmInfo[alarm_num].alarmTime.Year + 1970);
}


void settime()
// if connected to time server, it will overwrite asap
{
  String h = server.arg("hour");
  String m = server.arg("min");
  String s = server.arg("sec");
  String d = server.arg("day");
  String mth = server.arg("month");
  String y = server.arg("year");
  setTime(h.toInt(), m.toInt(), s.toInt(), d.toInt(), mth.toInt(), y.toInt());
  ClockInitialized = false;
  sprintf(buf, "Clock Set to %d:%02d:%02d %s %d %s %d", h.toInt(), m.toInt(), s.toInt(),
          daysOfWeek[d.toInt()].c_str(), d.toInt(), monthNames[mth.toInt()].c_str(),  y.toInt());
  server.send(200, "text/plain", buf);
  Serial.println(buf);
}


void setBright()
{
  String db = server.arg("day");
  String nb = server.arg("night");
  if (strlen(db.c_str()) > 0) day_brightness = db.toInt();
  if (strlen(nb.c_str()) > 0) night_brightness = nb.toInt();
  String rsp = "daybrightness set to: " + db + ", nightbrightness to  " + nb;
  server.send(200, "text/plain", rsp);
}

void calcSun() // calculates sunrise, sets etc. sets time for day and night mode
{
  double sunrise;
  double sunset;
  double civilsunrise;
  double civilsunset;
  double astrosunrise;
  double astrosunset;
  double nauticalsunrise;
  double nauticalsunset;

  /* Get the current time, and set the Sunrise code to use the current date */
  currentTime = now();

  sprintf(buf, "calcSun at %d:%02d:%02d %s %d %s %d", hour(currentTime), minute(currentTime),
          second(currentTime), daysOfWeek[weekday(currentTime)].c_str(), day(currentTime),
          monthNames[month(currentTime)].c_str(), year(currentTime));
  webSocket.sendTXT(websocketId_num, buf);

  sun.setCurrentDate(year(currentTime), month(currentTime), day(currentTime));
  /* Check to see if we need to update our timezone value */
  if (IsDst())
    sun.setTZOffset(DST_OFFSET);
  else
    sun.setTZOffset(CST_OFFSET);

  // These are all minutes after midnight
  sunrise = sun.calcSunrise();
  sunset = sun.calcSunset();
  civilsunrise = sun.calcCivilSunrise();
  civilsunset = sun.calcCivilSunset();
  nauticalsunrise = sun.calcNauticalSunrise();
  nauticalsunset = sun.calcNauticalSunset();
  astrosunrise = sun.calcAstronomicalSunrise();
  astrosunset = sun.calcAstronomicalSunset();

  Sunset.Hour = sunset / 60;
  Sunset.Minute = sunset - 60 * Sunset.Hour + 0.5;
  CivilSunset.Hour = civilsunset / 60;
  CivilSunset.Minute = civilsunset - 60 * CivilSunset.Hour + 0.5;
  NauticalSunset.Hour = nauticalsunset / 60;
  NauticalSunset.Minute = nauticalsunset - 60 * NauticalSunset.Hour + 0.5;
  AstroSunset.Hour = astrosunset / 60;
  AstroSunset.Minute = astrosunset - 60 * AstroSunset.Hour + 0.5;

  Sunrise.Hour = sunrise / 60;
  Sunrise.Minute = sunrise - 60 * Sunrise.Hour + 0.5;
  CivilSunrise.Hour = civilsunrise / 60;
  CivilSunrise.Minute = civilsunrise - 60 * CivilSunrise.Hour + 0.5;
  NauticalSunrise.Hour = nauticalsunrise / 60;
  NauticalSunrise.Minute = nauticalsunrise - 60 * NauticalSunrise.Hour + 0.5;
  AstroSunrise.Hour = astrosunrise / 60;
  AstroSunrise.Minute = astrosunrise - 60 * AstroSunrise.Hour + 0.5;

  WeekMorning = Sunrise;
  WeekNight = CivilSunset;

  WeekendNight = WeekNight;
  WeekendMorning = WeekMorning;

  nextCalcTime = currentTime;
  nextCalcTime += 24 * 3600;
  breakTime(nextCalcTime, calcTime);
  calcTime.Hour = 0;
  calcTime.Minute = 0;
  calcTime.Second = 1;


  Serial.print("Sunrise is ");
  Serial.print(sunrise);
  Serial.println(" minutes past midnight.");
  Serial.print("Sunset is ");
  Serial.print(sunset);
  Serial.print(" minutes past midnight.");


  sprintf(buf, "Sunset at %d:%02d, Sunrise at %d:%02d", Sunset.Hour, Sunset.Minute, Sunrise.Hour, Sunrise.Minute);
  webSocket.sendTXT(websocketId_num, buf);

  sprintf(buf, "CivilSunset at %d:%02d, CivilSunrise at %d:%02d", CivilSunset.Hour, CivilSunset.Minute, CivilSunrise.Hour, CivilSunrise.Minute);
  webSocket.sendTXT(websocketId_num, buf);

  sprintf(buf, "NauticalSunset at %d:%02d, NauticalSunrise at %d:%02d", NauticalSunset.Hour, NauticalSunset.Minute, NauticalSunrise.Hour, NauticalSunrise.Minute);
  webSocket.sendTXT(websocketId_num, buf);

  sprintf(buf, "AstroSunset at %d:%02d, AstroSunrise at %d:%02d", AstroSunset.Hour, AstroSunset.Minute, AstroSunrise.Hour, AstroSunrise.Minute);
  webSocket.sendTXT(websocketId_num, buf);

  sprintf(buf, "Next calcSun %d:%02d:%02d %s %d %s %d", hour(makeTime(calcTime)), minute(makeTime(calcTime)),
          second(makeTime(calcTime)), daysOfWeek[weekday(makeTime(calcTime))].c_str(), day(makeTime(calcTime)),
          monthNames[month(makeTime(calcTime))].c_str(), year(makeTime(calcTime)));
  webSocket.sendTXT(websocketId_num, buf);

  sprintf(buf, "dim at %d:%02d, brighten at %d:%02d", WeekNight.Hour, WeekNight.Minute, WeekMorning.Hour, WeekMorning.Minute);
  webSocket.sendTXT(websocketId_num, buf);
}

bool SetClockFromNTP()
{
  // get the time from the NTP server (takes upto 1 sec) if last update was longer ago than update_interval OR has never been successful yet
  bool updated = false;
  //Serial.println("Trying to get info from NTP");
  if (WiFi.status() == WL_CONNECTED)
  {
    //Serial.println("  Connected to try timeClient update");
    updated = timeClient.update();
  }
  // only starting using setTime once timeClient.update() has returned True once then it will have set EpochTime
  if (ClockInitialized) {
    setTime(timeClient.getEpochTime()); // Set the system time from the EpochTime
    if (IsDst()) adjustTime(3600); // offset the system time by an hour for Daylight Savings
  }
  return updated;
}

// Modified for Southern Hemisphere DST
// NZ daylight savings ends first Sunday of April at 3AM
// NZ daylight starts last Sunday of September at 2AM
bool IsDst()
{
  int previousSunday = day() - weekday() + 1;
  //Serial.print("    IsDst ");
  //Serial.print(month());
  //Serial.println(previousSunday);
  if (month() < 4 || month() > 9)  return true;
  if (month() > 4 && month() < 9)  return false;


  if (month() == 4) return previousSunday < 1;
  if (month() == 9) return previousSunday > 23;
  return false; // this line never gonna happend
}

////////////////////////////////////////////////////////////
void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.println();
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  //Serial.println(" ");
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
  if (Phase <= 0) // Set all pixels black
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(ClockCorrect(i), strip.Color(0, 0, 0));

  int disp_ind;
  //disp_ind = IsDay(t) ?  day_disp_ind : 4;
  disp_ind = force_day or ( not force_night and IsDay(t)) ?  day_disp_ind : 4;

  if (Phase >= 1) // Draw all pixels background color
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(ClockCorrect(i), strip.Color(Background[disp_ind].r, Background[disp_ind].g, Background[disp_ind].b));

  if (Phase >= 2) // Draw 5 min divisions
    for (int i = 0; i < NUM_LEDS; i = i + 5)
      strip.setPixelColor(ClockCorrect(i), strip.Color(Divisions[disp_ind].r, Divisions[disp_ind].g, Divisions[disp_ind].b)); // for Phase = 2 or more, draw 5 minute divisions

  if (Phase >= 3) { // Draw 15 min markers
    for (int i = 0; i < NUM_LEDS; i = i + 15)
      strip.setPixelColor(ClockCorrect(i), strip.Color(Quarters[disp_ind].r, Quarters[disp_ind].g, Quarters[disp_ind].b));
    strip.setPixelColor(ClockCorrect(0), strip.Color(Twelve[disp_ind].r, Twelve[disp_ind].g, Twelve[disp_ind].b));
  }

  if (Phase >= 4) { // Draw hands
    int isecond = second(t);
    int iminute = (60 * minute(t) + isecond + 30) / 60; // round to nearest minute
    int ihour = ((hour(t) % 12) * 5) + (iminute + 6) / 12; // round to nearest LED
    //hour
    strip.setPixelColor(ClockCorrect(ihour), strip.Color(Hour[disp_ind].r, Hour[disp_ind].g, Hour[disp_ind].b));
    for (int i = 0; i <= hour_width[disp_ind]; i++) {
      strip.setPixelColor(ClockCorrect(ihour - i), strip.Color(Hour[disp_ind].r, Hour[disp_ind].g, Hour[disp_ind].b));
      strip.setPixelColor(ClockCorrect(ihour + i), strip.Color(Hour[disp_ind].r, Hour[disp_ind].g, Hour[disp_ind].b));
    }
    //minute on top of hour
    uint32_t use_color;
    if (minute_width[disp_ind] >= 0) {
      // optional to help identification, minute hand flshes between normal and half intensity
      if ((second() % 2) | (not minute_blink[disp_ind])) {
        use_color = strip.Color(Minute[disp_ind].r, Minute[disp_ind].g, Minute[disp_ind].b);
      } else {
        use_color = strip.Color(Minute[disp_ind].r / 2, Minute[disp_ind].g / 2, Minute[disp_ind].b / 2 );
      }
      strip.setPixelColor(ClockCorrect(iminute), use_color);
      for (int i = 1; i <= minute_width[disp_ind]; i++) {
        strip.setPixelColor(ClockCorrect(iminute - i), use_color); // to help identification, minute hand flshes between normal and half intensity
        strip.setPixelColor(ClockCorrect(iminute + i), use_color); // to help identification, minute hand flshes between normal and half intensity
      }
    }
    //second on top of both
    if (second_width[disp_ind] >= 0) {
      strip.setPixelColor(ClockCorrect(isecond), strip.Color(Second[disp_ind].r, Second[disp_ind].g, Second[disp_ind].b));
      for (int i = 0; i <= second_width[disp_ind]; i++) {
        strip.setPixelColor(ClockCorrect(isecond - i), strip.Color(Second[disp_ind].r, Second[disp_ind].g, Second[disp_ind].b));
        strip.setPixelColor(ClockCorrect(isecond + i), strip.Color(Second[disp_ind].r, Second[disp_ind].g, Second[disp_ind].b));
      }
    }
  }

  SetBrightness(t); // Set the clock brightness dependant on the time
  strip.show(); // show all the pixels
}

bool IsDay(time_t t)
{
  // return false; // for debug if want to test night behaviour
  int NowHour = hour(t);
  int NowMinute = minute(t);
  if ((weekday() >= 2) && (weekday() <= 6))
    if ((NowHour > WeekNight.Hour) || ((NowHour == WeekNight.Hour) && (NowMinute >= WeekNight.Minute)) || ((NowHour == WeekMorning.Hour) && (NowMinute <= WeekMorning.Minute)) || (NowHour < WeekMorning.Hour))
      return false;
    else
      return true;
  else if ((NowHour > WeekendNight.Hour) || ((NowHour == WeekendNight.Hour) && (NowMinute >= WeekendNight.Minute)) || ((NowHour == WeekendMorning.Hour) && (NowMinute <= WeekendMorning.Minute)) || (NowHour < WeekendMorning.Hour))
    return false;
  else
    return true;
}

//************* Function to set the clock brightness ******************************
void SetBrightness(time_t t)
{

  //if (IsDay(t) & ClockInitialized)
  if ((force_day or ( not force_night and IsDay(t))) and ClockInitialized)
    strip.setBrightness(day_brightness);
  else
    strip.setBrightness(night_brightness);
}

//************* This function reverses the pixel order ******************************
//              and ajusts top of clock
int ClockCorrect(int Pixel)
{
  Pixel = (Pixel + TopOfClock) % NUM_LEDS;
  if (ClockGoBackwards)
    return ((NUM_LEDS - Pixel + NUM_LEDS / 2) % NUM_LEDS); // my first attempt at clock driving had it going backwards :)
  else
    return (Pixel);
}


void showlights(uint16_t duration, int w1, int w2, int w3, int w4, int w5, int w6, int w7, int w8, time_t t)
{
  time_elapsed = 0;
  uint16_t time_start = millis();
  sprintf(buf, "showlights called with %d %d %d %d %d %d %d %d %d %d\n ", duration, w1, w2, w3, w4, w5, w6, w7, w8, t);
  Serial.println(buf);
  while (time_elapsed < duration)
    //TODO need to interupt otherwise will always go thru loop an integral number of times
    //  if duration too small will be ignored.
  {
    // Fill along the length of the strip in various colors...
    if (w1 >= 0) colorWipe(strip.Color(255,   0,   0), w1, t); // Red
    if (w2 >= 0) colorWipe(strip.Color(  0, 255,   0), w2, t); // Green
    if (w3 >= 0) colorWipe(strip.Color(  0,   0, 255), w3, t); // Blue
    // Do a theater marquee effect in various colors...
    if (w4 >= 0) theaterChase(strip.Color(127, 127, 127), w4, t); // White, half brightness
    if (w5 >= 0) theaterChase(strip.Color(127,   0,   0), w5, t); // Red, half brightness
    if (w6 >= 0) theaterChase(strip.Color(  0,   0, 127), w6, t); // Blue, half brightness
    if (w7 >= 0) rainbow(w7, 1, 0, 256, 4, 1, 15, t, duration);            // Flowing rainbow cycle along the whole strip
    if (w8 >= 0) theaterChaseRainbow(w8, t); // Rainbow-enhanced theaterChase variant
    time_elapsed = millis() - time_start;
  }
}

//********* Bills NeoPixel Routines

#include <cmath>
#include <vector>

using namespace std;
int8_t piecewise_linear(int8_t x, vector<pair<int8_t, int8_t>> points) {
  for (int i = 0; i < points.size() - 1; i++) {
    if (x < points[i + 1].first) {
      int x1 = points[i].first;
      int y1 = points[i].second;
      int x2 = points[i + 1].first;
      int y2 = points[i + 1].second;
      return y1 + (y2 - y1) * (x - x1) / static_cast<double>(x2 - x1);
    }
  }
  return static_cast<int8_t>(points.back().second);
}


// Set All Leds to given color for wait seconds
void colorAll(uint32_t color, int duration, time_t t) {
  strip.fill(color);
  SetBrightness(t); // Set the clock brightness dependant on the time
  strip.show();                          //  Update strip to match
  limited_delay(duration);                           //  Pause for a moment
}

vector<pair<int8_t, int8_t>> points_for_embedding(int embedding) {
  vector<pair<int8_t, int8_t>> points;
  switch (embedding) {
    case 1: // full mapping
      points = {{0, 0}, {NUM_LEDS - 1, NUM_LEDS - 1}};
      break;
    case 2: // up, down, half peak 1/2 way
      points = {{0, 0}, {NUM_LEDS / 2 - 1, NUM_LEDS / 2 - 1}, {NUM_LEDS / 2, NUM_LEDS / 2 - 1}, {NUM_LEDS - 1, 0}};
      break;
    case 3: // up, down, full peak 1/2 way
      points = {{0, 0}, {NUM_LEDS / 2 - 1, NUM_LEDS - 2}, {NUM_LEDS / 2, NUM_LEDS - 2}, {NUM_LEDS - 1, 0}};
      break;
    case 4: // up, down, up max 1/3
      points = {{0, 0}, {NUM_LEDS / 3 - 1, NUM_LEDS / 3 - 1}, {NUM_LEDS / 3, NUM_LEDS / 3 - 1 }, {2 * NUM_LEDS / 3, 0}, {NUM_LEDS - 1, NUM_LEDS / 3 - 1}};
      break;
    case 5: // up, up twice full
      points = {{0, 0}, {NUM_LEDS / 2 - 1, NUM_LEDS - 2 }, {NUM_LEDS / 2, 0}, {NUM_LEDS - 1, NUM_LEDS - 2}};
      break;
    case 6: // up, up, up three time full
      points = {{0, 0}, {NUM_LEDS / 3 - 1, NUM_LEDS - 3}, {NUM_LEDS / 3, 0}, {2 * NUM_LEDS / 3 - 1, NUM_LEDS - 3}, {2 * NUM_LEDS / 3, 0}, {NUM_LEDS - 1, NUM_LEDS - 3}};
      break;
    case 7: // up, up, up, up 4 times
      points = {{0, 0}, {NUM_LEDS / 4 - 1, NUM_LEDS - 4}, {NUM_LEDS / 4, 0},  {NUM_LEDS / 2 - 1, NUM_LEDS - 4}, {NUM_LEDS / 2, 0}, {3 * NUM_LEDS / 4 - 1, NUM_LEDS - 4},  {3 * NUM_LEDS / 4, 0}, {NUM_LEDS - 1, NUM_LEDS - 4}};
      break;
    case 8: // 2 levels
      points = {{0, 0}, {NUM_LEDS / 2 - 1, 0}, {NUM_LEDS / 2, NUM_LEDS / 2}, {NUM_LEDS - 1, NUM_LEDS / 2}};
      break;
    case 9: // 3 levels
      points = {{0, 0}, {NUM_LEDS / 3 - 1, 0}, {NUM_LEDS / 3, NUM_LEDS / 3}, {2 * NUM_LEDS / 3 - 1, NUM_LEDS / 3}, {2 * NUM_LEDS / 3, 2 * NUM_LEDS / 3}, {NUM_LEDS - 1, 2 * NUM_LEDS / 3}};
      break;
    case 10: // 4 levels
      points = {{0, 0}, {NUM_LEDS / 4 - 1, 0}, {NUM_LEDS / 4, NUM_LEDS / 4},  {NUM_LEDS / 2 - 1, NUM_LEDS / 4}, {NUM_LEDS / 2, NUM_LEDS / 2}, {3 * NUM_LEDS / 4 - 1, NUM_LEDS / 2},  {3 * NUM_LEDS / 4, 3 * NUM_LEDS / 4}, {NUM_LEDS - 1, 3 * NUM_LEDS / 4}};
      break;
    default: // up full half way, then constant
      points = {{0, 0}, {NUM_LEDS / 2 - 1, NUM_LEDS - 2},  {NUM_LEDS - 1, NUM_LEDS - 2}};
  }
  return points;
}

// Mod of Adafruit Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait, int embedding, long firsthue, int hueinc,  int ncolorloop, int ncolorfrac, int nodepix, time_t t, uint16_t duration) {
  sprintf(buf, "Rainbow wait %d, embedding = %d, firsthue = %d, hueinc = %d, ncolorloop = %d, ncolorfrac = %d, nodepix = %d, duration = %d",
          wait, embedding, firsthue, hueinc, ncolorloop, ncolorfrac, nodepix, duration);
  Serial.println(buf);

  // Hue of first pixel runs ncolorloop complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to ncolorloop*65536. Adding 256 to firstPixelHue each time
  // means we'll make ncolorloop*65536/256   passes through this outer loop:
  int pixelHue;
  time_elapsed = 0;
  uint16_t time_start = millis();
  long firstPixelHue = firsthue;
  vector<pair<int8_t, int8_t>> points;
  int j;
  points = points_for_embedding(embedding);
  while (time_elapsed < duration) {
    firstPixelHue += hueinc;

    // routine setting all leds (pixels) for one frame of the animation
    // must have yield() in loop
    for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make ncolorloop/ ncolorfrac  revolutions of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      j = piecewise_linear(i, points);

      if (ncolorfrac != 0) {
        pixelHue = firstPixelHue + (j * ncolorloop * 65536L / strip.numPixels() / ncolorfrac);
      } else {
        pixelHue = firstPixelHue + (j * ncolorloop * 65536L / strip.numPixels());
      }
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(ClockCorrect(i + nodepix), strip.gamma32(strip.ColorHSV(pixelHue)));
      yield();
    }

    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    limited_delay(wait);  // Pause for a moment
    time_elapsed = millis() - time_start;
  }
}


void color_wipe(int wait, int embedding, long firsthue, int hueinc,  int ncolorloop, int blocksize, int nodepix, time_t t, uint16_t duration) {
  sprintf(buf, "Color wipe wait %d, embedding = %d, firsthue = %d, hueinc = %d, ncolorloop = %d, blocksize = %d, nodepix = %d, duration = %d",
          wait, embedding, firsthue, hueinc, ncolorloop, blocksize, nodepix, duration);
  Serial.println(buf);

  // Hue of first pixel runs ncolorloop complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to ncolorloop*65536. Adding 256 to firstPixelHue each time
  // means we'll make ncolorloop*65536/256   passes through this outer loop:
  int pixelHue;
  time_elapsed = 0;
  uint16_t time_start = millis();
  long firstPixelHue = firsthue;
  int i = 0; // starting index
  vector<pair<int8_t, int8_t>> points;
  int j;
  int maxcnt;
  points = points_for_embedding(embedding);
  strip.fill(); // clear
  while (time_elapsed < duration) {
    if (blocksize > 0) {
      maxcnt = blocksize;
    } else {
      maxcnt = random(1, 1 - blocksize);
    }
    for (int cnt = 0; cnt < maxcnt; cnt++) {
      // routine setting all leds (pixels) for one frame of the animation
      // must have yield() in loop
      if (ncolorloop >= 0) {
        i++;
        if (i  >= strip.numPixels()) {
          i = 0;
          firstPixelHue += hueinc;
        }
      } else {
        i --;
        if (i  < 0) {
          i =   - 1 + strip.numPixels();
          firstPixelHue += hueinc;
        }
      }
      j = piecewise_linear(i, points);
      pixelHue = firstPixelHue + (j * 65536L  * ncolorloop / 100  / strip.numPixels() );
      strip.setPixelColor(ClockCorrect(i + nodepix), strip.gamma32(strip.ColorHSV(pixelHue)));
      yield();
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    limited_delay(wait);  // Pause for a moment
    time_elapsed = millis() - time_start;
  }
}

class Worm
{
    // colors, a list of worm segment (starting with head) hues
    // path a list of the LED indices over which the worm will travel (from 0 to 59 for clock)
    // cyclelen controls speed, worm movement only when LED upload cycles == 0 mod cyclelen
    // height (of worm segments) is same length as colors: higher value worms segments go over top of lower value worms
    // equal value segments have later worm having priority
  private:
    vector < int >colors;
    vector < int >path;
    int cyclelen;
    vector < int >height;
    int activecount;
    int direction;
    int headposition;
  public:
    Worm (vector < int >colors, vector < int >path, int cyclelen,
          int direction, vector < int >height)
    {
      this->colors = colors;
      this->colors.push_back (0); // add blank seqment to end worm
      this->path = path;
      this->cyclelen = cyclelen;  // movement only occurs on LED upload cycles == 0 mod cyclelen
      this->height = height;
      this->height.push_back (-1);  // add lowest value for height
      this->activecount = 0;
      this->direction = direction;
      this->headposition = -this->direction;
    }

    //    void move (vector < int >&LEDStripBuf,
    //               vector < int >&LEDsegheights)
    void move ()
    {
      bool acted = this->activecount == 0;
      if (acted)
      {
        // % does not work with negative
        this->headposition = this->headposition + this->direction + this->path.size ();
        this->headposition %= this->path.size ();
        // Put worm into strip and blank end
        int segpos = this->headposition;
        //Serial.println(" ");
        for (int x = 0; x < this->colors.size (); x++)
        {
          int strippos = this->path[segpos];
          //sprintf(buf, "x = %d, c[x]=%d,  segpos=%d, strippos=%d, pathsize=%d", x, this->colors[x], segpos,   strippos, this->path.size() );
          //Serial.println(buf);
          //sprintf(buf, "%d, ",this->colors[x] );
          //sprintf(buf, "%d, ", segpos);
          //sprintf(buf, "%d, ", strippos);
          //Serial.print(buf);

          if (true) //(this->height[x] >= LEDsegheights[this->path[segpos]])
          {
            if (this->colors[x] == 0) {
              strip.setPixelColor(ClockCorrect(strippos), 0, 0, 0);
              //strip.setPixelColor(strippos, 0, 0, 0);
            } else {
              strip.setPixelColor(ClockCorrect(strippos), strip.gamma32(strip.ColorHSV(this->colors[x])));
              //strip.setPixelColor(strippos, strip.gamma32(strip.ColorHSV(this->colors[x])));
              //LEDsegheights[this->path[segpos]] = this->height[x];
            }

          }
          // % does not work with negative
          segpos = segpos - this->direction + this->path.size ();
          segpos  %= this->path.size ();
        }
      };
      this->activecount++;
      this->activecount %= this->cyclelen;
    };
};

void moveworms(int wait, int nworms, time_t t, uint16_t duration) {
  time_elapsed = 0;
  uint16_t time_start = millis();
  vector < int >path =   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59};
  int direction = 1;
  nworms = min(abs(nworms), 10);
  vector<Worm>Worms;
  for (int i = 0; i < nworms; i++) {
    int lenworm = random(2, 15);
    int cyclelen = random(1, 8);
    int dir = random(2);
    dir = 2 * dir - 1;
    vector <int> colors = {};
    int col = random(65535);
    for (int len = 0; len < lenworm; len++) {
      colors.push_back(col);
    }
    Worm w(colors, path, cyclelen, dir, colors);
    Worms.push_back(w);
  }

  strip.fill(); // clear
  while (time_elapsed < duration) {
    for (int i = 0; i < nworms; i++) {
      Worms[i].move();
      yield();
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    //yield();
    limited_delay(wait);  // Pause for a moment
    time_elapsed = millis() - time_start;
  }
}

void firefly(int wait, int numff, long minHue, long maxHue, uint16_t hueInc, uint8_t minSat, uint8_t maxSat, uint16_t minVal, uint16_t maxVal, time_t t, uint16_t duration) {
  time_elapsed = 0;
  uint8_t pixel;
  long pixelHue;
  uint8_t Sat;
  uint8_t Val;
  uint16_t time_start = millis();
  pixelHue = minHue;
  numff = max(1, numff); // will blow up otherwise
  while (time_elapsed < duration) {
    strip.fill(); // clear
    for (int i = 0; i < numff; i++) {
      pixel = random(NUM_LEDS);
      if (hueInc == 0) {
        // Choose random color
        pixelHue = random(minHue, maxHue);
      } else { // inc
        pixelHue += hueInc;
        if (pixelHue > maxHue) {
          pixelHue = minHue;
        }
      }
      Sat = random(minSat, maxSat);
      Val = random(minVal, maxVal);
      strip.setPixelColor(pixel, strip.gamma32(strip.ColorHSV(pixelHue, Sat, Val)));
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    yield();
    limited_delay(wait);  // Pause for a moment
    time_elapsed = millis() - time_start;
  }
}


/**
    fire idea from
    Arduino Uno - NeoPixel Fire v. 1.0
    Copyright (C) 2015 Robert Ulbricht
    https://www.arduinoslovakia.eu
*/

//uint32_t Blend(uint32_t color1, uint32_t color2)
//{
//  uint8_t r1, g1, b1;
//  uint8_t r2, g2, b2;
//  uint8_t r3, g3, b3;
//
//  r1 = (uint8_t)(color1 >> 16),
//  g1 = (uint8_t)(color1 >>  8),
//  b1 = (uint8_t)(color1 >>  0);
//
//  r2 = (uint8_t)(color2 >> 16),
//  g2 = (uint8_t)(color2 >>  8),
//  b2 = (uint8_t)(color2 >>  0);
//
//  return strip.Color(constrain(r1 + r2, 0, 255), constrain(g1 + g2, 0, 255), constrain(b1 + b2, 0, 255));
//}

uint32_t Substract(uint32_t color1, uint32_t color2)
{
  uint8_t r1, g1, b1;
  uint8_t r2, g2, b2;
  uint8_t r3, g3, b3;
  int16_t r, g, b;

  r1 = (uint8_t)(color1 >> 16),
  g1 = (uint8_t)(color1 >>  8),
  b1 = (uint8_t)(color1 >>  0);

  r2 = (uint8_t)(color2 >> 16),
  g2 = (uint8_t)(color2 >>  8),
  b2 = (uint8_t)(color2 >>  0);

  r = (int16_t)r1 - (int16_t)r2;
  g = (int16_t)g1 - (int16_t)g2;
  b = (int16_t)b1 - (int16_t)b2;
  if (r < 0) r = 0;
  if (g < 0) g = 0;
  if (b < 0) b = 0;

  return strip.Color(r, g, b);
}


//void AddColor(uint8_t position, uint32_t color)
//{
//  uint32_t blended_color = Blend(strip.getPixelColor(position), color);
//  strip.setPixelColor(position, blended_color);
//}

void SubstractColor(uint8_t position, uint32_t color)
{
  uint32_t blended_color = Substract(strip.getPixelColor(position), color);
  strip.setPixelColor(position, blended_color);
}

void fire(time_t t, uint16_t duration) {
  time_elapsed = 0;
  uint32_t fire_color   = strip.Color ( 255,  127,  00);
  uint16_t time_start = millis();
  while (time_elapsed < duration) {
    //strip.fill(); // clear
    strip.fill(fire_color);
    for (int i = 0; i < NUM_LEDS; i++) {
      //AddColor(i, fire_color);
      int r = random(255);
      uint32_t diff_color = strip.Color ( r, r , r / 2);
      SubstractColor(i, diff_color);
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    yield();
    delay(random(50, 150)); // Pause for a random moment
    time_elapsed = millis() - time_start;
  }
}

//https://en.wikipedia.org/wiki/Elementary_cellular_automaton#
void cellularAutomata(int wait, uint8_t rule, long pixelHue, time_t t, uint16_t duration) {
  time_elapsed = 0;
  vector < int >next(NUM_LEDS);
  uint16_t time_start = millis();
  // initial state
  strip.fill();
  strip.setPixelColor(0, strip.gamma32(strip.ColorHSV(pixelHue)));
  while (time_elapsed < duration) {
    for (int i = 0; i < NUM_LEDS; i++) {
      uint8_t nbrs = ((strip.getPixelColor(i) != 0) << 2) + ((strip.getPixelColor((i + 1) % NUM_LEDS) != 0) << 1) +  (strip.getPixelColor((i + 2) % NUM_LEDS) != 0);
      // have 3 CAs for r,g,b then use
      uint8_t nbrsR = ((((uint8_t)(strip.getPixelColor(i) >> 16)) != 0) << 2) + ((((uint8_t)(strip.getPixelColor((i + 1) % NUM_LEDS) >> 16)) != 0) << 1) +  (((uint8_t)(strip.getPixelColor((i + 2) % NUM_LEDS) >> 16)) != 0);
      //uint8_t nbrsG = ((((uint8_t)(strip.getPixelColor(i) >> 8))!=0)<<2) + ((((uint8_t)(strip.getPixelColor((i+1)%NUM_LEDS) >> 8))!=0)<<1) +  (((uint8_t)(strip.getPixelColor((i+2)%NUM_LEDS) >> 8))!=0);
      //uint8_t nbrsB = ((((uint8_t)(strip.getPixelColor(i) >> 0))!=0)<<2) + ((((uint8_t)(strip.getPixelColor((i+1)%NUM_LEDS) >> 0))!=0)<<1) +  (((uint8_t)(strip.getPixelColor((i+2)%NUM_LEDS) >> 0))!=0);
      next[(i + 1) % NUM_LEDS] = (rule >> nbrs) & 0x1;
    }
    strip.fill(); // clear
    for (int i = 0; i < NUM_LEDS; i++) {
      if (next[i]) {
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    yield();
    limited_delay(wait); // wait
    time_elapsed = millis() - time_start;
  }
}


void cellularAutomata(int wait, uint8_t ruleR, uint8_t ruleG, uint8_t ruleB, long pixelHue, time_t t, uint16_t duration) {
  time_elapsed = 0;
  vector < int >nextR(NUM_LEDS);
  vector < int >nextG(NUM_LEDS);
  vector < int >nextB(NUM_LEDS);
  uint16_t time_start = millis();
  // initial state
  strip.fill();
  strip.setPixelColor(0, 0xff0000);
  strip.setPixelColor(20, 0x00ff00);
  strip.setPixelColor(40, 0x0000ff);
  while (time_elapsed < duration) {
    for (int i = 0; i < NUM_LEDS; i++) {
      // have 3 CAs for r,g,b then use
      uint8_t nbrsR = ((((uint8_t)(strip.getPixelColor(i) >> 16)) != 0) << 2) + ((((uint8_t)(strip.getPixelColor((i + 1) % NUM_LEDS) >> 16)) != 0) << 1) +  (((uint8_t)(strip.getPixelColor((i + 2) % NUM_LEDS) >> 16)) != 0);
      uint8_t nbrsG = ((((uint8_t)(strip.getPixelColor(i) >> 8)) != 0) << 2) + ((((uint8_t)(strip.getPixelColor((i + 1) % NUM_LEDS) >> 8)) != 0) << 1) +  (((uint8_t)(strip.getPixelColor((i + 2) % NUM_LEDS) >> 8)) != 0);
      uint8_t nbrsB = ((((uint8_t)(strip.getPixelColor(i) >> 0)) != 0) << 2) + ((((uint8_t)(strip.getPixelColor((i + 1) % NUM_LEDS) >> 0)) != 0) << 1) +  (((uint8_t)(strip.getPixelColor((i + 2) % NUM_LEDS) >> 0)) != 0);
      nextR[(i + 1) % NUM_LEDS] = (ruleR >> nbrsR) & 0x1;
      nextG[(i + 1) % NUM_LEDS] = (ruleG >> nbrsG) & 0x1;
      nextB[(i + 1) % NUM_LEDS] = (ruleB >> nbrsB) & 0x1;
    }
    strip.fill(); // clear
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, nextR[i] ? 100 : 0, nextG[i] ? 100 : 0, nextB[i] ? 100 : 0);
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    yield();
    limited_delay(wait); // wait
    time_elapsed = millis() - time_start;
  }
}


//********* Adafruit NeoPIxel Routines
// Some functions of our own for creating animated effects -----------------

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait, time_t t) {
  for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(ClockCorrect(i), color);         //  Set pixel's color (in RAM)
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show();                          //  Update strip to match
    yield();
    limited_delay(wait);                           //  Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait, time_t t) {
  for (int a = 0; a < 10; a++) { // Repeat 10 times...
    for (int b = 0; b < 3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for (int c = b; c < strip.numPixels(); c += 3) {
        strip.setPixelColor(ClockCorrect(c), color); // Set pixel 'c' to value 'color'
      }
      SetBrightness(t); // Set the clock brightness dependant on the time
      strip.show(); // Update strip with new contents
      yield();
      limited_delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait, time_t t) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for (int a = 0; a < 10; a++) { // Repeat 10 times...
    for (int b = 0; b < 3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for (int c = b; c < strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + ClockCorrect(c) * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(ClockCorrect(c), color); // Set pixel 'c' to value 'color'
      }
      SetBrightness(t); // Set the clock brightness dependant on the time
      strip.show();                // Update strip with new contents
      yield();
      limited_delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}
