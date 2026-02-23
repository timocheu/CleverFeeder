#ifndef LCD_FUNCTIONS_H
#define LCD_FUNCTIONS_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

void setupLCD(LiquidCrystal_I2C &LCD_SCREEN);
void lcdUpdateMovement(LiquidCrystal_I2C &LCD_SCREEN, int movementState);
void lcdUpdateFood(LiquidCrystal_I2C &LCD_SCREEN, int foodLevelPercentage);
void lcdUpdateFood(LiquidCrystal_I2C &LCD_SCREEN, bool catNearby, int foodLevelPercentage);

#endif
