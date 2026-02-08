#ifndef LCD_FUNCTIONS_H
#define LCD_FUNCTIONS_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

void setupLCD(LiquidCrystal_I2C &LCD_SCREEN);
void updateMovement(LiquidCrystal_I2C &LCD_SCREEN, int movementState);
void updateFoodLevel(LiquidCrystal_I2C &LCD_SCREEN, int foodLevelPercentage);

#endif
