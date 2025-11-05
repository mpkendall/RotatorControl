#include "motor.h"

MotorControl::MotorControl(int pin1, int pin2, int enablePin, int potPin, int minRaw, int maxRaw, int minDegree, int maxDegree)
    : _pin1(pin1), _pin2(pin2), _enablePin(enablePin), _potPin(potPin), _minRaw(minRaw), _maxRaw(maxRaw), _minDegree(minDegree), _maxDegree(maxDegree) {}

void MotorControl::begin() {
    pinMode(_pin1, OUTPUT);
    pinMode(_pin2, OUTPUT);
    pinMode(_potPin, INPUT);
    pinMode(_enablePin, OUTPUT);
    // default enable to HIGH (full power) - update() may override for PWM
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
    // disable motor output when stopped
    digitalWrite(_enablePin, LOW);
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
    return map(ADC_LUT[analogRead(_potPin)], _minRaw, _maxRaw, _minDegree, _maxDegree);
}

void MotorControl::setSlew(float degreesPerSecond) {
    if (degreesPerSecond <= 0.0f) {
        clearSlew();
        return;
    }
    _desiredSlew = degreesPerSecond;
    _slewActive = true;
    _lastMeasuredBearing = getBearing();
    _lastMeasureMillis = millis();
    _duty = 1.0f; // start full on
    _pwmCycleStartMicros = micros();
}

void MotorControl::clearSlew() {
    _slewActive = false;
    _desiredSlew = 0.0f;
    _duty = 1.0f;
    // ensure enable pin is fully on when not slew-controlling
    digitalWrite(_enablePin, HIGH);
}

// periodic update: measure speed and PWM enable pin to approximate desired degrees/sec
void MotorControl::update() {
    if (!_slewActive) return;

    unsigned long nowMillis = millis();
    // measure every ~200ms
    const unsigned long measureInterval = 200;
    if (_lastMeasureMillis == 0) {
        _lastMeasureMillis = nowMillis;
        _lastMeasuredBearing = getBearing();
    }
    if (nowMillis - _lastMeasureMillis >= measureInterval) {
        int current = getBearing();
        int diff = current - _lastMeasuredBearing;
        int range = _maxDegree - _minDegree;
        // account for wrap-around
        if (abs(diff) > range / 2) {
            if (diff > 0) diff -= range;
            else diff += range;
        }
        float dt = (nowMillis - _lastMeasureMillis) / 1000.0f;
    float speed = 0.0f;
    if (dt > 0.0f) speed = diff / dt; // degrees per second
    // store last measured speed (absolute value)
    _lastMeasuredSpeed = fabs(speed);

        // control duty using a simple proportional controller on speed magnitude
        float error = _desiredSlew - fabs(speed);
        _duty += _kp * error;
        if (_duty > 1.0f) _duty = 1.0f;
        if (_duty < 0.0f) _duty = 0.0f;

        _lastMeasuredBearing = current;
        _lastMeasureMillis = nowMillis;
    }

    // software PWM on _enablePin using micros()
    unsigned long nowMicros = micros();
    unsigned long period = _pwmPeriodMicros;
    if (_pwmCycleStartMicros == 0) _pwmCycleStartMicros = nowMicros;
    unsigned long elapsed = nowMicros - _pwmCycleStartMicros;
    if (elapsed >= period) {
        // advance cycle start by integer number of periods to avoid drift
        unsigned long cycles = elapsed / period;
        _pwmCycleStartMicros += cycles * period;
        elapsed = nowMicros - _pwmCycleStartMicros;
    }
    unsigned long onTime = (unsigned long)(_duty * (float)period);
    if (onTime == 0) {
        digitalWrite(_enablePin, LOW);
    } else if (onTime >= period) {
        digitalWrite(_enablePin, HIGH);
    } else {
        if (elapsed < onTime) digitalWrite(_enablePin, HIGH);
        else digitalWrite(_enablePin, LOW);
    }
}

float MotorControl::getSpeed() {
    return _lastMeasuredSpeed;
}