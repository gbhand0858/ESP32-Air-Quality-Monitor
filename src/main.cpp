#include <Arduino.h>
#include <Wire.h>
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

bool pmsDataValid = false;

// =====================================================
// PMS5003 parser
// =====================================================

uint8_t pmsFrame[32];
uint8_t pmsIndex = 0;

// =====================================================
// Timing
// =====================================================

// Environmental sensor update
unsigned long lastEnvironmentUpdate = 0;
const unsigned long ENVIRONMENT_INTERVAL = 2000;

// OLED screen switching
unsigned long lastDisplaySwitch = 0;
const unsigned long DISPLAY_INTERVAL = 3000;

// false = environmental screen
// true  = air-quality screen
bool showAirQualityScreen = false;

// =====================================================
// Read PMS5003 UART data
// =====================================================

void readPMS5003() {

  while (Serial2.available() > 0) {

    uint8_t incomingByte = Serial2.read();

    // -------------------------------------------------
    // Look for first header byte: 0x42
    // -------------------------------------------------

    if (pmsIndex == 0) {

      if (incomingByte == 0x42) {
        pmsFrame[pmsIndex++] = incomingByte;
      }

      continue;
    }

    // -------------------------------------------------
    // Look for second header byte: 0x4D
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
    // Store remaining frame bytes
    // -------------------------------------------------

    pmsFrame[pmsIndex++] = incomingByte;

    // -------------------------------------------------
    // Full PMS5003 frame received
    // -------------------------------------------------

    if (pmsIndex == 32) {

      uint16_t calculatedChecksum = 0;

      for (int i = 0; i < 30; i++) {
        calculatedChecksum += pmsFrame[i];
      }

      uint16_t receivedChecksum =
        ((uint16_t)pmsFrame[30] << 8) |
        pmsFrame[31];

      // -----------------------------------------------
      // Validate checksum
      // -----------------------------------------------

      if (calculatedChecksum == receivedChecksum) {

        // Atmospheric PM1.0
        pm1 =
          ((uint16_t)pmsFrame[10] << 8) |
          pmsFrame[11];

        // Atmospheric PM2.5
        pm25 =
          ((uint16_t)pmsFrame[12] << 8) |
          pmsFrame[13];

        // Atmospheric PM10
        pm10 =
          ((uint16_t)pmsFrame[14] << 8) |
          pmsFrame[15];

        pmsDataValid = true;

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

        Serial.println("----------------------------");
      }

      else {

        Serial.println();
        Serial.println(
          "ERROR: PMS5003 checksum failed"
        );

        pmsDataValid = false;
      }

      // Prepare for next packet
      pmsIndex = 0;
    }
  }
}

// =====================================================
// Read environmental sensors
// =====================================================

void updateEnvironment() {

  TempAndHumidity dhtData =
    dhtSensor.getTempAndHumidity();

  temperature = dhtData.temperature;
  humidity = dhtData.humidity;

  int32_t pressurePa =
    bmp.readPressure();

  pressureHPa =
    pressurePa / 100.0f;

  Serial.println();
  Serial.println("----- ENVIRONMENT -----");

  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  Serial.print("Pressure: ");
  Serial.print(pressureHPa, 1);
  Serial.println(" hPa");

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

  display.print("Temp: ");
  display.print(temperature, 1);
  display.println(" C");

  display.print("Hum:  ");
  display.print(humidity, 1);
  display.println(" %");

  display.print("Pres: ");
  display.print(pressureHPa, 1);
  display.println(" hPa");

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

  // If we haven't received a valid PMS packet yet
  if (!pmsDataValid) {

    display.println();
    display.println("Waiting for");
    display.println("PMS5003 data...");

  }

  else {

    display.print("PM1.0: ");
    display.print(pm1);
    display.println(" ug/m3");

    display.print("PM2.5: ");
    display.print(pm25);
    display.println(" ug/m3");

    display.print("PM10:  ");
    display.print(pm10);
    display.println(" ug/m3");
  }

  display.display();
}

// =====================================================
// Update whichever OLED screen is active
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

  if (!bmp.begin()) {

    Serial.println(
      "ERROR: BMP180 not detected!"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println(
    "BMP180 initialized."
  );

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
  // Startup OLED screen
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

  // Get initial environmental readings immediately
  updateEnvironment();

  // Start with environmental screen
  showAirQualityScreen = false;

  updateDisplay();

  // Start timers from current time
  lastEnvironmentUpdate = millis();
  lastDisplaySwitch = millis();

  Serial.println();
  Serial.println("System ready.");
}

// =====================================================
// Main loop
// =====================================================

void loop() {

  // ---------------------------------------------------
  // Always listen for PMS5003 data
  // ---------------------------------------------------

  readPMS5003();

  unsigned long currentTime = millis();

  // ---------------------------------------------------
  // Environmental sensor update every 2 seconds
  // ---------------------------------------------------

  if (
    currentTime - lastEnvironmentUpdate >=
    ENVIRONMENT_INTERVAL
  ) {

    lastEnvironmentUpdate = currentTime;

    updateEnvironment();

    // Refresh the display with newest values
    updateDisplay();
  }

  // ---------------------------------------------------
  // Switch OLED screen every 3 seconds
  // ---------------------------------------------------

  if (
    currentTime - lastDisplaySwitch >=
    DISPLAY_INTERVAL
  ) {

    lastDisplaySwitch = currentTime;

    // Toggle screen
    showAirQualityScreen =
      !showAirQualityScreen;

    updateDisplay();
  }
}