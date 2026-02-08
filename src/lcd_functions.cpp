#include "lcd_functions.h"

void setupLCD(LiquidCrystal_I2C &LCD_SCREEN) {
  LCD_SCREEN.begin(16,2);

  // Initialize Movement text
  LCD_SCREEN.print("Movement: "); // 10 chars

  // Initialize Distance text
  LCD_SCREEN.setCursor(0,1);
  LCD_SCREEN.print("Food Value: "); // 12 chars
}

void updateMovement(LiquidCrystal_I2C &LCD_SCREEN, int movementState) {
  LCD_SCREEN.setCursor(10, 0);
  LCD_SCREEN.print((movementState) ? "True " : "False");
}

void updateFoodLevel(LiquidCrystal_I2C &LCD_SCREEN, int foodPercentage) {
  LCD_SCREEN.setCursor(12, 1);

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
