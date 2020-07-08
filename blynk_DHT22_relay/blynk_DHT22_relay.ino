#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <string.h>
#include <time.h>
#include "settings.h"
//---------DHT init---------
#define DHTPIN D2 // DHT digital pin 1
#define DHTPIN2 D3 // DHT digital pin 2
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
//---------RELAY init---------
#define RELAYPIN1 D5 // Relay digital out 1
#define RELAYPIN2 D6 // Relay digital out 2
//---------BLYNK init---------
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
BlynkTimer timer;
BlynkTimer timer_rtc;
BlynkTimer timer_clock;
//---------NTP init---------
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
const unsigned long seventyYears = 2208988800UL;
WiFiUDP udp; // A UDP instance to let us send and receive packets over UDP
unsigned long epoch = 0;
//---------Business logic init---------
int heat_mode = 0;
int heat_temp = 0;


void setup()
{
  Serial.begin(115200);
  //params auth, ssid, pass to be defined in settings.h
  Blynk.begin(auth, ssid, pass);
  dht.begin();
  dht2.begin();
  timer.setInterval(5000L, Send);
  pinMode(RELAYPIN1, OUTPUT);
  digitalWrite(RELAYPIN1,true);
  pinMode(RELAYPIN2, OUTPUT);
  digitalWrite(RELAYPIN2,true);
  Serial.println("...");
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  updateTime();
  timer_rtc.setInterval(600000L, updateTime);
  timer_clock.setInterval(1000L, updateClock);
}

void loop()
{
  Blynk.run();
  timer.run();
  timer_rtc.run();
  timer_clock.run();
}

BLYNK_WRITE(V0) // чтение режима нагрева: 0 - выкл, 1 - климат, 2 - вкл
{
  heat_mode = param.asInt();
  Serial.print("Heating mode: ");
  Serial.print(heat_mode);

  switch (heat_mode)
  {
    case 0:
      digitalWrite(RELAYPIN1, true);
      Serial.println(" (off)");
      break;
    case 1:
      Serial.print(" (climate control)");
      Serial.print(" controlled temperature ");
      Serial.println(heat_temp);
      break;
    case 2:
      digitalWrite(RELAYPIN1, false);
      Serial.println(" (permanently on)");
      break;
    default:
      Serial.println(" (not defined)");
  }
}

BLYNK_CONNECTED() {
    Blynk.syncAll();
}

BLYNK_WRITE(V1) // чтение данных с пина 1
{
  heat_temp = param.asInt();
  Serial.print("Controlled temperature changed. New value is ");
  Serial.println(heat_temp);
}


void run_climate(float t)
{
  int isHeating = (digitalRead(RELAYPIN1)==0) ? 1 : 0;
  if(isHeating==1 & t>heat_temp+1){
    digitalWrite(RELAYPIN1,true);
  }else if(isHeating==0 & t<heat_temp){
    digitalWrite(RELAYPIN1,false);
  }
}


void Send(){
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  delay(200);
  Serial.print("T1: ");
  Serial.println(t);
  float h1 = dht2.readHumidity();
  float t1 = dht2.readTemperature();
  Serial.print("T2: ");
  Serial.println(t1);

  //if (stat == 1)
  //{
  Blynk.virtualWrite(V2, h);
  Blynk.virtualWrite(V3, t);
  Blynk.virtualWrite(V4, h1);
  Blynk.virtualWrite(V5, t1);
  struct tm *u;
  char *f;
  const time_t timer = epoch;
  u = localtime(&timer);
  Blynk.virtualWrite(V10, settime(u));
  if(heat_mode == 1){
    run_climate(t);
  }
}

char * settime(struct tm *u)
{
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

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
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
    epoch = secsSince1900 - seventyYears + 10800;
    // print Unix time:
    Serial.println(epoch);
 }
}
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address) {
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
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void updateClock() {
  epoch++;
}
