#include <Arduino.h>
#include <unity.h>

// Mock constants for testing
#define TEST_CAPACITY_HEIGHT 100.0

// Function to test (Logic extracted from your updateFoodLevel)
int calculatePercentage(float distance) {
    float constrainedDist = constrain(distance, 0, TEST_CAPACITY_HEIGHT);
    return ((TEST_CAPACITY_HEIGHT - constrainedDist) * 100) / TEST_CAPACITY_HEIGHT;
}

// Test 1: Full Tank
void test_percentage_full(void) {
    // If distance is 0, tank is 100% full
    TEST_ASSERT_EQUAL_INT(100, calculatePercentage(0));
}

// Test 2: Half Tank
void test_percentage_half(void) {
    // If distance is 50mm, tank is 50% full
    TEST_ASSERT_EQUAL_INT(50, calculatePercentage(50));
}

// Test 3: Empty Tank (or over limit)
void test_percentage_empty(void) {
    // If distance is 100mm (or 120mm due to noise), tank is 0%
    TEST_ASSERT_EQUAL_INT(0, calculatePercentage(100));
    TEST_ASSERT_EQUAL_INT(0, calculatePercentage(150)); 
}

// Test 4: Feeding Window Logic
void test_feeding_window(void) {
    int lastFeed = 8;
    int currentHour = 12;
    int interval = 4;
    
    // Logic: (currentHour - lastFeed >= interval)
    TEST_ASSERT_TRUE(currentHour - lastFeed >= interval);
}

void setup() {
    delay(2000); // Service delay for PlatformIO Serial
    UNITY_BEGIN();
    
    RUN_TEST(test_percentage_full);
    RUN_TEST(test_percentage_half);
    RUN_TEST(test_percentage_empty);
    RUN_TEST(test_feeding_window);

    UNITY_END();
}

void loop() {
    // Nothing to do here
}