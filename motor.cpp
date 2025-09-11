#include "motor.h"

MotorControl::MotorControl(int pin1, int pin2, int enablePin, int potPin)
    : _pin1(pin1), _pin2(pin2), _enablePin(enablePin), _potPin(potPin) {}

void MotorControl::begin() {
    pinMode(_pin1, OUTPUT);
    pinMode(_pin2, OUTPUT);
    pinMode(_potPin, INPUT);
    pinMode(_enablePin, OUTPUT);
    digitalWrite(_enablePin, HIGH);
    stop();
}

void MotorControl::forward() {
    digitalWrite(_pin1, HIGH);
    digitalWrite(_pin2, LOW);
}

void MotorControl::backward() {
    digitalWrite(_pin1, LOW);
    digitalWrite(_pin2, HIGH);
}

void MotorControl::stop() {
    digitalWrite(_pin1, LOW);
    digitalWrite(_pin2, LOW);
}

void MotorControl::setDirection(Direction direction) {
    switch (direction) {
        case 0:
            forward();
            break;
        case 1:
            backward();
            break;
        case 2:
            stop();
            break;
    }
}

int MotorControl::getBearing() {
    return analogRead(_potPin);
}