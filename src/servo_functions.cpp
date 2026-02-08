#include "servo_functions.h"

void openFeeder(Servo &servol) {
    int currentDegrees = servol.read();
    // Make it divisible by 10, by ceiling 
    // Ex. 4 -> 10, 14 -> 20
    currentDegrees += 10 - (currentDegrees % 10);

    for (int i = currentDegrees; i <= 180; i += 10) {
        servol.write(i);
    }
}

void closeFeeder(Servo &servol) {
    int currentDegrees = servol.read();
    // Make it divisible by 10, by flooring
    // Ex. 4 -> 0, 14 -> 10
    currentDegrees -= (currentDegrees % 10);

    for (int i = currentDegrees; i >= 0; i -= 10) {
        servol.write(i);
    }
}
