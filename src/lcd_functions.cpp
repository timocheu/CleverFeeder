#include "lcd_functions.h"

void setupLCD(LiquidCrystal_I2C &LCD_SCREEN) {
  // Initialize Movement text
  LCD_SCREEN.print("Movement: "); // 10 chars

  // Initialize Distance text
  LCD_SCREEN.setCursor(0,1);
  LCD_SCREEN.print("Food Value: "); // 12 chars
}

void lcdUpdateMovement(LiquidCrystal_I2C &LCD_SCREEN, bool catNearby) {
  // -- Movement
  LCD_SCREEN.setCursor(10, 0);
  LCD_SCREEN.print((catNearby) ? "True " : "False");
}

void lcdUpdateFood(LiquidCrystal_I2C &LCD_SCREEN, int foodPercentage) {
  // -- Food Value
  LCD_SCREEN.setCursor(12, 1);
  // food value: 0%  
  if (foodPercentage < 10) {
    // 1 Digit
    LCD_SCREEN.print(foodPercentage + "%  ");
  } else if (foodPercentage < 99) {
    // 2 Digit
    LCD_SCREEN.print(foodPercentage + "% ");
  } else {
    // 3 Digit
    LCD_SCREEN.print(foodPercentage + "%");
  }
}

void lcdUpdateAll(LiquidCrystal_I2C &LCD_SCREEN, bool catNearby, int foodPercentage) {
  // -- Movement
  LCD_SCREEN.setCursor(10, 0);
  LCD_SCREEN.print((catNearby) ? "True " : "False");

  // -- Food Value
  LCD_SCREEN.setCursor(12, 1);
  // food value: 0%  
  if (foodPercentage < 10) {
    // 1 Digit
    LCD_SCREEN.print(foodPercentage + "%  ");
  } else if (foodPercentage < 99) {
    // 2 Digit
    LCD_SCREEN.print(foodPercentage + "% ");
  } else {
    // 3 Digit
    LCD_SCREEN.print(foodPercentage + "%");
  }
}
