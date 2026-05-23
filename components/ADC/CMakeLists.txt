# components/meu_sensor/CMakeLists.txt

idf_component_register(SRCS "ADC.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_adc log freertos)