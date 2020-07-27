#include <Adafruit_BMP280.h>
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
#define Ledpin 2

//---------BMP280 init---------
Adafruit_BMP280 bmp;
#define SEALEVELPRESSURE_HPA (1006.581)           // Local sea level pressure
#define HISTORY_VOLUME 16                         // Historical data storage volume
#define HISTORY_INTERVAL 60000L                   // Historical data interval in millis
//---------Rain sensor init---------
#define RAINSENSOR_PIN A0                         // Rain sensor pin number
#define RAINRELAY_PIN D5                          // Rain sensor enabling relay turns sensor periodically
#define RAINREADINTERVAL 180000L                  // Rain sensor reading interval
#define ENABLED_VALUE 0                           // Output value to enable sensor power
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
  int rain_sensor_value;                      // Rain sensor data
  unsigned long rs_last_read_time;            // Rain sensor last time read
} current_status;

struct historicalData {
  int current_index;
  float sensor_temp[HISTORY_VOLUME];                      // Temperature data
  float sensor_pressure[HISTORY_VOLUME];                  // Pressure data
  unsigned long measurement_time[HISTORY_VOLUME];         // Time (millis)
  int rain_sensor_value[HISTORY_VOLUME];
} historical_data;

struct timeData {
  unsigned long epoch;                                    // Recieved time
  unsigned long localmillis;                              // Local time when epoch recieved
} time_data;


void init_structures() {
  current_status.sensor_temp = 0;
  current_status.sensor_altitude = 0;
  current_status.sensor_pressure = 0;
  current_status.rain_sensor_value = 0;
  current_status.rs_last_read_time = 0;
  historical_data.current_index = 0;
  time_data.epoch = seventyYears;
  time_data.localmillis = 0;
}

int reverseValue(int sourceValue){
  if(sourceValue==0){
    return 1;
  }else{
    return 0;
  }
}

void readSensors() {
  current_status.sensor_temp = bmp.readTemperature();
  current_status.sensor_pressure = bmp.readPressure() * 0.00750062;         // Convert mm/Hg to Pa
  current_status.sensor_altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
  if(current_status.rs_last_read_time==0 || millis()-current_status.rs_last_read_time>RAINREADINTERVAL){
    digitalWrite(RAINRELAY_PIN, ENABLED_VALUE);
    Serial.println("Relay on");
    delay(150);
    current_status.rs_last_read_time = millis();
    current_status.rain_sensor_value = map(analogRead(RAINSENSOR_PIN), 0, 1023, 100, 0);
    digitalWrite(RAINRELAY_PIN, reverseValue(ENABLED_VALUE));
  }
  
}

void printCurrentToSerial() {
  Serial.print("T ");
  Serial.print(millis());
  Serial.print(", Tmp ");
  Serial.print(current_status.sensor_temp);
  Serial.print(", Pr ");
  Serial.print(current_status.sensor_pressure);
  Serial.print(", Rain ");
  Serial.println(current_status.rain_sensor_value);
  Serial.println(createBlynkString(current_status.sensor_temp, current_status.sensor_pressure));
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
        shiftT = i+2;
        break;
      case 80: //P
        shiftP = i+2;
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

void reportData() {
  if(time_data.localmillis == 0){
    updateTime();
  }
  //printToSerial();
  printCurrentToSerial();
  Blynk.virtualWrite(V6, gettime(1));
  Blynk.virtualWrite(V7, createBlynkString(current_status.sensor_temp, current_status.sensor_pressure));
  
  if(current_status.rain_sensor_value>1 && current_status.rain_sensor_value<=10){
    Blynk.virtualWrite(V11, "Wet: probably light rain");
  }else if (current_status.rain_sensor_value>40&&current_status.rain_sensor_value<=60){
    Blynk.virtualWrite(V11, "Wet: light rain");
  }else if (current_status.rain_sensor_value>40&&current_status.rain_sensor_value<=60){
    Blynk.virtualWrite(V11, "Wet: rain");
  }else if (current_status.rain_sensor_value>60){
    Blynk.virtualWrite(V11, "Wet: heavy rain");
  }else{
    Blynk.virtualWrite(V11, "Dry");
  }
  pinMode(Ledpin, OUTPUT);
  digitalWrite(Ledpin, HIGH);
  delay(500);
  digitalWrite(Ledpin, LOW);
  pinMode(Ledpin, INPUT);
  
}

void updateHistory() {

  historical_data.sensor_temp[historical_data.current_index] = current_status.sensor_temp;
  historical_data.sensor_pressure[historical_data.current_index] = current_status.sensor_pressure;
  historical_data.rain_sensor_value[historical_data.current_index] = current_status.rain_sensor_value;
  historical_data.measurement_time[historical_data.current_index] = millis();

  if (historical_data.current_index + 1 == HISTORY_VOLUME) {
    historical_data.current_index = 0;
  } else {
    historical_data.current_index++;
  }

}

void setup() {
  Serial.begin(115200);
  pinMode(RAINRELAY_PIN, OUTPUT);
  //params auth, ssid, pass to be defined in settings.h
  Blynk.begin(auth, ssid, pass);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("UDP local port: ");
  Serial.println(udp.localPort());
  timer_read.setInterval(2000L, readSensors);
  timer_report.setInterval(10000L, reportData);
  timer_history.setInterval(HISTORY_INTERVAL, updateHistory);
  timer_rtc_update.setInterval(3600000L, updateTime);
  init_structures();
  if (!bmp.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    delay(300);
    while (1);
  }
  readSensors();
  updateHistory();
  updateTime();
  Serial.println("MCU started...");
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
