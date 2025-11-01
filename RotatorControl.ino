// STARS Rotator Control Board

#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "motor.h"

WebServer server(80);

int az_limit_min = POT_AZ_LIMIT_MIN;
int az_limit_max = POT_AZ_LIMIT_MAX;
int el_limit_min = POT_EL_LIMIT_MIN;
int el_limit_max = POT_EL_LIMIT_MAX;

MotorControl azimuthMotor(M1P1, M1P2, ENA, AZPOT, POT_AZ_MIN, POT_AZ_MAX, az_limit_min, az_limit_max);
MotorControl elevationMotor(M2P1, M2P2, ENB, ELPOT, POT_EL_MIN, POT_EL_MAX, el_limit_min, el_limit_max);

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
  server.on("/calibrate", handleCalibrate);
  server.on("/status", []() {
    Bearing b = getCurrentBearing();
    String status = "Azimuth: " + String(b.azimuth) + ", Elevation: " + String(b.elevation);
    server.send(200, "text/plain", status);
  });
  server.on("/ping", []() {
    server.send(200, "text/plain", "pong");
  });
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

void updateMotorLimits() {
  azimuthMotor = MotorControl(M1P1, M1P2, ENA, AZPOT, POT_AZ_MIN, POT_AZ_MAX, az_limit_min, az_limit_max);
  elevationMotor = MotorControl(M2P1, M2P2, ENB, ELPOT, POT_EL_MIN, POT_EL_MAX, el_limit_min, el_limit_max);
}

void handleCalibrate() {
  bool updated = false;
  String msg = "";
  if (server.hasArg("az_limit_min")) {
    az_limit_min = server.arg("az_limit_min").toInt();
    updated = true;
  }
  if (server.hasArg("az_limit_max")) {
    az_limit_max = server.arg("az_limit_max").toInt();
    updated = true;
  }
  if (server.hasArg("el_limit_min")) {
    el_limit_min = server.arg("el_limit_min").toInt();
    updated = true;
  }
  if (server.hasArg("el_limit_max")) {
    el_limit_max = server.arg("el_limit_max").toInt();
    updated = true;
  }
  if (updated) {
    updateMotorLimits();
    msg = "Calibration updated. ";
  }
  msg += "az_limit_min=" + String(az_limit_min) + ", az_limit_max=" + String(az_limit_max);
  msg += ", el_limit_min=" + String(el_limit_min) + ", el_limit_max=" + String(el_limit_max);
  server.send(200, "text/plain", msg);
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
  if(targetAz > 360) targetAz -= 360;
  if(targetAz < 0) targetAz += 360;
  Bearing currentBearing = getCurrentBearing();
  if(targetAz > currentBearing.azimuth) {
    REVERSE_AZ ? azimuthMotor.setDirection(backward) : azimuthMotor.setDirection(forward);
  } else if (targetAz < currentBearing.azimuth) {
    REVERSE_AZ ? azimuthMotor.setDirection(forward) :  azimuthMotor.setDirection(backward);
  } else {
    azimuthMotor.setDirection(stop);
  }

  if(targetEl > 180) targetEl = 180;
  if(targetEl < 0) targetEl = 0;
  if(targetEl > currentBearing.elevation) {
    REVERSE_EL ? elevationMotor.setDirection(backward) : elevationMotor.setDirection(forward);
  } else if (targetEl < currentBearing.elevation) {
    REVERSE_EL ? elevationMotor.setDirection(forward) : elevationMotor.setDirection(backward);
  } else {
    elevationMotor.setDirection(stop);
  }
}