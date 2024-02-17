/*
WiFi utilities FreeRTOS component
https://www.freertos.org/FreeRTOS-Coding-Standard-and-Style-Guide.html
*/

/* Library includes */
#include <stdio.h>

/* FreeRTOS includes */

/* Other includes */
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"

/* Defines */
#define WIFI_TAG "WIFI"
#define CWF_AP_SSID_PREFIX "detector-"
#define CWF_AP_PASS "abcde12345"
#define CWF_SSID_MAXLEN 32
#define CWF_PASSWD_MAXLEN 32
#define CWD_DEFAULT_GW "192.168.1.1"
#define CWD_DEFAULT_NETMASK "255.255.255.0"
#define CWD_CONF_PAGE_IP "192.168.1.2"

/* Types */
typedef struct WiFiDataDef_t
{
  char szWiFiStaSsid[CWF_SSID_MAXLEN];
  char szWiFiStaPass[CWF_PASSWD_MAXLEN];
} WiFiData_t;

/* Function prototypes */

void vWiFiInitAp(void* pvEvHandler, char* pcRndId);

void vWiFiInitSta(WiFiData_t* pxData, void* pvEvHandler);
