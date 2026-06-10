#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <math.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"      

// Inclusão dos seus dois componentes e interfaces nativas
#include "wifi.h"       // Seu componente de rede Wi-Fi
#include "MQTT_lib.h"   // Seu componente de mensageria MQTT
#include "RTOS.h"

// Definições de Rede do seu ambiente Debian 13
#define MQTT_BROKER_URI   "mqtt://192.168.0.9"
#define MQTT_BROKER_PORT  1844
#define MQTT_USERNAME     "isac"
#define MQTT_PASSWORD     "isac"
#define MQTT_TOPIC        "casa/temp"

// ESTRUTURA UNIFICADA: Passa todas as informações de uma vez só, evitando loops de filas
typedef struct {
    // Valores brutos calculados no RMS
    float t1, t2, t3;
    float c1, c2, c3;
    float p1_bruta, p2_bruta, p3_bruta; // Potências ativas brutas

    // Valores reais convertidos pela calibração física
    float tr1, tr2, tr3;
    float cr1, cr2, cr3;
    float pr1, pr2, pr3; // Potência Ativa Real (W)
    float qr1, qr2, qr3; // Potência Reativa Real (VAr)
    float sr1, sr2, sr3; // Potência Aparente Real (VA)
    float fp1, fp2, fp3; // Fator de Potência Real
} telemetria_trifasica_t;

static adc_channel_t channel[6] = {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7};

static QueueHandle_t fila_dados;
static QueueHandle_t fila_para_calculo; // Da Task_RMS para a Task_calculoVreal
static QueueHandle_t fila_para_network; // Substituiu a fila_para_bluetooth de forma limpa

static TaskHandle_t s_task_handle1 = NULL, s_task_handle2 = NULL, s_task_handle3 = NULL, s_task_handle4 = NULL;
static const char *TAG_START = "INICIALIZADOR";
static const char *TAG_NET   = "TAREFA_NETWORK";

static dados dado = {0};

static int TENSAO = 0;
static int CORRENTE = 0;

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle1, &mustYield);
    return (mustYield == pdTRUE);
}

static bool IRAM_ATTR s_pool_ovf_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    esp_rom_printf("\nWARNING: ADC Pool Overflow!\n");
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

    dados dados_locais;

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

            telemetria_trifasica_t pct;

            float dc_c1 = media(local_corrente1, 100);
            float dc_c2 = media(local_corrente2, 100);
            float dc_c3 = media(local_corrente3, 100);
            float dc_t1 = media(local_tensao1, 100);
            float dc_t2 = media(local_tensao2, 100);
            float dc_t3 = media(local_tensao3, 100);

            pct.c1 = rms(local_corrente1, 100);
            pct.c2 = rms(local_corrente2, 100);
            pct.c3 = rms(local_corrente3, 100);
            pct.t1 = rms(local_tensao1, 100);
            pct.t2 = rms(local_tensao2, 100);
            pct.t3 = rms(local_tensao3, 100);

            float soma_p1 = 0, soma_p2 = 0, soma_p3 = 0;
            for(int i = 0; i < 100; i++) {
                soma_p1 += (local_tensao1[i] - dc_t1) * (local_corrente1[i] - dc_c1);
                soma_p2 += (local_tensao2[i] - dc_t2) * (local_corrente2[i] - dc_c2);
                soma_p3 += (local_tensao3[i] - dc_t3) * (local_corrente3[i] - dc_c3);
            }
            
            pct.p1_bruta = soma_p1 / 100000000.0f;
            pct.p2_bruta = soma_p2 / 100000000.0f;
            pct.p3_bruta = soma_p3 / 100000000.0f;

            xQueueSend(fila_para_calculo, &pct, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void Task_calculoVreal(void *pvParameters) {
    telemetria_trifasica_t pct;

    while(1) {
        if (xQueueReceive(fila_para_calculo, &pct, portMAX_DELAY) == pdPASS) {
            pct.tr1 = VReal_tensao(pct.t1);
            pct.tr2 = VReal_tensao(pct.t2);
            pct.tr3 = VReal_tensao(pct.t3);
            pct.cr1 = VReal_corrente(pct.c1);
            pct.cr2 = VReal_corrente(pct.c2);
            pct.cr3 = VReal_corrente(pct.c3);
            
            float fator_potencia_ativa = 879.719f * 145.161f;
            pct.pr1 = pct.p1_bruta * fator_potencia_ativa;
            pct.pr2 = pct.p2_bruta * fator_potencia_ativa;
            pct.pr3 = pct.p3_bruta * fator_potencia_ativa;

            pct.sr1 = pct.tr1 * pct.cr1;
            pct.sr2 = pct.tr2 * pct.cr2;
            pct.sr3 = pct.tr3 * pct.cr3;

            pct.fp1 = (pct.sr1 > 0.1f) ? (pct.pr1 / pct.sr1) : 0.0f;
            pct.fp2 = (pct.sr2 > 0.1f) ? (pct.pr2 / pct.sr2) : 0.0f;
            pct.fp3 = (pct.sr3 > 0.1f) ? (pct.pr3 / pct.sr3) : 0.0f;

            if (pct.fp1 > 1.0f) pct.fp1 = 1.0f;
            if (pct.fp2 > 1.0f) pct.fp2 = 1.0f;
            if (pct.fp3 > 1.0f) pct.fp3 = 1.0f;
            
            pct.qr1 = sqrtf(fmaxf(0.0f, (pct.sr1 * pct.sr1) - (pct.pr1 * pct.pr1)));
            pct.qr2 = sqrtf(fmaxf(0.0f, (pct.sr2 * pct.sr2) - (pct.pr2 * pct.pr2)));
            pct.qr3 = sqrtf(fmaxf(0.0f, (pct.sr3 * pct.sr3) - (pct.pr3 * pct.pr3)));

            // Envia para a nova fila de rede com timeout protetivo de 10ms
            xQueueSend(fila_para_network, &pct, pdMS_TO_TICKS(10));
        }
    }
}

// 🌐 NOVA TAREFA: Substituiu o Bluetooth para postar em JSON no Debian 13
void Task_NetworkTransmit(void *pvParameters) {
    telemetria_trifasica_t dados_enviar;
    char json_buffer[1024]; 

    while(1) {
        if (xQueueReceive(fila_para_network, &dados_enviar, portMAX_DELAY) == pdPASS) {
            
            // Só executa o envio se o seu componente acusar conexão ativa com o Broker
            if (mqtt_connected()) {
                memset(json_buffer, 0, sizeof(json_buffer));

                // Montagem do payload estruturado em JSON estruturado para o Dashboard ler
                int len = snprintf(json_buffer, sizeof(json_buffer),
                    "{"
                    "\"t1\":%.1f,\"t2\":%.1f,\"t3\":%.1f,"
                    "\"c1\":%.1f,\"c2\":%.1f,\"c3\":%.1f,"
                    "\"tr1\":%.1f,\"cr1\":%.2f,\"pr1\":%.1f,\"qr1\":%.1f,\"sr1\":%.1f,\"fp1\":%.3f,"
                    "\"tr2\":%.1f,\"cr2\":%.2f,\"pr2\":%.1f,\"qr2\":%.1f,\"sr2\":%.1f,\"fp2\":%.3f,"
                    "\"tr3\":%.1f,\"cr3\":%.2f,\"pr3\":%.1f,\"qr3\":%.1f,\"sr3\":%.1f,\"fp3\":%.3f"
                    "}",
                    dados_enviar.t1, dados_enviar.t2, dados_enviar.t3,
                    dados_enviar.c1, dados_enviar.c2, dados_enviar.c3,
                    
                    dados_enviar.tr1, dados_enviar.cr1, dados_enviar.pr1, dados_enviar.qr1, dados_enviar.sr1, dados_enviar.fp1,
                    dados_enviar.tr2, dados_enviar.cr2, dados_enviar.pr2, dados_enviar.qr2, dados_enviar.sr2, dados_enviar.fp2,
                    dados_enviar.tr3, dados_enviar.cr3, dados_enviar.pr3, dados_enviar.qr3, dados_enviar.sr3, dados_enviar.fp3
                );

                if (len > 0 && len < sizeof(json_buffer)) {
                    // Publica diretamente no tópico casa/temp usando sua biblioteca
                    mqtt_publish(MQTT_TOPIC, json_buffer, 0, 0);
                }
            } else {
                ESP_LOGW(TAG_NET, "Aviso: Dados prontos, mas MQTT desconectado do Debian.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Cadência idêntica ao teste anterior
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

    fila_dados = xQueueCreate(30, sizeof(dados));
    fila_para_calculo = xQueueCreate(30, sizeof(telemetria_trifasica_t));
    fila_para_network = xQueueCreate(10, sizeof(telemetria_trifasica_t)); 

    if (fila_dados == NULL || fila_para_calculo == NULL || fila_para_network == NULL) {
        ESP_LOGE(TAG_START, "Erro crítico: Falha ao criar as Filas em RAM!");
        return;
    }

    // 🌐 Inicialização Sequencial dos seus componentes de Rede
    ESP_LOGI(TAG_START, "Conectando ao ponto de acesso Wi-Fi local...");
    wifi_init_sta(); // Bloqueia a execução até conseguir o IP da rede local

    ESP_LOGI(TAG_START, "Inicializando cliente MQTT para o Broker Debian 13...");
    mqtt_start(); // Dispara o cliente MQTT assíncrono em background

    // Alocação das tarefas nos núcleos do processador Dual Core
    xTaskCreatePinnedToCore(Task_Adc, "TarefaNoCore1", 4096, NULL, 2, &s_task_handle1, 1);
    xTaskCreatePinnedToCore(Task_RMS, "TarefaNoCore0", 4096, NULL, 2, &s_task_handle2, 0);
    xTaskCreatePinnedToCore(Task_calculoVreal, "TarefaCalculo", 2048, NULL, 2, &s_task_handle4, 0);
    xTaskCreatePinnedToCore(Task_NetworkTransmit, "TarefaNet", 4096, NULL, 1, &s_task_handle3, 0); 
}