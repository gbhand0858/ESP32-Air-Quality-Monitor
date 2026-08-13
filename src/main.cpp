#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <WiFi.h>
#include <WebServer.h>

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
// WiFi configuration
// =====================================================

const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";

// Wokwi-GUEST uses channel 6.
// Specifying it avoids the WiFi scan delay.
const int WIFI_CHANNEL = 6;

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
// HTTP server
// =====================================================

WebServer server(80);

bool wifiHealthy = false;

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
// Create JSON response
// =====================================================

String createSensorJSON() {

  String json = "{";

  // ---------------------------------------------------
  // Environmental readings
  // ---------------------------------------------------

  json += "\"temperature_c\":";

  if (dhtHealthy) {
    json += String(temperature, 1);
  } else {
    json += "null";
  }

  json += ",";

  json += "\"humidity_percent\":";

  if (dhtHealthy) {
    json += String(humidity, 1);
  } else {
    json += "null";
  }

  json += ",";

  json += "\"pressure_hpa\":";

  if (bmpHealthy) {
    json += String(pressureHPa, 1);
  } else {
    json += "null";
  }

  json += ",";

  // ---------------------------------------------------
  // Particulate readings
  // ---------------------------------------------------

  json += "\"pm1_ug_m3\":";

  if (pmsHealthy) {
    json += String(pm1);
  } else {
    json += "null";
  }

  json += ",";

  json += "\"pm25_ug_m3\":";

  if (pmsHealthy) {
    json += String(pm25);
  } else {
    json += "null";
  }

  json += ",";

  json += "\"pm10_ug_m3\":";

  if (pmsHealthy) {
    json += String(pm10);
  } else {
    json += "null";
  }

  json += ",";

  // ---------------------------------------------------
  // Sensor-health information
  // ---------------------------------------------------

  json += "\"status\":{";

  json += "\"dht22\":";
  json += dhtHealthy ? "true" : "false";

  json += ",";

  json += "\"bmp180\":";
  json += bmpHealthy ? "true" : "false";

  json += ",";

  json += "\"pms5003\":";
  json += pmsHealthy ? "true" : "false";

  json += ",";

  json += "\"wifi\":";
  json += wifiHealthy ? "true" : "false";

  json += "}";

  json += "}";

  return json;
}

// =====================================================
// HTTP root endpoint
// =====================================================

void handleRoot() {

  String response =
    "ESP32 Air Quality Monitor\n\n"
    "JSON API:\n"
    "/api/readings\n";

  server.send(
    200,
    "text/plain",
    response
  );
}

// =====================================================
// JSON API endpoint
// =====================================================

void handleReadings() {

  String json =
    createSensorJSON();

  // This will also help when we build the dashboard.
  server.sendHeader(
    "Access-Control-Allow-Origin",
    "*"
  );

  server.send(
    200,
    "application/json",
    json
  );
}

// =====================================================
// 404 response
// =====================================================

void handleNotFound() {

  String response =
    "404 - Endpoint not found\n";

  server.send(
    404,
    "text/plain",
    response
  );
}

// =====================================================
// Connect to WiFi
// =====================================================

void setupWiFi() {

  Serial.println();
  Serial.print(
    "Connecting to WiFi: "
  );

  Serial.println(
    WIFI_SSID
  );

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD,
    WIFI_CHANNEL
  );

  unsigned long startTime =
    millis();

  // Give WiFi up to 10 seconds.
  // Do not permanently freeze the monitoring system
  // if networking fails.
  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 10000
  ) {

    delay(100);

    Serial.print(".");
  }

  Serial.println();

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    wifiHealthy = true;

    Serial.println(
      "WiFi connected."
    );

    Serial.print(
      "ESP32 IP address: "
    );

    Serial.println(
      WiFi.localIP()
    );

  } else {

    wifiHealthy = false;

    Serial.println(
      "WARNING: WiFi connection failed."
    );
  }
}

// =====================================================
// Configure HTTP API
// =====================================================

void setupWebServer() {

  if (!wifiHealthy) {

    Serial.println(
      "HTTP server not started: WiFi unavailable."
    );

    return;
  }

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/api/readings",
    HTTP_GET,
    handleReadings
  );

  server.onNotFound(
    handleNotFound
  );

  server.begin();

  Serial.println(
    "HTTP server started."
  );

  Serial.println(
    "API endpoint: /api/readings"
  );
}

// =====================================================
// Read PMS5003 UART data
// =====================================================

void readPMS5003() {

  while (
    Serial2.available() > 0
  ) {

    uint8_t incomingByte =
      Serial2.read();

    // -------------------------------------------------
    // First header byte
    // -------------------------------------------------

    if (pmsIndex == 0) {

      if (
        incomingByte == 0x42
      ) {

        pmsFrame[pmsIndex++] =
          incomingByte;
      }

      continue;
    }

    // -------------------------------------------------
    // Second header byte
    // -------------------------------------------------

    if (pmsIndex == 1) {

      if (
        incomingByte == 0x4D
      ) {

        pmsFrame[pmsIndex++] =
          incomingByte;

      } else {

        pmsIndex = 0;
      }

      continue;
    }

    // -------------------------------------------------
    // Store remaining packet bytes
    // -------------------------------------------------

    pmsFrame[pmsIndex++] =
      incomingByte;

    // -------------------------------------------------
    // Full packet received
    // -------------------------------------------------

    if (
      pmsIndex == 32
    ) {

      uint16_t calculatedChecksum =
        0;

      for (
        int i = 0;
        i < 30;
        i++
      ) {

        calculatedChecksum +=
          pmsFrame[i];
      }

      uint16_t receivedChecksum =
        ((uint16_t)pmsFrame[30] << 8) |
        pmsFrame[31];

      // -----------------------------------------------
      // Valid packet
      // -----------------------------------------------

      if (
        calculatedChecksum ==
        receivedChecksum
      ) {

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

        lastPMSPacketTime =
          millis();

        Serial.println();
        Serial.println(
          "----- PARTICULATE DATA -----"
        );

        Serial.print(
          "PM1.0: "
        );

        Serial.print(pm1);

        Serial.println(
          " ug/m3"
        );

        Serial.print(
          "PM2.5: "
        );

        Serial.print(pm25);

        Serial.println(
          " ug/m3"
        );

        Serial.print(
          "PM10:  "
        );

        Serial.print(pm10);

        Serial.println(
          " ug/m3"
        );

        Serial.println(
          "PMS5003 Status: OK"
        );

        Serial.println(
          "----------------------------"
        );

      } else {

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
// Check PMS5003 timeout
// =====================================================

void checkPMSHealth() {

  unsigned long currentTime =
    millis();

  if (
    pmsHealthy &&
    currentTime - lastPMSPacketTime >
      PMS_TIMEOUT
  ) {

    pmsHealthy = false;

    Serial.println();
    Serial.println(
      "WARNING: PMS5003 communication timeout"
    );
  }
}

// =====================================================
// Read DHT22 and BMP180
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

    temperature =
      dhtData.temperature;

    humidity =
      dhtData.humidity;

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

  if (
    newPressureHPa >= 300.0 &&
    newPressureHPa <= 1100.0
  ) {

    pressureHPa =
      newPressureHPa;

    bmpHealthy = true;

  } else {

    bmpHealthy = false;
  }

  // ---------------------------------------------------
  // Serial output
  // ---------------------------------------------------

  Serial.println();
  Serial.println(
    "----- ENVIRONMENT -----"
  );

  if (dhtHealthy) {

    Serial.print(
      "Temperature: "
    );

    Serial.print(
      temperature,
      1
    );

    Serial.println(
      " C"
    );

    Serial.print(
      "Humidity: "
    );

    Serial.print(
      humidity,
      1
    );

    Serial.println(
      " %"
    );

  } else {

    Serial.println(
      "DHT22 Status: ERROR"
    );
  }

  if (bmpHealthy) {

    Serial.print(
      "Pressure: "
    );

    Serial.print(
      pressureHPa,
      1
    );

    Serial.println(
      " hPa"
    );

  } else {

    Serial.println(
      "BMP180 Status: ERROR"
    );
  }

  Serial.print(
    "DHT22 Status: "
  );

  Serial.println(
    dhtHealthy ?
    "OK" :
    "ERROR"
  );

  Serial.print(
    "BMP180 Status: "
  );

  Serial.println(
    bmpHealthy ?
    "OK" :
    "ERROR"
  );

  Serial.println(
    "-----------------------"
  );
}

// =====================================================
// Environmental OLED screen
// =====================================================

void displayEnvironmentScreen() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(
    0,
    0
  );

  display.println(
    "ENVIRONMENT"
  );

  display.println(
    "----------------"
  );

  if (dhtHealthy) {

    display.print(
      "Temp: "
    );

    display.print(
      temperature,
      1
    );

    display.println(
      " C"
    );

    display.print(
      "Hum:  "
    );

    display.print(
      humidity,
      1
    );

    display.println(
      " %"
    );

  } else {

    display.println(
      "Temp: ERROR"
    );

    display.println(
      "Hum:  ERROR"
    );
  }

  if (bmpHealthy) {

    display.print(
      "Pres: "
    );

    display.print(
      pressureHPa,
      1
    );

    display.println(
      " hPa"
    );

  } else {

    display.println(
      "Pres: ERROR"
    );
  }

  display.display();
}

// =====================================================
// Air-quality OLED screen
// =====================================================

void displayAirQualityScreen() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(
    0,
    0
  );

  display.println(
    "AIR QUALITY"
  );

  display.println(
    "----------------"
  );

  if (pmsHealthy) {

    display.print(
      "PM1.0: "
    );

    display.print(pm1);

    display.println(
      " ug/m3"
    );

    display.print(
      "PM2.5: "
    );

    display.print(pm25);

    display.println(
      " ug/m3"
    );

    display.print(
      "PM10:  "
    );

    display.print(pm10);

    display.println(
      " ug/m3"
    );

  } else {

    display.println();

    display.println(
      "PMS5003 OFFLINE"
    );

    display.println(
      "Waiting for data..."
    );
  }

  display.display();
}

// =====================================================
// OLED display manager
// =====================================================

void updateDisplay() {

  if (
    showAirQualityScreen
  ) {

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

  Wire.begin(
    21,
    22
  );

  // ---------------------------------------------------
  // BMP180
  // ---------------------------------------------------

  if (
    bmp.begin()
  ) {

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

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS
    )
  ) {

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
  // Startup OLED
  // ---------------------------------------------------

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(
    0,
    0
  );

  display.println(
    "AIR QUALITY"
  );

  display.println(
    "MONITOR"
  );

  display.println();

  display.println(
    "Starting..."
  );

  display.display();

  delay(1000);

  // ---------------------------------------------------
  // Initial environmental readings
  // ---------------------------------------------------

  updateEnvironment();

  showAirQualityScreen =
    false;

  updateDisplay();

  lastEnvironmentUpdate =
    millis();

  lastDisplaySwitch =
    millis();

  // ---------------------------------------------------
  // WiFi + HTTP
  // ---------------------------------------------------

  setupWiFi();

  setupWebServer();

  Serial.println();

  Serial.println(
    "System ready."
  );
}

// =====================================================
// Main loop
// =====================================================

void loop() {

  // ---------------------------------------------------
  // Continuously service PMS5003
  // ---------------------------------------------------

  readPMS5003();

  checkPMSHealth();

  // ---------------------------------------------------
  // Continuously service HTTP clients
  // ---------------------------------------------------

  if (wifiHealthy) {

    server.handleClient();
  }

  unsigned long currentTime =
    millis();

  // ---------------------------------------------------
  // Environmental update
  // ---------------------------------------------------

  if (
    currentTime -
      lastEnvironmentUpdate >=
    ENVIRONMENT_INTERVAL
  ) {

    lastEnvironmentUpdate =
      currentTime;

    updateEnvironment();

    updateDisplay();
  }

  // ---------------------------------------------------
  // OLED page switching
  // ---------------------------------------------------

  if (
    currentTime -
      lastDisplaySwitch >=
    DISPLAY_INTERVAL
  ) {

    lastDisplaySwitch =
      currentTime;

    showAirQualityScreen =
      !showAirQualityScreen;

    updateDisplay();
  }
}