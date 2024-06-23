/*
 * Шаг №4
 * Step4-FSWebServer
 * Справка по распределению памяти https://github.com/esp8266/Arduino/blob/master/doc/filesystem.md
 */
#include <ESP8266WiFi.h>        //Содержится в пакете. Видео с уроком http://esp8266-arduinoide.ru/step1-wifi
#include <ESP8266WebServer.h>   //Содержится в пакете. Видео с уроком http://esp8266-arduinoide.ru/step2-webserver
#include <ESP8266SSDP.h>        //Содержится в пакете. Видео с уроком http://esp8266-arduinoide.ru/step3-ssdp
#include <FS.h>                 //Содержится в пакете. Видео с уроком http://esp8266-arduinoide.ru/step4-fswebserver
#include "settings.h"

IPAddress apIP(192, 168, 4, 1);

// Web интерфейс для устройства
ESP8266WebServer HTTP(80);
// Для файловой системы
File fsUploadFile;

// Определяем переменные wifi
String _ssidAP = "WiFi";   // SSID AP точки доступа
String _passwordAP = ""; // пароль точки доступа
String SSDP_Name="FSWebServer"; // Имя SSDP

#define SPEAKER 14

void setup() {
  Serial.begin(115200);
  Serial.println("");
  //Запускаем файловую систему 
  Serial.println("Start 4-FS");
  FS_init();
  tone(SPEAKER, 7000, 50);
  delay(200);
  Serial.println("Start 1-WIFI");
   //Запускаем WIFI
  getConnected();
  tone(SPEAKER, 6000, 50);
  delay(200);
  //Настраиваем и запускаем SSDP интерфейс
  Serial.println("Start 3-SSDP");
  SSDP_init();
  tone(SPEAKER, 5000, 50);
  delay(200);
  //Настраиваем и запускаем HTTP интерфейс
  Serial.println("Start 2-WebServer");
  HTTP_init();
  tone(SPEAKER, 4000, 50);
  Serial.println("Hardware initialized");
}

void loop() {
  HTTP.handleClient();
  delay(1);
}



