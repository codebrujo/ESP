#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp8266.h>
#include "DHT.h"
#include "settings.h"
//---------IO init---------
#define RELAYPIN D5                               // Pump relay
#define RELAY_ENABLED_VALUE true                  // Value when relay is enabled
#define HUMIDITY_PIN A0
//---------DHT init---------
#define DHTPIN D2                                 // DHT digital pin 1
#define DHTTYPE DHT22                             // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);
//---------BLYNK init---------
#define BLYNK_PRINT Serial                        // Comment this out to disable prints and save space
#define BLYNK_IRR_BUTTON V11                      // Blynk virtual PIN to start irrigation
BlynkTimer report_timer;
BlynkTimer timer_rtc_update;
BlynkTimer timer_clock;
#define BLYNK_HB_VPIN V8                          // heart beat
#define BLYNK_SENSORS_VPIN V9                     // sensors
#define BLYNK_IRR_STATUS_VPIN V12                    // irrigation status
#define BLYNK_LAST_IRR_VPIN V13                   // last irrigated
//---------NTP init---------
unsigned int localPort = 2390;                    // local port to listen for UDP packets
IPAddress timeServerIP;                           // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;                   // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];              //buffer to hold incoming and outgoing packets
const unsigned long seventyYears = 2208988800UL;
WiFiUDP udp;                                      // A UDP instance to let us send and receive packets over UDP
const int GMT = 3;                                // GMT time conversion
#define TIME_UPDATE_INTERVAL 3600000L             // Time update period
//---------Business logic init---------
struct deviceData {
  float sensor_temp;
  int sensor_hum;
  int sensor_ground_hum;
  byte irrigation_allowed;
  byte report_mode;
  unsigned int irrigation_time;
  unsigned long last_irrigation;
} current_status;

struct timeData {
  unsigned long epoch;                            // Recieved time
  unsigned long localmillis;                      // Local time when epoch recieved
} time_data;

void init_structures() {
  current_status.sensor_temp = 0;
  current_status.sensor_hum = 0;
  current_status.sensor_ground_hum = 100;
  current_status.irrigation_allowed = true;
  current_status.irrigation_time = 6;
  current_status.last_irrigation = 0;
  current_status.report_mode = 0;
  time_data.epoch = seventyYears;
  time_data.localmillis = 0;
}

byte revert_value(byte value) {
  if (value == 0) {
    return 1;
  } else {
    return 0;
  }
}

void setup()
{
  Serial.begin(115200);
  init_structures();
  //params auth, ssid, pass to be defined in settings.h
  Blynk.begin(auth, ssid, pass);
  dht.begin();
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  Serial.println("...");
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  report_timer.setInterval(5000L, reportData);
  updateTime();
  timer_rtc_update.setInterval(TIME_UPDATE_INTERVAL, updateTime);
  timer_clock.setInterval(1000L, ticker);
  readSensors();
}

void read_console() {
  if (Serial.available() > 0) {
    String str = Serial.readString();
    Serial.print(str);
    if (str.equals("RESET\n")) {
      Serial.println("Reset..");
      ESP.restart();
    } else if (str.equals("PIN_ON\n")) {
      digitalWrite(RELAYPIN, true);
    } else if (str.equals("PIN_OFF\n")) {
      digitalWrite(RELAYPIN, false);
    } else if (str.equals("CLEAR_IRR_TIME\n")) {
      current_status.last_irrigation = 0;
    } else if (str.equals("READ_SENSOR\n")) {
      readSensors();
    } else if (str.equals("RUN_REPORT\n")) {
      reportData();
    } else if (str.equals("START_IRRIGATION\n")) {
      current_status.irrigation_time = 0;
    }
  }
}

void loop()
{
  Blynk.run();
  report_timer.run();
  timer_rtc_update.run();
  timer_clock.run();
  read_console();
}

void performBusinessLogic() {
  if (current_status.irrigation_time <= 5) {
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    readSensors();
  } else {
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  }
}

BLYNK_WRITE(BLYNK_IRR_BUTTON)
{
  int param_value = param.asInt();
  Serial.print("BLYNK VPIN change: ");
  Serial.println(param_value);
  current_status.irrigation_time = 0;
  performBusinessLogic();
}

char * createBlynkString() {
  int i, ccode;
  int shift;
  char Str_sensor_1[10];
  char Str_sensor_2[10];
  char StrResult[16] = "T:     H:    % ";
  char *tmp;
  for (int i = 0; i < sizeof(Str_sensor_1) / sizeof(char); i++) {
    Str_sensor_1[i] = ' ';
    Str_sensor_2[i] = ' ';
  }

  if (current_status.report_mode = 0) {
    current_status.report_mode = 1;
    shift = 3;
    dtostrf(current_status.sensor_temp, 4, 1, Str_sensor_1);
    for (int i = 0; i < sizeof(Str_sensor_1) / sizeof(char); i++) {
      ccode = (int)Str_sensor_1[i];
      if (ccode == 0 || ccode == 32) {
        continue;
      }
      StrResult[i + shift] = (char)ccode;
    }
    shift = 12;
    dtostrf(current_status.sensor_hum, 3, 0, Str_sensor_2);
    for (int i = 0; i < sizeof(Str_sensor_2) / sizeof(char); i++) {
      ccode = (int)Str_sensor_2[i];
      if (ccode == 0 || ccode == 32) {
        continue;
      }
      StrResult[i + shift] = (char)ccode;
    }

  } else {
    current_status.report_mode = 0;
    for (int i = 0; i < sizeof(StrResult) / sizeof(char); i++) {
      StrResult[i] = ' ';
    }
    StrResult[0] = 'G';
    StrResult[1] = 'R';
    StrResult[2] = 'O';
    StrResult[3] = 'U';
    StrResult[4] = 'N';
    StrResult[5] = 'D';
    StrResult[13] = '%';
    shift = 7;
    dtostrf(current_status.sensor_ground_hum, 3, 0, Str_sensor_1);
    for (int i = 0; i < sizeof(Str_sensor_1) / sizeof(char); i++) {
      ccode = (int)Str_sensor_1[i];
      if (ccode == 0 || ccode == 32) {
        continue;
      }
      StrResult[i + shift] = (char)ccode;
    }
  }
  if (!(tmp = (char*)malloc(sizeof(StrResult)))) {
    Serial.println("Error: can't allocate memory");
    return StrResult;
  }
  strcpy(tmp, StrResult);
  return (tmp);
}

void blynkSend() {
  Blynk.virtualWrite(BLYNK_HB_VPIN, gettime(3, (millis() - time_data.localmillis) / 1000 + time_data.epoch));
  Blynk.virtualWrite(BLYNK_SENSORS_VPIN, createBlynkString());
  int lvalue = digitalRead(RELAYPIN);
  if (lvalue = RELAY_ENABLED_VALUE) {
    Blynk.virtualWrite(BLYNK_IRR_STATUS_VPIN, 255);
  } else {
    Blynk.virtualWrite(BLYNK_IRR_STATUS_VPIN, 0);
  }
  Blynk.virtualWrite(BLYNK_LAST_IRR_VPIN, gettime(1, current_status.last_irrigation));
}

void reportData() {
  readSensors();
  Serial.print(gettime(1, (millis() - time_data.localmillis) / 1000 + time_data.epoch));
  Serial.print(": sensor_ground_hum = ");
  Serial.print(current_status.sensor_ground_hum);
  Serial.print(": sensor_temp = ");
  Serial.print(current_status.sensor_temp);
  Serial.print(": sensor_hum = ");
  Serial.println(current_status.sensor_hum);
  blynkSend();
}

void readSensors() {
  current_status.sensor_ground_hum = analogRead(HUMIDITY_PIN);
  float sensor_temp = dht.readTemperature();
  if (sensor_temp != sensor_temp) {
    int lvalue = digitalRead(RELAYPIN);
    if (lvalue = revert_value(RELAY_ENABLED_VALUE)) {
      Serial.println("DHT22 lib returned NaN");
      dht.begin();
    }
  } else {
    current_status.sensor_temp = sensor_temp;
    current_status.sensor_hum = dht.readHumidity();
  }

}

void ticker() {
  current_status.irrigation_time++;
  performBusinessLogic();
}

//real time functions
char * gettime(int time_mode, unsigned long epoch)
{
  struct tm *u;
  char *f;
  const time_t timer = epoch;
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
    time_data.epoch = secsSince1900 - seventyYears + GMT * 3600;
    time_data.localmillis = _localmillis;
    // print Unix time:
    Serial.println(time_data.epoch);
  }
}
