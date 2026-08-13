#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "DHTesp.h"
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// Pin configuration
// =====================================================

const int DHT_PIN = 15;

const int PMS_RX_PIN = 16;
const int PMS_TX_PIN = 17;

// =====================================================
// OLED configuration
// =====================================================

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
const int OLED_ADDRESS = 0x3C;

// =====================================================
// Sensor objects
// =====================================================

DHTesp dhtSensor;
Adafruit_BMP085 bmp;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =====================================================
// Environmental measurements
// =====================================================

float temperature = 0.0;
float humidity = 0.0;
float pressureHPa = 0.0;

// =====================================================
// Particulate measurements
// =====================================================

uint16_t pm1 = 0;
uint16_t pm25 = 0;
uint16_t pm10 = 0;

// =====================================================
// Sensor health
// =====================================================

bool dhtHealthy = false;
bool bmpHealthy = false;
bool pmsHealthy = false;

// =====================================================
// PMS5003 parser
// =====================================================

uint8_t pmsFrame[32];
uint8_t pmsIndex = 0;

unsigned long lastPMSPacketTime = 0;

// If no valid PMS packet arrives for 3 seconds,
// consider the sensor offline.
const unsigned long PMS_TIMEOUT = 3000;

// =====================================================
// Timing
// =====================================================

unsigned long lastEnvironmentUpdate = 0;
const unsigned long ENVIRONMENT_INTERVAL = 2000;

unsigned long lastDisplaySwitch = 0;
const unsigned long DISPLAY_INTERVAL = 3000;

bool showAirQualityScreen = false;

// =====================================================
// PMS5003 parser
// =====================================================

void readPMS5003() {

  while (Serial2.available() > 0) {

    uint8_t incomingByte = Serial2.read();

    // -------------------------------------------------
    // First PMS5003 header byte
    // -------------------------------------------------

    if (pmsIndex == 0) {

      if (incomingByte == 0x42) {
        pmsFrame[pmsIndex++] = incomingByte;
      }

      continue;
    }

    // -------------------------------------------------
    // Second PMS5003 header byte
    // -------------------------------------------------

    if (pmsIndex == 1) {

      if (incomingByte == 0x4D) {
        pmsFrame[pmsIndex++] = incomingByte;
      } else {
        pmsIndex = 0;
      }

      continue;
    }

    // -------------------------------------------------
    // Store packet bytes
    // -------------------------------------------------

    pmsFrame[pmsIndex++] = incomingByte;

    if (pmsIndex == 32) {

      uint16_t calculatedChecksum = 0;

      for (int i = 0; i < 30; i++) {
        calculatedChecksum += pmsFrame[i];
      }

      uint16_t receivedChecksum =
        ((uint16_t)pmsFrame[30] << 8) |
        pmsFrame[31];

      // -------------------------------------------------
      // Valid packet
      // -------------------------------------------------

      if (calculatedChecksum == receivedChecksum) {

        pm1 =
          ((uint16_t)pmsFrame[10] << 8) |
          pmsFrame[11];

        pm25 =
          ((uint16_t)pmsFrame[12] << 8) |
          pmsFrame[13];

        pm10 =
          ((uint16_t)pmsFrame[14] << 8) |
          pmsFrame[15];

        pmsHealthy = true;
        lastPMSPacketTime = millis();

        Serial.println();
        Serial.println("----- PARTICULATE DATA -----");

        Serial.print("PM1.0: ");
        Serial.print(pm1);
        Serial.println(" ug/m3");

        Serial.print("PM2.5: ");
        Serial.print(pm25);
        Serial.println(" ug/m3");

        Serial.print("PM10:  ");
        Serial.print(pm10);
        Serial.println(" ug/m3");

        Serial.println("PMS5003 Status: OK");

        Serial.println("----------------------------");
      }

      // -------------------------------------------------
      // Invalid checksum
      // -------------------------------------------------

      else {

        Serial.println();
        Serial.println(
          "ERROR: PMS5003 checksum failed"
        );
      }

      pmsIndex = 0;
    }
  }
}

// =====================================================
// PMS5003 timeout check
// =====================================================

void checkPMSHealth() {

  unsigned long currentTime = millis();

  if (
    pmsHealthy &&
    currentTime - lastPMSPacketTime > PMS_TIMEOUT
  ) {

    pmsHealthy = false;

    Serial.println();
    Serial.println(
      "WARNING: PMS5003 communication timeout"
    );
  }
}

// =====================================================
// Environmental sensors
// =====================================================

void updateEnvironment() {

  // ---------------------------------------------------
  // DHT22
  // ---------------------------------------------------

  TempAndHumidity dhtData =
    dhtSensor.getTempAndHumidity();

  if (
    !isnan(dhtData.temperature) &&
    !isnan(dhtData.humidity)
  ) {

    temperature = dhtData.temperature;
    humidity = dhtData.humidity;

    dhtHealthy = true;

  } else {

    dhtHealthy = false;
  }

  // ---------------------------------------------------
  // BMP180
  // ---------------------------------------------------

  int32_t pressurePa =
    bmp.readPressure();

  float newPressureHPa =
    pressurePa / 100.0f;

  // Basic plausibility range for atmospheric pressure.
  if (
    newPressureHPa >= 300.0 &&
    newPressureHPa <= 1100.0
  ) {

    pressureHPa = newPressureHPa;
    bmpHealthy = true;

  } else {

    bmpHealthy = false;
  }

  // ---------------------------------------------------
  // Serial output
  // ---------------------------------------------------

  Serial.println();
  Serial.println("----- ENVIRONMENT -----");

  if (dhtHealthy) {

    Serial.print("Temperature: ");
    Serial.print(temperature, 1);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity, 1);
    Serial.println(" %");

  } else {

    Serial.println(
      "DHT22 Status: ERROR"
    );
  }

  if (bmpHealthy) {

    Serial.print("Pressure: ");
    Serial.print(pressureHPa, 1);
    Serial.println(" hPa");

  } else {

    Serial.println(
      "BMP180 Status: ERROR"
    );
  }

  Serial.print("DHT22 Status: ");
  Serial.println(
    dhtHealthy ? "OK" : "ERROR"
  );

  Serial.print("BMP180 Status: ");
  Serial.println(
    bmpHealthy ? "OK" : "ERROR"
  );

  Serial.println("-----------------------");
}

// =====================================================
// Environmental OLED screen
// =====================================================

void displayEnvironmentScreen() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);

  display.println("ENVIRONMENT");
  display.println("----------------");

  if (dhtHealthy) {

    display.print("Temp: ");
    display.print(temperature, 1);
    display.println(" C");

    display.print("Hum:  ");
    display.print(humidity, 1);
    display.println(" %");

  } else {

    display.println("Temp: ERROR");
    display.println("Hum:  ERROR");
  }

  if (bmpHealthy) {

    display.print("Pres: ");
    display.print(pressureHPa, 1);
    display.println(" hPa");

  } else {

    display.println("Pres: ERROR");
  }

  display.display();
}

// =====================================================
// Air-quality OLED screen
// =====================================================

void displayAirQualityScreen() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);

  display.println("AIR QUALITY");
  display.println("----------------");

  if (pmsHealthy) {

    display.print("PM1.0: ");
    display.print(pm1);
    display.println(" ug/m3");

    display.print("PM2.5: ");
    display.print(pm25);
    display.println(" ug/m3");

    display.print("PM10:  ");
    display.print(pm10);
    display.println(" ug/m3");

  } else {

    display.println();
    display.println("PMS5003 OFFLINE");
    display.println("Waiting for data...");
  }

  display.display();
}

// =====================================================
// Display manager
// =====================================================

void updateDisplay() {

  if (showAirQualityScreen) {

    displayAirQualityScreen();

  } else {

    displayEnvironmentScreen();
  }
}

// =====================================================
// Setup
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println(
    "Air Quality Monitor starting..."
  );

  // ---------------------------------------------------
  // DHT22
  // ---------------------------------------------------

  dhtSensor.setup(
    DHT_PIN,
    DHTesp::DHT22
  );

  Serial.println(
    "DHT22 initialized."
  );

  // ---------------------------------------------------
  // I2C
  // ---------------------------------------------------

  Wire.begin(21, 22);

  // ---------------------------------------------------
  // BMP180
  // ---------------------------------------------------

  if (bmp.begin()) {

    bmpHealthy = true;

    Serial.println(
      "BMP180 initialized."
    );

  } else {

    bmpHealthy = false;

    Serial.println(
      "ERROR: BMP180 not detected!"
    );
  }

  // ---------------------------------------------------
  // OLED
  // ---------------------------------------------------

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    Serial.println(
      "ERROR: OLED not detected!"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println(
    "OLED initialized."
  );

  // ---------------------------------------------------
  // PMS5003 UART
  // ---------------------------------------------------

  Serial2.begin(
    9600,
    SERIAL_8N1,
    PMS_RX_PIN,
    PMS_TX_PIN
  );

  Serial.println(
    "PMS5003 UART initialized."
  );

  // ---------------------------------------------------
  // Startup screen
  // ---------------------------------------------------

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);

  display.println("AIR QUALITY");
  display.println("MONITOR");
  display.println();
  display.println("Starting...");

  display.display();

  delay(1000);

  // ---------------------------------------------------
  // Initial sensor update
  // ---------------------------------------------------

  updateEnvironment();

  showAirQualityScreen = false;

  updateDisplay();

  lastEnvironmentUpdate =
    millis();

  lastDisplaySwitch =
    millis();

  Serial.println();
  Serial.println("System ready.");
}

// =====================================================
// Main loop
// =====================================================

void loop() {

  // Always process incoming UART bytes
  readPMS5003();

  // Continuously check for PMS timeout
  checkPMSHealth();

  unsigned long currentTime =
    millis();

  // ---------------------------------------------------
  // Update environmental sensors
  // ---------------------------------------------------

  if (
    currentTime - lastEnvironmentUpdate >=
    ENVIRONMENT_INTERVAL
  ) {

    lastEnvironmentUpdate =
      currentTime;

    updateEnvironment();

    updateDisplay();
  }

  // ---------------------------------------------------
  // Alternate OLED screens
  // ---------------------------------------------------

  if (
    currentTime - lastDisplaySwitch >=
    DISPLAY_INTERVAL
  ) {

    lastDisplaySwitch =
      currentTime;

    showAirQualityScreen =
      !showAirQualityScreen;

    updateDisplay();
  }
}