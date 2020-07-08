#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp8266.h>
#include "DHT.h"
#include <string.h>
#include <time.h>
#include "settings.h"
//---------DHT init---------
#define DHTPIN D2                       // DHT digital pin 1
#define DHTTYPE DHT22                   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);
//---------RELAY init---------
#define RELAYPIN1 D5                    // Sensor relay
#define RELAYPIN2 D6                    // Pump relay
#define IRRIGATE_START false            // Start irrigation trigger
#define IRRIGATE_STOP true              // Stop irrigation trigger
#define SENSOR_START true               // Start irrigation trigger
#define SENSOR_STOP false               // Stop irrigation trigger
#define SENSOR_READ_INTERVAL 3600000L   // Reading sensor frequency (millis)

//---------INPUT init---------
#define HUMIDITY_IN D3
//---------BLYNK init---------
#define BLYNK_PRINT Serial              // Comment this out to disable prints and save space
BlynkTimer timer;
BlynkTimer timer_rtc;
BlynkTimer timer_clock;
BlynkTimer timer_read_sensor;
//---------NTP init---------
unsigned int localPort = 2390;          // local port to listen for UDP packets
IPAddress timeServerIP;                 // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;         // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];    //buffer to hold incoming and outgoing packets
const unsigned long seventyYears = 2208988800UL;
WiFiUDP udp;                            // A UDP instance to let us send and receive packets over UDP
unsigned long epoch = 0;
#define TIME_UPDATE_INTERVAL 600000L    // NTP request frequency (millis)
//---------Business logic init---------
struct deviceData {
  float sensor1_temp;
  int sensor1_hum;
  byte relay1_status;
  byte relay2_status;
  byte irrigation_needed;
  byte is_sensor_wet;
  byte is_irrigating;
  unsigned long irrigation_time;
  unsigned long last_irrigation;
} cur_status;
const int irrigationDuration = 30, irrigationInterval = 86400;

void init_current_status(){
  cur_status.sensor1_temp = 0;
  cur_status.sensor1_hum = 0;
  cur_status.irrigation_time = epoch;
  cur_status.last_irrigation = 0;
}

void setup()
{
  Serial.begin(115200);
  //params auth, ssid, pass to be defined in settings.h
  Blynk.begin(auth, ssid, pass);
  dht.begin();
  timer.setInterval(5000L, Send);
  pinMode(RELAYPIN1, OUTPUT);
  digitalWrite(RELAYPIN1, SENSOR_STOP);
  pinMode(HUMIDITY_IN, INPUT);
  Serial.println("...");
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  updateTime();
  init_current_status();
  readSensor();
  timer_rtc.setInterval(TIME_UPDATE_INTERVAL, updateTime);
  timer_clock.setInterval(1000L, updateClock);
  timer_read_sensor.setInterval(SENSOR_READ_INTERVAL, readSensor);
}

void read_console() {
  if (Serial.available() > 0) {
    String str = Serial.readString();
    Serial.print(str);
    if (str.equals("RESET\n")) {
      Serial.println("Reset..");
      ESP.restart();
    } else if (str.equals("PIN1_ON\n")) {
      digitalWrite(RELAYPIN1, true);
    } else if (str.equals("PIN1_OFF\n")) {
      digitalWrite(RELAYPIN1, false);
    } else if (str.equals("PIN2_ON\n")) {
      digitalWrite(RELAYPIN2, true);
    } else if (str.equals("PIN2_OFF\n")) {
      digitalWrite(RELAYPIN2, false);
    } else if (str.equals("CLEAR_IRR_TIME\n")) {
      cur_status.last_irrigation = 0;
    } else if (str.equals("READ_SENSOR\n")) {
      readSensor();
    } else if (str.equals("REPORT_IRRIGATION_TIME\n")) {
      Serial.println(cur_status.last_irrigation);
    } else if (str.equals("START_IRRIGATION\n")) {
      cur_status.irrigation_needed = 1;
      cur_status.last_irrigation = 0;
    }
  }
}

void loop()
{
  timer_rtc.run();
  timer_clock.run();
  Blynk.run();
  timer.run();
  read_console();
  cur_status.relay1_status = digitalRead(RELAYPIN1);
  cur_status.relay2_status = digitalRead(RELAYPIN2);
  timer_read_sensor.run();
}

void irrigate() {
  if (!cur_status.irrigation_needed) {
    if(digitalRead(RELAYPIN2) != IRRIGATE_STOP){
      Serial.println("Change pin to stop irrigating");
      digitalWrite(RELAYPIN2, IRRIGATE_STOP);
      pinMode(RELAYPIN2, INPUT);  
    }
    
    if (cur_status.is_irrigating) {
      cur_status.last_irrigation = epoch;
      cur_status.irrigation_time = 0;
      Serial.println("Irrigating complete");
      cur_status.is_irrigating = 0;
    }
    
    return;
  } else {
    if (!cur_status.is_irrigating) {
      Serial.println("Irrigating..");
      cur_status.is_irrigating = 1;
      cur_status.irrigation_time = epoch;
    }
  }
  pinMode(RELAYPIN2, OUTPUT);
  digitalWrite(RELAYPIN2, IRRIGATE_START);
  delay(200);
  readSensor();
}

BLYNK_WRITE(V11) // чтение данных с пина 11
{
  int param_value = param.asInt();
  Serial.print("V11 change: ");
  Serial.println(param_value);
  cur_status.last_irrigation = 0;
  readSensor();
}

void Send() {
  float sensor_temp = dht.readTemperature();
  if(sensor_temp != sensor_temp){
    Serial.println("DHT22 lib returned NaN");
  }else{
    cur_status.sensor1_hum = dht.readHumidity();
    cur_status.sensor1_temp = sensor_temp;
  }
  Serial.print("T1: ");
  Serial.println(cur_status.sensor1_temp);
  Serial.print("RELAYPIN1: ");
  Serial.print(cur_status.relay1_status);
  Serial.print("; RELAYPIN2: ");
  Serial.print(cur_status.relay2_status);
  Serial.print("; SENSOR: ");
  Serial.print(cur_status.is_sensor_wet);
  Serial.print("; IRRIGATION NEEDED: ");
  Serial.println(cur_status.irrigation_needed);
  if (cur_status.is_irrigating) {
    Serial.print("Irrigating duration is ");
    Serial.print(epoch - cur_status.irrigation_time);
    Serial.println(" seconds");
  }
  Blynk.virtualWrite(V8, cur_status.sensor1_hum);
  Blynk.virtualWrite(V9, cur_status.sensor1_temp);
  if(cur_status.is_sensor_wet){
    Blynk.virtualWrite(V12, 0);
  }else{
    Blynk.virtualWrite(V12, 255);
  }
  if(cur_status.last_irrigation>0){
    struct tm *u;
    char *f;
    const time_t timer = cur_status.last_irrigation;
    u = localtime(&timer);
    Blynk.virtualWrite(V13, settime(u));
  }else{
    Blynk.virtualWrite(V13, "never");
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

void readSensor() {
  digitalWrite(RELAYPIN1, SENSOR_START);
  delay(100);
  int i;
  int sum = 0;
  for (i = 1; i <= 5; i++)
  {
    sum = sum + digitalRead(HUMIDITY_IN);
    delay(50);
  }

  if (sum == 5) {
     cur_status.is_sensor_wet = 0;
  } else {
     cur_status.is_sensor_wet = 1;
  }
  if(!cur_status.is_sensor_wet){
    cur_status.irrigation_needed = 1;
    if ((epoch - cur_status.irrigation_time > irrigationDuration && cur_status.irrigation_time > 0) || epoch - cur_status.last_irrigation < irrigationInterval) {
      Serial.println("Irrigation cancelled by frequency restriction.");
      cur_status.irrigation_needed = 0;
    }    
  }else{
    cur_status.irrigation_needed = 0;
  }
  if (!cur_status.irrigation_needed) {
    digitalWrite(RELAYPIN1, SENSOR_STOP);
  }
}

void updateClock() {
  epoch++;
  irrigate();
}
