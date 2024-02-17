idf_component_register(
  SRCS "cwifi.c"
  INCLUDE_DIRS "include"
  REQUIRES esp_http_server
  REQUIRES esp_wifi
)
