#include "motion_functions.h"

int detectMotion(int MOTION_PIN) {
  int val = digitalRead(MOTION_PIN);

  Serial.print("state: ");
  Serial.print(String(val) + " ");

  return val;
}
