#include <Arduino.h>
#include <Wire.h>
#include "DHTesp.h"
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int DHT_PIN = 15;

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
const int OLED_ADDRESS = 0x3C;

DHTesp dhtSensor;
Adafruit_BMP085 bmp;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

void setup() {
  Serial.begin(115200);

  Serial.println("Air Quality Monitor starting...");

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  Serial.println("DHT22 initialized.");

  Wire.begin(21, 22);

  if (!bmp.begin()) {
    Serial.println("ERROR: BMP180 not detected!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("BMP180 initialized.");

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERROR: OLED not detected!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("OLED initialized.");

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("AIR QUALITY");
  display.println("MONITOR");
  display.println();
  display.println("Starting...");

  display.display();

  delay(1500);

  Serial.println("System ready.");
}

void loop() {
  TempAndHumidity dhtData = dhtSensor.getTempAndHumidity();

  int32_t pressurePa = bmp.readPressure();
  float pressureHPa = pressurePa / 100.0f;

  Serial.println("----- ENVIRONMENT -----");

  Serial.print("Temperature: ");
  Serial.print(dhtData.temperature, 1);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(dhtData.humidity, 1);
  Serial.println(" %");

  Serial.print("Pressure: ");
  Serial.print(pressureHPa, 1);
  Serial.println(" hPa");

  Serial.println("-----------------------");
  Serial.println();

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("ENVIRONMENT");
  display.println("----------------");

  display.print("Temp: ");
  display.print(dhtData.temperature, 1);
  display.println(" C");

  display.print("Hum:  ");
  display.print(dhtData.humidity, 1);
  display.println(" %");

  display.print("Pres: ");
  display.print(pressureHPa, 1);
  display.println(" hPa");

  display.display();

  delay(2000);
}