/**
      Adapted for 112 LED strip on Liz shelves from bottom to top: 20+20+72
      based on LEDClock using tasks
      NOTE If have two strips defined with same pin
        Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
        Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
        both can be used.
        USES: if strip.show(); strip2.show();    acts like sending data to led strip of joined length
              if have tasks to flip between the two shows, then will mix.
        can call strip.show() and strip2.show() to alternate
        also if call one after the other acts like joining two strips as very little gap between signals sent.
  Test on Generic ESP8266 module (swage) 2MB with 256K FS
  Test on Esp8266 Node bread board attached to 4MB 2 MB FS
*/

//TODO allow more general light animations for alarms
// need to add parameters and fix configuration json for this
#define TEST_CLOCK
#ifdef TEST_CLOCK
#define NUM_LEDS 60
#else
#define NUM_LEDS 112
#endif
//#define _TASK_SLEEP_ON_IDLE_RUN
#define _TASK_STATUS_REQUEST
#define _TASK_LTS_POINTER       // Compile with support for Local Task Storage pointer
#include <TaskScheduler.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <TimeLib.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <string.h>
#include "pitches.h"
#include "sunset.h"
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#define TIME_CORRECTION 1 // emprical so in sync with iphone
/* Cass Bay */
#define LATITUDE        -43.601131
#define LONGITUDE       172.689831
#define CST_OFFSET      12
#define DST_OFFSET      13

SunSet sun;


ESP8266WebServer server(80);       // create a web server on port 80
WebSocketsServer webSocket(81);    // create a websocket server on port 81
uint8_t websocketId_num = 0;

File fsUploadFile;                                    // a File variable to temporarily store the received file

// OTA and mDns must have same name
#ifdef TEST_CLOCK
const char *OTAandMdnsName = "ShelvesOld";           // A name and a password for the OTA and mDns service
#else
const char *OTAandMdnsName = "Shelves";           // A name and a password for the OTA and mDns service
#endif
const char *OTAPassword = "pass";

// must be longer than longest message
char buf[200];

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
//uint16_t HourHues[6] = {0, 65536/12, 65536/6, 65536/3, 65536 * 2 / 3, 65536 * 9/12};
//                        R     O         Y        G          B            V
uint16_t HourHues[6] = {0, 5461, 10922, 21845, 43690, 49152};
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
RGB Background[NUM_DISP_OPTIONS] = {{ 0, 0, 0 }, {40, 40, 40}};

//The Hour hand
RGB Hour[NUM_DISP_OPTIONS] = {{ 0, 255, 0 }, { 0, 0, 255 }, { 0, 255, 0 }, { 0, 255, 0 }, {100, 0, 0}}; //green
//The Minute hand
RGB Minute[NUM_DISP_OPTIONS] = {{ 255, 255, 0 }, { 0, 0, 255 }, { 255, 255, 0 }, { 255, 255, 0 }, {0, 0, 0} }; //yellow
//The Second hand
RGB Second[NUM_DISP_OPTIONS] = {{ 0, 0, 255 }, { 0, 0, 0 }, { 0, 0, 255 }, { 0, 0, 255 }, { 0, 0, 0 }};

// Make clock go forwards or backwards (dependant on hardware)
bool ClockGoBackwards = false;
int day_disp_ind = 1;
#ifdef TEST_CLOCK
bool auto_night_disp = true;
#else
bool auto_night_disp = false;
#endif

bool minute_blink[NUM_DISP_OPTIONS] = {true, true};
int minute_width[NUM_DISP_OPTIONS] = {2, 4, 2, 2, -1}; //-1 means don't show
int hour_width[NUM_DISP_OPTIONS] = {3, 3, 3, 3, 0};
int second_width[NUM_DISP_OPTIONS] = {0, -1, 0, 0, -1};


//Set brightness by time for night and day mode
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

byte day_brightness = 255;
byte night_brightness = 32; //64; //16;

//Set your timezone in hours difference rom GMT
int hours_Offset_From_GMT = 12;

String daysOfWeek[8] = {"dummy", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String monthNames[13] = {"dummy", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

bool sound_alarm_flag = false;
int light_alarm_num = 0;
int frames_per_sec = 10;
bool light_alarm_flag = false;
uint32_t led_color_alarm_rgb;
#define LED_SHOW_TIME 2*TASK_SECOND
tmElements_t calcTime = {0};

struct ALARM {
  bool alarmSet = false;
  uint16_t alarmType;
  uint16_t duration;
  uint32_t repeat;
  tmElements_t alarmTime;
};

#define NUM_ALARMS 5
ALARM alarmInfo[NUM_ALARMS];

uint16_t time_elapsed = 0;
int TopOfClock = 0; // to make given pixel the top

// notes in the melody for the sound alarm:
int melody1[] = { // Shave and a hair cut (3x) two bits terminate with -1 = STOP
  NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, REST,
  NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, REST,
  NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, REST, NOTE_B4, NOTE_C5, STOP
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations1[] = {
  4, 8, 8, 4, 4, 4, 4, 8, 8, 4, 4, 4, 4, 8, 8, 4, 4, 4, 4, 4
};

int whole_note_duration = 1000; // milliseconds
int * melody = melody1;
int * noteDurations = noteDurations1;
int melodyNoteIndex;

#include "localwificonfig.h"
Scheduler ts;

//************* Declare user functions ******************************
void Draw_Clock(time_t t, byte Phase);
int ClockCorrect(int Pixel);
void SetBrightness(time_t t);
bool SetClockFromNTP();
bool IsDst();
bool IsDay();
void rainbowFrame();
void color_wipe_frame();
void fire_fly_frame();
void flicker_frame();
int8_t piecewise_linear(int8_t x, std::vector<std::pair<int8_t, int8_t>> points);
// An array of function pointers for making frame of animations
void (*alarm_frame[])() = {rainbowFrame, color_wipe_frame, fire_fly_frame, flicker_frame};
#define array_size(arr) *(&arr + 1) - arr //(sizeof(arr) / sizeof(*(arr)))
int num_animations = array_size(alarm_frame);

// for tvisual_alarm parms for call back
typedef struct {
  long firstPixelHue;
  long start;
  int percentWheelPerFrame;
  int percentWheelPerStrip;
  int wait = 50; // base ms between frames
  int wait_delta = 0; // random variation of wait
  int wait_use = 50; // ms to use between frames
  int wait_adj = 0;
  int ex = 4;
  long firsthue;
  int hueinc = 256;
  int ncolorloop = 1;
  int ncolorfrac = 1;
  int nodepix = 0; // hundreths of pixel
  int nodepix_diff = 0; // hundreths of pixel
  int nodepix_ind = 0;
  // starting node index of animation a quadratic fcn of frame
  int nodepix_coef0 = 0; // constant coef
  int nodepix_coef1 = 0; // linear coef
  int nodepix_coef2 = 0; // quadratic coef
  uint16_t duration = 10000;
  int numpixeltouse = NUM_LEDS;
  uint32_t pixelHSV[NUM_LEDS] = {0};
  void (*frame)() = rainbowFrame;
} visual_alarm_parm;

visual_alarm_parm alarmParm;
std::vector<std::pair<int8_t, int8_t>> points_input;

#define CONNECT_TIMEOUT   30      // Seconds
#define CONNECT_OK        0       // Status of successful connection to WiFi
#define CONNECT_FAILED    (-99)   // Status of failed connection to WiFi
#define NTP_CHECK_SEC  3601       //36001       // NTP server called every interval
#define NTP_RE_CHECK_SEC  600     // if NTP server failed try again after this interval
// NTP Related Definitions
#define NTP_PACKET_SIZE  48       // NTP time stamp is in the first 48 bytes of the message


// Callback methods prototypes
void connectInit();
void ledCallback();
bool ledOnEnable();
void ledOnDisable();
void ledRed();
void ledBlue();
void ntpUpdateInit();
void ntpCheck();
void serverRun();
void webSocketRun();
void OTARun();
void MDNSRun();
void playMelody();
bool playMelodyOnEnable();
void playMelodyOnDisable();
void changeClock();
void led_color_alarm();
//bool led_color_alarmOnEnable();
void visual_alarmCallback();
//void rainbowOnDisable();
bool visual_alarmOnEnable();
void shelfLoop();
bool shelfLoopOnEnable();

// Tasks

//TODO should tConnect be started in setup?
Task  tConnect    (TASK_SECOND, TASK_FOREVER, &connectInit, &ts, true);
Task  tRunServer  (TASK_IMMEDIATE, TASK_FOREVER, &serverRun, &ts, false);
Task  tRunWebSocket  (TASK_IMMEDIATE, TASK_FOREVER, &webSocketRun, &ts, false);
Task  tOTARun  (TASK_IMMEDIATE, TASK_FOREVER, &OTARun, &ts, false);
Task  tMDNSRun  (TASK_IMMEDIATE, TASK_FOREVER, &MDNSRun, &ts, false);
Task  tLED        (TASK_IMMEDIATE, TASK_FOREVER, &ledCallback, &ts, false, &ledOnEnable, &ledOnDisable);
Task  tplayMelody (TASK_IMMEDIATE, TASK_FOREVER, &playMelody, &ts, false, &playMelodyOnEnable, &playMelodyOnDisable);
//Task tchangeClock (TASK_SECOND, TASK_FOREVER, &changeClock, &ts, false);
Task tvisual_alarm (TASK_IMMEDIATE, TASK_FOREVER, &visual_alarmCallback, &ts, false, &visual_alarmOnEnable);
// changed from TASK_IMMEDIATE to every 100 ms to see if prevents clock from freezing
Task tshelfLoop (100 * TASK_MILLISECOND, TASK_FOREVER, &shelfLoop, &ts, false, &shelfLoopOnEnable);
Task tLed_color_alarm (TASK_IMMEDIATE, TASK_ONCE, &led_color_alarm, &ts, false);
Task tntpCheck ( TASK_SECOND, CONNECT_TIMEOUT, &ntpCheck, &ts, false );
// Tasks running on events
Task  tntpUpdate  (&ntpUpdateInit, &ts);



long  ledDelayRed, ledDelayBlue;

IPAddress     timeServerIP;       // time.nist.gov NTP server address
const char*   ntpServerName = "nz.pool.ntp.org";
byte          packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
unsigned long epoch;

WiFiUDP udp;                      // A UDP instance to let us send and receive packets over UDP

#define LOCAL_NTP_PORT  2390      // Local UDP port for NTP update

// Which pin on the ESP8266 is connected to the NeoPixels?
#define NEOPIXEL_PIN 4      // This is the D2 pin
#define PIEZO_PIN 5         // This is D1
#define analogInPin  A0     // ESP8266 Analog Pin ADC0 = A0

//************* Declare NeoPixel ******************************
//Using WS2812B 5050 RGB
// use NEO_KHZ800 but maybe 400 makes wifi more stable???

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
//Adafruit_NeoPixel stripred = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
//Adafruit_NeoPixel stripblue = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripred = Adafruit_NeoPixel(20, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripblue = Adafruit_NeoPixel(20, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool ClockInitialized = false;

time_t currentTime;
time_t nextCalcTime;
time_t nextAlarmTime;
time_t prevDisplay = 0;

template <typename T, typename U>
T nonNegMod(T n, U d ) {
  // computes non-negative n % d
  // result always >= 0 and < d
  n %= d;
  if (n >= 0) {
    return n;
  }
  return n + d;
}


// With assistance of ChatBot but buggy
// Define the virtual LED strips as substrip
// (NO checks that startPixel ... startPixel + lenPixels -1 is contained in strip, it will be clipped to strip)
// have option of wrap (default=true) on virtualLED Strip, otherwise clipping
class VirtualLEDStrip {
  private:
    Adafruit_NeoPixel& strip;
    uint16_t startPixel;
    uint16_t lenPixels;
    bool wrap;
  public:
    VirtualLEDStrip(Adafruit_NeoPixel& strip, uint16_t startPixel, uint16_t lenPixels, bool wrap = true)
      : strip(strip), startPixel(startPixel), lenPixels(lenPixels), wrap(wrap) {
    }
    void begin() {
      strip.begin();
    }
    void show() {
      strip.show();
    }
    void setPixelColor(int16_t n, uint32_t color) {
      if (wrap | (n >= 0 & n < lenPixels)) {
        strip.setPixelColor(startPixel + nonNegMod(n, lenPixels), color);
      }
    }

    void clear() {
      strip.fill(0, startPixel, lenPixels);
    }
    void fill(uint32_t color) {
      strip.fill(color, startPixel, lenPixels);
    }
    void fill(uint32_t color, int16_t stPix, uint16_t lenP) {
      if (lenP > 0) {
        strip.fill(color, startPixel + nonNegMod(stPix, lenPixels), 1 + nonNegMod((lenP - 1), lenPixels));
      }
    }
    uint32_t getPixelColor(int16_t n) const {
      if (wrap | (n >= 0 & n < lenPixels)) {
        return strip.getPixelColor(startPixel + nonNegMod(n, lenPixels));
      } else {
        return 0;
      }
    }
    //TODO BUG this affects whole strip
    //    void setBrightness(uint8_t brightness) {
    //      strip.setBrightness(brightness);
    //    }
    uint16_t getNumPixels() const {
      return lenPixels;
    }
};

// Create virtual LED strips
#ifdef TEST_CLOCK
VirtualLEDStrip virtualStripBottomShelf(strip, 0, 10);
VirtualLEDStrip virtualStripMiddleShelf(strip, 10, 10);
VirtualLEDStrip virtualStripTopShelf(strip, 20, 40);
#else
VirtualLEDStrip virtualStripBottomShelf(strip, 0, 20);    // First 20 LEDs
VirtualLEDStrip virtualStripMiddleShelf(strip, 20, 20);   // Next 20 LEDs in reverse order
VirtualLEDStrip virtualStripTopShelf(strip, 40, 72);   // Last 72 LEDs
#endif

//unsigned long clockInterval = 1000;  // show clock every 1000 milliseconds
uint8_t previousSecond = 0;
// Blinking variables
unsigned long blinkInterval = 5000;  // Blinking interval in milliseconds
unsigned long previousBlinkTime = 0;
bool blinkState = false;
// Moving pixel variables
unsigned long moveInterval = 1000;  // Moving interval in milliseconds
unsigned long previousMoveTime = 0;
uint16_t pixelIndex = 0;
// Rainbow cycling variables
unsigned long hueInterval = 200;  // Hue change interval in milliseconds
unsigned long previousHueTime = 0;
long hue = 0;

void setup() {
  Serial.begin(115200);
  delay(10); // Needed???
  Serial.println();
  // NOTE F(string)  forces to be in flash memory
#ifdef TEST_CLOCK
  Serial.println(F("TEST LED CLOCK with modified TaskScheduler test #14 - Yield and internal StatusRequests"));
#else
  Serial.println(F("LED CLOCK with modified TaskScheduler test #14 - Yield and internal StatusRequests"));
#endif

  Serial.println(F("=========================================================="));
  Serial.println();
  tntpUpdate.waitFor( tConnect.getInternalStatusRequest() );  // NTP Task will start only after connection is made
  sun.setPosition(LATITUDE, LONGITUDE, DST_OFFSET);
  startWebSocket();            // Start a WebSocket server
  startMDNS();                 // Start the mDNS responder
  startServer();               // Start a HTTP server with a file read handler and an upload handler
  startOTA();                  // Start the OTA service
  startSPIFFS();               // Start the SPIFFS and list all contents
  strip.begin(); // This initializes the NeoPixel library.
  stripred.begin(); // This initializes the NeoPixel library.
  stripblue.begin(); // This initializes the NeoPixel library.
  stripred.fill(0x100000);
  stripblue.fill(0x000010);
  //virtualStripBottomShelf.begin(); // not needed as strip.begin(); starts all virtualStrips on it
  //virtualStripMiddleShelf.begin();
  //virtualStripTopShelf.begin();
  randomSeed(now());

  // Initialize alarmTime(s) to default (now)
  for (int alarm_ind = 0; alarm_ind < NUM_ALARMS; alarm_ind++) {
    breakTime(now(), alarmInfo[alarm_ind].alarmTime);
    sprintf(buf, "Default alarmInfo[%d] set=%d, duration=%d, repeat=%d\n %d:%02d:%02d %s %d %s %d", alarm_ind, alarmInfo[alarm_ind].alarmSet, alarmInfo[alarm_ind].duration, alarmInfo[alarm_ind].repeat,
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
      sprintf(buf, "alarmInfo[%d] set=%d, type=%d, duration=%d, repeat=%d\n %d:%02d:%02d %s %d %s %d", alarm_ind, alarmInfo[alarm_ind].alarmSet, alarmInfo[alarm_ind].alarmType, alarmInfo[alarm_ind].duration, alarmInfo[alarm_ind].repeat,
              hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)),
              second(makeTime(alarmInfo[alarm_ind].alarmTime)), daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), day(makeTime(alarmInfo[alarm_ind].alarmTime)),
              monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), year(makeTime(alarmInfo[alarm_ind].alarmTime)));
      //Serial.println();
      Serial.println(buf);
    }
  }
  calcSun();
  // Must be here in startup
  tvisual_alarm.setLtsPointer (&alarmParm);
}

void loop() {
  ts.execute();                   // Only Scheduler should be executed in the loop
}

/**
   Initiate connection to the WiFi network
*/
void connectInit() {
  Serial.print(millis());
  Serial.println(F(": connectInit."));
  Serial.println(F("WiFi parameters: "));
  Serial.print(F("SSID: ")); Serial.println(homeSSID);
  Serial.print(F("PWD : ")); Serial.println(homePW);

  WiFi.mode(WIFI_STA);
  WiFi.hostname("esp8266");
  WiFi.begin(homeSSID, homePW);
  yield();

  ledDelayRed = TASK_SECOND / 2;
  ledDelayBlue = TASK_SECOND / 4;
  tLED.enable();

  tConnect.yield(&connectCheck);            // This will pass control back to Scheduler and then continue with connection checking
}

/**
   Periodically check if connected to WiFi
   Re-request connection every 5 seconds
   Stop trying after a timeout
*/
void connectCheck() {
  Serial.print(millis());
  Serial.println(F(": connectCheck."));

  if (WiFi.status() == WL_CONNECTED) {                // Connection established
    Serial.print(millis());
    Serial.print(F(": Connected to AP. Local ip: "));
    Serial.println(WiFi.localIP());
    tConnect.disable();
    tRunServer.enable();
    tRunWebSocket.enable();
    tOTARun.enable();
    tMDNSRun.enable();
  }
  else {

    if (tConnect.getRunCounter() % 5 == 0) {          // re-request connection every 5 seconds

      Serial.print(millis());
      Serial.println(F(": Re-requesting connection to AP..."));

      WiFi.disconnect(true);
      yield();                                        // This is an esp8266 standard yield to allow linux wifi stack run
      WiFi.hostname("esp8266");
      WiFi.mode(WIFI_STA);
      WiFi.begin(homeSSID, homePW);
      yield();                                        // This is an esp8266 standard yield to allow linux wifi stack run
    }

    if (tConnect.getRunCounter() == CONNECT_TIMEOUT) {  // Connection Timeout
      tConnect.getInternalStatusRequest()->signal(CONNECT_FAILED);  // Signal unsuccessful completion
      tConnect.disable();

      Serial.print(millis());
      Serial.println(F(": connectOnDisable."));
      Serial.print(millis());
      Serial.println(F(": Unable to connect to WiFi."));

      ledDelayRed = TASK_SECOND / 16;                  // Blink LEDs quickly due to error
      ledDelayBlue = TASK_SECOND / 16;
      tLED.enable();
    }
  }
}

/**
   Initiate NTP update if connection was established
*/
void ntpUpdateInit() {
  Serial.println();
  Serial.print(millis());
  Serial.println(F(": ntpUpdateInit."));

  if ( tConnect.getInternalStatusRequest()->getStatus() != CONNECT_OK ) {  // Check status of the Connect Task
    Serial.print(millis());
    Serial.println(F(": cannot update NTP - not connected."));
    return;
  }

  udp.begin(LOCAL_NTP_PORT);
  if ( WiFi.hostByName(ntpServerName, timeServerIP) ) { //get a random server from the pool

    Serial.print(millis());
    Serial.print(F(": timeServerIP = "));
    Serial.println(timeServerIP);

    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  }
  else {
    Serial.print(millis());
    Serial.println(F(": NTP server address lookup failed."));
    tLED.disable();
    udp.stop();
    tntpUpdate.disable();
    return;
  }

  ledDelayRed = TASK_SECOND / 8;
  ledDelayBlue = TASK_SECOND / 8;
  tLED.enable();

  // check NTP server response
  //tntpCheck.enableDelayed(); // did not work if NTP update failed previously
  tntpCheck.restartDelayed();
  //Serial.print(F(": 534"));
}

void serverRun () {
  server.handleClient();                      // run the server
}

void webSocketRun () {
  webSocket.loop();                           // constantly check for websocket events
}

void OTARun () {
  ArduinoOTA.handle();                        // listen for OTA events
}

void MDNSRun () {
  MDNS.update();                              // check for MDNS
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
  return false; // this line never gonna happen
}


/**
   Check if NTP packet was received
   Re-request every 5 seconds
   Stop trying after a timeout
*/
void ntpCheck() {
  Serial.print(millis());
  Serial.println(F(": ntpCheck."));

  // The last iteration will only occur if fails to update
  if ( tntpCheck.isLastIteration() ) {
    Serial.print(millis());
    Serial.println(F(": NTP Update failed, try again in 10 minutes"));
    currentTime = now();
    sprintf(buf, "NTP Update failed at %d:%02d:%02d retry in 10 minutes", hour(currentTime), minute(currentTime),
            second(currentTime));
    webSocket.sendTXT(websocketId_num, buf);
    udp.stop();
    tLED.disable();
    tshelfLoop.enable();
    tntpCheck.disable(); // not sure neccessary as only get here is last interation
    //reschedual in shorter time than usual
    tntpUpdate.restartDelayed(NTP_RE_CHECK_SEC * TASK_SECOND);
    return;
  }

  //  if ( tntpUpdate.getRunCounter() % 5 == 0) {
  if ( tntpCheck.getRunCounter() % 5 == 0) {

    Serial.print(millis());
    Serial.println(F(": Re-requesting NTP update..."));

    udp.stop();
    yield();
    udp.begin(LOCAL_NTP_PORT);
    sendNTPpacket(timeServerIP);
    return;
  }

  if ( doNtpUpdateCheck()) {
    Serial.print(millis());
    Serial.println(F(": NTP Update successful"));
    currentTime = now();
    sprintf(buf, "NTP Update at %d:%02d:%02d", hour(currentTime), minute(currentTime),
            second(currentTime));
    webSocket.sendTXT(websocketId_num, buf);

    Serial.printf("now %d\n", now());
    Serial.print(millis());
    Serial.print(F(": Unix time = "));
    Serial.println(epoch);
    setTime(epoch);
    adjustTime(TIME_CORRECTION);
    adjustTime(hours_Offset_From_GMT * 3600);
    if (IsDst()) adjustTime(3600); // offset the system time by an hour for Daylight Savings
    Serial.printf("now fixed %d\n", now());
    ClockInitialized = true;
    //ledDelayRed = TASK_SECOND / 3; //1
    //ledDelayBlue = 2 * TASK_SECOND; //2
    udp.stop();
    tLED.disable();
    //tchangeClock.enable();
    tshelfLoop.enable();
    tntpCheck.disable();
    tntpUpdate.restartDelayed(NTP_CHECK_SEC * TASK_SECOND);
  }
}

/**
   Send NTP packet to NTP server
*/
void sendNTPpacket(IPAddress & address)
{
  Serial.print(millis());
  Serial.println(F(": sendNTPpacket."));

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println(F(": 661"));
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  //Serial.println(F(": 663"));
  udp.endPacket();
  //Serial.println(F(": 665"));
  yield();
  //Serial.println(F(": 667"));
}

/**
   Check if a packet was recieved.
   Process NTP information if yes
*/
bool doNtpUpdateCheck() {

  Serial.print(millis());
  Serial.println(F(": doNtpUpdateCheck."));

  yield();
  int cb = udp.parsePacket();
  if (cb) {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    epoch = secsSince1900 - seventyYears;
    return (epoch != 0);
  }
  return false;
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

  server.on("/restart", []() {
    server.send(200, "text/plain", "Restarting ...");
    ESP.restart();
  });

  server.on("/spiff", []() {
    Dir dir = SPIFFS.openDir("/");
    String output = "SPIFF: \r\n\n";
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      //Serial.println("NAME " + fileName);
      sprintf(buf, "%s\t %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
      output += buf;
    }
    server.send(200, "text/plain", output);
  });

  server.on("/whattime", []() {
    sprintf(buf, "%d:%02d:%02d %s %d %s %d", hour(), minute(), second(), daysOfWeek[weekday()].c_str(), day(), monthNames[month()].c_str(), year());
    server.send(200, "text/plain", buf);
    //delay(1000);
  });

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
  server.send(200, "application/json", output);
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
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] payload: %s length: %d\n", num, payload, length);
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
        led_color_alarm_rgb = strip.Color(r >> 2, g >> 2, b >> 2);
        tLed_color_alarm.restart(); // one shot task that turns all LEDS the above color
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
      } else if (payload[0] == 'R') {                      // the browser sends an R when the visual_alarm effect is enabled
        light_alarm_flag = true;
        //TODO why if light_alarm_num was declared byte did this blow up had to make int
        int num_points;
        int num_bytes;
        int num_b;
        // %n format is number of bytes scanned at this point
        sscanf((char *) payload, "R%d %d %d %d %d %d %d %d %d %d %d %n", &alarmParm.percentWheelPerFrame, &alarmParm.percentWheelPerStrip, &alarmParm.firsthue, &alarmParm.nodepix_coef2, &alarmParm.nodepix_coef1, &alarmParm.nodepix_coef0, &frames_per_sec, &light_alarm_num, &alarmParm.ex, &alarmParm.duration, &num_points, &num_bytes);
        // extract points from above and use push_back to insert into
        //points_input
        int xpt;
        int ypt;
        points_input.clear();
        for (int i = 0; i < num_points; i++) {
          sscanf((char *)payload + num_bytes, "%d %d%n", &xpt, &ypt, &num_b);
          points_input.push_back({xpt * NUM_LEDS / 300, ypt* NUM_LEDS / 300});
          num_bytes += num_b;
        }

        alarmParm.duration *= 1000;
        alarmParm.firsthue *= 256;
        if (frames_per_sec < 1) {
          alarmParm.wait = 1000;
        } else {
          alarmParm.wait = 1000 / frames_per_sec;
        }
        alarmParm.wait_delta = 0;
        if (light_alarm_num < 0) {
          alarmParm.wait_delta = alarmParm.wait / 2;
        }
        alarmParm.frame = alarm_frame[abs(light_alarm_num) % num_animations];
        Serial.printf("Wh/Fr = %d, Wh/Str = %d, 1stHue = %d, nodeC2 = %d,  nodeC1 = %d,  nodeC0 = %d,  fps = %d, anim# = %d, ex = %d, dt = %d, dur = %d\n", alarmParm.percentWheelPerFrame, alarmParm.percentWheelPerStrip, alarmParm.firsthue / 256, alarmParm.nodepix_coef2, alarmParm.nodepix_coef1, alarmParm.nodepix_coef0, frames_per_sec, abs(light_alarm_num) % num_animations, alarmParm.ex, alarmParm.wait_delta, alarmParm.duration / 1000);
        //Serial.printf("%d\n", num_bytes);
        //sprintf(buf, "Wh/Fr = %d, Wh/Str = %d, 1stHue = %d, fps = %d, anim# = %d, ex = %d, dt = %d", alarmParm.percentWheelPerFrame, alarmParm.percentWheelPerStrip, alarmParm.firsthue / 256, frames_per_sec, abs(light_alarm_num) % num_animations, alarmParm.ex, alarmParm.wait_delta);
        //webSocket.sendTXT(num, buf);
        if (alarmParm.ex == 6) {
          Serial.printf("Graph has %d points\n", num_points);
          for (int i = 0; i < num_points; i++) {
            Serial.printf("(%d, %d) ", points_input[i].first, points_input[i].second);
          }
          Serial.printf("\n");
        }
        tvisual_alarm.enable();
      } else if (payload[0] == 'L') {                      // the browser sends an L when the meLody effect is enabled
        sound_alarm_flag = true;
        tplayMelody.enable();
        //digitalWrite(ESP_BUILTIN_LED, 1);  // turn off the LED
      } else if (payload[0] == 'W') {                      // the browser sends an W for What time?
        sprintf(buf, "%d:%02d:%02d %s %d %s %d", hour(), minute(), second(), daysOfWeek[weekday()].c_str(), day(), monthNames[month()].c_str(), year());
        webSocket.sendTXT(num, buf);
        //digitalWrite(ESP_BUILTIN_LED, 0);  // turn on the LED
      } else if (payload[0] == 'A') {                      // the browser sends an A to set alarms
        char Aday[4]; //3 char
        char Amonth[4]; //3 char
        int  AmonthNum;
        int Adate;
        int Ayear;
        int Ahour;
        int Aminute;
        int alarm_ind;
        int alarmtype;
        int alarmrepeat;
        int alarmduration;
        //sprintf(buf, "Set Alarm for %s length: %d", payload, length);
        //webSocket.sendTXT(num, buf);
        sscanf((char *) payload, "A%d %d %d %d %d %s %s %2d %4d %2d:%2d", &alarm_ind, &alarmtype, &alarmrepeat, &alarmduration, &AmonthNum, Aday, Amonth, &Adate, &Ayear, &Ahour, &Aminute);
        //PREVENT bad input
        if (alarm_ind >= 5) alarm_ind = 0;
        if (alarm_ind < 0) alarm_ind = 0;
        Serial.printf("Set alarm[%d] for %s %s %2d %2d %4d %2d:%2d\n", alarm_ind, Aday, Amonth, Adate, AmonthNum + 1, Ayear, Ahour, Aminute);
        sprintf(buf, "Set alarm for %s %s %2d %2d %4d %2d:%2d", Aday, Amonth, Adate, AmonthNum + 1, Ayear, Ahour, Aminute);
        webSocket.sendTXT(num, buf);
        setalarm(alarm_ind, alarmtype, alarmduration, alarmrepeat, 0, Aminute, Ahour, Adate, AmonthNum + 1, Ayear);
        alarmInfo[alarm_ind].alarmSet = true;

        sprintf(buf, "Alarm[%d] Set to %d:%02d:%02d %s %d %s %d, duration %d ms repeat %d sec", alarm_ind,
                hour(makeTime(alarmInfo[alarm_ind].alarmTime)), minute(makeTime(alarmInfo[alarm_ind].alarmTime)), second(makeTime(alarmInfo[alarm_ind].alarmTime)),
                daysOfWeek[weekday(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), day(makeTime(alarmInfo[alarm_ind].alarmTime)),
                monthNames[month(makeTime(alarmInfo[alarm_ind].alarmTime))].c_str(), year(makeTime(alarmInfo[alarm_ind].alarmTime)),
                alarmInfo[alarm_ind].duration, alarmInfo[alarm_ind].repeat);
        webSocket.sendTXT(num, buf);

      } else if (payload[0] == 'S') {                      // the browser sends an S to compute sunsets
        Serial.printf("Compute Sunsets\n");
        calcSun();
      }
      break;
  }
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

void setalarm(int alarm_ind, int alarmtype,  uint16_t t, uint32_t r, uint8_t s, uint8_t m, uint8_t h, uint8_t d, uint8_t mth, uint16_t y) {
  Serial.printf("setalarmtime: %d %d %d %d %d %d %d %d %d\n", alarmtype, t, r, s, m, h, d, mth, y);
  alarmInfo[alarm_ind].alarmType = alarmtype;
  alarmInfo[alarm_ind].duration = t;
  alarmInfo[alarm_ind].repeat = r;
  alarmInfo[alarm_ind].alarmTime.Second = s;
  alarmInfo[alarm_ind].alarmTime.Minute = m;
  alarmInfo[alarm_ind].alarmTime.Hour = h;
  alarmInfo[alarm_ind].alarmTime.Day = d;
  alarmInfo[alarm_ind].alarmTime.Month = mth;
  //NOTE year is excess from 1970
  alarmInfo[alarm_ind].alarmTime.Year = y - 1970;
}

void calcSun()
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

  sprintf(buf, "Prev Calculated Sunset at %d:%02d, Sunrise at %d:%02d", Sunset.Hour, Sunset.Minute, Sunrise.Hour, Sunrise.Minute);
  webSocket.sendTXT(websocketId_num, buf);

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

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/
// Taken from ConfigFile example
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }
  int alarm_ind = 0;
  for (JsonObject alarm : doc["alarms"].as<JsonArray>()) {
    //TODO if alarm_ind >=NUM_ALARMS abort
    alarmInfo[alarm_ind].alarmSet = alarm["alarmSet"]; // true, true, true, true, true
    alarmInfo[alarm_ind].alarmType = alarm["alarmType"]; // 0, 0, 0, 0, 0
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
  return true;
}

bool saveConfig() {

  StaticJsonDocument<1024> doc;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (int alarm_ind = 0; alarm_ind < NUM_ALARMS; alarm_ind++) {
    JsonObject alarms_nested = alarms.createNestedObject();
    alarms_nested["alarmSet"] = alarmInfo[alarm_ind].alarmSet;
    alarms_nested["alarmType"] = alarmInfo[alarm_ind].alarmType;
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

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  serializeJson(doc, configFile);
  return true;
}

/*_____________For Play Melody Task______________*/

bool playMelodyOnEnable() {
  melodyNoteIndex = 0;
  return true;
}

void playMelodyOnDisable() {
  noTone(PIEZO_PIN);
}
void playMelody() {
  if (melody[melodyNoteIndex] == STOP) {  //STOP is defined to be -1
    tplayMelody.disable();
    return;
  }
  // to calculate the note duration, take whole note durations divided by the note type.
  //e.g. quarter note = whole_note_duration / 4, eighth note = whole_note_duration/8, etc.
  int noteDuration = whole_note_duration / noteDurations[melodyNoteIndex];
  if (melody[melodyNoteIndex] > 30) {
    tone(PIEZO_PIN, melody[melodyNoteIndex], noteDuration);
  }
  melodyNoteIndex++;
  // to distinguish the notes, set a minimum time between them.
  // the note's duration + 30% seems to work well:
  int pauseBetweenNotes = noteDuration * 1.30;
  tplayMelody.delay( pauseBetweenNotes );
}

//bool led_color_alarmOnEnable() {
//  tchangeClock.disable();
//  return true;
//}

void led_color_alarm() {
  //tchangeClock.enableDelayed(LED_SHOW_TIME);
  tshelfLoop.enableDelayed(LED_SHOW_TIME);
  strip.fill(led_color_alarm_rgb);
  SetBrightness(now()); // Set the clock brightness dependant on the time
  strip.show(); // Update strip with new contents
}

bool shelfLoopOnEnable() {
  // Clock variables
  //clockInterval = 1000;
  previousSecond = 0;
  // Blinking variables
  blinkInterval = 5000;  // Blinking interval in milliseconds
  previousBlinkTime = 0;
  blinkState = false;
  // Moving pixel variables
  moveInterval = 1000;  // Moving interval in milliseconds
  previousMoveTime = 0;
  pixelIndex = 0;
  // Rainbow cycling variables
  hueInterval = 200;  // Hue change interval in milliseconds
  previousHueTime = 0;
  hue = 0;
  return true;
}

// This is callback of main Loop task for LED displays
// They all use timers to specify their frequency of occurance
void shelfLoop() {
  switch (day_disp_ind) {
    case 0:
      // Alternate color of all LEDs on the first virtual strip every specified period
      if (millis() - previousBlinkTime >= blinkInterval) {
        previousBlinkTime = millis();
        blinkState = !blinkState;
        if (blinkState) {
          virtualStripBottomShelf.fill(virtualStripTopShelf.getPixelColor(4));
          //virtualStripBottomShelf.fill(strip.Color(0, 0, 255));  // Blue color
        } else {
          virtualStripBottomShelf.fill(virtualStripTopShelf.getPixelColor(24));
          //virtualStripBottomShelf.fill(strip.Color(127, 0, 127));    // Magenta
        }
        virtualStripBottomShelf.show();
      }

      // Move a single white pixel along the second virtual strip every specified period
      if (millis() - previousMoveTime >= moveInterval) {
        previousMoveTime = millis();
        prevDisplay = now();
        //virtualStripMiddleShelf.setPixelColor(pixelIndex, strip.Color(0, 0, 0));  // blank
        virtualStripMiddleShelf.setPixelColor(pixelIndex, virtualStripTopShelf.getPixelColor(14));
        virtualStripMiddleShelf.show();
        pixelIndex = (pixelIndex + 1) % virtualStripMiddleShelf.getNumPixels();
        virtualStripMiddleShelf.setPixelColor(pixelIndex, strip.Color(255, 255, 255));        // white on next
      }

      // Cycle through a rainbow pattern on the third virtual strip every specified periiod
      if (millis() - previousHueTime >= hueInterval) {
        previousHueTime = millis();
        uint16_t numPixels = virtualStripTopShelf.getNumPixels();
        for (uint16_t i = 0; i < numPixels; ++i) {
          int pixelHue = hue + i * 65536L / numPixels;
          virtualStripTopShelf.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        }
        virtualStripTopShelf.show();
        hue += 256;
      }
      break;
    default:
      // NOTE needed to use timer, without it was calling Serial output
      //   in the changeClock() routine so much kept getting WDT resets
      //   using change of second() so will change clock as close to true time as pos
      if (second() != previousSecond) {
        prevDisplay = now();
        previousSecond = second();
        changeClock();
      }
  }

  // Check for alarms
  // Note if multiple alarms scheduled for same time only highest alarm_ind will display
  //
  for (int alarm_ind = 0; alarm_ind < NUM_ALARMS; alarm_ind++) {
    // NO use prevDisplay + 2 since time is adusted by 2 seconds to improve sync
    if (alarmInfo[alarm_ind].alarmSet && prevDisplay  >= makeTime(alarmInfo[alarm_ind].alarmTime))
    {
      if (prevDisplay - 10 < makeTime(alarmInfo[alarm_ind].alarmTime)) {
        // only show the alarm if close to set time
        // this prevents alarm from going off on a restart where configured alarm is in past
        currentTime = now();
        //HACK alarmType XXYY gives percentWheelPerFrame XX, percentWheelPerStrip YY0
        alarmParm.percentWheelPerFrame = (alarmInfo[alarm_ind].alarmType / 100);
        alarmParm.percentWheelPerStrip = (alarmInfo[alarm_ind].alarmType % 100) * 10;
        alarmParm.duration = alarmInfo[alarm_ind].duration;
        alarmParm.frame = &rainbowFrame;
        alarmParm.wait = 50;
        alarmParm.wait_delta = 0;
        alarmParm.ex = 1;
        tvisual_alarm.enable();
        //show_alarm_pattern(alarmInfo[alarm_ind].alarmType, alarmInfo[alarm_ind].duration);
        // redraw clock now to restore clock leds (thus not leaving alarm display on past its duration)
        //Draw_Clock(now(), 4); // Draw the whole clock face with hours minutes and seconds
        sprintf(buf, "%d %d %d Alarm [%d] at %d:%02d:%02d %s %d %s %d", alarmInfo[alarm_ind].alarmType, alarmParm.percentWheelPerFrame, alarmParm.percentWheelPerStrip, alarm_ind, hour(currentTime), minute(currentTime),
                second(currentTime), daysOfWeek[weekday(currentTime)].c_str(), day(currentTime),
                monthNames[month(currentTime)].c_str(), year(currentTime));
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
}
//void rainbowFrame(long firstPixelHue, int percentWheelPerStrip, int nodepix){
void rainbowFrame() {
  Task& T = ts.currentTask();
  visual_alarm_parm& parm = *((visual_alarm_parm*) T.getLtsPointer());
  for (int i = 0; i < parm.numpixeltouse; i++) { // For each pixel to use ...
    //int pixelHue = parm.firstPixelHue + (i * parm.ncolorloop *  65536L / strip.numPixels() / parm.ncolorfrac);
    int pixelHue = parm.firstPixelHue + (i * 65536L  * parm.percentWheelPerStrip / 100  / parm.numpixeltouse );
    //strip.setPixelColor(ClockCorrect(i + parm.nodepix), strip.gamma32(strip.ColorHSV(pixelHue)));
    parm.pixelHSV[i] = strip.gamma32(strip.ColorHSV(pixelHue));
  }
}

void color_wipe_frame() {
  Task& T = ts.currentTask();
  visual_alarm_parm& parm = *((visual_alarm_parm*) T.getLtsPointer());
  if (parm.percentWheelPerFrame >= 0) {
    parm.nodepix_ind ++;
    if (parm.nodepix_ind  >= parm.numpixeltouse) {
      parm.nodepix_ind = 0;
      parm.firsthue += parm.hueinc;
    }
  } else {
    parm.nodepix_ind --;
    if (parm.nodepix_ind  < 0) {
      parm.nodepix_ind =  parm.numpixeltouse - 1;
      parm.firsthue += parm.hueinc;
    }
  }
  int pixelHue = parm.firsthue + (parm.nodepix_ind * 65536L  * parm.percentWheelPerStrip / 100  / parm.numpixeltouse );
  parm.pixelHSV[parm.nodepix_ind] = strip.gamma32(strip.ColorHSV(pixelHue));
  //strip.setPixelColor(ClockCorrect(parm.nodepix_ind), strip.gamma32(strip.ColorHSV(pixelHue)));

}

void fire_fly_frame() {
  Task& T = ts.currentTask();
  visual_alarm_parm& parm = *((visual_alarm_parm*) T.getLtsPointer());
  uint8_t pixel;
  long pixelHue;
  int numff = 5;
  long minHue = 0;
  long maxHue = 64000;
  uint16_t hueInc = 200;
  //strip.fill(); // clear
  for (int i = 0;  i < parm.numpixeltouse; i++) {
    parm.pixelHSV[i] = 0;
  }
  for (int i = 0; i < numff; i++) {
    pixel = random(parm.numpixeltouse);
    if (hueInc == 0) {
      // Choose random color
      pixelHue = random(minHue, maxHue);
    } else { // inc
      pixelHue += hueInc;
      if (pixelHue > maxHue) {
        pixelHue = minHue;
      }
    }
    //Sat = random(minSat, maxSat);
    //Val = random(minVal, maxVal);
    //strip.setPixelColor(pixel, strip.gamma32(strip.ColorHSV(pixelHue, Sat, Val)));
    parm.pixelHSV[pixel] = strip.gamma32(strip.ColorHSV(pixelHue));

    //strip.setPixelColor(pixel, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
}

void flicker_frame() {
  Task& T = ts.currentTask();
  visual_alarm_parm& parm = *((visual_alarm_parm*) T.getLtsPointer());
  uint32_t fire_color   = strip.Color ( 255,  127,  00);
  for (int i = 0; i < parm.numpixeltouse; i++) {
    int r = random(255);
    parm.pixelHSV[i] = Subtract(fire_color, strip.Color ( r, r , r / 2));
  }
}

bool visual_alarmOnEnable() {
  Task& T = ts.currentTask();
  visual_alarm_parm& parm = *((visual_alarm_parm*) T.getLtsPointer());
  parm.firstPixelHue = parm.firsthue;
  parm.hueinc = parm.percentWheelPerFrame * 65536L / 100;
  parm.nodepix_ind = 0;
  parm.nodepix = parm.nodepix_coef0;
  parm.nodepix_diff = parm.nodepix_coef1 + parm.nodepix_coef2;
  parm.start = millis();
  tshelfLoop.disable();
  parm.wait_adj = 0;
  parm.wait_use = parm.wait;
  strip.fill(); // clear
  for (int i = 0;  i < parm.numpixeltouse; i++) {
    parm.pixelHSV[i] = 0;
  }
  return true;
}

void visual_alarmCallback() {
  std::vector<std::pair<int8_t, int8_t>> points;
  int j;
  Task& T = ts.currentTask();
  visual_alarm_parm& parm = *((visual_alarm_parm*) T.getLtsPointer());
  //TODO assuming NUM_LEDS is even so there are two middle point
  switch (parm.ex) {
    case 0:
      points = {{0, 0}, {NUM_LEDS - 1, NUM_LEDS - 1}};
      break;
    case 1:
      points = {{0, 0}, {NUM_LEDS / 2 - 1, NUM_LEDS - 1}, {NUM_LEDS / 2, NUM_LEDS - 1},  {NUM_LEDS - 1, 0}};
      break;
    case 2:
      points = {{0, 0}, {NUM_LEDS / 3, 2 * NUM_LEDS / 3 - 1}, {2 * NUM_LEDS / 3, NUM_LEDS / 3 - 1}, {NUM_LEDS - 1, NUM_LEDS - 1}};
      break;
    case 3:
      points = {{0, 0}, {NUM_LEDS / 3, NUM_LEDS / 2}, {2 * NUM_LEDS / 3, NUM_LEDS / 2 }, {NUM_LEDS - 1, NUM_LEDS - 1}};
      break;
    case 4:
      points = {{0, 0}, {NUM_LEDS / 3, NUM_LEDS - 1}, {2 * NUM_LEDS / 3, NUM_LEDS - 1 }, {NUM_LEDS - 1, 0}};
      break;
    case 5:
      points = {{0, 0}, {NUM_LEDS / 4, NUM_LEDS - 1}, {NUM_LEDS / 2, NUM_LEDS / 2}, {3 * NUM_LEDS / 4, NUM_LEDS - 1}, {NUM_LEDS - 1, 0}};
      break;
    default:
      points = points_input;
  }

  // parm.frame() setup for specific animation giving next frame
  // parm.frame() returns parm.pixelHSB[i] for i = 0 to parm.numpixeltouse
  parm.frame();
  // Embed parm.pixelHSV into strip
  for (int i = 0; i < parm.numpixeltouse; i++) {
    j = piecewise_linear(i, points);
    strip.setPixelColor(ClockCorrect(i + parm.nodepix / 100), parm.pixelHSV[j]);
    //if (parm.pixelHSV[j] != 0) Serial.printf("(%d, %d)-", i, j);
    //strip.setPixelColor(i + parm.nodepix, parm.pixelHSV[j]);
  }
  //Serial.printf("\n");
  SetBrightness(now()); // Set the clock brightness dependant on the time
  strip.show(); // Update strip with new contents
  if ((millis() - parm.start) > parm.duration) {
    tvisual_alarm.disable();
    Draw_Clock(now(), 4); // Draw the whole clock face with hours minutes and seconds so visual alarm stops immediately
    tshelfLoop.enable();
  } else {
    parm.firstPixelHue += parm.hueinc;
    parm.nodepix += parm.nodepix_diff;
    parm.nodepix_diff += 2 * parm.nodepix_coef2;
    //Serial.printf(" n = %d, d = %d\n", parm.nodepix, parm.nodepix_diff);
    //TODO parm.nodepix_ind can not vary, vary randomly, vary periodically sine, sawtooth, triangle
    //TODO parm.wait can not vary, vary randomly, vary periodically sine, sawtooth, triangle
    if (parm.wait_delta > 0) {
      parm.wait_adj = random(-parm.wait_delta, parm.wait_delta);
      parm.wait_use = parm.wait + parm.wait_adj;
    }
    tvisual_alarm.delay(parm.wait_use);  // Pause for a moment
  }
}

/*
   Flip the LED state based on the current state
*/
bool ledState;
void ledCallback() {
  if ( ledState ) ledBlue();
  else ledRed();
}

/**
   Make sure the LED starts red
*/
bool ledOnEnable() {
  ledRed();
  return true;
}

/**
   Make sure LED ends dimmed
*/
void ledOnDisable() {
  ledBlue();
}

/**
   Turn ring red
*/
void ledRed() {
  ledState = true;
  //strip.fill(0x100000);
  //strip.show();
  stripred.show();
  tLED.delay( ledDelayRed );
}

/**
   Turn ring blue.
*/
void ledBlue() {
  ledState = false;
  //strip.fill(0x000010);
  //strip.show();
  stripblue.show();
  tLED.delay( ledDelayBlue );
}


// This is run when second() changes (tested in shelfLoop())
void changeClock() {
  time_t tnow = now(); // Get the current time seconds
  Draw_Clock(tnow, 4); // Draw the whole clock face with hours minutes and seconds
  if (second() == 0)
    digitalClockDisplay();
  else
    Serial.print('-');
  //TODO could make daily task here
  // Check if new day and recalculate sunSet etc.
  if (tnow >= makeTime(calcTime))
  {
    calcSun(); // computes sun rise and sun set, updates calcTime
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
//TODO check, tried to make change exactly on change of second by showing
// pre calculated strip then calculating display for coming time. but
//   seems to be worse.
void Draw_Clock(time_t tnow, byte Phase)
{
  //NOTE time as been adjusted coming time makes clockdisplay in sync with iphone time second hand
  //   thought it should have been 1 but 2 is better :-O

  // show previous set up LED for time
  SetBrightness(tnow); // Set the clock brightness dependant on the time
  strip.show(); // show all the pixels

  // Now calculate display for next time

  if (Phase <= 0) // Set all pixels black
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(ClockCorrect(i), strip.Color(0, 0, 0));

  int disp_ind;
  if (auto_night_disp) {
    disp_ind = IsDay(tnow) ?  day_disp_ind : 4;
  } else {
    disp_ind = day_disp_ind;
  }

  if (Phase >= 1) // Draw all pixels background color
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(ClockCorrect(i), strip.Color(Background[disp_ind].r, Background[disp_ind].g, Background[disp_ind].b));

  if (Phase >= 2) // Draw hour markers on top shelf
    for (int i = 0; i < virtualStripTopShelf.getNumPixels(); i = i + virtualStripTopShelf.getNumPixels() / 12)
      virtualStripTopShelf.setPixelColor(i, strip.Color(Divisions[disp_ind].r, Divisions[disp_ind].g, Divisions[disp_ind].b)); // for Phase = 2 or more, draw 5 minute divisions

  if (Phase >= 3) { // Draw 15 min markers on middle shelf
    for (int i = 0; i < virtualStripMiddleShelf.getNumPixels(); i = i + virtualStripMiddleShelf.getNumPixels() / 4)
      virtualStripMiddleShelf.setPixelColor(i, strip.Color(Quarters[disp_ind].r, Quarters[disp_ind].g, Quarters[disp_ind].b));
  }

  if (Phase >= 4) { // Draw hands
    // find indices of nearest LED
    int isecond = second(tnow) * virtualStripBottomShelf.getNumPixels() / 60;
    int iminute = virtualStripMiddleShelf.getNumPixels() - (60 * minute(tnow) + second(tnow)) * virtualStripMiddleShelf.getNumPixels() / 3600;
    int ihour = (( 3600 * (hour(tnow) + 6) + 60 * minute(tnow) + second(tnow)) * virtualStripTopShelf.getNumPixels() / 43200) % virtualStripTopShelf.getNumPixels();

    //hour on Top Shelf
    // Make color for hour
    uint32_t hourcolor;
    uint16_t hourhue;
    uint8_t hoursat = 0;
    // Special case for disp_ind == 1
    // Top shelf show hours 6, 7, 8, 9, 10, 11, 12,  1, 2, 3, 4, 5
    // Use colors           R  O  Y  G  B   V White  V  B  G  Y  O
    if (disp_ind == 1) {
      int adjhour = (hour(tnow) + 6) % 12;
      adjhour = (adjhour < 7) ? adjhour : 12 - adjhour;
      if (adjhour == 6) {
        hourcolor = strip.Color(255, 255, 255);
        hoursat = 0;
      } else {
        hoursat = 255;
        //hourhue = adjhour * 65536 / 6;
        hourhue = HourHues[adjhour]; // index must be between 0 and 5 inc
        hourcolor = strip.gamma32(strip.ColorHSV(hourhue));
      }
    } else { // other cases use stored selected colors
      hourcolor = strip.Color(Hour[disp_ind].r, Hour[disp_ind].g, Hour[disp_ind].b);
    }

    virtualStripTopShelf.setPixelColor(ihour, hourcolor);
    for (int i = 0; i <= hour_width[disp_ind]; i++) {
      virtualStripTopShelf.setPixelColor(ihour - i, hourcolor);
      virtualStripTopShelf.setPixelColor(ihour + i, hourcolor);
    }
    //minute on middle shelf (this shelf is reversed)
    uint32_t use_color;

    if (disp_ind == 1) {
      // Special case fill with hourcolor from left to proportion of hour eg 15 min is filled 1/4 of middle shelf
      virtualStripMiddleShelf.fill(hourcolor, iminute, virtualStripMiddleShelf.getNumPixels() - iminute);
    } else {
      // other case used selected color, width and flashing mode
      if (minute_width[disp_ind] >= 0) {
        // optional to help identification, minute hand flshes between normal and half intensity
        if ((second(tnow) % 2) | (not minute_blink[disp_ind])) {
          use_color = strip.Color(Minute[disp_ind].r, Minute[disp_ind].g, Minute[disp_ind].b);
        } else {
          use_color = strip.Color(Minute[disp_ind].r / 2, Minute[disp_ind].g / 2, Minute[disp_ind].b / 2 );
        }
        virtualStripMiddleShelf.setPixelColor(iminute, use_color);
        for (int i = 1; i <= minute_width[disp_ind]; i++) {
          virtualStripMiddleShelf.setPixelColor(iminute - i, use_color); // to help identification, minute hand flashes between normal and half intensity
          virtualStripMiddleShelf.setPixelColor(iminute + i, use_color); // to help identification, minute hand flashes between normal and half intensity
        }
      }
    }

    //second on bottom shelf
    if (disp_ind == 1) {
      // Special case fill with hourcolor from left to proportion of minute eg 30 sec is filled 1/2 of lower shelf
      //         and brightness starts at and rest proportional to square of this fraction eg 64 + 1/4 * 191 for above example
      virtualStripBottomShelf.clear();
      virtualStripBottomShelf.fill(strip.gamma32(strip.ColorHSV(hourhue, hoursat, 64 + second(tnow) * second(tnow) * 191 / 3600)), 0, isecond);
    } else {
      // other case used selected color and width
      if (second_width[disp_ind] >= 0) {
        virtualStripBottomShelf.setPixelColor(isecond, strip.Color(Second[disp_ind].r, Second[disp_ind].g, Second[disp_ind].b));
        for (int i = 0; i <= second_width[disp_ind]; i++) {
          virtualStripBottomShelf.setPixelColor(isecond - i, strip.Color(Second[disp_ind].r, Second[disp_ind].g, Second[disp_ind].b));
          virtualStripBottomShelf.setPixelColor(isecond + i, strip.Color(Second[disp_ind].r, Second[disp_ind].g, Second[disp_ind].b));
        }
      }
    }
  }
}

bool IsDay(time_t t)
{
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
  if (IsDay(t) & ClockInitialized)
    strip.setBrightness(day_brightness);
  else
    strip.setBrightness(night_brightness);
}

//************* This function reverses the pixel order ******************************
//              and ajusts top of clock
int ClockCorrect(int Pixel)
{
  Pixel = nonNegMod(Pixel + TopOfClock, NUM_LEDS);
  if (ClockGoBackwards)
    return nonNegMod((NUM_LEDS - Pixel + NUM_LEDS / 2), NUM_LEDS); // my first attempt at clock driving had it going backwards :)
  else
    return (Pixel);
}


//********* Bills NeoPixel Routines


#include <cmath>
//#include <vector>

//using namespace std;
int8_t piecewise_linear(int8_t x, std::vector<std::pair<int8_t, int8_t>> points) {
  if (points.size() == 0) {
    return 0;
  }
  if (x < points[0].first) {
    return static_cast<int8_t>(points[0].second);
  }
  for (int i = 0; i < points.size() - 1; i++) {
    if (x < points[i + 1].first) {
      int x1 = points[i].first;
      int y1 = points[i].second;
      int x2 = points[i + 1].first;
      int y2 = points[i + 1].second;
      //NOTE can never get (x2 == x1) since points[i+1] is first point above x
      return y1 + (y2 - y1) * (x - x1) / static_cast<double>(x2 - x1);
    }
  }
  return static_cast<int8_t>(points.back().second);
}
/*
  // Mod of Adafruit Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
  void rainbow2(int wait, int ex, long firsthue, int hueinc,  int ncolorloop, int ncolorfrac, int nodepix, time_t t, uint16_t duration) {
  // Hue of first pixel runs ncolorloop complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to ncolorloop*65536. Adding 256 to firstPixelHue each time
  // means we'll make ncolorloop*65536/256   passes through this outer loop:
  time_elapsed = 0;
  uint16_t time_start = millis();
  long firstPixelHue = firsthue;
  std::vector<std::pair<int8_t, int8_t>> points;
  int j;
  switch (ex) {
    case 1:
      points = {{0, 0}, {NUM_LEDS - 1, NUM_LEDS}};
      break;
    case 2:
      points = {{0, 0}, {NUM_LEDS / 2, NUM_LEDS / 2 - 1}, {NUM_LEDS - 1, 0}};
      break;
    case 3:
      points = {{0, 0}, {NUM_LEDS / 2, NUM_LEDS}, {NUM_LEDS - 1, 0}};
      break;
    case 4:
      points = {{0, 0}, {NUM_LEDS / 3, NUM_LEDS / 3 - 1}, {2 * NUM_LEDS / 3, 0}, {NUM_LEDS - 1, NUM_LEDS / 3}};
      break;
    default:
      points = {{0, 0}, {NUM_LEDS / 4, NUM_LEDS}, {NUM_LEDS / 2, 0}, {3 * NUM_LEDS / 4, NUM_LEDS}, {NUM_LEDS - 1, 0}};
  }

  while (time_elapsed < duration) {
    firstPixelHue += hueinc;
    for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make ncolorloop/ ncolorfrac  revolutions of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      j = piecewise_linear(i, points);
      int pixelHue = firstPixelHue + (j * ncolorloop * 65536L / strip.numPixels() / ncolorfrac);
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(ClockCorrect(i + nodepix), strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
    time_elapsed = millis() - time_start;
  }
  }
*/

/*
  class Worm
  {
    // colors, a list of worm segment (starting with head) hues
    // path a list of the LED indices over which the worm will travel (from 0 to 59 for clock)
    // cyclelen controls speed, worm movement only when LED upload cycles == 0 mod cyclelen
    // height (of worm segments) is same length as colors: higher value worms segments go over top of lower value worms
    // equal value segments have later worm having priority
  private:
    std::vector < int >colors;
    std::vector < int >path;
    int cyclelen;
    std::vector < int >height;
    int activecount;
    int direction;
    int headposition;
  public:
    Worm (std::vector < int >colors, std::vector < int >path, int cyclelen,
          int direction, std::vector < int >height)
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
        Serial.println(" ");
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

  void moveworms(int wait, int ex, int ncolorloop, time_t t, uint16_t duration) {
  time_elapsed = 0;
  uint16_t time_start = millis();
  std::vector < int >colors = {100, 101, 102, 103, 104, 105};
  std::vector < int >colors2 = {15000, 15000, 15000};
  std::vector < int >colors3 = {25000, 25000, 25000};
  std::vector < int >path2 = {5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29};
  std::vector < int >path =   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59};
  int cyclelen = 1;
  int direction = 1;
  std::vector < int >height = {100, 100, 100, 100, 100, 100};
  Worm w1(colors, path, cyclelen, direction, height);
  Worm w2(colors2, path, 5, -1, colors2);
  Worm w3(colors3, path, 4, 1, colors3);
  while (time_elapsed < duration) {
    w1.move();
    w2.move();
    w3.move();
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
    time_elapsed = millis() - time_start;
  }
  }
*/


/**
    fire idea from
    Arduino Uno - NeoPixel Fire v. 1.0
    Copyright (C) 2015 Robert Ulbricht
    https://www.arduinoslovakia.eu
*/

uint32_t Subtract(uint32_t color1, uint32_t color2)
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
/*
  //https://en.wikipedia.org/wiki/Elementary_cellular_automaton#
  void cellularAutomata(int wait, uint8_t rule, long pixelHue, time_t t, uint16_t duration) {
  time_elapsed = 0;
  std::vector < int >next(NUM_LEDS);
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
    delay(wait); // wait
    time_elapsed = millis() - time_start;
  }
  }


  void cellularAutomata(int wait, uint8_t ruleR, uint8_t ruleG, uint8_t ruleB, long pixelHue, time_t t, uint16_t duration) {
  time_elapsed = 0;
  std::vector < int >nextR(NUM_LEDS);
  std::vector < int >nextG(NUM_LEDS);
  std::vector < int >nextB(NUM_LEDS);
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
    delay(wait); // wait
    time_elapsed = millis() - time_start;
  }
  }
*/

/*
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
    delay(wait);                           //  Pause for a moment
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
      delay(wait);  // Pause for a moment
    }
  }
  }

  // Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
  void rainbow(int wait, int ncolorloop, time_t t) {
  // Hue of first pixel runs ncolorloop complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to ncolorloop*65536. Adding 256 to firstPixelHue each time
  // means we'll make ncolorloop*65536/256   passes through this outer loop:
  for (long firstPixelHue = 0; firstPixelHue < ncolorloop * 65536; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (ClockCorrect(i) * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(ClockCorrect(i), strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    SetBrightness(t); // Set the clock brightness dependant on the time
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
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
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
  }
*/
