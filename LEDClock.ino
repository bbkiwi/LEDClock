/*********
 Using tttapa examples with clock code by Jon Fuge *mod by bbkiwi
//https://github.com/PaulStoffregen/Time
*********/

/*********** IMPORTANT #include <ESP8266mDNS.h> below
 * With 1.8.10 and board 2.6.3
 * Was having problem with mDns
 * Changed ~/Library/Arduino15/packages/esp8266/hardware/esp8266/2.6.3/libraries/ESP8266mDNS/src/ESP8266mDNS.h
 * uncomment line 51 and comment line 52 to use legacy version which from googling works well for simple use
 *
 * This is the reverse of comment at line 40 in example
 * ~/Library/Arduino15/packages/esp8266/hardware/esp8266/2.6.3/libraries/ESP8266mDNS/examples/LEAmDNS/mDNS_Clock/mDNS_Clock.ino
 */

/************* Declare included libraries ******************************/
#include <NTPClient.h>
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>
#include <string.h>
#include "pitches.h"

/************ NOTE __has_include not work for esp8266
#if __has_include ("localwificonfig.h")
#  include "localwificonfig.h"
#endif
#ifndef homeSSID
#  define homeSSID "homeSSID"
#  define homePW "homePW"
#endif
*************/
// So MUST have this file which defines homeSSID and homePW
#include "localwificonfig.h"

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>

#include <ESP8266mDNS.h>
#include <FS.h>
#include <WebSocketsServer.h>

ESP8266WiFiMulti wifiMulti;       // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);       // create a web server on port 80
WebSocketsServer webSocket(81);    // create a websocket server on port 81

File fsUploadFile;                                    // a File variable to temporarily store the received file

const char *ssid = "LED Clock Access Point"; // The name of the Wi-Fi network that will be created
const char *password = "ledclock";   // The password required to connect to it, leave blank for an open network

// OTA and mDns must have same name
const char *OTAandMdnsName = "LEDClock";           // A name and a password for the OTA and mDns service
const char *OTAPassword = "ledclock";

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
RGB Twelve = {0,0,60}; // blue
//The colour of the "quarters" 3, 6 & 9 to give visual reference
RGB Quarters = { 0,0,60 }; //blue
//The colour of the "divisions" 1,2,4,5,7,8,10 & 11 to give visual reference
RGB Divisions = { 0,0,30 }; //blue
//All the other pixels with no information
RGB Background = { 0,0,6 };//blue

//The Hour hand
RGB Hour = { 0, 100, 0 };//green
//The Minute hand
RGB Minute = { 64, 32, 0 };//orange medium
//The Second hand
RGB Second = { 0,0,100 };//blue

// Make clock go forwards or backwards (dependant on hardware)
bool ClockGoBackwards = true;

//Set brightness by time for night and day mode
TIME WeekNight = {20, 00}; // Night time to go dim
TIME WeekMorning = {6, 15}; //Morning time to go bright
TIME WeekendNight = {20, 30}; // Night time to go dim
TIME WeekendMorning = {9, 30}; //Morning time to go bright

byte day_brightness = 127;
byte night_brightness = 16;

//Set your timezone in hours difference rom GMT
int hours_Offset_From_GMT = 12;


String daysOfWeek[8] = {"dummy", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String monthNames[13] = {"dummy", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
bool ota_flag = true;
bool sound_alarm_flag = false;
bool light_alarm_flag = false;
tmElements_t alarmTime;
typedef enum {
  ONCE,
  DAILY,
  WEEKLY
} alarmType;

struct {
  alarmType at;
  uint16_t duration;
  uint32_t repeat;
  //tmElements_t alarmTime;
} alarmInfo;

uint16_t time_elapsed = 0;
int TopOfClock = 15; // to make given pixel the top

// notes in the melody for the sound alarm:
int melody[] = { // Shave and a hair cut two bits
  NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, 0, NOTE_B4, NOTE_C5
};
int whole_note_duration = 4000; // milliseconds
// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);
unsigned long int update_interval_secs = 3601;
NTPClient timeClient(ntpUDP, "nz.pool.ntp.org", hours_Offset_From_GMT * 3600, update_interval_secs * 1000);
// Which pin on the ESP8266 is connected to the NeoPixels?
#define PIN            3 // This is the D9 pin
#define PIEZO_PIN 5 // This is D1
#define analogInPin  A0  // ESP8266 Analog Pin ADC0 = A0
//************* Declare user functions ******************************
void Draw_Clock(time_t t, byte Phase);
int ClockCorrect(int Pixel);
void SetBrightness(time_t t);
bool SetClockFromNTP ();
bool IsDst();


//************* Declare NeoPixel ******************************
//Using 1M WS2812B 5050 RGB Non-Waterproof 60 LED Strip
// use NEO_KHZ800 but maybe 400 makes wifi more stable???
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);
bool ClockInitialized = false;


const int ESP_BUILTIN_LED = 2;

void setup() {
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startOTA();                  // Start the OTA service

  startSPIFFS();               // Start the SPIFFS and list all contents

  startWebSocket();            // Start a WebSocket server

  startMDNS();                 // Start the mDNS responder

  startServer();               // Start a HTTP server with a file read handler and an upload handler

  strip.begin(); // This initializes the NeoPixel library.

  Draw_Clock(0, 3); // Add the quater hour indicators

  ClockInitialized = SetClockFromNTP(); //// sync first time, updates system clock and adjust it for daylight savings

  pinMode(ESP_BUILTIN_LED, OUTPUT);
}

/*___________________LOOP__________________________________________________________*/
time_t prevDisplay = 0; // when the digital clock was displayed

void loop() {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events
  MDNS.update();                              // must have above as well

  if(light_alarm_flag)  showlights(10000, 50, 50, 50, 50, 50, 50, 10, 50, now());

  if(sound_alarm_flag)
  {
    uint16_t time_start = millis();
    Serial.println("1 second sound alarm, then clock will start");
    for (int thisNote = 0; thisNote < 8; thisNote++) {
      // to calculate the note duration, take whole note durations divided by the note type.
      //e.g. quarter note = whole_note_duration / 4, eighth note = whole_note_duration/8, etc.
      int noteDuration = whole_note_duration / noteDurations[thisNote];
      if (melody[thisNote] > 30) {
        tone(PIEZO_PIN, melody[thisNote], noteDuration);
      }
      // to distinguish the notes, set a minimum time between them.
      // the note's duration + 30% seems to work well:
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
      // stop the tone playing:
      noTone(PIEZO_PIN);
    }
    sound_alarm_flag = false;
  }

  // read the analog in value coming from microphone
  int sensorValue = analogRead(analogInPin);
  if (sensorValue > 900 ) {
    Serial.println(sensorValue);
    light_alarm_flag = true;
    time_elapsed = 0;
  }

  time_t nextalarmtime;
  time_t t = now(); // Get the current time
  if (now() != prevDisplay) { //update the display only if time has changed
    prevDisplay = now();
    if (second() == 0)
      digitalClockDisplay();
    else
      Serial.print('-');

    Draw_Clock(t, 4); // Draw the whole clock face with hours minutes and seconds
    ClockInitialized |= SetClockFromNTP(); // sync initially then every update_interval_secs seconds, updates system clock and adjust it for daylight savings
    if (prevDisplay == makeTime(alarmTime))
    {
      showlights(alarmInfo.duration, -1, -1, 10, -1, -1, -1, -1, -1, t);
      // For daily repeat
      nextalarmtime = makeTime(alarmTime);
      nextalarmtime += alarmInfo.repeat;
      breakTime(nextalarmtime, alarmTime);

      char buf[50];
      sprintf(buf, "Next Alarm %d:%02d:%02d %s %d %s %d", hour(makeTime(alarmTime)), minute(makeTime(alarmTime)),
        second(makeTime(alarmTime)), daysOfWeek[weekday(makeTime(alarmTime))].c_str(), day(makeTime(alarmTime)),
        monthNames[month(makeTime(alarmTime))].c_str(), year(makeTime(alarmTime)));
      Serial.println();
      Serial.println(buf);
    }
  }
  delay(10); // needed to keep wifi going
}


/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi() { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  wifiMulti.addAP(homeSSID, homePW);  // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED && WiFi.softAPgetStationNum() < 1) {  // Wait for the Wi-Fi to connect to station or a station connects to AP
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  if(WiFi.softAPgetStationNum() == 0) {      // If the ESP is connected to an AP
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
  ArduinoOTA.setPassword(OTAPassword);

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

  server.on("/restart",[](){
    server.send(200,"text/plain", "Restarting ...");
    delay(1000);
    ESP.restart();
  });

//  server.on("/startota",[](){
//    server.send(200,"text/plain", "Make OTA ready ...");
//    delay(1000);
//    ota_flag = true;
//    time_elapsed = 0;
//  });

  server.on("/lightalarm",[](){
    server.send(200,"text/plain", "Light alarm Starting ...");
    delay(1000);
    light_alarm_flag = true;
    time_elapsed = 0;
  });

  server.on("/soundalarm",[](){
    server.send(200,"text/plain", "Sound alarm Starting ...");
    delay(1000);
    sound_alarm_flag = true;
    time_elapsed = 0;
  });

  server.on("/whattime",[](){
    char buf[50];
    sprintf(buf, "%d:%02d:%02d %s %d %s %d", hour(), minute(), second(), daysOfWeek[weekday()].c_str(), day(), monthNames[month()].c_str(), year());
    server.send(200,"text/plain", buf);
    delay(1000);
  });

  server.on("/setbright", setBright);

  server.on("/settime", settime);

  server.on("/setalarm", setalarm);

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
                                              // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound(){ // if the requested file or page doesn't exist, return a 404 not found error
  if(!handleFileRead(server.uri())){          // check if the file exists in the flash memory (SPIFFS), if so, send it
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

void handleFileUpload(){ // upload a new file to the SPIFFS
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  String path;
  if(upload.status == UPLOAD_FILE_START){
    path = upload.filename;
    if(!path.startsWith("/")) path = "/"+path;
    if(!path.endsWith(".gz")) {                          // The file server always prefers a compressed version of a file
      String pathWithGz = path+".gz";                    // So if an uploaded file is not compressed, the existing compressed
      if(SPIFFS.exists(pathWithGz))                      // version of that file must be deleted (if it exists)
         SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
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


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
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
      Serial.printf("[%u] get Text: %s\n", num, payload);
      if (payload[0] == '#') {            // we get RGB data
        uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);   // decode rgb data
        int r = ((rgb >> 20) & 0x3FF);                     // 10 bits per color, so R: bits 20-29
        int g = ((rgb >> 10) & 0x3FF);                     // G: bits 10-19
        int b =          rgb & 0x3FF;                      // B: bits  0-9
        analogWrite(ESP_BUILTIN_LED, b);
        //Serial.printf("%d\n", b);
      } else if (payload[0] == 'R') {                      // the browser sends an R when the rainbow effect is enabled
        digitalWrite(ESP_BUILTIN_LED, 1);  // turn off the LED
      } else if (payload[0] == 'W') {                      // the browser sends an N when the rainbow effect is disabled
        char buf[50];
        sprintf(buf, "%d:%02d:%02d %s %d %s %d", hour(), minute(), second(), daysOfWeek[weekday()].c_str(), day(), monthNames[month()].c_str(), year());
        webSocket.sendTXT(num, buf);
        digitalWrite(ESP_BUILTIN_LED, 0);  // turn on the LED
      }
      break;
  }
}

void setalarm()
{
  String t = server.arg("duration"); // in ms
  String r = server.arg("repeat"); // in seconds
  String h = server.arg("hour");
  String m = server.arg("min");
  String s = server.arg("sec");
  String d = server.arg("day");
  String mth = server.arg("month");
  String y = server.arg("year");
  alarmInfo.duration = 10000;
  if (strlen(t.c_str()) > 0) alarmInfo.duration = t.toInt();
  Serial.println(alarmInfo.duration);

  alarmInfo.repeat = SECS_PER_DAY;
  if (strlen(r.c_str()) > 0) alarmInfo.repeat = r.toInt();
  Serial.println(alarmInfo.repeat);

  breakTime(now(), alarmTime);
  if (strlen(s.c_str()) > 0) alarmTime.Second = s.toInt();
  if (strlen(m.c_str()) > 0) alarmTime.Minute = m.toInt();
  if (strlen(h.c_str()) > 0) alarmTime.Hour = h.toInt();
  if (strlen(d.c_str()) > 0) alarmTime.Day = d.toInt();
  if (strlen(mth.c_str()) > 0) alarmTime.Hour = mth.toInt();

  char buf[50];
  sprintf(buf, "Alarm Set to %d:%02d:%02d %s %d %s %d", hour(makeTime(alarmTime)), minute(makeTime(alarmTime)), second(makeTime(alarmTime)), daysOfWeek[weekday(makeTime(alarmTime))].c_str(), day(makeTime(alarmTime)), monthNames[month(makeTime(alarmTime))].c_str(), year(makeTime(alarmTime)));
  server.send(200,"text/plain", buf);
  Serial.println(buf);
}


void settime()
{
  String h = server.arg("hour");
  String m = server.arg("min");
  String s = server.arg("sec");
  String d = server.arg("day");
  String mth = server.arg("month");
  String y = server.arg("year");
  setTime(h.toInt(), m.toInt(), s.toInt(), d.toInt(), mth.toInt(), y.toInt());
  ClockInitialized = false;
  char buf[50];
  sprintf(buf, "Clock Set to %d:%02d:%02d %s %d %s %d", h.toInt(), m.toInt(), s.toInt(), daysOfWeek[d.toInt()].c_str(), day(makeTime(alarmTime)), monthNames[mth.toInt()].c_str(),  y.toInt());
  server.send(200,"text/plain", buf);
  Serial.println(buf);
}


void setBright()
{
  String db = server.arg("day");
  String nb = server.arg("night");
  if (strlen(db.c_str()) > 0) day_brightness = db.toInt();
  if (strlen(nb.c_str()) > 0) night_brightness = nb.toInt();
  String rsp = "daybrightness set to: "+db+", nightbrightness to  "+nb;
  server.send(200,"text/plain",rsp);
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
bool IsDst()
{
  int previousSunday = day() - weekday();
  //Serial.print("    IsDst ");
  //Serial.print(month());
  //Serial.println(previousSunday);
  if (month() < 4 || month() > 9)  return true;
  if (month() > 4 && month() < 9)  return false;


  if (month() == 4) return previousSunday < 8;
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
  if (Phase <= 0) // Set all pixes black
    for (int i = 0; i < 60; i++)
      strip.setPixelColor(ClockCorrect(i), strip.Color(0, 0, 0));

  if (Phase >= 1) // Draw all pixels background color
    for (int i = 0; i < 60; i++)
      strip.setPixelColor(ClockCorrect(i), strip.Color(Background.r, Background.g, Background.b));

  if (Phase >= 2) // Draw 5 min divisions
    for (int i = 0; i < 60; i = i + 5)
      strip.setPixelColor(ClockCorrect(i), strip.Color(Divisions.r, Divisions.g, Divisions.b)); // for Phase = 2 or more, draw 5 minute divisions

  if (Phase >= 3) { // Draw 15 min markers
    for (int i = 0; i < 60; i = i + 15)
      strip.setPixelColor(ClockCorrect(i), strip.Color(Quarters.r, Quarters.g, Quarters.b));
      strip.setPixelColor(ClockCorrect(0), strip.Color(Twelve.r, Twelve.g, Twelve.b));
  }

  if (Phase >= 4) { // Draw hands
    int iminute = minute(t);
    int ihour = ((hour(t) % 12) * 5) + minute(t) / 12;
    strip.setPixelColor(ClockCorrect(ihour-1), strip.Color(Hour.r/4, Hour.g/4, Hour.b/4));
    strip.setPixelColor(ClockCorrect(ihour), strip.Color(Hour.r, Hour.g, Hour.b));
    strip.setPixelColor(ClockCorrect(ihour+1), strip.Color(Hour.r/4, Hour.g/4, Hour.b/4));
    strip.setPixelColor(ClockCorrect(second(t)), strip.Color(Second.r, Second.g, Second.b));
   if (second() % 2)
      strip.setPixelColor(ClockCorrect(iminute), strip.Color(Minute.r, Minute.g, Minute.b)); // to help identification, minute hand flshes between normal and half intensity
    else
      strip.setPixelColor(ClockCorrect(iminute), strip.Color(Minute.r / 2, Minute.g / 2, Minute.b / 2)); // lower intensity minute hand
  }

  SetBrightness(t); // Set the clock brightness dependant on the time
  strip.show(); // show all the pixels
}

//************* Function to set the clock brightness ******************************
void SetBrightness(time_t t)
{
  int NowHour = hour(t);
  int NowMinute = minute(t);

  if ((weekday() >= 2) && (weekday() <= 6))
    if ((NowHour > WeekNight.Hour) || ((NowHour == WeekNight.Hour) && (NowMinute >= WeekNight.Minute)) || ((NowHour == WeekMorning.Hour) && (NowMinute <= WeekMorning.Minute)) || (NowHour < WeekMorning.Hour))
      strip.setBrightness(night_brightness);
    else
      strip.setBrightness(day_brightness);
  else if ((NowHour > WeekendNight.Hour) || ((NowHour == WeekendNight.Hour) && (NowMinute >= WeekendNight.Minute)) || ((NowHour == WeekendMorning.Hour) && (NowMinute <= WeekendMorning.Minute)) || (NowHour < WeekendMorning.Hour))
    strip.setBrightness(night_brightness);
  else
    strip.setBrightness(day_brightness);
}

//************* This function reverses the pixel order ******************************
//              and ajusts top of clock
int ClockCorrect(int Pixel)
{
  Pixel = (Pixel + TopOfClock) % 60;
  if (ClockGoBackwards)
    return ((60 - Pixel +30) % 60); // my first attempt at clock driving had it going backwards :)
  else
    return (Pixel);
}


void showlights(uint16_t duration, int w1, int w2, int w3, int w4, int w5, int w6, int w7, int w8, time_t t)
  {
    time_elapsed = 0;
    uint16_t time_start = millis();
    Serial.println("10 second minimum light alarm, then clock will start");
    while(time_elapsed < duration)
    {
     // Fill along the length of the strip in various colors...
      if (w1 > 0) colorWipe(strip.Color(255,   0,   0), w1, t); // Red
      if (w2 > 0) colorWipe(strip.Color(  0, 255,   0), w2, t); // Green
      if (w3 > 0) colorWipe(strip.Color(  0,   0, 255), w3, t); // Blue

      // Do a theater marquee effect in various colors...
      if (w4 > 0) theaterChase(strip.Color(127, 127, 127), w4, t); // White, half brightness
      if (w5 > 0) theaterChase(strip.Color(127,   0,   0), w5, t); // Red, half brightness
      if (w6 > 0) theaterChase(strip.Color(  0,   0, 127), w6, t); // Blue, half brightness

      if (w7 > 0) rainbow(w7, 1, t);             // Flowing rainbow cycle along the whole strip
      if (w8 > 0) theaterChaseRainbow(w8, t); // Rainbow-enhanced theaterChase variant
      time_elapsed = millis() - time_start;
    }
    light_alarm_flag = false;
  }

//********* Adafruit NeoPIxel Routines
// Some functions of our own for creating animated effects -----------------

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait, time_t t) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
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
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
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
  for(long firstPixelHue = 0; firstPixelHue < ncolorloop*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
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
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
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
