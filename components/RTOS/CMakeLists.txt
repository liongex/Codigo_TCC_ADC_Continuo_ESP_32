# components/meu_sensor/CMakeLists.txt

idf_component_register(SRCS "RTOS.c"
                       INCLUDE_DIRS "include"
                       REQUIRES log freertos spi_flash esp_adc driver ADC wifi MQTT_lib FUNCTIONS)