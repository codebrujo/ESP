#include "Wire.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"

const float SEA_LEVEL_PRESSURE_HPA = 1013.25;
const int DELAY = 3000;
const int STARTUP_DELAY = 500;


Adafruit_BME280 bme;

void setup() 
{
 Serial.begin(115200);
 Wire.begin(D6, D5);
 Wire.setClock(100000); 
 if(!bme.begin(0x76))
 {
 Serial.println("Could not find a valid BMP280 sensor, check wiring!");
 while (1)
 {
 yield();
 delay(DELAY);
 }
 }
 delay(STARTUP_DELAY);


}

void loop() 
{
 float tempC = bme.readTemperature();
 float pressurePascals = bme.readPressure();

// Print to serial monitor
 printToSerial(tempC, pressurePascals);

// Display data on screen in metric units
 yield();
 delay(DELAY);
}

void printToSerial(float tempC, float pressurePascals)
{
 // Temperature
 float tempF = 9.0/5.0 * tempC + 32.0;

Serial.println("Temperature:");
 printValueAndUnits(tempC, "*C");
 printValueAndUnits(tempF, "*F");
 //printValueAndUnits(tempC, "°C");
 //printValueAndUnits(tempF, "°F");
 Serial.println("");

// Barometric pressure
 float pressureHectoPascals = pressurePascals / 100.0;
 float pressureInchesOfMercury = 0.000295299830714 * pressurePascals;

Serial.println("Pressure:");
 printValueAndUnits(pressurePascals, "Pa");
 printValueAndUnits(pressureHectoPascals, "hPa");
 printValueAndUnits(pressureInchesOfMercury, "inHg");
 Serial.println("");

// Approximate altitude
 float altitudeMeters = bme.readAltitude(SEA_LEVEL_PRESSURE_HPA);
 float altitudeFeet = 3.28 * altitudeMeters;
 
 Serial.println("Approx. Altitude:");
 printValueAndUnits(altitudeMeters, "m");
 printValueAndUnits(altitudeFeet, "ft");
 Serial.println();
}

void printValueAndUnits(float value, String units)
{
 Serial.print(" ");
 Serial.print(value);
 Serial.print(" ");
 Serial.println(units);
}
