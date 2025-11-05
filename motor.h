#pragma once
#include <Arduino.h>
#include "config.h"

struct Bearing {
  int azimuth;
  int elevation;
};

enum Direction {
    forward = 0,
    backward,
    stop
};

class MotorControl
{
public:
    MotorControl(int pin1, int pin2, int enablePin, int potPin, int minRaw, int maxRaw, int minDegree, int maxDegree);

    void begin();
    void forward();
    void backward();
    void stop();
    void setDirection(Direction direction);
    int getBearing();
    float getSpeed();
    // Slew control (degrees per second). Call setSlew to enable closed-loop speed control.
    // Pass <= 0 to disable and run at full speed.
    void setSlew(float degreesPerSecond);
    void clearSlew();
    // Call periodically from the main loop to run the slew controller and PWM the enable pin.
    void update();

private:
    int _pin1, _pin2, _enablePin, _potPin, _minRaw, _maxRaw, _minDegree, _maxDegree;
    // slew control state
    float _desiredSlew = 0.0f; // degrees per second
    bool _slewActive = false;
    int _lastMeasuredBearing = 0;
    unsigned long _lastMeasureMillis = 0;
    float _duty = 1.0f; // 0..1
    unsigned long _pwmPeriodMicros = 20000; // 20ms default PWM period
    unsigned long _pwmCycleStartMicros = 0;
    // simple proportional gain for duty adjustment
    float _kp = 0.05f;
    // last measured speed (degrees per second), positive value
    float _lastMeasuredSpeed = 0.0f;
};