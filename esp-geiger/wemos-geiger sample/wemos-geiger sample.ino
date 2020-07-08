
// IDE settings

// Board Wemos D1
// IDE Settings: Board: LOLIN (Wemos) D1 R2 & mini
// 4MB (no spiffs), 160MHz (80 tez dziala), v2 lower memory, 921600 on COM5

// Board: ESP01
// IDE settings: Generic ESP8266 module, 80MHz, vtables Flash, reset method: ck, crystal 26MHz, flash 40MHZ, flash mode QIO, 921600 on COM5
// as per https://arduino-esp8266.readthedocs.io/en/latest/boards.html#generic-esp8266-module
// VCC->3.3V, GND-GND, RX->TX, TX->RX
// RST->RTS(R/C) [connect for programming, disconnect for work/test], GPIO0->DTR 
// set receiver pin to 2 (GPIO2)
// Flash: 1M (no spiffs or 64K spiffs), enough for OTA
// info from CheckFlashConfig:
// Flash real id:   001440C8
// Flash real size: 1048576 bytes

/// Geiger counter
#define RECEIVER_PIN 2  // 4=GPIO4=D2 any interrupt able pin (Wemos) // 2=GPIO2 on ESP-01
/// Geiger tube parameters
#define TUBE_NAME "j305"
#define TUBE_FACTOR 0.00812
/// 

#define DEBUG 1
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

#include "wifipassword.h"
//#define WIFIPASSWORD "12345secret"

const char* myhostname = "esp01geiger";
const char* ssid = "Springfield";
const char* password =  WIFIPASSWORD;
const char* mqttServer = "iotlocal.lan";
const uint32_t mqttPort = 1883;
const char* mqttPrefix = "esphome";
String myId;
String connTopic;
String statusTopic;

//////////////////////////////////////////////
/// MQTT+Wifi

WiFiClient espClient;
PubSubClient client(espClient);

void setupMQTT() {
  myId = String(system_get_chip_id(),HEX);

  Serial.println("My ID is "+myId);

  WiFi.hostname(myhostname);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  Serial.print("\nLocal IP: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, mqttPort);
  connTopic = String(mqttPrefix+String("/sensor/")+myId+String("/connection"));
  statusTopic = String(mqttPrefix+String("/sensor"));

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(myId.c_str()),NULL,NULL,connTopic.c_str(),0,true,"offline") {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  client.publish(connTopic.c_str(), "online", true);
}

//////////////////////////////////////////////
/// counter part

#define LOG_PERIOD 60000     // Logging period in milliseconds, recommended value 15000-60000.
#define MAX_PERIOD 60000     // Maximum logging period

volatile unsigned long counts;  // variable for GM Tube events
volatile bool sig;              // flag that at least one impulse occured interrupt
unsigned long cpm;              // variable for CPM
unsigned int multiplier;        // variable for calculation CPM in this sketch
unsigned long previousMillis;   // variable for time measurement

void tube_impulse(){            // procedure for capturing events from Geiger Kit
  counts++;
  sig = true;
}

void setupGeiger() {
  // setup interrupt for falling edge on receier pin
  sig = false;
  counts = 0;
  cpm = 0;

  multiplier = MAX_PERIOD / LOG_PERIOD;     // calculating multiplier, depend on your log period    
  pinMode(RECEIVER_PIN, INPUT);             // set pin as input for capturing GM Tube events
  attachInterrupt(digitalPinToInterrupt(RECEIVER_PIN), tube_impulse, RISING);  // define external interrupts  
}

void loopGeiger() {
  unsigned long currentMillis = millis();

#ifdef DEBUG
  if (sig) {
    Serial.println("tick!");
    sig = false;
  }
#endif

  if(currentMillis - previousMillis > LOG_PERIOD){
    previousMillis = currentMillis;
    cpm = counts * multiplier;
    counts = 0;
    float uS = TUBE_FACTOR * cpm;
#ifdef DEBUG
    Serial.print(cpm); Serial.print(" cpm\t");
    Serial.print(uS); Serial.println(" uSv/h");
#endif
    // send message through MQTT
    while (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting to WiFi..");
      delay(500);
    }
    while (!client.connected()) {
      Serial.println("Reconnecting to MQTT...");
      if (client.connect(myId.c_str()),NULL,NULL,connTopic.c_str(),0,true,"offline") {
        Serial.println("connected");
      } else {
        Serial.print("failed with state ");
        Serial.print(client.state());
        delay(2000);
      }
    }
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& doc = jsonBuffer.createObject();
    doc["receiver"] = myId.c_str(); // setup receiver id
    doc["cpm"] = cpm;
    doc["usv_per_h"] = uS;
    String topic = String(statusTopic+"/"+TUBE_NAME+"/state");
    String output;
    doc.printTo(output);
    client.beginPublish(topic.c_str(), output.length(), false);
    int bytes_written = client.write((const uint8_t*)output.c_str(),output.length());
    client.endPublish();
    if (bytes_written!=output.length()) {
      Serial.print("failed to send message: ");
      Serial.print(bytes_written);
      Serial.print(" instead of ");
      Serial.println(output.length());
    }
  }
}

//////////////////////////////////////////////
/// OTA part

void setupOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(myhostname);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  //
  setupOTA();
  //
  setupMQTT();
  //
  setupGeiger();
}

void loop() {
  // no need to process MQTT loop, we are not subscribed to anything
  client.loop();
  // Geiger events
  loopGeiger();
  // OTA check
  ArduinoOTA.handle();
}

