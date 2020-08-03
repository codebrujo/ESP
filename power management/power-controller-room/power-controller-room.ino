//---------External libs---------
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp8266.h>
#include "DHT.h"
#include "settings.h"
//---------IO init---------
#define BUTTONPIN                D4               // Button pin
#define RELAYPIN                 D5               // Power relay out
#define RELAYPIN_LIGHT           D6               // Light power relay out
#define RELAYPIN_HEATER          D7               // Heater relay out
#define RELAY_ENABLED_VALUE      true             // Value when relay is enabled
#define LEDPIN                   D8               // Power on LED out
#define LEDPIN_HEATER            D9               // Heater on LED out
#define LEDPIN_LIGHT             D10              // Light on LED out
//---------BLYNK init---------
BlynkTimer report_timer;
BlynkTimer timer_rtc_update;
BlynkTimer timer_clock;
BlynkTimer timer_btn;
#define BLYNKVPIN                V12              // BLYNK power management pin
#define BLYNKHBVPIN              V16              // BLYNK heartbeat virtual pin number
#define BLYNKTEMPVPIN            V17              // BLYNK temperature indicator
#define BLYNKLIGHTVPIN           V13              // BLYNK light management button
#define BLYNKHEATPVPIN           V14              // BLYNK heater power button
#define BLYNKHEATIVPIN           V18              // BLYNK heating indicator
#define BLYNKHEATSVPIN           V15              // BLYNK heater temperature slider
#define BLYNKOUTVPIN             V20              // BLYNK out of home button
//---------NTP init---------
unsigned int localPort = 2390;                    // local port to listen for UDP packets
IPAddress timeServerIP;                           // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;                   // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];              // buffer to hold incoming and outgoing packets
const unsigned long seventyYears = 2208988800UL;
WiFiUDP udp;                                      // A UDP instance to let us send and receive packets over UDP
const int GMT = 3;                                // GMT time conversion
//---------Sensor init---------
#define DHTPIN                   D2               // DHT digital pin definition
#define DHTTYPE                  DHT22            // DHT 22  (AM2302), AM2321
volatile unsigned int cooler_on_temp;             // Temperature turns relay on
DHT dht(DHTPIN, DHTTYPE);
//---------Business logic init---------
struct deviceData {
  byte enabled;
  byte light_enabled;
  byte heater_enabled;
  int heater_on_temp;
  byte climate_on;
  byte last_button_state;
  volatile float sensor_temp;                     // Temperature sensor data
  volatile unsigned int sensor_hum;               // Humidity sensor data
  unsigned int online_status;                     // 0 - disconnected, 1 - WiFi connected, 2 - Internet connected
  volatile unsigned int current_LED;              // Lighting LED indicator: 1 - power, 2 - light, 3 - heater
  volatile unsigned long millis_LED;              // When current_LED was lighted
} current_status;

struct timeData {
  volatile unsigned long epoch;                   // Recieved time
  volatile unsigned long localmillis;             // Local time when epoch recieved
} time_data;


void init_structures() {
  current_status.enabled = 0;
  current_status.light_enabled = 0;
  current_status.heater_enabled = 0;
  current_status.heater_on_temp = 20;
  current_status.climate_on = 0;
  current_status.current_LED = 0;
  current_status.millis_LED = 0;
  current_status.last_button_state = 1;           // Button is not pushed
  current_status.online_status = 0;
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
        shift1 = i + 2;
        break;
      case 72: //H
        shift2 = i + 2;
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
  if (current_status.online_status < 3) {
    return;
  }
  Blynk.virtualWrite(BLYNKHBVPIN, gettime(1));
  Blynk.virtualWrite(BLYNKTEMPVPIN, createBlynkString(current_status.sensor_temp, current_status.sensor_hum));
  Blynk.virtualWrite(BLYNKVPIN, current_status.enabled);
  Blynk.virtualWrite(BLYNKLIGHTVPIN, current_status.light_enabled);
  Blynk.virtualWrite(BLYNKHEATPVPIN, current_status.heater_enabled);
  Blynk.virtualWrite(BLYNKHEATIVPIN, current_status.climate_on);
}

void changeLED(int led_pin) {
  int def_led_pin = led_pin;
  if (def_led_pin == 0) {
    if(millis()-current_status.millis_LED<15000){
      return;
    }
    switch (current_status.current_LED)
    {
      case LEDPIN:
        def_led_pin = LEDPIN_HEATER;
        break;
      case LEDPIN_HEATER:
        def_led_pin = LEDPIN_LIGHT;
        break;
      case LEDPIN_LIGHT:
        def_led_pin = LEDPIN;
        break;
      default:
        def_led_pin = LEDPIN;
    }
  }

  if (led_pin == LEDPIN) {
    digitalWrite(LEDPIN, current_status.enabled);
    digitalWrite(LEDPIN_HEATER, false);
    digitalWrite(LEDPIN_LIGHT, false);
  }
  if (led_pin == LEDPIN_HEATER) {
    digitalWrite(LEDPIN, false);
    digitalWrite(LEDPIN_HEATER, current_status.heater_enabled);
    digitalWrite(LEDPIN_LIGHT, false);
  }
  if (led_pin == LEDPIN_LIGHT) {
    digitalWrite(LEDPIN, false);
    digitalWrite(LEDPIN_HEATER, false);
    digitalWrite(LEDPIN_LIGHT, current_status.light_enabled);
  }
  current_status.current_LED = led_pin;
  current_status.millis_LED = millis();
}

void reportData() {
  readSensors();
  getConnected();
  Serial.print(gettime(1));
  Serial.print(": enabled = ");
  Serial.print(current_status.enabled);
  Serial.print(": sensor_temp = ");
  Serial.print(current_status.sensor_temp);
  Serial.print(": sensor_hum = ");
  Serial.println(current_status.sensor_hum);
  blynkSend();
  changeLED(0);
}

void readSensors() {
  float sensor_temp = dht.readTemperature();
  if (sensor_temp != sensor_temp) {
    Serial.println("DHT22 lib returned NaN");
    dht.begin();
  } else {
    current_status.sensor_temp = sensor_temp;
    current_status.sensor_hum = dht.readHumidity();
  }
}

void getConnected() {
  if (current_status.online_status == 3) {
    return;
  }
  //params auth, ssid, pass to be defined in settings.h
  int attempts = 5;
  if (current_status.online_status == 0) {                                 //try to connect to WiFi
    Serial.print("Connecting to WiFi.");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    delay(1000);
    while (WiFi.status() != WL_CONNECTED && attempts > 0) {
      delay(500);
      Serial.print(".");
      attempts--;
    }
    Serial.println("");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi connected to ");
      Serial.println(ssid);
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      current_status.online_status = 1;
    } else {
      Serial.print("Cannot connect to WiFi network ");
      Serial.println(ssid);
    }
  }

  if (current_status.online_status == 1) {                                 //try to obtain time
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

  if (current_status.online_status == 2) {                                 //try to connect to Blynk
    Blynk.begin(auth, ssid, pass);
    current_status.online_status = 3;
    Serial.println("Blynk service connected");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTONPIN, INPUT_PULLUP);                                        // Set pin to input for capturing manual events
  pinMode(RELAYPIN, OUTPUT);                                               // Set pin to manage power relay
  pinMode(LEDPIN, OUTPUT);                                                 // Set pin to manage LED indicator
  init_structures();
  dht.begin();
  getConnected();
  report_timer.setInterval(15000L, reportData);
  timer_rtc_update.setInterval(3600000L, updateTime);
  timer_btn.setInterval(100L, checkButton);
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(BLYNKVPIN)
{
  current_status.enabled = param.asInt();
  changeLED(LEDPIN);
  performBusinessLogic();
}

BLYNK_WRITE(BLYNKLIGHTVPIN)
{
  current_status.light_enabled = param.asInt();
  changeLED(LEDPIN_LIGHT);
  performBusinessLogic();
}

BLYNK_WRITE(BLYNKHEATPVPIN)
{
  current_status.heater_enabled = param.asInt();
  changeLED(LEDPIN_HEATER);
  performBusinessLogic();
}

BLYNK_WRITE(BLYNKHEATSVPIN)
{
  current_status.heater_on_temp = param.asInt();
  performBusinessLogic();
}

BLYNK_WRITE(BLYNKOUTVPIN)
{
  int out_of_home = param.asInt();
  if (out_of_home == 1) {
    current_status.enabled = 0;
    current_status.light_enabled = 0;
    current_status.heater_enabled = 0;
    performBusinessLogic();
  }
}

void invertButtonState() {
  if (current_status.enabled == 0) {
    current_status.enabled = 1;
  } else {
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

void performBusinessLogic() {
  if (current_status.enabled) {
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
  } else {
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  }
  if (current_status.light_enabled) {
    digitalWrite(RELAYPIN_LIGHT, RELAY_ENABLED_VALUE);
  } else {
    digitalWrite(RELAYPIN_LIGHT, revert_value(RELAY_ENABLED_VALUE));
  }
  if (current_status.heater_enabled) {
    if(current_status.heater_on_temp>current_status.sensor_temp){
      digitalWrite(RELAYPIN_HEATER, RELAY_ENABLED_VALUE);
      current_status.climate_on = 1;
    }else{
      digitalWrite(RELAYPIN_HEATER, revert_value(RELAY_ENABLED_VALUE));
      current_status.climate_on = 0;
    }
  } else {
    digitalWrite(RELAYPIN_HEATER, revert_value(RELAY_ENABLED_VALUE));
    current_status.climate_on = 0;
  }
  blynkSend();
}

void checkButton() {
  byte current_button_state = digitalRead(BUTTONPIN);
  if (current_status.last_button_state != current_button_state && current_status.last_button_state == 0) {
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
