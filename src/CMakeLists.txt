file(GLOB MAIN_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/*.c"
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

idf_component_register(
  SRCS ${MAIN_SRCS}
  INCLUDE_DIRS "."
  REQUIRES freertos driver esp_wifi esp_event nvs_flash log esp_http_client esp_https_ota
  PRIV_REQUIRES led_strip
)