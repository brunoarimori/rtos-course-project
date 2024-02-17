/*
WiFi utilities FreeRTOS component
https://www.freertos.org/FreeRTOS-Coding-Standard-and-Style-Guide.html
*/

/* Library includes */
#include <stdio.h>

/* FreeRTOS includes */

/* Other includes */
#include "esp_log.h"
#include "mqtt_client.h"

/* Defines */
#define CMQTT_URI_MAXLEN 255
#define CMQTT_USER_MAXLEN 32
#define CMQTT_PASS_MAXLEN 32

/* Types */
typedef struct MqttDataDef_t
{
  char szMqttUri[CMQTT_URI_MAXLEN];
  char szMqttUser[CMQTT_USER_MAXLEN];
  char szMqttPass[CMQTT_PASS_MAXLEN];
} MqttData_t;

/* Function prototypes */

void vMqttInit(MqttData_t* xMqttConfig, void* pvMqttEvHandler);
int iMqttPublish(char* pcTopic, char* pcData, size_t pcDataSize);
