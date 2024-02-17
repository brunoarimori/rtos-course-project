/*
Ultrasonic ESP32 FreeRTOS component for HC-SR04
https://www.freertos.org/FreeRTOS-Coding-Standard-and-Style-Guide.html
*/

/* Library includes */
#include <stdio.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Other includes */
#include "driver/gpio.h"
#include "esp_log.h"

/* Defines */

// Divide time in microseconds by this number to get cm
#define HC_SR04_TIME_CONST (58)

// Max distance in cm
#define HC_SR04_MAX_DIST (300)

// Timeout in microseconds 
#define HC_SR04_TIMEOUT (HC_SR04_MAX_DIST * HC_SR04_TIME_CONST)

/* Types */

typedef struct UltrasonicConfigDef_t 
{
  gpio_num_t xTrigger;
  gpio_num_t xEcho;
} UltrasonicConfig_t;

typedef struct UltrasonicDataDef_t
{
  uint16_t iMinTrigCm;
  uint16_t iMaxTrigCm;
} UltrasonicData_t;

/* Function prototypes */

// todo: describe 
void vInitializeUltrasonic(UltrasonicConfig_t* config);

// todo: describe 
uint16_t usGetDistance(UltrasonicConfig_t* config);
