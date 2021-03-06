//---------External libs---------
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHTesp.h"
#include "settings.h"
//---------IO init---------
#define BUTTONPIN 5                               // Button pin
#define EXTLIGHTMPIN 33                           // External light management pin
#define RELAY1PIN 12                              // Power
#define RELAY2PIN 13                              // Light
#define RELAY3PIN 14                              // Heater
#define LEDREDPIN 25                              // Red LED out
#define LEDGREENPIN 26                            // Green LED out
#define LEDBLUEPIN 27                             // Blue LED out
//---------BLYNK init---------
BlynkTimer report_timer;
BlynkTimer timer_rtc_update;
BlynkTimer timer_clock;
BlynkTimer timer_btn;
#define BLYNKVPIN V12                             // BLYNK power management pin
#define BLYNKLPIN V13                             // BLYNK light management pin
#define BLYNKHPIN V14                             // BLYNK heater management pin
#define BLYNKHTEMPPIN V15                         // BLYNK heating temperature pin
#define BLYNKHBVPIN V16                           // BLYNK heartbeat virtual pin number
#define BLYNKTEMPVPIN V17                         // BLYNK temperature indicator
//---------NTP init---------
unsigned int localPort = 2390;                    // local port to listen for UDP packets
IPAddress timeServerIP;                           // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;                   // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];              //buffer to hold incoming and outgoing packets
const unsigned long seventyYears = 2208988800UL;
WiFiUDP udp;                                      // A UDP instance to let us send and receive packets over UDP
const int GMT = 3;                                //GMT time conversion
//---------DHT init---------
#define DHTPIN 4                                  // DHT digital pin definition
#define DHTTYPE DHT22                             // DHT 22  (AM2302), AM2321
DHTesp dht;
//---------Business logic init---------
struct deviceData {
  unsigned long epoch;
  byte enabled;
  byte last_button_state;
  float sensor_temp;                              // Temperature sensor data
  int sensor_hum;                                 // Humidity sensor data
} current_status;

struct timeData {
  unsigned long epoch;                            // Recieved time
  unsigned long localmillis;                      // Local time when epoch recieved
} time_data;


void init_structures(){
  current_status.epoch = 0;
  current_status.enabled = 0;
  current_status.last_button_state = 1;                                    // Button is not pushed
  time_data.epoch = seventyYears;
  time_data.localmillis = 0;
}

char * createBlynkString(float sensor_1, float sensor_2) {
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


void blynkSend() {
  Blynk.virtualWrite(BLYNKHBVPIN, gettime(1));
  Blynk.virtualWrite(BLYNKTEMPVPIN, createBlynkString(current_status.sensor_temp, current_status.sensor_hum));
}

void reportData(){
  readDHT();
  Serial.print(gettime(1));
  Serial.print(": enabled = ");
  Serial.print(current_status.enabled);
  Serial.print(": sensor_temp = ");
  Serial.print(current_status.sensor_temp);
  Serial.print(": sensor_hum = ");
  Serial.println(current_status.sensor_hum);
  blynkSend();
}

void readDHT() {
  float sensor_temp = dht.getTemperature();
  if (sensor_temp != sensor_temp) {
    Serial.println("DHT22 lib returned NaN");
    dht.setup(DHTPIN, DHTesp::DHTTYPE);
  } else {
    current_status.sensor_temp = sensor_temp;
    current_status.sensor_hum = dht.getHumidity();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTONPIN, INPUT_PULLUP);                                        // Set Button pin
  pinMode(EXTLIGHTMPIN, INPUT_PULLUP);                                     // Set External light management pin
  pinMode(RELAY1PIN, OUTPUT);                                              // Set Power pin
  pinMode(RELAY2PIN, OUTPUT);                                              // Set Light pin
  pinMode(RELAY3PIN, OUTPUT);                                              // Set Heater pin
  pinMode(LEDREDPIN, OUTPUT);                                              // Set Red LED out pin
  pinMode(LEDGREENPIN, OUTPUT);                                            // Set Green LED out pin
  pinMode(LEDBLUEPIN, OUTPUT);                                             // Set Blue LED out pin

  init_structures();
  Blynk.begin(auth, ssid, pass);
  dht.setup(DHTPIN, DHTesp::DHTTYPE);
  Serial.println("...");
  Serial.println("Starting UDP");
  udp.begin(localPort);
  report_timer.setInterval(5000L, reportData);
  updateTime();
  timer_rtc_update.setInterval(30000L, updateTime);
  timer_btn.setInterval(100L, checkButton);
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(BLYNKVPIN)
{
  current_status.enabled = param.asInt();
  performBusinessLogic();
}

void invertButtonState(){
  if(current_status.enabled == 0){
    current_status.enabled = 1;
  }else{
    current_status.enabled = 0;
  }
}

byte revert_value(byte value) {
  if (value == 0) {
    return 1;
  } else {
    return 0;
  }
}

void performBusinessLogic(){
  if(current_status.enabled){
    //digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    digitalWrite(LEDREDPIN, true);
  }else{
    //digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
    digitalWrite(LEDREDPIN, false);
  }
  Blynk.virtualWrite(BLYNKVPIN, current_status.enabled);
}

void checkButton(){
  byte current_button_state = digitalRead(BUTTONPIN);
  if(current_status.last_button_state != current_button_state && current_status.last_button_state == 0){
    Serial.print("current_button_state = ");
    Serial.println(current_button_state);
    invertButtonState();
    performBusinessLogic();
  }
  current_status.last_button_state = current_button_state;
}

void loop() {
  Blynk.run();
  timer_btn.run();
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
