#include <Adafruit_BMP280.h>
#include <BlynkSimpleEsp8266.h>
#include "settings.h"

//---------BLYNK init---------
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
BlynkTimer timer_read;
BlynkTimer timer_report;
BlynkTimer timer_history;

//---------BMP280 init---------
Adafruit_BMP280 bmp;
#define SEALEVELPRESSURE_HPA (1006.581)       // Local sea level pressure
#define HISTORY_VOLUME 16                     // Historical data storage volume
#define HISTORY_INTERVAL 60000L               // Historical data interval in millis

struct deviceData {
  float sensor_temp;                          // Temperature data
  float sensor_altitude;                      // Altitude data
  float sensor_pressure;                      // Pressure data
} current_status;

struct historicalData {
  int current_index;
  float sensor_temp[HISTORY_VOLUME];                      // Temperature data
  float sensor_pressure[HISTORY_VOLUME];                  // Pressure data
  unsigned long measurement_time[HISTORY_VOLUME];         // Time (millis)
} historical_data;


void init_structures(){
  current_status.sensor_temp = 0;
  current_status.sensor_altitude = 0;
  current_status.sensor_pressure = 0;
  historical_data.current_index = 0;
}

void readSensor(){
  current_status.sensor_temp = bmp.readTemperature();
  current_status.sensor_pressure = bmp.readPressure() * 0.00750062;         // Convert mm/Hg to Pa
  current_status.sensor_altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
}

void reportData(){
  int i,j;
  char Str[25];
  Serial.println(" Time                     Temp                     Pressure");
  for (i = 0; i<HISTORY_VOLUME; i++)  // цикл по строкам
  {
    int current_index = 0;
    if(historical_data.current_index==0){
      current_index = HISTORY_VOLUME;
    }else{
      current_index = historical_data.current_index-1;
    }
    if(i==current_index){
      Serial.print(">");
    }else{
      Serial.print(" ");
    }
    for (int j=0; j<sizeof(Str)/sizeof(char); j++) {Str[j]=' ';}
    itoa(historical_data.measurement_time[i], Str, 10);
    Serial.write(Str, sizeof(Str)/sizeof(char));
    for (int j=0; j<sizeof(Str)/sizeof(char); j++) {Str[j]=' ';}
    dtostrf(historical_data.sensor_temp[i], 3, 2, Str);
    Serial.write(Str, sizeof(Str)/sizeof(char));
    for (int j=0; j<sizeof(Str)/sizeof(char); j++) {Str[j]=' ';}
    dtostrf(historical_data.sensor_pressure[i], 3, 3, Str);
    Serial.write(Str, sizeof(Str)/sizeof(char));
    Serial.println("");
  }
  Blynk.virtualWrite(V6, current_status.sensor_temp);
  Blynk.virtualWrite(V7, current_status.sensor_pressure);
}

void updateHistory(){

  historical_data.sensor_temp[historical_data.current_index] = current_status.sensor_temp;
  historical_data.sensor_pressure[historical_data.current_index] = current_status.sensor_pressure;
  historical_data.measurement_time[historical_data.current_index] = millis();
  
  if(historical_data.current_index+1 == HISTORY_VOLUME){
    historical_data.current_index = 0;
  }else{
    historical_data.current_index++;
  }
  
}

void setup() {
  Serial.begin(115200);
  //params auth, ssid, pass to be defined in settings.h
  Blynk.begin(auth, ssid, pass);
  timer_read.setInterval(2000L, readSensor);
  timer_report.setInterval(10000L, reportData);
  timer_history.setInterval(HISTORY_INTERVAL, updateHistory);
  init_structures();
  if (!bmp.begin(0x76)) {                               
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    delay(300);
    while (1);
  }
  readSensor();
  updateHistory();
  Serial.println("MCU started...");
}

void loop() {
  Blynk.run();
  timer_read.run();
  timer_report.run();
  timer_history.run();
}
