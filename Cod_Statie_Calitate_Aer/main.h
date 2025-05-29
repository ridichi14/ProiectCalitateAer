/**
 * @file main.h
 * @author Modified for RAK12039 particle sensor + OLED display on RAK1900_VB
 * @brief Includes, definitions and global declarations for RAK12039 + DeepSleep LoRaWAN + OLED display
 * @version 1.1
 * @date 2025
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRaWan-RAK4630.h>
#include "Adafruit_PM25AQI.h"
#include <Adafruit_SSD1306.h>

// Comment the next line if you want DEBUG output. But the power savings are not as good then!
// #define MAX_SAVE

/* Time the device is sleeping in milliseconds = 1 minute * 60 seconds * 1000 milliseconds */
#define SLEEP_TIME 1 * 60 * 1000

// LoRaWAN application port
#define LORAWAN_APP_PORT 2

// OLED display configuration
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET    -1  // Reset pin # (or -1 if sharing Arduino reset pin)

// Particle data structure
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

// LoRaWan stuff
int8_t initLoRaWan(void);
bool sendLoRaFrame(void);

// Particle sensor functions
void setup_particle_sensor(void);
bool read_particle_data(void);

// OLED display functions
void setup_oled_display(void);
void display_particle_data(void);

// Main loop stuff
void periodicWakeup(TimerHandle_t unused);
extern SemaphoreHandle_t taskEvent;
extern uint8_t rcvdLoRaData[];
extern uint8_t rcvdDataLen;
extern uint8_t eventType;
extern SoftwareTimer taskWakeupTimer;

// Particle sensor globals
extern Adafruit_PM25AQI aqi;
extern particle_data_s particle_data;

// OLED display global
extern Adafruit_SSD1306 display;
