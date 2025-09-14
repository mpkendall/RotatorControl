// STARS Rotator Control Board

#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "motor.h"
#include "esp_adc_cal.h"

WebServer server(80);

MotorControl azimuthMotor(M1P1, M1P2, ENA, AZPOT, POT_AZ_MIN, POT_AZ_MAX, POT_AZ_LIMIT_MIN, POT_AZ_LIMIT_MAX);
MotorControl elevationMotor(M2P1, M2P2, ENB, ELPOT, POT_EL_MIN, POT_EL_MAX, POT_EL_LIMIT_MIN, POT_EL_LIMIT_MAX);

Direction motorDirection;

int targetAzimuth = -1;
int targetElevation = -1;

void setup() {
  azimuthMotor.begin();
  elevationMotor.begin();

  Serial.begin(115200);
  Serial.println("NEST Rotator Controller");
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/command", handleCommand);
  server.begin();
}

void loop() {
  server.handleClient();
  if (targetAzimuth != -1 && targetElevation != -1) {
    Bearing current = getCurrentBearing();
    if (abs(current.azimuth - targetAzimuth) <= POSITION_THRESHOLD) {
      azimuthMotor.setDirection(stop);
    }
    if (abs(current.elevation - targetElevation) <= POSITION_THRESHOLD) {
      elevationMotor.setDirection(stop);
    }
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Rotator Control</title></head><body>";
  html += "<h1>Rotator Control</h1>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCommand() {
  String az = server.arg("az");
  String el = server.arg("el");
  Serial.print("az=");
  Serial.print(az);
  Serial.print(", el=");
  Serial.println(el);

  if (!az.length() || !el.length()) {
    server.send(400, "text/plain", "Missing azimuth or elevation parameter");
    return;
  }
  int azVal = az.toInt();
  int elVal = el.toInt();
  bool azValid = (az == String(azVal));
  bool elValid = (el == String(elVal));
  if (!azValid || !elValid) {
    server.send(400, "text/plain", "Invalid azimuth or elevation value");
    return;
  }
  if (azVal < POT_AZ_LIMIT_MIN || azVal > POT_AZ_LIMIT_MAX || elVal < POT_EL_LIMIT_MIN || elVal > POT_EL_LIMIT_MAX) {
    server.send(400, "text/plain", "Coordinates out of range");
    return;
  }

  String response = "azimuth: " + az + ", elevation: " + el;
  targetAzimuth = azVal;
  targetElevation = elVal;
  setMotorCoords(targetAzimuth, targetElevation);
  server.send(200, "text/plain", response);
}

Bearing getCurrentBearing() {
  Bearing b;
  b.azimuth = azimuthMotor.getBearing();
  b.elevation = elevationMotor.getBearing();
  return b;
}

void setMotorCoords(int targetAz, int targetEl) {
  targetAz += AZ_OFFSET;
  targetEl += EL_OFFSET;
  Bearing currentBearing = getCurrentBearing();
  if(targetAz > currentBearing.azimuth) {
    azimuthMotor.setDirection(forward);
  } else if (targetAz < currentBearing.azimuth) {
    azimuthMotor.setDirection(backward);
  } else {
    azimuthMotor.setDirection(stop);
  }

  if(targetEl > currentBearing.elevation) {
    elevationMotor.setDirection(forward);
  } else if (targetEl < currentBearing.elevation) {
    elevationMotor.setDirection(backward);
  } else {
    elevationMotor.setDirection(stop);
  }
}