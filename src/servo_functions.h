#ifndef SERVO_FUNCTIONS_H
#define SERVO_FUNCTIONS_H

#include <Arduino.h>
#include <ESP32Servo.h>

void openFeeder(Servo &servol);
void closeFeeder(Servo &servol);

#endif
