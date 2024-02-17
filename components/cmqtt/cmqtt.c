#include <stdio.h>
#include "cmqtt.h"

esp_mqtt_client_handle_t prvClient = {0};

void vMqttInit(MqttData_t* xMqttConfig, void* pvMqttEvHandler)
{
  const esp_mqtt_client_config_t xEspMqttCfg = {
    .uri = xMqttConfig->szMqttUri,
    .username = xMqttConfig->szMqttUser,
    .password = xMqttConfig->szMqttPass,
  };
  prvClient = esp_mqtt_client_init(&xEspMqttCfg);
  ESP_ERROR_CHECK(esp_mqtt_client_register_event(prvClient, ESP_EVENT_ANY_ID, pvMqttEvHandler, prvClient));
  ESP_ERROR_CHECK(esp_mqtt_client_start(prvClient));
}

int iMqttPublish(char* pcTopic, char* pcData, size_t pcDataSize) {
  return esp_mqtt_client_publish(prvClient, pcTopic, pcData, pcDataSize, 0, 0);
}
