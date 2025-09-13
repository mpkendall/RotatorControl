#pragma once
#include <Arduino.h>

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
    MotorControl(int pin1, int pin2, int enablePin, int potPin);

    void begin();
    void forward();
    void backward();
    void stop();
    void setDirection(Direction direction);
    int getBearing(int max);

private:
    int _pin1, _pin2, _enablePin, _potPin;
};