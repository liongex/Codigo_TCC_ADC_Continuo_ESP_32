# components/meu_sensor/CMakeLists.txt

idf_component_register(SRCS "FUNCTIONS.c"
                       INCLUDE_DIRS "include"
                       REQUIRES log freertos)