#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("Air Quality Monitor starting...");
}

void loop() {
  Serial.println("ESP32 alive");
  delay(1000);
}