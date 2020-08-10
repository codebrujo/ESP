/*
   Geiger.ino

   This code interacts with the Aliexpress RadiationD-v1.1 (CAJOE) Geiger counter board
   and reports readings in CPM (Counts Per Minute).

   Idea: Mark A. Heckler (@MkHeck, mark.heckler@gmail.com), solution author: Sergei Ogarkov (misc@sogarkov.ru)

   License: MIT License

   Please use freely with attribution. Thank you!
*/

//---------External libs---------
#include <Ticker.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266HTTPClient.h>
#include "settings.h"

//---------Display init---------
SSD1306Wire display(0x3c, D1, D2);            // ADDRESS, SDA, SCL
#define DISP_UPDATE_PERIOD 3000               // Period of display re-draw

//---------Geiger consts---------
#define CLEAR_LEVEL 1000                      // Clear counters to avoid overflow
#define CHANGE_LEVEL_DETECTION 8              // Level to recalc average in case of significant input change
#define WARNING_LEVEL_CPM 30                  // Radiation level indicates warning
#define HIGH_LEVEL_CPM 50                     // High level of radiation
#define DANGER_LEVEL_CPM 100                  // Danger level of radiation
#define LOG_PERIOD 5000                       // Logging period in milliseconds, recommended value 15000-60000.
#define MAX_PERIOD 60000                      // Maximum logging period
#define TUBE_CORRECTOR 0.8                    // Tube correction multiplier
#define RAD_NOTIF_FREQ 3600000                // Frequency of high radiation push notifications
#define GEIGER_IN D5

//---------DHT init---------
#define DHTPIN D6                             // DHT digital pin definition
#define DHTTYPE DHT22                         // DHT 22  (AM2302), AM2321
volatile unsigned int cooler_on_temp;         // Temperature turns relay on
DHT dht(DHTPIN, DHTTYPE);

//---------RELAY init---------
#define RELAYPIN  D7                          // Cooler relay pin
#define RELAY_ENABLED_VALUE 1                 // Define NO or NC relay connection
#define RELAYPIN1 D8                          // Router relay pin
//---------Timer init---------
Ticker timer;

//---------BLYNK init---------
#define BLYNK_PRINT Serial                    // Comment this out to disable prints and save space
BlynkTimer report_timer;
BlynkTimer connection_check_timer;
const char* blynk_host = "blynk-cloud.com";
String url = "http://" + String(blynk_host) + "/" + String(auth) + "/notify";
const int httpPort = 80;

//---------Business logic init---------
// Calculation multiplier
const unsigned int multiplier = MAX_PERIOD / LOG_PERIOD * TUBE_CORRECTOR;

struct deviceData {
  unsigned long counts;                       // GM Tube events counter
  unsigned long prev_counts;                  // Counter value from previouse logging cycle
  unsigned long cpm;                          // Radioactivity (counts per minute)
  unsigned long lmc;                          // Radioactivity (last measurement cpm)
  unsigned long mp;                           // Measurement period
  long timeSinceLastDraw;                     // Last display re-draw time
  float sensor_temp;                          // Temperature sensor data
  int sensor_hum;                             // Humidity sensor data
  int warning_indicator;                      // 0 - capturing data, 1 - normal, 2 - warning, 3 - HIGH, 4 - DANGER
  int relay_enabled;                          // Coolant relay trigger
  int is_relay_enabled;                       // Coolant relay current status
  unsigned long last_rad_notification;        // Time of the last high radiation push notification
  boolean is_connected;                       // Internet connection indicator
  unsigned long inet_reset_time;              // Time of begin reset procedure
  boolean is_rebooting;                       // Indicates reboot status
  int online_status;                          // 0 - disconnected, 1 - WiFi connected, 2 - Internet connected
  unsigned int hw_error_detected;
} current_status;

int revert_value(int value) {
  if (value == 0) {
    return 1;
  } else {
    return 0;
  }
}

void calc() {
  int diff = current_status.counts - current_status.prev_counts;
  if(diff>200){                       //chineese shitboard correction
    current_status.counts = current_status.prev_counts;
    current_status.hw_error_detected = current_status.hw_error_detected + 2;
    Serial.print("Difference (hw error detected): ");
    Serial.println(diff);
    return;
  }
  if(current_status.hw_error_detected>0){
    current_status.counts = 0;
    current_status.mp = 0;
    current_status.prev_counts = 0;
    Serial.print("HW errors: ");
    Serial.println(current_status.hw_error_detected);
    current_status.hw_error_detected--;
    return;
  }
  current_status.mp++;
  current_status.lmc = diff * multiplier;
  if (diff > CHANGE_LEVEL_DETECTION) {
    current_status.cpm = current_status.lmc;
    current_status.counts = diff;
    current_status.mp = 1;
    current_status.warning_indicator = 2;
  } else {
    current_status.cpm = current_status.counts * multiplier / current_status.mp;
    if (current_status.cpm > HIGH_LEVEL_CPM) {
      current_status.warning_indicator = 3;
    } else if (current_status.cpm > DANGER_LEVEL_CPM) {
      current_status.warning_indicator = 4;
    } else if (current_status.cpm > WARNING_LEVEL_CPM) {
      current_status.warning_indicator = 2;
    } else if (current_status.cpm == 0) {
      current_status.warning_indicator = 0;
    } else {
      current_status.warning_indicator = 1;
    }
  }


  if (current_status.counts > CLEAR_LEVEL) {
    current_status.counts = current_status.counts / 10;
    current_status.mp = current_status.mp / 10;
  }
  Serial.print(current_status.cpm);
  Serial.print(" мкР/ч, ");
  Serial.print(current_status.counts);
  Serial.print(" counts totally, ");
  Serial.print(diff);
  Serial.print(" counts last, ");
  Serial.print(current_status.mp);
  Serial.print(" periods, ");
  Serial.print(current_status.sensor_temp);
  Serial.print("°C, ");
  Serial.print(current_status.sensor_hum);
  Serial.print("%, ");
  Serial.print("cooler switch: ");
  Serial.print(current_status.relay_enabled);
  Serial.print(", cooler run status: ");
  Serial.println(current_status.is_relay_enabled);

  current_status.prev_counts = current_status.counts;
}

void ICACHE_RAM_ATTR tube_impulse() { // Captures count of events from Geiger counter board
  current_status.counts++;
}

void displayInit() {
  display.init();
  display.flipScreenVertically();
}

void displayDraw(String dispString) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, dispString);
  display.drawString(0, 10, "Cool temp " + String(cooler_on_temp) + " at pin " + String(RELAYPIN));
  display.drawString(0, 20, "R.warns " + String(WARNING_LEVEL_CPM) + "/" + String(HIGH_LEVEL_CPM) + "/" + String(DANGER_LEVEL_CPM));
  display.drawString(0, 30, "Log period " + String(LOG_PERIOD) + " ms");
  display.drawString(0, 40, "DHT at pin " + String(DHTPIN));
  display.display();
}

void showIndicators() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  switch (current_status.warning_indicator)
  {
    case 1:
      display.drawString(0, 0, "NORMAL");
      break;
    case 2:
      display.drawString(0, 0, "WARNING");
      break;
    case 3:
      display.drawString(0, 0, "HIGH");
      break;
    case 4:
      display.drawString(0, 0, "DANGER!");
      break;
    default:
      display.drawString(0, 0, "Measuring...");
      break;
  }
  display.drawString(0, 10, "Ra: " + String(current_status.cpm) + " µR/h " + "Rl: " + String(current_status.lmc) + " µR/h");
  display.drawString(0, 20, "T: " + String(current_status.sensor_temp) + "°C H: " + String(current_status.sensor_hum) + "%");
  switch (current_status.relay_enabled)
  {
    case 0:
      display.drawString(0, 30, "COOLER: OFF");
      break;
    case 1:
      if (current_status.is_relay_enabled == 0) {
        display.drawString(0, 30, "COOLER: AUTO OFF");
      } else {
        display.drawString(0, 30, "COOLER: AUTO ON");
      }
      break;
    case 2:
      display.drawString(0, 30, "COOLER: ON");
      break;
    default:
      display.drawString(0, 30, "COOLER: UNKNOWN");
      break;
  }
  if (current_status.is_connected) {
    display.drawString(0, 40, "INET: CONNECTED");
  } else {
    if (current_status.is_rebooting) {
      display.drawString(0, 40, "INET: REBOOTING");
    } else {
      display.drawString(0, 40, "INET: DISCONNECTED");
    }

  }
  display.display();
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

void structure_init() {
  current_status.counts = 0;
  current_status.prev_counts = 0;
  current_status.cpm = 0;
  current_status.lmc = 0;
  current_status.mp = 0;
  current_status.timeSinceLastDraw = millis();
  current_status.sensor_temp = 0;
  current_status.sensor_hum = 0;
  current_status.warning_indicator = 0;
  current_status.relay_enabled = 0;
  current_status.is_relay_enabled = 0;
  current_status.last_rad_notification = 0;
  current_status.hw_error_detected = 0;
  current_status.is_rebooting = false;
}

BLYNK_WRITE(V2) // чтение данных с пина 2
{
  current_status.relay_enabled = param.asInt();
  Serial.print("Relay switch change: ");
  Serial.println(current_status.relay_enabled);
}

BLYNK_WRITE(V3) // чтение данных с пина 3
{
  cooler_on_temp = param.asInt();
  Serial.print("Relay temp change: ");
  Serial.println(cooler_on_temp);

}

BLYNK_CONNECTED() {
  Blynk.syncAll();
  current_status.is_connected = true;
}

BLYNK_APP_DISCONNECTED() {
  current_status.is_connected = false;
  runResetConnection();
}

void blynkSend() {
  if(current_status.hw_error_detected>0){
    Blynk.virtualWrite(V0, "HW error");
  }else{
    Blynk.virtualWrite(V0, current_status.cpm);
  }
  Blynk.virtualWrite(V1, current_status.sensor_temp);
}

void runCooler() {
  if (current_status.relay_enabled == 0) {
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  } else if (current_status.relay_enabled == 2) {
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
  } else if (current_status.relay_enabled == 1) {
    if (current_status.sensor_temp > cooler_on_temp) {
      digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    } else if (current_status.sensor_temp < cooler_on_temp - 1) {
      digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
    }
  }
  current_status.is_relay_enabled =  (digitalRead(RELAYPIN) == RELAY_ENABLED_VALUE) ? 1 : 0;
  Blynk.virtualWrite(V4, current_status.is_relay_enabled * 255);
}

void sendPushNotification() {
  if (!current_status.is_connected || current_status.hw_error_detected>0) {
    return;
  }
  if (current_status.cpm < WARNING_LEVEL_CPM) {
    current_status.last_rad_notification = 0;
    return;
  }
  if (millis() - current_status.last_rad_notification < RAD_NOTIF_FREQ && current_status.last_rad_notification > 0) {
    Serial.print("Push: already notified. ");
    Serial.print("millis - ");
    Serial.print(millis());
    Serial.print(",last - ");
    Serial.print(current_status.last_rad_notification);
    Serial.print(",FREQ - ");
    Serial.println(RAD_NOTIF_FREQ);
    return;
  }
  String postData = "{\"body\": \"Radioactivity: " + String(current_status.cpm) + " µR/h\"}";
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  auto httpCode = http.POST(postData);
  if (httpCode != 200) {
    String payload = http.getString();
    Serial.println(url);
    Serial.println(postData);
    Serial.print("Response code:");
    Serial.println(httpCode); //Print HTTP return code
    Serial.print("Payload:");
    Serial.println(payload); //Print request response payload
    http.end(); //Close connection Serial.println();
  } else {
    current_status.last_rad_notification = millis();
    Serial.println("Notification sent");
  }
}

void reportData() {
  readDHT();
  showIndicators();
  blynkSend();
  runCooler();
  sendPushNotification();
}

void runResetConnection() {
  if (current_status.is_connected) {
    current_status.inet_reset_time = 0;
    current_status.is_rebooting = false;
    return;
  }
  if (millis() < 60000) {
    return;
  }
  //Turn off router for 5 secs
  if (current_status.inet_reset_time == 0) {
    digitalWrite(RELAYPIN1, revert_value(RELAY_ENABLED_VALUE));
    current_status.inet_reset_time = millis();
    current_status.is_rebooting = true;
    Serial.println("No connection. Turn off router...");
  } else if (millis() - current_status.inet_reset_time > 3000 && current_status.is_rebooting) {
    digitalWrite(RELAYPIN1, RELAY_ENABLED_VALUE);
    Serial.println("Restarting router...");
  } else if (millis() - current_status.inet_reset_time > 180000) {                           //Connection is still down, retry
    current_status.inet_reset_time = 0;
    Serial.println("No connection. Retry router reboot...");
  }
  if (millis() - current_status.inet_reset_time > 20000 && current_status.is_rebooting) {
    current_status.is_rebooting = false;
  }

}

void checkConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    current_status.is_connected = false;
  } else {
    WiFiClient client;

    if (!client.connect(blynk_host, httpPort))
    {
      current_status.is_connected = false;
    } else {
      current_status.is_connected = true;
    }
    client.stop();
  }
  runResetConnection();
}

void setup() {
  Serial.begin(115200);                                                     // Start serial monitor
  displayInit();
  displayDraw("Starting firmware...");
  pinMode(RELAYPIN1, OUTPUT);                                               // Set pin to manage reboot relay
  pinMode(GEIGER_IN, INPUT);                                                // Set pin to input for capturing GM Tube events
  pinMode(RELAYPIN, OUTPUT);                                                // Set pin to manage cooler relay
  digitalWrite(RELAYPIN1, RELAY_ENABLED_VALUE);
  Blynk.begin(auth, ssid, pass);                                            // Start Blynk client
  dht.begin();
  report_timer.setInterval(DISP_UPDATE_PERIOD, reportData);
  connection_check_timer.setInterval(10000, checkConnection);
  interrupts();                                                             // Enable interrupts (in case they were previously disabled)
  attachInterrupt(digitalPinToInterrupt(GEIGER_IN), tube_impulse, FALLING); // Define external interrupts
  timer.attach(LOG_PERIOD / 1000, calc);
  structure_init();
  displayDraw("Measuring...");
}

void loop() {
  Blynk.run();
  report_timer.run();
  connection_check_timer.run();
}
