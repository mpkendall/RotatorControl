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

private:
    int _pin1, _pin2, _enablePin, _potPin, _minRaw, _maxRaw, _minDegree, _maxDegree;
};