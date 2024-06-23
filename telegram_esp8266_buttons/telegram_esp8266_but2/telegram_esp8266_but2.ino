//---------External libs---------
#include "DHT.h"
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FastBot.h>
#include <Pinger.h>
#include "settings.h"
extern "C" {
#include <lwip/icmp.h>  // needed for icmp packet definitions
}
//---------DHT init---------
#define DHTPIN D2                                 // DHT digital pin definition
#define DHTTYPE DHT22                             // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);
//---------Network tool---------
Pinger pinger;
//---------IO init---------
#define RELAYPIN 14                // Relay out D5 mapped to GPIO 14
#define RELAY_ENABLED_VALUE true  // Value when relay is enabled
// Wire reference:
// blue - D2
// yellow - D5
// green - G
// orange - 3V
// red - 3V
// black - G
//---------Bot init---------
#define CHAT_ID "-805760286"
FastBot bot(auth);
//---------Business logic init---------
#define DEVICE_NAME "but2"
#define ACTION_INTERVAL_1 5000
#define ACTION_INTERVAL_2 14400000
#define ACTION_INTERVAL_3 20000
struct deviceData {
  uint32_t epoch;
  float sensor_temp;                          // Temperature data
  float sensor_humidity;                      // Humidity data
  int online_status;                          // 0 - disconnected, 1 - WiFi connected, 2 - Internet connected
  int success_network;                        // index of successfully connected WiFi network
  int start_action_time;                      // Time when start action was initiated (in SoC millis)
  int shutdown_action_time;                   // Time when shutdown action was initiated (in SoC millis)
  int last_action_time_1;                     // Last time when action 1 was run (in SoC millis)
  int last_action_time_2;                     // Last time when action 2 was run (in SoC millis)
  int last_action_time_3;                     // Last time when action 3 was run (in SoC millis)
  int wan_status;                             // status of WAN: 0 - disconnected, 1 - connected
  int is_pinging;                             // ping command completion status
  int is_planned_to_notify;                   // is planned to notify: 0 - no, 1 - planned
  int number_of_net_downs;
} current_status;

struct {
  String controller_is_online;
  String options_response;
  String resetesp;
  String data;
  String on;
  String off;
  String start;
  String shutdown;
  String relay_switched_on;
  String relay_switched_off;
} messageStrings;

const uint32_t START_PRESSED_TIME_MS = 1000;
const uint32_t SHUTDOWN_PRESSED_TIME_MS = 8000;

void init_structures() {
  current_status.epoch = 0;
  current_status.sensor_temp = 0;
  current_status.sensor_humidity = 0;
  current_status.online_status = 0;
  current_status.success_network = 0;
  current_status.start_action_time = 0;
  current_status.shutdown_action_time = 0;
  current_status.last_action_time_1 = 0;
  current_status.last_action_time_2 = 0;
  current_status.last_action_time_3 = 0;
  current_status.wan_status = 0;
  current_status.is_pinging = 0;
  current_status.is_planned_to_notify = 0;
  current_status.number_of_net_downs = 0;
  messageStrings.controller_is_online = String(DEVICE_NAME) + " controller is online";
  messageStrings.resetesp = String(DEVICE_NAME) + "_resetesp";
  messageStrings.data = String(DEVICE_NAME) + "_data";
  messageStrings.on = String(DEVICE_NAME) + "_on";
  messageStrings.off = String(DEVICE_NAME) + "_off";
  messageStrings.start = String(DEVICE_NAME) + "_start";
  messageStrings.shutdown = String(DEVICE_NAME) + "_shutdown";
  messageStrings.relay_switched_on = String(DEVICE_NAME) + " relay switched on";
  messageStrings.relay_switched_off = String(DEVICE_NAME) + " relay switched off";
  String options_response = String(DEVICE_NAME) + " controller: options, ";
  options_response += String(messageStrings.resetesp) + ", ";
  options_response += String(messageStrings.data) + ", ";
  options_response += String(messageStrings.on) + ", ";
  options_response += String(messageStrings.off) + ", ";
  options_response += String(messageStrings.start) + ", ";
  options_response += String(DEVICE_NAME) + "_shutdown";
  messageStrings.options_response = options_response;
}

void readSensors() {
  float sensor_temp = dht.readTemperature();
  if (sensor_temp != sensor_temp) {
    Serial.println("DHT22 lib returned NaN");
    dht.begin();
  } else {
    current_status.sensor_temp = sensor_temp;
    current_status.sensor_humidity = dht.readHumidity();
  }
}

void printCurrentToSerial() {
  Serial.print("T ");
  Serial.print(millis());
  Serial.print(", ");
  String stringTemp = String("Temp: ") + current_status.sensor_temp;
  String stringHum = String(", H ") + current_status.sensor_humidity;
  String stringMsg = stringTemp + stringHum;
  Serial.println(stringMsg);
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
  if (current_status.online_status > 0) {
    current_status.online_status = 3;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("MCU init...");
  pinMode(RELAYPIN, OUTPUT);  // Set pin to manage power relay
  digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
  init_structures();
  dht.begin();
  
  getConnected();
  readSensors();

  if (current_status.online_status == 3) {
    bot.setChatID(CHAT_ID);
    bot.skipUpdates();
    bot.attach(newMsg);
    setupPinger();
  }

  printCurrentToSerial();

  Serial.println("MCU setup complete. Switched to operating mode.");
}

byte revert_value(byte value) {
  if (value == 0) {
    return 1;
  } else {
    return 0;
  }
}

void newMsg(FB_msg& msg) {
  msg.text.replace("/", "");
  Serial.println(msg.text);
  if (msg.text.equals("options")) {
    bot.sendMessage(messageStrings.options_response, msg.chatID);
    return;
  } 
  if (msg.text.equals(messageStrings.resetesp)) {
    bot.sendMessage("Rebooting esp...", msg.chatID);
    delay(1000);
    bot.tickManual();
    ESP.restart();
  } 
  if (msg.text.equals(messageStrings.data)) {
    Serial.println(messageStrings.data);
    readSensors();
    printCurrentToSerial();
    String stringTemp = String("Temp: ") + current_status.sensor_temp;
    String stringHum = String(", H ") + current_status.sensor_humidity;
    String stringMsg = stringTemp + stringHum;
    bot.sendMessage(stringMsg);
  }
  if (msg.text.equals(messageStrings.on)) {
    Serial.println(messageStrings.on);
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    bot.sendMessage(messageStrings.relay_switched_on);
  }
  if (msg.text.equals(messageStrings.off)) {
    Serial.println(messageStrings.off);
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
    bot.sendMessage(messageStrings.relay_switched_off);
  }
  if (msg.text.equals(messageStrings.start)) {
    current_status.start_action_time = millis();
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    bot.sendMessage(messageStrings.relay_switched_on);
  } 
  if (msg.text.equals(messageStrings.shutdown)) {
    current_status.shutdown_action_time = millis();
    digitalWrite(RELAYPIN, RELAY_ENABLED_VALUE);
    bot.sendMessage(messageStrings.relay_switched_on);
  }
}

void runTick() {
  if (millis() >= current_status.last_action_time_1 + ACTION_INTERVAL_1) {
    current_status.last_action_time_1 = millis();
    readSensors();
    printCurrentToSerial();
    if (current_status.is_planned_to_notify == 1) {
      current_status.is_planned_to_notify = 0;
      bot.sendMessage(messageStrings.controller_is_online);
    }
  }
  if (millis() >= current_status.last_action_time_2 + ACTION_INTERVAL_2) {
    current_status.last_action_time_2 = millis();
    String stringTemp = String("Temp: ") + current_status.sensor_temp;
    String stringHum = String(", H ") + current_status.sensor_humidity;
    String stringMsg = stringTemp + stringHum;
    bot.sendMessage(stringMsg);
  }
  if (millis() >= current_status.last_action_time_3 + ACTION_INTERVAL_3 && current_status.is_pinging == 0) {
    pingTarget();
    current_status.last_action_time_3 = millis();
  }
  if (current_status.start_action_time > 0 && millis() >= current_status.start_action_time + START_PRESSED_TIME_MS) {
    current_status.start_action_time = 0;
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
    bot.sendMessage(messageStrings.relay_switched_off);
  }
  if (current_status.shutdown_action_time > 0 && millis() >= current_status.shutdown_action_time + SHUTDOWN_PRESSED_TIME_MS) {
    current_status.shutdown_action_time = 0;
    digitalWrite(RELAYPIN, revert_value(RELAY_ENABLED_VALUE));
    bot.sendMessage(messageStrings.relay_switched_off);
  }
}

void setWANStatus(int wan_status) {
  if (current_status.wan_status == 0 && wan_status == 1) {
    Serial.println(messageStrings.controller_is_online);
    current_status.is_planned_to_notify = 1;
  }
  if (current_status.wan_status == 1 && wan_status == 0) {
    Serial.println("Controller is getting offline");
    current_status.number_of_net_downs += 1;
  }
  current_status.wan_status = wan_status;
}

void pingTarget() {
  // Ping google.com
  Serial.printf("\n\nPinging google.com\n");
  if (pinger.Ping("google.com", 3, 2000) == false) {
    Serial.println("Error during ping command.");
    setWANStatus(0);
    current_status.is_pinging = 0;
  } else {
    current_status.is_pinging = 1;
  }
}

void setupPinger() {
  pinger.OnReceive([](const PingerResponse& response) {
    if (response.ReceivedResponse) {
      Serial.printf(
        "Reply from %s: bytes=%d time=%lums TTL=%d\n",
        response.DestIPAddress.toString().c_str(),
        response.EchoMessageSize - sizeof(struct icmp_echo_hdr),
        response.ResponseTime,
        response.TimeToLive);
    } else {
      Serial.printf("Request timed out.\n");
    }

    // Return true to continue the ping sequence.
    // If current event returns false, the ping sequence is interrupted.
    return true;
  });

  pinger.OnEnd([](const PingerResponse& response) {
    // Evaluate lost packet percentage
    float loss = 100;
    if (response.TotalReceivedResponses > 0) {
      loss = (response.TotalSentRequests - response.TotalReceivedResponses) * 100 / response.TotalSentRequests;
    }

    // Print packet trip data
    Serial.printf(
      "Ping statistics for %s:\n",
      response.DestIPAddress.toString().c_str());
    Serial.printf(
      "    Packets: Sent = %lu, Received = %lu, Lost = %lu (%.2f%% loss),\n",
      response.TotalSentRequests,
      response.TotalReceivedResponses,
      response.TotalSentRequests - response.TotalReceivedResponses,
      loss);

    // Print time information
    if (response.TotalReceivedResponses > 0) {
      Serial.printf("Approximate round trip times in milli-seconds:\n");
      Serial.printf(
        "    Minimum = %lums, Maximum = %lums, Average = %.2fms\n",
        response.MinResponseTime,
        response.MaxResponseTime,
        response.AvgResponseTime);
    }

    // Print host data
    Serial.printf("Destination host data:\n");
    Serial.printf(
      "    IP address: %s\n",
      response.DestIPAddress.toString().c_str());
    if (response.DestMacAddress != nullptr) {
      Serial.printf(
        "    MAC address: " MACSTR "\n",
        MAC2STR(response.DestMacAddress->addr));
    }
    if (response.DestHostname != "") {
      Serial.printf(
        "    DNS name: %s\n",
        response.DestHostname.c_str());
    }
    if (loss == 100) {
      setWANStatus(0);
    } else {
      setWANStatus(1);
    }
    current_status.is_pinging = 0;

    return true;
  });
}

void loop() {
  bot.tick();
  runTick();
}
