//---------External libs---------
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp8266.h>
#include <Ticker.h>  //Ticker Library
#include "DHT.h"
#include "settings.h"
//---------IO init---------
#define BUTTONPIN D4                              // Button pin
#define RELAYPIN D5                               // Relay out
#define RELAY_ENABLED_VALUE false                 // Value when relay is enabled
#define LEDPIN D6                                 // LED out (heating)
#define LEDPINREADY D8                            // LED out (waiting for threshold)
//---------BLYNK init---------
BlynkTimer report_timer;
BlynkTimer timer_rtc_update;
BlynkTimer timer_clock;
BlynkTimer timer_wifi;
#define BLYNKVPIN V25                             // BLYNK managed pin
#define BLYNKSTATUSVPIN V26                       // BLYNK temperature indicator
#define BLYNKTEMPVPIN V27                         // BLYNK temperature indicator
#define ATTEMPTS_TO_REBOOT 10                     // Temperature that turns relay off
#define BLYNKHOURPIN V28                          // Auto mode off hour pin
#define BLYNKTCONTROLVPIN V29                     // Temperature control pin
#define BLYNKSTARTHOURPIN V30                     // Auto mode on hour pin
//---------Independent timers-----
Ticker btn_ticker;
//---------NTP init---------
unsigned int localPort = 2390;                    // local port to listen for UDP packets
IPAddress timeServerIP;                           // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;                   // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];              //buffer to hold incoming and outgoing packets
const unsigned long seventyYears = 2208988800UL;
WiFiUDP udp;                                      // A UDP instance to let us send and receive packets over UDP
const int GMT = 3;                                // GMT time conversion
unsigned int start_heat_hour = 8;                 // Hour when auto mode is off
unsigned int stop_heat_hour = 23;                 // Hour when auto mode is off
//---------DHT init---------
#define DHTPIN D2                                 // DHT digital pin definition
#define DHTTYPE DHT22                             // DHT 22  (AM2302), AM2321
unsigned int heat_off_temp = 19;                  // Temperature that turns relay off
DHT dht(DHTPIN, DHTTYPE);
//---------Business logic init---------
struct deviceData {
  unsigned long epoch;
  byte enabled;
  byte controlState;
  byte last_button_state;
  byte red_led_state;
  byte green_led_state;
  float sensor_temp;                              // Temperature sensor data
  int sensor_hum;                                 // Humidity sensor data
  int connection_state;                           // Internet connection state: 0 - no connection, 1 - WiFi connected, 2 - internet connected
  int failed_attempts;                            // Failed connection attempts counter
  int report_tick;                                // Failed connection attempts counter
} current_status;

struct timeData {
  unsigned long epoch;                            // Recieved time
  unsigned long localmillis;                      // Local time when epoch recieved
  int current_hour;                               // Current hour
} time_data;


void init_structures(){
  current_status.epoch = 0;
  current_status.enabled = 0;
  current_status.controlState = 0;
  current_status.last_button_state = 1;           // Button is not pushed
  current_status.red_led_state = 0;
  current_status.green_led_state = 0;
  current_status.connection_state = 0;
  current_status.sensor_temp = 100;
  current_status.failed_attempts = 0;
  current_status.report_tick = 0;
  time_data.epoch = seventyYears;
  time_data.localmillis = 0;
  time_data.current_hour = 0;
}

char * createSensorString(float sensor_1, float sensor_2) {
  int i, ccode; 
  int shift1 = 3;
  int shift2 = 12;
  char Str[10];
  char StrResult[16] = "T:     H:   % ";
  char *tmp;
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    Str[i] = ' ';
  }
  for (int i = 0; i < sizeof(StrResult) / sizeof(char); i++) {
    ccode = (int)StrResult[i];
    switch (ccode)
    {
      case 84: //T
        shift1 = i+2;
        break;
      case 72: //H
        shift2 = i+2;
        break;
    }
  }
  dtostrf(sensor_1, 4, 1, Str);
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    ccode = (int)Str[i];
    if (ccode == 0 || ccode == 32) {
      continue;
    }
    StrResult[i + shift1] = (char)ccode;
  }
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    Str[i] = ' ';
  }
  dtostrf(sensor_2, 3, 0, Str);
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    ccode = (int)Str[i];
    if (ccode == 0 || ccode == 32) {
      continue;
    }
    StrResult[i + shift2] = (char)ccode;
  }
  if (!(tmp = (char*)malloc(sizeof(StrResult)))) {
    Serial.println("Error: can't allocate memory");
    return StrResult;
  }
  strcpy(tmp, StrResult);
  return (tmp);
}

void reportStatusToBlynk(){
  if (current_status.controlState == 2){          // heater on
    Blynk.virtualWrite(BLYNKSTATUSVPIN, "MODE: ON");
  }else if(current_status.controlState == 1){     // temp control
    if(current_status.enabled){
      Blynk.virtualWrite(BLYNKSTATUSVPIN, "MODE: AUTO ON");
    }else{
      if(getScheduleRestriction()){
        Blynk.virtualWrite(BLYNKSTATUSVPIN, "MODE: TIMER OFF");
      }else{
        Blynk.virtualWrite(BLYNKSTATUSVPIN, "MODE: AUTO OFF");
      }
    }
  }else if(current_status.controlState == 0){
    Blynk.virtualWrite(BLYNKSTATUSVPIN, "MODE: OFF");
  }else{
    Blynk.virtualWrite(BLYNKSTATUSVPIN, "MODE: N/A");
  }
}

void reportStateToBlynk() {
  if(current_status.report_tick == 0){
    reportStatusToBlynk();
  }else if(current_status.report_tick == 1){
    Blynk.virtualWrite(BLYNKSTATUSVPIN, gettime(1));
  }
  Blynk.virtualWrite(BLYNKTEMPVPIN, createSensorString(current_status.sensor_temp, current_status.sensor_hum));
  current_status.report_tick++;
  if(current_status.report_tick>1){
    current_status.report_tick = 0;
  }
}

void blynkSend() {
  if (current_status.connection_state < 2) {
    if (current_status.failed_attempts > ATTEMPTS_TO_REBOOT){
      ESP.restart();
    }
    current_status.failed_attempts++;
    return;
  }
  reportStateToBlynk();
}

void reportData(){
  readDHT();
  performBusinessLogic();
  Serial.print(gettime(1));
  Serial.print(": mode=");
  Serial.print(current_status.controlState);
  Serial.print(": on=");
  Serial.print(current_status.enabled);
  Serial.print(": temp = ");
  Serial.print(current_status.sensor_temp);
  Serial.print(": hum = ");
  Serial.println(current_status.sensor_hum);
  blynkSend();
}

void readDHT() {
  float sensor_temp = dht.readTemperature();
  if (sensor_temp != sensor_temp) {
    Serial.println("DHT22 lib returned NaN");
    dht.begin();
  } else {
    current_status.sensor_temp = sensor_temp;
    current_status.sensor_hum = dht.readHumidity();
  }
}

void connectWiFi(){
  if(current_status.connection_state > 0){
    return;
  }
  int attempts = 15;
  int attempt = 0;
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(attempt > attempts){
      break;
    }
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" success");
    current_status.connection_state = 1;
    return;
  } else {
    Serial.println(" failed");
  }
  attempt = 0;
  Serial.print("Connecting to ");
  Serial.print(ssid1);
  WiFi.begin(ssid1, pass1);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(attempt > attempts){
      break;
    }
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" success");
    current_status.connection_state = 1;
    return;
  } else {
    Serial.println(" failed");
  }
}

void connectBlynk(){
  if(current_status.connection_state < 2){
    Serial.println("connectBlynk - no connection");
    return;
  }
  Serial.print("Connecting to Blynk");
  Blynk.begin(auth, ssid, pass);
  Serial.println("...");
  timer_rtc_update.setInterval(3600000L, updateTime);
}

void reconnectServices(){
  if (current_status.connection_state == 2) {
    current_status.failed_attempts = 0;
    return;
  }
  connectWiFi();
  connectBlynk();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTONPIN, INPUT_PULLUP);                                        // Set pin to input for capturing GM Tube events
  pinMode(RELAYPIN, OUTPUT);                                               // Set pin to manage power relay
  digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  pinMode(LEDPIN, OUTPUT);                                                 // Set pin to manage LED indicator
  pinMode(LEDPINREADY, OUTPUT);                                            // Set pin to manage LED indicator
  init_structures();
  Serial.println("");
  Serial.println("Starting hardware...");
  btn_ticker.attach_ms(150L, checkButton);
  dht.begin();
  connectWiFi();
  Serial.println("Starting UDP");
  udp.begin(localPort);
  updateTime();
  connectBlynk();
  report_timer.setInterval(5000L, reportData);
  timer_wifi.setInterval(30000L, reconnectServices);
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(BLYNKTCONTROLVPIN)
{
  heat_off_temp = param.asInt();
  performBusinessLogic();
}

BLYNK_WRITE(BLYNKVPIN)
{
  current_status.controlState = param.asInt();
  performBusinessLogic();
}

BLYNK_WRITE(BLYNKHOURPIN)
{
  stop_heat_hour = param.asInt();
}

BLYNK_WRITE(BLYNKSTARTHOURPIN)
{
  start_heat_hour = param.asInt();
}

void changeControlState(){
  current_status.controlState = current_status.controlState + 1;
  if(current_status.controlState > 2){
    current_status.controlState = 0;
  }
  performBusinessLogic();
}

byte revert_value(byte value) {
  if (value == 0) {
    return 1;
  } else {
    return 0;
  }
}

byte getScheduleRestriction(){
  byte restrictedBySchedule = 0;
  if(time_data.current_hour < start_heat_hour || time_data.current_hour >= stop_heat_hour){
    restrictedBySchedule = 1;
  }
  return restrictedBySchedule;
}

void performBusinessLogic(){
  byte restrictedBySchedule = getScheduleRestriction();
  if (current_status.controlState == 2){          // heater on
    current_status.enabled = 1;
  }else if(current_status.controlState == 1){     // temp control
    if(current_status.sensor_temp > heat_off_temp + 1){
      current_status.enabled = 0;
    }else if (current_status.sensor_temp < heat_off_temp){
      current_status.enabled = 1;
    }
    if(restrictedBySchedule){
      current_status.enabled = 0;
    }
  }else{
    current_status.enabled = 0;
  }
  if(current_status.enabled){
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
  }else{
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  }
  if (current_status.controlState == 2){          // heater on
    digitalWrite(LEDPINREADY, false);
    digitalWrite(LEDPIN, true);
  }else if(current_status.controlState == 1){     // temp control
    if(current_status.enabled){
      if(current_status.green_led_state){
        digitalWrite(LEDPINREADY, false);
        digitalWrite(LEDPIN, true);
      }else{
        digitalWrite(LEDPINREADY, true);
        digitalWrite(LEDPIN, false);
      }
    }else{
      if(restrictedBySchedule){
        digitalWrite(LEDPINREADY, revert_value(current_status.green_led_state));
      }else{
        digitalWrite(LEDPINREADY, true);
      }
      digitalWrite(LEDPIN, false);
    }
  }else{
    digitalWrite(LEDPINREADY, false);
    digitalWrite(LEDPIN, false);
  }
  current_status.green_led_state = digitalRead(LEDPINREADY);
  current_status.red_led_state = digitalRead(LEDPIN);

  Blynk.virtualWrite(BLYNKVPIN, current_status.controlState);
}

void checkButton(){
  byte current_button_state = digitalRead(BUTTONPIN);
  if(current_status.last_button_state != current_button_state && current_status.last_button_state == 0){
    Serial.print("current_button_state = ");
    Serial.println(current_button_state);
    changeControlState();
  }
  current_status.last_button_state = current_button_state;
}

void loop() {
  Blynk.run();
  timer_wifi.run();
  report_timer.run();
  timer_rtc_update.run();
}

//real time functions
char * gettime(int time_mode)
{
  struct tm *u;
  char *f;
  const time_t timer = (millis()-time_data.localmillis)/1000+time_data.epoch;
  u = localtime(&timer);
  char s[40];
  char *tmp;
  for (int i = 0; i < 40; i++) s[i] = 0;
  
  char h[40];
  for (int i = 0; i < 40; i++) h[i] = 0;
  strftime(h, 40, "%H", u);
  time_data.current_hour = ((int)h[0]-48)*10+(int)h[1]-48;
  int length;
  switch (time_mode)
  {
    case 1:
      length = strftime(s, 40, "%H:%M:%S", u);
      break;
    case 2:
      length = strftime(s, 40, "%d.%m.%Y", u);
      break;
    case 3:
      length = strftime(s, 40, "%d.%m.%Y %H:%M:%S", u);
      break;
    default:
      length = strftime(s, 40, "%d.%m.%Y %H:%M:%S, %A", u);
  }  
  tmp = (char*)malloc(sizeof(s));
  strcpy(tmp, s);
  return (tmp);
}

void updateTime() {
  if(current_status.connection_state == 0){
    Serial.println("updateTime -- no connection");
    return;
  }
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  Serial.println("sending NTP packet...");
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
  udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  unsigned long _localmillis = millis();
  // wait to see if a reply is available
  delay(1000);

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  } else {
    current_status.connection_state = 2;
    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    // subtract seventy years:
    time_data.epoch = secsSince1900 - seventyYears + GMT*3600;
    time_data.localmillis = _localmillis;
    // print Unix time:
    Serial.println(time_data.epoch);
 }
}
