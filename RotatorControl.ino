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

// forward declaration: explicit prototype avoids Arduino auto-prototype issues with default args
void setMotorCoords(int targetAz, int targetEl, float slew = -1.0f);

// Simple bearer token check for all endpoints. Returns true if authorized.
bool checkAuth() {
  // If no token configured, deny access (require token).
  String expected = String("Bearer ") + String(BEARER_TOKEN);
  if (!server.hasHeader("Authorization")) {
    server.send(401, "text/plain", "Unauthorized");
    return false;
  }
  String auth = server.header("Authorization");
  if (auth != expected) {
    server.send(401, "text/plain", "Unauthorized");
    return false;
  }
  return true;
}

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
    if (!checkAuth()) return;
    Bearing b = getCurrentBearing();
    float azSpeed = azimuthMotor.getSpeed();
    float elSpeed = elevationMotor.getSpeed();
    String status = "Azimuth: " + String(b.azimuth) + " (" + String(azSpeed, 2) + " deg/s), ";
    status += "Elevation: " + String(b.elevation) + " (" + String(elSpeed, 2) + " deg/s)";
    server.send(200, "text/plain", status);
  });
  server.on("/ping", []() {
    if (!checkAuth()) return;
    server.send(200, "text/plain", "pong");
  });
  server.on("/test", handleSpeedTest);
  server.begin();
}

void loop() {
  server.handleClient();
  // update slew controllers (if active)
  azimuthMotor.update();
  elevationMotor.update();
  // run speed test sequence if active
  updateSpeedTest();
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
  if (!checkAuth()) return;
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
  if (!checkAuth()) return;
  String html = "<!DOCTYPE html><html><head><title>Rotator Control</title></head><body>";
  html += "<h1>Rotator Control</h1>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCommand() {
  if (!checkAuth()) return;
  String az = server.arg("az");
  String el = server.arg("el");
  String slewArg = server.arg("slew");
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
  // optional slew parameter (degrees per second). If not provided or invalid, pass -1 for full speed.
  float slewVal = -1.0f;
  if (slewArg.length()) {
    // toFloat returns 0.0 on failure; we accept zero as 'stop/change' so check string
    slewVal = slewArg.toFloat();
    // if slew is negative, treat as unspecified (full speed)
    if (slewVal <= 0.0f) slewVal = -1.0f;
  }
  targetAzimuth = azVal;
  targetElevation = elVal;
  setMotorCoords(targetAzimuth, targetElevation, slewVal);
  server.send(200, "text/plain", response);
}



Bearing getCurrentBearing() {
  Bearing b;
  b.azimuth = azimuthMotor.getBearing();
  b.elevation = elevationMotor.getBearing();
  return b;
}

// Speed test sequence state
unsigned long testStartMillis = 0;
int testPhase = 0;
bool testRunning = false;

void handleSpeedTest() {
  if (!checkAuth()) return;
  if (testRunning) {
    server.send(200, "text/plain", "Test already in progress");
    return;
  }
  testRunning = true;
  testPhase = 0;
  testStartMillis = millis();
  server.send(200, "text/plain", "Speed test started");
}

void updateSpeedTest() {
  if (!testRunning) return;
  
  unsigned long now = millis();
  unsigned long elapsed = now - testStartMillis;
  const unsigned long PHASE_DURATION = 5000; // 5 seconds per phase
  
  // Update test phase every PHASE_DURATION ms
  int currentPhase = elapsed / PHASE_DURATION;
  if (currentPhase != testPhase) {
    testPhase = currentPhase;
    // Test sequence:
    // 0: Stop both motors
    // 1: Az forward at 5 deg/s
    // 2: Az forward at 10 deg/s
    // 3: Az forward at full speed
    // 4: Az stop
    // 5: El up at 5 deg/s
    // 6: El up at 10 deg/s
    // 7: El up at full speed
    // 8: El stop, end test
    switch(testPhase) {
      case 0:
        azimuthMotor.setDirection(stop);
        elevationMotor.setDirection(stop);
        Serial.println("Test phase 0: All stop");
        break;
      case 1:
        azimuthMotor.setDirection(REVERSE_AZ ? backward : forward);
        azimuthMotor.setSlew(5.0f);
        Serial.println("Test phase 1: Az 5 deg/s");
        break;
      case 2:
        azimuthMotor.setSlew(10.0f);
        Serial.println("Test phase 2: Az 10 deg/s");
        break;
      case 3:
        azimuthMotor.clearSlew();
        Serial.println("Test phase 3: Az full speed");
        break;
      case 4:
        azimuthMotor.setDirection(stop);
        Serial.println("Test phase 4: Az stop");
        break;
      case 5:
        elevationMotor.setDirection(REVERSE_EL ? backward : forward);
        elevationMotor.setSlew(5.0f);
        Serial.println("Test phase 5: El 5 deg/s");
        break;
      case 6:
        elevationMotor.setSlew(10.0f);
        Serial.println("Test phase 6: El 10 deg/s");
        break;
      case 7:
        elevationMotor.clearSlew();
        Serial.println("Test phase 7: El full speed");
        break;
      case 8:
        elevationMotor.setDirection(stop);
        testRunning = false;
        Serial.println("Test phase 8: Test complete");
        break;
    }
  }
}

void setMotorCoords(int targetAz, int targetEl, float slew) {
  targetAz += AZ_OFFSET;
  targetEl += EL_OFFSET;
  if(targetAz > 360) targetAz -= 360;
  if(targetAz < 0) targetAz += 360;
  Bearing currentBearing = getCurrentBearing();
  if(targetAz > currentBearing.azimuth) {
    REVERSE_AZ ? azimuthMotor.setDirection(backward) : azimuthMotor.setDirection(forward);
    if (slew > 0) azimuthMotor.setSlew(slew); else azimuthMotor.clearSlew();
  } else if (targetAz < currentBearing.azimuth) {
    REVERSE_AZ ? azimuthMotor.setDirection(forward) :  azimuthMotor.setDirection(backward);
    if (slew > 0) azimuthMotor.setSlew(slew); else azimuthMotor.clearSlew();
  } else {
    azimuthMotor.setDirection(stop);
    azimuthMotor.clearSlew();
  }

  if(targetEl > 180) targetEl = 180;
  if(targetEl < 0) targetEl = 0;
  if(targetEl > currentBearing.elevation) {
    REVERSE_EL ? elevationMotor.setDirection(backward) : elevationMotor.setDirection(forward);
    if (slew > 0) elevationMotor.setSlew(slew); else elevationMotor.clearSlew();
  } else if (targetEl < currentBearing.elevation) {
    REVERSE_EL ? elevationMotor.setDirection(forward) : elevationMotor.setDirection(backward);
    if (slew > 0) elevationMotor.setSlew(slew); else elevationMotor.clearSlew();
  } else {
    elevationMotor.setDirection(stop);
    elevationMotor.clearSlew();
  }
}