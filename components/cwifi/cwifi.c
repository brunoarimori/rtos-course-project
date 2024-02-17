#include "cwifi.h"

void vWiFiInitAp(void* pvEvHandler, char* pcRndId)
{
  esp_event_handler_instance_t xInstanceAnyId;
  esp_event_handler_instance_t xInstanceGotIp;
  wifi_init_config_t xCfg;
  wifi_config_t xWifiConfig;
  tcpip_adapter_ip_info_t xIpAddrInfo;
  char szApSsid[MAX_SSID_LEN];
  // uint8_t mac[10];

  ESP_LOGI(WIFI_TAG, "running vWiFiInitAp()!");

  // initialize network interface
  ESP_ERROR_CHECK(esp_netif_init());
  // create event loop 
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // create wifi access point
  esp_netif_create_default_wifi_ap();
  // initialize wifi
  xCfg = (wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&xCfg));
  // register wifi events on the event loop
  esp_event_handler_instance_register(
    WIFI_EVENT,
    ESP_EVENT_ANY_ID,
    pvEvHandler,
    NULL,
    &xInstanceAnyId
  );
  // register ip event on the event loop
  esp_event_handler_instance_register(
    IP_EVENT,
    IP_EVENT_AP_STAIPASSIGNED,
    pvEvHandler,
    NULL,
    &xInstanceGotIp 
  );

  // start tcpip config
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
  memset(&xIpAddrInfo, 0 , sizeof(xIpAddrInfo));
  ipaddr_aton(CWD_CONF_PAGE_IP, (ip_addr_t*)&xIpAddrInfo.ip);
  ipaddr_aton(CWD_DEFAULT_GW, (ip_addr_t*)&xIpAddrInfo.gw);
  ipaddr_aton(CWD_DEFAULT_NETMASK, (ip_addr_t*)&xIpAddrInfo.netmask);

  ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &xIpAddrInfo));
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

  memset(szApSsid, 0 , sizeof(szApSsid));
  strcat(szApSsid, CWF_AP_SSID_PREFIX);
  strcat(szApSsid, pcRndId);

  xWifiConfig = (wifi_config_t) {
    .ap = {
      .ssid_len = strlen(szApSsid),
      .password = CWF_AP_PASS,
      .channel = 0,
      .authmode = WIFI_AUTH_WPA_WPA2_PSK,
      .max_connection = 10,
    },
  };
  strncpy((char*)xWifiConfig.ap.ssid, szApSsid, sizeof(xWifiConfig.ap.ssid));

  ESP_LOGI(WIFI_TAG, "initializing wifi as AP!");
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &xWifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void vWiFiInitSta(WiFiData_t* pxData, void* pvEvHandler)
{
  esp_event_handler_instance_t xInstanceAnyId;
  esp_event_handler_instance_t xInstanceGotIp;
  wifi_init_config_t xCfg;
  wifi_config_t xWifiConfig;

  ESP_LOGI(WIFI_TAG, "running prvWifiInit()!");
  // initialize network interface
  ESP_ERROR_CHECK(esp_netif_init());
  // create event loop 
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // create wifi station
  esp_netif_create_default_wifi_sta();
  // initialize wifi
  xCfg = (wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&xCfg));
  // register wifi events on the event loop
  esp_event_handler_instance_register(
    WIFI_EVENT,
    ESP_EVENT_ANY_ID,
    pvEvHandler,
    NULL,
    &xInstanceAnyId
  );
  // register ip event on the event loop
  esp_event_handler_instance_register(
    IP_EVENT,
    IP_EVENT_STA_GOT_IP,
    pvEvHandler,
    NULL,
    &xInstanceGotIp
  );

  xWifiConfig = (wifi_config_t) {
    .sta = {},
  };
  strncpy((char*)xWifiConfig.sta.ssid, pxData->szWiFiStaSsid, sizeof(xWifiConfig.sta.ssid));
  strncpy((char*)xWifiConfig.sta.password, pxData->szWiFiStaPass, sizeof(xWifiConfig.sta.password));

  ESP_LOGI(WIFI_TAG, "initializing wifi!");
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &xWifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());
}
