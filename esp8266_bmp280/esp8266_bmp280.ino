#include "Wire.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BMP280.h"
#include <BlynkSimpleEsp8266.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "settings.h"

//---------BLYNK init---------
#define BLYNK_PRINT Serial                        // Comment this out to disable prints and save space
BlynkTimer timer_read;
BlynkTimer timer_report;
BlynkTimer timer_history;
BlynkTimer timer_rtc_update;
//---------BMP280 init---------
Adafruit_BMP280 bmp;
#define SEALEVELPRESSURE_HPA (1006.581)           // Local sea level pressure
#define HISTORY_VOLUME 16                         // Historical data storage volume
#define HISTORY_INTERVAL 60000L                   // Historical data interval in millis
//---------NTP init---------
unsigned int localPort = 2390;                    // local port to listen for UDP packets
IPAddress timeServerIP;                           // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;                   // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];              //buffer to hold incoming and outgoing packets
WiFiUDP udp;                                      // A UDP instance to let us send and receive packets over UDP
const unsigned long seventyYears = 2208988800UL;
const int GMT = 3;                                //GMT time conversion

struct deviceData {
  float sensor_temp;                          // Temperature data
  float sensor_altitude;                      // Altitude data
  float sensor_pressure;                      // Pressure data
  int online_status;                          // 0 - disconnected, 1 - WiFi connected, 2 - Internet connected
  int success_network;                        // index of successfully connected WiFi network
} current_status;

struct historicalData {
  int current_index;
  float sensor_temp[HISTORY_VOLUME];                      // Temperature data
  float sensor_pressure[HISTORY_VOLUME];                  // Pressure data
  unsigned long measurement_time[HISTORY_VOLUME];         // Time (millis)
} historical_data;

struct timeData {
  unsigned long epoch;                                    // Recieved time
  unsigned long localmillis;                              // Local time when epoch recieved
} time_data;


void init_structures() {
  current_status.sensor_temp = 0;
  current_status.sensor_altitude = 0;
  current_status.sensor_pressure = 0;
  current_status.online_status = 0;
  current_status.success_network = 0;
  historical_data.current_index = 0;
  time_data.epoch = seventyYears;
  time_data.localmillis = 0;
}

int reverseValue(int sourceValue) {
  if (sourceValue == 0) {
    return 1;
  } else {
    return 0;
  }
}

void readSensors() {
  float temp = bmp.readTemperature();
  current_status.sensor_temp = temp;
  current_status.sensor_pressure = bmp.readPressure() * 0.00750062;         // Convert mm/Hg to Pa
  current_status.sensor_altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);

  /*if (temp < 100) {
    bmp.begin(0x76);
    Serial.println("Sensor error");
  } else {
    current_status.sensor_temp = temp;
    current_status.sensor_pressure = bmp.readPressure() * 0.00750062;         // Convert mm/Hg to Pa
    current_status.sensor_altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
  }*/
}

void printCurrentToSerial() {
  Serial.print("T ");
  Serial.print(millis());
  Serial.print(", Tmp ");
  Serial.print(current_status.sensor_temp);
  Serial.print(", Pr ");
  Serial.println(current_status.sensor_pressure);
  //Serial.print("Blynk String: ");
  //Serial.println(createBlynkString(current_status.sensor_temp, current_status.sensor_pressure));
}

void printToSerial() {
  int i, j;
  char Str[25];
  Serial.println(" Time                     Temp                     Pressure");
  for (i = 0; i < HISTORY_VOLUME; i++) // цикл по строкам
  {
    int current_index = 0;
    if (historical_data.current_index == 0) {
      current_index = HISTORY_VOLUME;
    } else {
      current_index = historical_data.current_index - 1;
    }
    if (i == current_index) {
      Serial.print(">");
    } else {
      Serial.print(" ");
    }
    for (int j = 0; j < sizeof(Str) / sizeof(char); j++) {
      Str[j] = ' ';
    }
    itoa(historical_data.measurement_time[i], Str, 10);           //int to string
    Serial.write(Str, sizeof(Str) / sizeof(char));
    for (int j = 0; j < sizeof(Str) / sizeof(char); j++) {
      Str[j] = ' ';
    }
    dtostrf(historical_data.sensor_temp[i], 3, 2, Str);           //float to string
    Serial.write(Str, sizeof(Str) / sizeof(char));
    for (int j = 0; j < sizeof(Str) / sizeof(char); j++) {
      Str[j] = ' ';
    }
    dtostrf(historical_data.sensor_pressure[i], 3, 3, Str);       //float to string
    Serial.write(Str, sizeof(Str) / sizeof(char));
    Serial.println("");
  }
}

char * createBlynkString(float sensor_temp, float sensor_pressure) {
  int i, ccode;
  int shiftT = 3;
  int shiftP = 12;
  char Str[10];
  char StrResult[16] = "T:     P:      ";
  char *tmp;
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    Str[i] = ' ';
  }
  for (int i = 0; i < sizeof(StrResult) / sizeof(char); i++) {
    ccode = (int)StrResult[i];
    switch (ccode)
    {
      case 84: //T
        shiftT = i + 2;
        break;
      case 80: //P
        shiftP = i + 2;
        break;
    }
  }
  dtostrf(sensor_temp, 4, 1, Str);
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    ccode = (int)Str[i];
    if (ccode == 0 || ccode == 32) {
      continue;
    }
    StrResult[i + shiftT] = (char)ccode;
  }
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    Str[i] = ' ';
  }
  dtostrf(sensor_pressure, 5, 1, Str);
  for (int i = 0; i < sizeof(Str) / sizeof(char); i++) {
    ccode = (int)Str[i];
    if (ccode == 0 || ccode == 32) {
      continue;
    }
    StrResult[i + shiftP] = (char)ccode;
  }
  if (!(tmp = (char*)malloc(sizeof(StrResult)))) {
    Serial.println("Error: can't allocate memory");
    return StrResult;
  }
  strcpy(tmp, StrResult);
  return (tmp);
}

void sendCurrentToBlynk() {
  if (current_status.online_status != 3) {
    return;
  }
  Blynk.virtualWrite(V10, gettime(1));
  Blynk.virtualWrite(V2, createBlynkString(current_status.sensor_temp, current_status.sensor_pressure));
  Blynk.virtualWrite(V3, current_status.sensor_temp);
}

void reportData() {
  getConnected();
  if (time_data.localmillis == 0) {
    updateTime();
  }
  //printToSerial();
  printCurrentToSerial();
  sendCurrentToBlynk();
}

void updateHistory() {

  historical_data.sensor_temp[historical_data.current_index] = current_status.sensor_temp;
  historical_data.sensor_pressure[historical_data.current_index] = current_status.sensor_pressure;
  historical_data.measurement_time[historical_data.current_index] = millis();

  if (historical_data.current_index + 1 == HISTORY_VOLUME) {
    historical_data.current_index = 0;
  } else {
    historical_data.current_index++;
  }

}

int connectToWiFi(int netIndex) {
  int attempts = 10;
  Serial.print("SSID ");
  switch (netIndex)
  {
    case 1:
      Serial.print(ssid);
      WiFi.begin(ssid, pass);
      delay(1000);
      while (WiFi.status() != WL_CONNECTED && attempts > 0) {
        delay(1000);
        Serial.print(".");
        attempts--;
      }
      break;
    case 2:
      Serial.print(ssid2);
      WiFi.begin(ssid2, pass2);
      delay(1000);
      while (WiFi.status() != WL_CONNECTED && attempts > 0) {
        delay(1000);
        Serial.print(".");
        attempts--;
      }
      break;
    case 3:
      Serial.print(ssid3);
      WiFi.begin(ssid3, pass3);
      delay(1000);
      while (WiFi.status() != WL_CONNECTED && attempts > 0) {
        delay(1000);
        Serial.print(".");
        attempts--;
      }
      break;
    case 4:
      Serial.print(ssid4);
      WiFi.begin(ssid4, pass4);
      delay(1000);
      while (WiFi.status() != WL_CONNECTED && attempts > 0) {
        delay(1000);
        Serial.print(".");
        attempts--;
      }
      break;
    case 5:
      Serial.print(ssid5);
      WiFi.begin(ssid5, pass5);
      delay(1000);
      while (WiFi.status() != WL_CONNECTED && attempts > 0) {
        delay(1000);
        Serial.print(".");
        attempts--;
      }
      break;
    case 6:
      Serial.print(ssid6);
      WiFi.begin(ssid6, pass6);
      delay(1000);
      while (WiFi.status() != WL_CONNECTED && attempts > 0) {
        delay(1000);
        Serial.print(".");
        attempts--;
      }
      break;
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    return 1;
  } else {
    return 0;
  }
}

void getConnected() {
  if (current_status.online_status == 3) {
    return;
  }
  //params auth, ssid, pass to be defined in settings.h
  int attempts = COUNT_OF_AVAILABLE_NETWORKS;
  if (current_status.online_status == 0) {   //try to connect to WiFi
    Serial.println("Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    int isConnected = connectToWiFi(PREFERABLE_NETWORK);
    if (isConnected) {
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      attempts = 0;
      current_status.online_status = 1;
      current_status.success_network = PREFERABLE_NETWORK;
    }
    while (attempts > 0) {
      isConnected = connectToWiFi(attempts);
      if (isConnected) {
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        current_status.online_status = 1;
        current_status.success_network = attempts;
        break;
      }
      attempts--;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Cannot connect to WiFi networks");
    }
  }

  if (current_status.online_status == 1) { //try to obtain time
    attempts = 5;
    Serial.println("Starting UDP");
    udp.begin(localPort);
    Serial.print("UDP local port: ");
    Serial.println(udp.localPort());
    updateTime();
    while (time_data.epoch == seventyYears && attempts > 0) {
      delay(1000);
      updateTime();
      attempts--;
    }
    if (time_data.epoch != seventyYears) {
      Serial.println("Internet connection established");
      current_status.online_status = 2;
    } else {
      Serial.print("Cannot establish internet connection");
    }
  }
  if (current_status.online_status == 2) { //try to connect to Blynk
    Serial.print("Connecting to Blynk... ");
    if (current_status.success_network == 1) {
      Blynk.begin(auth, ssid, pass);
    } else if (current_status.success_network == 2) {
      Blynk.begin(auth, ssid2, pass2);
    } else if (current_status.success_network == 3) {
      Blynk.begin(auth, ssid3, pass3);
    } else if (current_status.success_network == 4) {
      Blynk.begin(auth, ssid4, pass4);
    } else if (current_status.success_network == 5) {
      Blynk.begin(auth, ssid5, pass5);
    } else if (current_status.success_network == 6) {
      Blynk.begin(auth, ssid6, pass6);
    }
    Serial.println("done");
    current_status.online_status = 3;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("MCU init...");
  timer_read.setInterval(2000L, readSensors);
  timer_report.setInterval(10000L, reportData);
  timer_history.setInterval(HISTORY_INTERVAL, updateHistory);
  timer_rtc_update.setInterval(3600000L, updateTime);
  init_structures();
  Wire.begin(D6, D5);
  Wire.setClock(100000); 
  if (!bmp.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    delay(300);
    while (1);
  }
  getConnected();
  readSensors();
  updateHistory();

  Serial.println("MCU setup complete. Switched to operating mode.");
}

void loop() {
  Blynk.run();
  timer_read.run();
  timer_report.run();
  timer_history.run();
  timer_rtc_update.run();
}

//real time functions
char * gettime(int time_mode)
{
  struct tm *u;
  char *f;
  const time_t timer = (millis() - time_data.localmillis) / 1000 + time_data.epoch;
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
  if (current_status.online_status == 0) { //No WiFi network
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