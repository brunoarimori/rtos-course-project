/*
Ultrasonic ESP32 FreeRTOS component for HC-SR04
https://www.freertos.org/FreeRTOS-Coding-Standard-and-Style-Guide.html
*/

/* Includes */
#include "ultrasonic.h"

/* Defines */

/* Static function prototypes */

// Get time in microseconds from ultrasonic trigger to return
static uint64_t prvGetUltrasonicTime(UltrasonicConfig_t* config);

// Get distance in cm from time in microseconds from ultrasonic trigger to return 
static uint16_t prvGetUltrasonicDist(uint64_t ullTimeUsParam);

/* File-scope variables */

// Stores the time in microseconds, used in prvGetUltrasonicTime
static uint64_t ullTimeUs;

/* Function definitions */
static uint64_t prvGetUltrasonicTime(UltrasonicConfig_t* config)
{
  int iEcho = 0;
  gpio_set_level(config->xTrigger, 1);
  vTaskDelay(1 / portTICK_PERIOD_MS);
  gpio_set_level(config->xTrigger, 0);
  ESP_LOGI("ULTRASONIC", "waiting echo detection");
  ullTimeUs = esp_timer_get_time();
  while (iEcho == 0)
  {
    if ((esp_timer_get_time() - ullTimeUs) > HC_SR04_TIMEOUT) iEcho = 1;
    else iEcho = gpio_get_level(config->xEcho);
  }
  ullTimeUs = esp_timer_get_time();
  ESP_LOGI("ULTRASONIC", "waiting echo to finish");
  while (iEcho != 0)
  {
    if ((esp_timer_get_time() - ullTimeUs) > HC_SR04_TIMEOUT) iEcho = 0;
    else iEcho = gpio_get_level(config->xEcho);
  }
  ullTimeUs = esp_timer_get_time() - ullTimeUs;
  ESP_LOGI("ULTRASONIC", "finished measuring");
  return ullTimeUs;
}
/*-----------------------------------------------------------*/

static uint16_t prvGetUltrasonicDist(uint64_t ullTimeUsParam)
{
  return ullTimeUsParam / HC_SR04_TIME_CONST;
}
/*-----------------------------------------------------------*/

void vInitializeUltrasonic(UltrasonicConfig_t* config)
{
  ESP_LOGI("ULTRASONIC", "TRIGGER %d, ECHO %d", config->xTrigger, config->xEcho);
  gpio_pad_select_gpio(config->xTrigger);
  gpio_set_direction(config->xTrigger, GPIO_MODE_OUTPUT);
  gpio_pad_select_gpio(config->xEcho);
  gpio_set_direction(config->xEcho, GPIO_MODE_INPUT);
}
/*-----------------------------------------------------------*/

uint16_t usGetDistance(UltrasonicConfig_t* config)
{
  uint64_t time = prvGetUltrasonicTime(config);
  return prvGetUltrasonicDist(time);
}
/*-----------------------------------------------------------*/
