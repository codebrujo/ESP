//---------External libs---------
#include <ESP8266WiFi.h>
#include "settings.h"
//-------------------------------------
#define DEVICE_NAME "beeper"
#define SPEAKER 14
//---------Business logic init---------
struct deviceData {
  int online_status;                          // 0 - disconnected, 1 - WiFi connected, 2 - Internet connected
  int success_network;                        // index of successfully connected WiFi network
} current_status;

void init_structures() {
  current_status.online_status = 0;
  current_status.success_network = 0;
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
      Serial.print("IP address: ");
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
  if (current_status.online_status > 0) {
    current_status.online_status = 3;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.print("Init ");
  Serial.print(DEVICE_NAME);
  Serial.println("...");
  
  init_structures();
  getConnected();
  tone(SPEAKER, 7000, 50);
}

void read_console() {
  if (Serial.available() > 0) {
    String str = Serial.readString();
    Serial.print(str);
    tone(SPEAKER, str.toDouble(), 50);
    // if (str.equals("RESET\n")) {
    //   Serial.println("Reset..");
    //   ESP.restart();
    // } else if (str.equals("PUMP_ON\n")) {
    //   digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    // } else if (str.equals("PUMP_OFF\n")) {
    //   digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
    // } else if (str.equals("CLEAR_IRR_TIME\n")) {
    //   current_status.last_irrigation = 0;
    // } else if (str.equals("READ_SENSOR\n")) {
    //   readSensors();
    // } else if (str.equals("RUN_REPORT\n")) {
    //   reportData();
    // } else if (str.equals("START_IRRIGATION\n")) {
    //   current_status.irrigation_time = 0;
    // }
  }
}

void loop() {
  read_console();
}
