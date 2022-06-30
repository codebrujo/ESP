//---------External libs---------
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp8266.h>
#include "settings.h"
//---------IO init---------
#define RELAYPIN D5                               // Relay out
#define RELAY_ENABLED_VALUE false                 // Value when relay is enabled
//---------BLYNK init---------
BlynkTimer report_timer;
BlynkTimer timer_rtc_update;
BlynkTimer timer_clock;
#define BLYNKVPIN V21                             // BLYNK managed pin
//---------NTP init---------
unsigned int localPort = 2390;                    // local port to listen for UDP packets
IPAddress timeServerIP;                           // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;                   // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];              //buffer to hold incoming and outgoing packets
const unsigned long seventyYears = 2208988800UL;
WiFiUDP udp;                                      // A UDP instance to let us send and receive packets over UDP
unsigned long epoch = 0;

//---------Business logic init---------
struct deviceData {
  unsigned long epoch;
  byte enabled;
} current_status;


void init_current_status(){
  current_status.epoch = 0;
  current_status.enabled = 0;
}

void reportData(){
  Serial.print(gettime());
  Serial.print(": enabled = ");
  Serial.println(current_status.enabled);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAYPIN, OUTPUT);                                               // Set pin to manage power relay
  digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  init_current_status();
  Blynk.begin(auth, ssid, pass);
  Serial.println("...");
  Serial.println("Starting UDP");
  udp.begin(localPort);
  report_timer.setInterval(5000L, reportData);
  updateTime();
  timer_rtc_update.setInterval(3600000L, updateTime);
  timer_clock.setInterval(1000L, updateClock);
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(BLYNKVPIN)
{
  current_status.enabled = param.asInt();
  performBusinessLogic();
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
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
  }else{
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  }
}

void loop() {
  Blynk.run();
  timer_clock.run();
  report_timer.run();
  timer_rtc_update.run();
}

//real time functions
char * gettime()
{
  struct tm *u;
  char *f;
  const time_t timer = current_status.epoch;
  u = localtime(&timer);
  char s[40];
  char *tmp;
  for (int i = 0; i < 40; i++) s[i] = 0;
  int length = strftime(s, 40, "%d.%m.%Y %H:%M:%S, %A", u);
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
    current_status.epoch = secsSince1900 - seventyYears + 10800;
    // print Unix time:
    Serial.println(current_status.epoch);
 }
}

void updateClock() {
  current_status.epoch++;
}
