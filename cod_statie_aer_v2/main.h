#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRaWan-RAK4630.h>
#include <Adafruit_PM25AQI.h>
#include <Adafruit_SSD1306.h>

// ============ DEBUG MODE ============
// #define MAX_SAVE  // Uncomment to disable serial output

// ============ LoRaWAN ============
#define SLEEP_TIME (1 * 60 * 1000)  // 1 minute in milliseconds
#define LORAWAN_APP_PORT 2

// ============ DISPLAY ============
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// ============ PARTICLE DATA STRUCTURE ============
struct particle_data_s {
    uint16_t pm10_standard;
    uint16_t pm25_standard;
    uint16_t pm100_standard;
    uint16_t pm10_env;
    uint16_t pm25_env;
    uint16_t pm100_env;
    uint16_t particles_03um;
    uint16_t particles_05um;
    uint16_t particles_10um;
    uint16_t particles_25um;
    uint16_t particles_50um;
    uint16_t particles_100um;
};

// ============ GLOBAL DECLARATIONS ============
extern Adafruit_PM25AQI aqi;
extern particle_data_s particle_data;
extern Adafruit_SSD1306 display;

extern SemaphoreHandle_t taskEvent;
extern uint8_t rcvdLoRaData[256];
extern uint8_t rcvdDataLen;
extern uint8_t eventType;
extern SoftwareTimer taskWakeupTimer;

// ============ FUNCTION DECLARATIONS ============
// Initialization
int8_t initLoRaWan(void);
bool setup_particle_sensor(void);
void setup_oled_display(void);

// Data reading
bool read_particle_data(void);
bool sendLoRaFrame(void);

// Display
void display_particle_data(void);
void display_status(const char* status);

// Utilities
void i2c_scan_and_print(void);
void periodicWakeup(TimerHandle_t unused);