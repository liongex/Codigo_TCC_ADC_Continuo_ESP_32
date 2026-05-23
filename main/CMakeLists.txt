idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash esp_adc driver ADC wifi MQTT_lib
                       INCLUDE_DIRS ".")
