/* Library includes */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

/* Other includes */
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"

#include "ultrasonic.h"
#include "cwifi.h"
#include "cmqtt.h"

/* Defines */
// #define LED GPIO_NUM_2
#define BUTTON GPIO_NUM_0
#define TRIG GPIO_NUM_21
#define ECHO GPIO_NUM_22

#define CONFIG_BIT BIT0
#define HTTP_SERVER_BIT BIT1
#define WIFI_STA_CONN_BIT BIT8
#define MQTT_CONN_BIT BIT9
#define MAIN_TASK_BIT BIT15
#define RESET_NVS_BIT BIT2
#define RECV_CONFIG_BIT BIT10

#define NVS_NAMESPACE "main_namespace"
#define NVS_DVC_CONFIGURED_KEY "dvc_configured"
#define NVS_WIFI_SSID_KEY "wifi_ssid"
#define NVS_WIFI_PASSWD_KEY "wifi_passwd"
#define NVS_MQTT_URI_KEY "mqtt_uri"
#define NVS_MQTT_USER_KEY "mqtt_user"
#define NVS_MQTT_PASSWD_KEY "mqtt_passwd"
#define NVS_DIST_MINTRIG_KEY "dist_mintrig"
#define NVS_DIST_MAXTRIG_KEY "dist_maxtrig"

// HTTP todo: module
#define POST_CONTENT_MAXLEN 255

#define LOG_TAG "MAIN"

/* Types */

typedef struct MainConfigDef_t
{
  uint8_t iConfigSet;
  MqttData_t xMqttData;
  WiFiData_t xWifiData;
  UltrasonicData_t xUltrasonicData;
} MainConfig_t;

/* Static function prototypes */
static void prvConfigTask(void *pvParameter);
static void prvHttpServerTask(void *pvParameter);
static void prvRunMainTask(void *pvParameter);
static void prvMqttInitTask(void *pvParameter);
static void prvResetTask(void *pvParameter);
static void prvPublishTask(void *pvParameter);
static void prvDistanceTask(void *pvParameter);
static void prvSaveConfigTask(void *pvParameter);

static void prvNvsReadConfig(nvs_handle_t iHandle, MainConfig_t* pxConfig);
static void prvNvsWriteConfig(nvs_handle_t iHandle, MainConfig_t* pxConfig);
static void prvNvsClearConfig(nvs_handle_t iHandle);
static void prvWifiEventHandler(void* pvArg, esp_event_base_t xEventBase, int32_t iEventId, void* pvEventData);
static void prvMqttEvHandler(void *pvHandlerArgs, esp_event_base_t pcBase, int32_t iEvId, esp_mqtt_event_handle_t pxEvData);
static void prvGpioIsrHandler(void *pvParameter);
static void prvParseConfigPost(MainConfig_t* pxConfig, char* szPostContent);
static void prvConfigureButton();

static esp_err_t prvHttpGetHandler(httpd_req_t* pxReq);
static esp_err_t prvHttpPostHandler(httpd_req_t* pxReq);

/* File-scope variables */
EventGroupHandle_t xMainEvGrp;
QueueHandle_t xTriggerQueue;

UltrasonicConfig_t xUltrasonicConfig;
MainConfig_t xConfig;

// HTTP todo: module
httpd_uri_t xUriGet = {
  .uri = "/",
  .method = HTTP_GET,
  .handler = prvHttpGetHandler,
  .user_ctx = NULL,
};
httpd_uri_t xUriPost = {
  .uri = "/conf",
  .method = HTTP_POST,
  .handler = prvHttpPostHandler,
  .user_ctx = NULL,
};

/* Function definitions */

static void prvWifiEventHandler(void* pvArg, esp_event_base_t xEventBase, int32_t iEventId, void* pvEventData)
{
  ESP_LOGI(LOG_TAG, "event handler triggered!");
  if (xEventBase == WIFI_EVENT)
  {
    switch (iEventId) {
      // Station
      case WIFI_EVENT_STA_START:
        ESP_LOGI(LOG_TAG, "received WIFI_EVENT_STA_START, connecting");
        esp_wifi_connect();
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(LOG_TAG, "received WIFI_EVENT_STA_DISCONNECTED, reconnecting");
        xEventGroupClearBits(xMainEvGrp, WIFI_STA_CONN_BIT);
        esp_wifi_connect();
        break;
      // Access point
      case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(LOG_TAG, "received WIFI_EVENT_AP_STACONNECTED");
        break;
      case WIFI_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(LOG_TAG, "received WIFI_EVENT_AP_STADISCONNECTED");
        break;
      default:
        break;
    }
    return;
  }
  if (xEventBase == IP_EVENT)
  {
    switch (iEventId) {
      // Station
      case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(LOG_TAG, "received IP_EVENT_STA_GOT_IP");
        xEventGroupSetBits(xMainEvGrp, WIFI_STA_CONN_BIT);
        break;
      case IP_EVENT_STA_LOST_IP:
        xEventGroupClearBits(xMainEvGrp, WIFI_STA_CONN_BIT);
        break;
      // Access Point 
      case IP_EVENT_AP_STAIPASSIGNED:
        ESP_LOGI(LOG_TAG, "received IP_EVENT_AP_STAIPASSIGNED");
        break;
      default:
        break;
    }
  }
}
/*-----------------------------------------------------------*/

// HTTP

static esp_err_t prvHttpGetHandler(httpd_req_t* pxReq)
{
  const char szResp[] = " \
    <!DOCTYPE html> \
    <html> \
    <body> \
    <h2>SOTR PROJETO FINAL</h2> \
    <form action=\"/conf\" method=\"post\"> \
      <label for=\"wifissid\">SSID:</label><br> \
      <input type=\"text\" id=\"wifissid\" name=\"wifissid\" value=\"DefaultSSID\"><br> \
      <label for=\"wifipassword\">SENHA:</label><br> \
      <input type=\"text\" id=\"wifipasswd\" name=\"wifipasswd\" value=\"DefaultPasswd\"><br> \
      <label for=\"mqtturi\">IP MQTT:</label><br> \
      <input type=\"text\" id=\"mqtturi\" name=\"mqtturi\" value=\"0.0.0.0\"><br> \
      <label for=\"mqttport\">PORTA MQTT:</label><br> \
      <input type=\"number\" id=\"mqttport\" name=\"mqttport\" value=\"1883\"><br> \
      <label for=\"mintrig\">DISTANCIA MINIMA EM CM PARA DISPARO DE MSG:</label><br> \
      <input type=\"number\" id=\"mintrig\" name=\"mintrig\" value=\"10\"><br> \
      <label for=\"maxtrig\">DISTANCIA MAXIMA EM CM PARA DISPARO DE MSG:</label><br> \
      <input type=\"number\" id=\"maxtrig\" name=\"maxtrig\" value=\"30\"><br> \
      <input type=\"submit\" value=\"Submit\"> \
    </form>  \
    </body> \
    </html> \
    ";
  ESP_ERROR_CHECK(httpd_resp_send(pxReq, szResp, HTTPD_RESP_USE_STRLEN));
  return ESP_OK;
}
/*-----------------------------------------------------------*/

static esp_err_t prvHttpPostHandler(httpd_req_t* pxReq)
{
  char pcPostContent[POST_CONTENT_MAXLEN];
  int iRet = 0;
  if (pxReq->content_len > POST_CONTENT_MAXLEN)
  {
    ESP_LOGI(LOG_TAG, "post length is %u, max allowed is %d", pxReq->content_len, POST_CONTENT_MAXLEN);
    httpd_resp_send_500(pxReq);
    return ESP_FAIL;
  }
  iRet = httpd_req_recv(pxReq, pcPostContent, pxReq->content_len);
  if (iRet <= 0)
  {
    if (iRet == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(pxReq);
    ESP_LOGI(LOG_TAG, "httpd_req_recv failed: %d", iRet);
    return ESP_FAIL;
  }

  prvParseConfigPost(&xConfig, pcPostContent);
  xEventGroupSetBits(xMainEvGrp, RECV_CONFIG_BIT);

  const char resp[] = "POST OK";
  ESP_LOGI(LOG_TAG, "URI POST Response");
  ESP_ERROR_CHECK(httpd_resp_send(pxReq, resp, HTTPD_RESP_USE_STRLEN));
  return ESP_OK;
}
/*-----------------------------------------------------------*/

static void prvParseConfigPost(MainConfig_t* pxConfig, char* szPostContent)
{
  const char szItemDelim[2] = "&";
  const char szKeyDelim[2] = "=";
  char *szItemToken;
  char *szKeyToken;
  int iItemCount = 0;
  int iKeyCount = 0;
  int iCounter = 0;
  char *rgItemArr[10] = {0};
  char *rgKeyArr[2] = {0};

  char szMqttUriWithoutScheme[128] = {0};
  uint16_t usMqttPort = 0;

  szItemToken = strtok(szPostContent, szItemDelim);
  while (szItemToken != NULL)
  {
    rgItemArr[iItemCount++] = szItemToken;
    szItemToken = strtok(NULL, szItemDelim);
  }

  for (iCounter = 0; iCounter < iItemCount; iCounter++)
  {
    ESP_LOGI(LOG_TAG, "parsing item: %s", rgItemArr[iCounter]);
    szKeyToken = strtok(rgItemArr[iCounter], szKeyDelim);
    while (szKeyToken != NULL)
    {
      rgKeyArr[iKeyCount++] = szKeyToken;
      szKeyToken = strtok(NULL, szKeyDelim);
    }
    iKeyCount = 0;
    ESP_LOGI(LOG_TAG, "key 0: %s", rgKeyArr[0]);
    ESP_LOGI(LOG_TAG, "key 1: %s", rgKeyArr[1]);
    if (strcmp("wifissid", rgKeyArr[0]) == 0)
      strcpy(xConfig.xWifiData.szWiFiStaSsid, rgKeyArr[1]);
    if (strcmp("wifipasswd", rgKeyArr[0]) == 0)
      strcpy(xConfig.xWifiData.szWiFiStaPass, rgKeyArr[1]);
    if (strcmp("mqtturi", rgKeyArr[0]) == 0)
      strcpy(szMqttUriWithoutScheme, rgKeyArr[1]);
    if (strcmp("mqttport", rgKeyArr[0]) == 0)
      usMqttPort = atoi(rgKeyArr[1]);
    if (strcmp("mintrig", rgKeyArr[0]) == 0)
      xConfig.xUltrasonicData.iMinTrigCm = atoi(rgKeyArr[1]);
    if (strcmp("maxtrig", rgKeyArr[0]) == 0)
      xConfig.xUltrasonicData.iMaxTrigCm = atoi(rgKeyArr[1]);
  }
  memset(rgKeyArr, 0, sizeof(rgKeyArr));
  snprintf(xConfig.xMqttData.szMqttUri, 255, "mqtt://%s:%d", szMqttUriWithoutScheme, usMqttPort);
  strncpy(xConfig.xMqttData.szMqttUser, "esp32", 6);
  strncpy(xConfig.xMqttData.szMqttPass, "esp32", 6);
  xConfig.iConfigSet = 1;

  xEventGroupSetBits(xMainEvGrp, RECV_CONFIG_BIT);
  ESP_LOGI(LOG_TAG, "wifi ssid: %s", xConfig.xWifiData.szWiFiStaSsid);
  ESP_LOGI(LOG_TAG, "wifi passwd: %s", xConfig.xWifiData.szWiFiStaPass);
  ESP_LOGI(LOG_TAG, "mqtt uri: %s", xConfig.xMqttData.szMqttUri);
}
/*-----------------------------------------------------------*/

// NVS

static void prvNvsReadConfig(nvs_handle_t iHandle, MainConfig_t* pxConfig)
{
  esp_err_t iNvsErr; 
  uint8_t iDvcConfigured;
  char szWifiSsid[CWF_SSID_MAXLEN];
  char szWifiPasswd[CWF_PASSWD_MAXLEN];
  char szMqttUri[CMQTT_URI_MAXLEN];
  char szMqttUser[CMQTT_USER_MAXLEN];
  char szMqttPasswd[CMQTT_PASS_MAXLEN];
  uint16_t iDistMinTrig;
  uint16_t iDistMaxTrig;
  size_t xBufLenRead;

  // Read configured
  iNvsErr = nvs_get_u8(iHandle, NVS_DVC_CONFIGURED_KEY, &iDvcConfigured);
  ESP_LOGI(LOG_TAG, "---> %d", iDvcConfigured);
  if (iNvsErr == ESP_ERR_NVS_NOT_FOUND) return;
  else ESP_ERROR_CHECK(iNvsErr);

  xBufLenRead = sizeof(szWifiSsid);
  ESP_ERROR_CHECK(nvs_get_str(iHandle, NVS_WIFI_SSID_KEY, szWifiSsid, &xBufLenRead));
  xBufLenRead = sizeof(szWifiPasswd);
  ESP_ERROR_CHECK(nvs_get_str(iHandle, NVS_WIFI_PASSWD_KEY, szWifiPasswd, &xBufLenRead));
  xBufLenRead = sizeof(szMqttUri);
  ESP_ERROR_CHECK(nvs_get_str(iHandle, NVS_MQTT_URI_KEY, szMqttUri, &xBufLenRead));
  xBufLenRead = sizeof(szMqttUser);
  ESP_ERROR_CHECK(nvs_get_str(iHandle, NVS_MQTT_USER_KEY, szMqttUser, &xBufLenRead));
  xBufLenRead = sizeof(szMqttPasswd);
  ESP_ERROR_CHECK(nvs_get_str(iHandle, NVS_MQTT_PASSWD_KEY, szMqttPasswd, &xBufLenRead));
  ESP_ERROR_CHECK(nvs_get_u16(iHandle, NVS_DIST_MINTRIG_KEY, &iDistMinTrig));
  ESP_ERROR_CHECK(nvs_get_u16(iHandle, NVS_DIST_MAXTRIG_KEY, &iDistMaxTrig));

  pxConfig->iConfigSet = 1;

  strcpy(pxConfig->xWifiData.szWiFiStaSsid, szWifiSsid);
  strcpy(pxConfig->xWifiData.szWiFiStaPass, szWifiPasswd);
  strcpy(pxConfig->xMqttData.szMqttUri, szMqttUri);
  strcpy(pxConfig->xMqttData.szMqttUser, szMqttUser);
  strcpy(pxConfig->xMqttData.szMqttPass, szMqttPasswd);
  pxConfig->xUltrasonicData.iMaxTrigCm = iDistMaxTrig;
  pxConfig->xUltrasonicData.iMinTrigCm = iDistMinTrig;
}
/*-----------------------------------------------------------*/

static void prvNvsWriteConfig(nvs_handle_t iHandle, MainConfig_t* pxConfig)
{
  ESP_ERROR_CHECK(nvs_set_u8(iHandle, NVS_DVC_CONFIGURED_KEY, pxConfig->iConfigSet));
  ESP_ERROR_CHECK(nvs_set_str(iHandle, NVS_WIFI_SSID_KEY, pxConfig->xWifiData.szWiFiStaSsid));
  ESP_ERROR_CHECK(nvs_set_str(iHandle, NVS_WIFI_PASSWD_KEY, pxConfig->xWifiData.szWiFiStaPass));
  ESP_ERROR_CHECK(nvs_set_str(iHandle, NVS_MQTT_URI_KEY, pxConfig->xMqttData.szMqttUri));
  ESP_ERROR_CHECK(nvs_set_str(iHandle, NVS_MQTT_USER_KEY, pxConfig->xMqttData.szMqttUser));
  ESP_ERROR_CHECK(nvs_set_str(iHandle, NVS_MQTT_PASSWD_KEY, pxConfig->xMqttData.szMqttPass));
  ESP_ERROR_CHECK(nvs_set_u16(iHandle, NVS_DIST_MAXTRIG_KEY, pxConfig->xUltrasonicData.iMaxTrigCm));
  ESP_ERROR_CHECK(nvs_set_u16(iHandle, NVS_DIST_MINTRIG_KEY, pxConfig->xUltrasonicData.iMinTrigCm));
}
/*-----------------------------------------------------------*/

static void prvNvsClearConfig(nvs_handle_t iHandle)
{
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_DVC_CONFIGURED_KEY));
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_WIFI_SSID_KEY));
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_WIFI_PASSWD_KEY));
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_MQTT_USER_KEY));
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_MQTT_PASSWD_KEY));
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_MQTT_URI_KEY));
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_DIST_MAXTRIG_KEY));
  ESP_ERROR_CHECK(nvs_erase_key(iHandle, NVS_DIST_MINTRIG_KEY));
}
/*-----------------------------------------------------------*/

static void prvGpioIsrHandler(void *pvParameter)
{
  BaseType_t xHigherPriorityTaskWoken, xResult;
  if (gpio_get_level(BUTTON) == 0) {
    xHigherPriorityTaskWoken = pdFALSE;
    xResult = xEventGroupSetBitsFromISR(xMainEvGrp, RESET_NVS_BIT, &xHigherPriorityTaskWoken);
    if (xResult != pdFAIL)
    {
      // Switch context
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}
/*-----------------------------------------------------------*/

static void prvConfigureButton()
{
  gpio_config_t xButtonConf = {
    .intr_type = GPIO_INTR_NEGEDGE, // Trigger on falling edge
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = (1ULL << BUTTON),
    .pull_up_en = GPIO_PULLUP_ENABLE, // Enable pullup resistor
    .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pulldown resistor
  };

  ESP_ERROR_CHECK(gpio_config(&xButtonConf));

  // Initialize interrupt service without interrupt level configuration
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Add interrupt
  ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON, prvGpioIsrHandler, NULL));
}
/*-----------------------------------------------------------*/

// MQTT

static void prvMqttEvHandler(void *pvHandlerArgs, esp_event_base_t pcBase, int32_t iEvId, esp_mqtt_event_handle_t pxEvData)
{
  switch (pxEvData->event_id)
  {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(LOG_TAG, "MQTT_EVENT_CONNECTED");
      xEventGroupSetBits(xMainEvGrp, MQTT_CONN_BIT);
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(LOG_TAG, "MQTT_EVENT_DISCONNECTED");
      xEventGroupClearBits(xMainEvGrp, MQTT_CONN_BIT);
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGI(LOG_TAG, "MQTT_EVENT_PUBLISHED, msg_id: %d", pxEvData->msg_id);
      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGI(LOG_TAG, "MQTT_EVENT_ERROR");
      xEventGroupClearBits(xMainEvGrp, MQTT_CONN_BIT);
      break;
    default:
      break;
  }
}

// TASKS

static void prvConfigTask(void *pvParameter)
{
  nvs_handle iNvsHandle;

  while (true)
  {
    xEventGroupWaitBits(xMainEvGrp, CONFIG_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(LOG_TAG, "RUNNING prvConfigTask");
    xConfig = (MainConfig_t){0};

    // Configure button interrupt
    prvConfigureButton();

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &iNvsHandle));
    prvNvsReadConfig(iNvsHandle, &xConfig);
    nvs_close(iNvsHandle);

    if (xConfig.iConfigSet)
    {
      xEventGroupSetBits(xMainEvGrp, MAIN_TASK_BIT);
      vTaskDelete(NULL);
      return;
    }
    vWiFiInitAp(&prvWifiEventHandler, "RdSdsA");
    vTaskDelay(100 / portTICK_PERIOD_MS);
    xEventGroupSetBits(xMainEvGrp, HTTP_SERVER_BIT);
    vTaskDelete(NULL);
  }
  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

static void prvHttpServerTask(void *pvParameter)
{
  while (true)
  {
    xEventGroupWaitBits(xMainEvGrp, HTTP_SERVER_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(LOG_TAG, "RUNNING prvHttpServerTask");

    /*
    // mock fill config
    nvs_handle iNvsHandle;
    xConfig.iConfigSet = 1;
    strcpy(xConfig.xWifiData.szWiFiStaSsid, "------------");
    strcpy(xConfig.xWifiData.szWiFiStaPass, "------------");
    strcpy(xConfig.xMqttData.szMqttPass, "passwd");
    strcpy(xConfig.xMqttData.szMqttUser, "esp32");
    strcpy(xConfig.xMqttData.szMqttUri, "mqtt://X.X.X.X");
    xConfig.xUltrasonicData.iMinTrigCm = 10;
    xConfig.xUltrasonicData.iMaxTrigCm = 20;

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &iNvsHandle));
    prvNvsWriteConfig(iNvsHandle, &xConfig);
    ESP_ERROR_CHECK(nvs_commit(iNvsHandle));
    nvs_close(iNvsHandle);

    ESP_LOGI(LOG_TAG, "WROTE CONFIG ON NVS, REBOOTING");
    esp_restart();
    vTaskDelete(NULL);
    return;
    */

    httpd_config_t xHttpConfig = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t xServerHandle = NULL;
    ESP_ERROR_CHECK(httpd_start(&xServerHandle, &xHttpConfig));
    ESP_ERROR_CHECK(httpd_register_uri_handler(xServerHandle, &xUriGet));
    ESP_ERROR_CHECK(httpd_register_uri_handler(xServerHandle, &xUriPost));
    vTaskDelete(NULL);
    return;     
  }
  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

static void prvRunMainTask(void *pvParameter)
{
  while (true)
  {
    xEventGroupWaitBits(xMainEvGrp, MAIN_TASK_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(LOG_TAG, "RUNNING MainTask");
    vWiFiInitSta(&xConfig.xWifiData, &prvWifiEventHandler);
    vTaskDelete(NULL);
    return;
  }

  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

static void prvMqttInitTask(void *pvParameter)
{
  while (true)
  {
    xEventGroupWaitBits(xMainEvGrp, WIFI_STA_CONN_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(LOG_TAG, "RUNNING MqttInitTask");
    vMqttInit(&xConfig.xMqttData, &prvMqttEvHandler); 
    vTaskDelete(NULL);
    return;
  }
  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

static void prvSaveConfigTask(void *pvParameter)
{
  nvs_handle iNvsHandle;
  while (true)
  {
    xEventGroupWaitBits(xMainEvGrp, RECV_CONFIG_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &iNvsHandle));
    prvNvsWriteConfig(iNvsHandle, &xConfig);
    ESP_ERROR_CHECK(nvs_commit(iNvsHandle));
    nvs_close(iNvsHandle);
    ESP_LOGI(LOG_TAG, "WROTE CONFIG ON NVS, REBOOTING");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

static void prvPublishTask(void *pvParameter)
{
  uint16_t usDistCm = 0;
  char usDistCmStr[10] = {0};
  while (true)
  {
    // xEventGroupWaitBits(xMainEvGrp, MQTT_CONN_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    xQueueReceive(xTriggerQueue, &usDistCm, portMAX_DELAY);
    snprintf(usDistCmStr, 10, "%hu", usDistCm);
    iMqttPublish("deviceid/dist", usDistCmStr, sizeof(usDistCmStr));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

static void prvResetTask(void *pvParameter)
{
  nvs_handle iNvsHandle;
  while (true)
  {
    xEventGroupWaitBits(xMainEvGrp, RESET_NVS_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(LOG_TAG, "RUNNING prvResetTask");

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &iNvsHandle));
    prvNvsClearConfig(iNvsHandle);
    ESP_ERROR_CHECK(nvs_commit(iNvsHandle));
    nvs_close(iNvsHandle);

    ESP_LOGI(LOG_TAG, "CLEARED NVS, REBOOTING");
    xEventGroupClearBits(xMainEvGrp, HTTP_SERVER_BIT);
    esp_restart();
    vTaskDelete(NULL);
    return;
  }
  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

static void prvDistanceTask(void *pvParameter)
{
  uint16_t dist = 0;
  while (true)
  {
    xEventGroupWaitBits(xMainEvGrp, MQTT_CONN_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    dist = usGetDistance(&xUltrasonicConfig);
    ESP_LOGI(LOG_TAG, "dist is %d", dist);
    if (dist > xConfig.xUltrasonicData.iMinTrigCm && dist < xConfig.xUltrasonicData.iMaxTrigCm)
		  xQueueSend(xTriggerQueue, &dist, portMAX_DELAY); 
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/

void app_main(void)
{
  // Initialize semaphore
  // xSemaphore = xSemaphoreCreateBinary();

  // Initialize trigger queue
  xTriggerQueue = xQueueCreate(10, sizeof(uint16_t));
  if (xTriggerQueue == NULL)
  {
    ESP_LOGI(LOG_TAG, "Failed to create xTriggerQueue");
    return;
  }

  // Initialize ultrasonic sensor logic
  xUltrasonicConfig.xEcho = ECHO;
  xUltrasonicConfig.xTrigger = TRIG;
  vInitializeUltrasonic(&xUltrasonicConfig);

  // Initialize NVS (necessary for wifi)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
  {
    ESP_LOGI(LOG_TAG, "NO FREE PAGES ON FLASH!!!!!!!! ERASING");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize event group
  xMainEvGrp = xEventGroupCreate();

  if (xTaskCreate(prvConfigTask, "config_task", 4048, NULL, 5, NULL) != pdTRUE) return;
  if (xTaskCreate(prvHttpServerTask, "server_task", 4048, NULL, 5, NULL) != pdTRUE) return;
  if (xTaskCreate(prvResetTask, "reset_task", 2048, NULL, 5, NULL) != pdTRUE) return;
  if (xTaskCreate(prvRunMainTask, "main_task", 4048, NULL, 5, NULL) != pdTRUE) return;
  if (xTaskCreate(prvMqttInitTask, "mqtt_task", 4048, NULL, 5, NULL) != pdTRUE) return;
  if (xTaskCreate(prvPublishTask, "pub_task", 4048, NULL, 5, NULL) != pdTRUE) return;
  if (xTaskCreate(prvDistanceTask, "dist_task", 4048, NULL, 5, NULL) != pdTRUE) return;
  if (xTaskCreate(prvSaveConfigTask, "save_task", 4048, NULL, 5, NULL) != pdTRUE) return;
  xEventGroupSetBits(xMainEvGrp, CONFIG_BIT);
}
