#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <math.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"      

// Componentes do seu projeto
#include "wifi.h"       
#include "MQTT_lib.h"   
#include "RTOS.h"

// Configuração de Rede Dedicada para Coleta (Sem Criptografia)
#define MQTT_BROKER_TOPIC_AMOSTRAS "casa/dados"

// ESTRUTURA EXPANDIDA DE CALIBRAÇÃO: 100 pontos das 3 fases simultâneas
typedef struct {
    float t1, t2, t3;
    float c1, c2, c3;
    
    // Vetores de amostragem real sem offset DC
    float onda_v1[100], onda_v2[100], onda_v3[100];
    float onda_i1[100], onda_i2[100], onda_i3[100];
} telemetria_calibracao_t;

static adc_channel_t channel[6] = {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7};

// Filas de comunicação entre tarefas
static QueueHandle_t fila_dados;
static QueueHandle_t fila_para_amostras;  

static TaskHandle_t s_task_handle1 = NULL, s_task_handle2 = NULL, s_task_handle5 = NULL;
static const char *TAG_START = "INICIALIZADOR_CALIB";

static dados dado = {0};
static int TENSAO = 0;
static int CORRENTE = 0;

// Instanciação fixa em RAM global (DRAM) para segurança absoluta do barramento
static telemetria_calibracao_t pct_calib;

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle1, &mustYield);
    return (mustYield == pdTRUE);
}

static bool IRAM_ATTR s_pool_ovf_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    return pdFALSE;
}

void Task_Adc(void *pvParameters){
    esp_err_t ret;
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
        .on_pool_ovf = s_pool_ovf_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    static dados dados_locais;

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while(1){
            memset(dados_locais.result, 0, sizeof(dados_locais.result));
            
            ret = adc_continuous_read(handle, dados_locais.result, EXAMPLE_READ_LEN, &dados_locais.num, 0);
            if (ret == ESP_OK) {
                xQueueSend(fila_dados, &dados_locais, 0);
            } else if (ret == ESP_ERR_TIMEOUT) {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void Task_RMS(void *pvParameters){
    dados recebido;
    adc_cali_handle_t adc1_cali_handle = NULL;
    bool do_calibration1 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_6, ADC_ATTEN_DB_2_5, &adc1_cali_handle);

    static float local_corrente1[100];
    static float local_corrente2[100];
    static float local_corrente3[100];
    static float local_tensao1[100];
    static float local_tensao2[100];
    static float local_tensao3[100];

    while(1){
        int j = 0, k = 0, n = 0, m = 0, v = 0, b = 0;

        if (xQueueReceive(fila_dados, &recebido, portMAX_DELAY) == pdPASS){
            
            for (int i = 0; i < recebido.num; i += EXAMPLE_ADC_DATA_LENGTH) {
                const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)(&recebido.result[i]);
                
                if (p->type1.channel < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                    if (do_calibration1 && p->type1.channel == 0 && j < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));
                        local_corrente1[j++] = (float)CORRENTE;
                    }
                    else if (do_calibration1 && p->type1.channel == 4 && k < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));
                        local_corrente2[k++] = (float)CORRENTE;
                    }
                    else if (do_calibration1 && p->type1.channel == 6 && n < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));
                        local_corrente3[n++] = (float)CORRENTE;
                    }
                    else if (do_calibration1 && p->type1.channel == 3 && m < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));
                        local_tensao1[m++] = (float)TENSAO;
                    }
                    else if (do_calibration1 && p->type1.channel == 5 && v < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));
                        local_tensao2[v++] = (float)TENSAO;
                    }
                    else if (do_calibration1 && p->type1.channel == 7 && b < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));
                        local_tensao3[b++] = (float)TENSAO;
                    }
                }
            }

            float dc_c1 = media(local_corrente1, 100);
            float dc_c2 = media(local_corrente2, 100);
            float dc_c3 = media(local_corrente3, 100);
            float dc_t1 = media(local_tensao1, 100);
            float dc_t2 = media(local_tensao2, 100);
            float dc_t3 = media(local_tensao3, 100);

            for(int i = 0; i < 100; i++) {
                float v1_limpo = local_tensao1[i] - dc_t1;
                float c1_limpo = local_corrente1[i] - dc_c1;
                float v2_limpo = local_tensao2[i] - dc_t2;
                float c2_limpo = local_corrente2[i] - dc_c2;
                float v3_limpo = local_tensao3[i] - dc_t3;
                float c3_limpo = local_corrente3[i] - dc_c3;

                // Converte as amostras limpas para escala real e injeta na struct global
                pct_calib.onda_v1[i] = VReal_tensao(v1_limpo);
                pct_calib.onda_v2[i] = VReal_tensao(v2_limpo);
                pct_calib.onda_v3[i] = VReal_tensao(v3_limpo);
                
                pct_calib.onda_i1[i] = VReal_corrente(c1_limpo);
                pct_calib.onda_i2[i] = VReal_corrente(c2_limpo);
                pct_calib.onda_i3[i] = VReal_corrente(c3_limpo);
            }

            xQueueSend(fila_para_amostras, &pct_calib, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Task_AmostrasTransmit(void *pvParameters) {
    telemetria_calibracao_t dados_env;
    // Buffer massivo de 8KB alocado de forma fixa na DRAM (Agora livre sem o TLS)
    static char amostras_json[8192]; 

    while(1) {
        if (xQueueReceive(fila_para_amostras, &dados_env, portMAX_DELAY) == pdPASS) {
            if (mqtt_connected()) {
                memset(amostras_json, 0, sizeof(amostras_json));

                // Construção sequencial controlada por offset
                int offset = snprintf(amostras_json, sizeof(amostras_json), "{\"v1\":[");
                for(int i = 0; i < 100; i++) {
                    offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "%.1f%s", dados_env.onda_v1[i], (i == 99) ? "" : ",");
                }
                
                offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "],\"v2\":[");
                for(int i = 0; i < 100; i++) {
                    offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "%.1f%s", dados_env.onda_v2[i], (i == 99) ? "" : ",");
                }

                offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "],\"v3\":[");
                for(int i = 0; i < 100; i++) {
                    offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "%.1f%s", dados_env.onda_v3[i], (i == 99) ? "" : ",");
                }

                offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "],\"i1\":[");
                for(int i = 0; i < 100; i++) {
                    offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "%.3f%s", dados_env.onda_i1[i], (i == 99) ? "" : ",");
                }

                offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "],\"i2\":[");
                for(int i = 0; i < 100; i++) {
                    offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "%.3f%s", dados_env.onda_i2[i], (i == 99) ? "" : ",");
                }

                offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "],\"i3\":[");
                for(int i = 0; i < 100; i++) {
                    offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "%.3f%s", dados_env.onda_i3[i], (i == 99) ? "" : ",");
                }
                
                offset += snprintf(amostras_json + offset, sizeof(amostras_json) - offset, "]}");

                if (offset > 0 && offset < sizeof(amostras_json)) {
                    mqtt_publish(MQTT_BROKER_TOPIC_AMOSTRAS, amostras_json, 0, 0);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Uma rajada de amostragem por segundo
    }
}

void Start(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    memset(dado.result, 0xcc, EXAMPLE_READ_LEN);

    // Inicialização direta e segura das estruturas de fila profunda (Espaço livre com a mbedTLS desligada)
    fila_dados = xQueueCreate(15, sizeof(dados));
    fila_para_amostras = xQueueCreate(5, sizeof(telemetria_calibracao_t));

    if (fila_dados == NULL || fila_para_amostras == NULL) {
        ESP_LOGE(TAG_START, "Erro fatal: Falha ao alocar as filas de coleta!");
        return;
    }

    wifi_init_sta(); 
    mqtt_start(); // Inicia o cliente comum (Lembre de ajustar a URI para "mqtt://192.168.0.155" e a porta para 1844 no seu MQTT_lib.c)

    // Escalonamento das 3 tarefas fundamentais
    xTaskCreatePinnedToCore(Task_Adc, "TarefaAdc", 4096, NULL, 2, &s_task_handle1, 1);
    xTaskCreatePinnedToCore(Task_RMS, "TarefaRMS", 4096, NULL, 2, &s_task_handle2, 0);
    xTaskCreatePinnedToCore(Task_AmostrasTransmit, "TarefaWave", 8192, NULL, 1, &s_task_handle5, 0); 
}