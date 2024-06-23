//---------External libs---------
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FastBot.h>
#include "settings.h"
//---------IO init---------
#define RELAYPIN 14                // Relay out D5 mapped to GPIO 14
#define RELAY_ENABLED_VALUE false  // Value when relay is enabled
//---------Bot init---------
#define CHAT_ID "-805760286"
FastBot bot(auth);

//---------Business logic init---------
struct deviceData {
  uint32_t epoch;
  byte isStart;
  byte isShutdown;
} current_status;

const uint32_t START_PRESSED_TIME_S = 1;
const uint32_t SHUTDOWN_PRESSED_TIME_S = 8;

void init_current_status() {
  current_status.epoch = 0;
  current_status.isStart = 0;
  current_status.isShutdown = 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAYPIN, OUTPUT);  // Set pin to manage power relay
  digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  init_current_status();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.println("");
  Serial.print("Connecting to WiFi access point "); Serial.print(ssid);
    while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("Starting UDP");

  bot.setChatID(CHAT_ID);
  bot.skipUpdates();
  bot.sendMessage("Laptop controller is online");
  bot.attach(newMsg);
}

byte revert_value(byte value) {
  if (value == 0) {
    return 1;
  } else {
    return 0;
  }
}

void newMsg(FB_msg& msg) {
  if (msg.text == "/lap_options") {
    bot.sendMessage("lap controller: lap_options, lap_resetesp, lap_start, lap_shutdown", msg.chatID);
    return;
  } 
  if (msg.text == "/lap_resetesp") {
    bot.sendMessage("Rebooting esp...", msg.chatID);
    delay(1000);
    bot.tickManual();
    ESP.restart();
  } 
  if (current_status.epoch > 0) return;
  if (msg.text == "/lap_start") {
    current_status.isStart = 1;
  } else if (msg.text == "/lap_shutdown") {
    current_status.isShutdown = 1;
  }
  if (current_status.isStart == 1 || current_status.isShutdown == 1) {
    current_status.epoch = bot.getUnix();
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    bot.sendMessage("Laptop button pressed", msg.chatID);
  }
}

void checkPressedTime() {
  if (current_status.epoch == 0) return;
  uint32_t epoch = bot.getUnix();
  if (current_status.isStart == 1) {
    if(epoch - current_status.epoch > START_PRESSED_TIME_S) {
      digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
      bot.sendMessage("Laptop button released", CHAT_ID);
      init_current_status();
      return;
    }
  }
  if (current_status.isShutdown == 1) {
    if(epoch - current_status.epoch > SHUTDOWN_PRESSED_TIME_S) {
      digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
      bot.sendMessage("Laptop button released", CHAT_ID);
      init_current_status();
      return;
    }
  }
}

void loop() {
  bot.tick();
  checkPressedTime();
}
