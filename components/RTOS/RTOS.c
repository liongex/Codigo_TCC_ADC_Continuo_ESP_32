#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"  
#include "esp_spp_api.h"
#include "nvs_flash.h"      

#include "RTOS.h"

#define SPP_SERVER_NAME "ESP32_RMS_SERVER"
#define DEVICE_NAME "ESP32_Bancada_Trifasica"

// ESTRUTURA UNIFICADA: Passa todas as informações de uma vez só, evitando loops de filas
typedef struct {
    float c1, c2, c3;   // Valores brutos calculados no RMS
    float t1, t2, t3;
    float cr1, cr2, cr3; // Valores reais convertidos pela calibração física
    float tr1, tr2, tr3;
} telemetria_trifasica_t;

static adc_channel_t channel[6] = {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7};

static QueueHandle_t fila_dados;
static QueueHandle_t fila_para_calculo; // Da Task_RMS para a Task_calculoVreal
static QueueHandle_t fila_para_bluetooth; // Da Task_calculoVreal para a Task_Bluetooth

static TaskHandle_t s_task_handle1 = NULL, s_task_handle2 = NULL, s_task_handle3 = NULL, s_task_handle4 = NULL;
static const char *TAG_START = "INICIALIZADOR";
static const char *TAG_RMS = "TAREFA_RMS";
static const char *TAG_CALC = "TAREFA_CALCULO";
static const char *TAG_BT = "TAREFA_BT";

static dados dado = {0};
static Valor_saida SAIDA = {0};

static int TENSAO = 0;
static int CORRENTE = 0;  
static uint32_t bt_conn_handle = 0; 

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
        case ESP_SPP_INIT_EVT:
            esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER, 0, SPP_SERVER_NAME);
            break;
        case ESP_SPP_START_EVT:
            esp_bt_gap_set_device_name(DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            break;
        case ESP_SPP_SRV_OPEN_EVT:
            ESP_LOGI(TAG_BT, "Notebook Conectado com sucesso!");
            bt_conn_handle = param->srv_open.handle;
            break;
        case ESP_SPP_CLOSE_EVT:
            ESP_LOGI(TAG_BT, "Notebook Desconectado.");
            bt_conn_handle = 0;
            break;
        default:
            break;
    }
}

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

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while(1){
            ret = adc_continuous_read(handle, dado.result, EXAMPLE_READ_LEN, &dado.num, 0);
            if (ret == ESP_OK) {
                if (xQueueSend(fila_dados, &dado, 0) != pdPASS) {
                    printf("Falha ao enviar para a fila_dados\n");
                }
            } else if (ret == ESP_ERR_TIMEOUT) {
                break;
            }
        }
    }
}

void Task_RMS(void *pvParameters){
    dados recebido;
    adc_cali_handle_t adc1_cali_handle = NULL;
    bool do_calibration1 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_6, ADC_ATTEN_DB_2_5, &adc1_cali_handle);

    while(1){
        int j = 0, k = 0, n = 0, m = 0, v = 0, b = 0; 

        if (xQueueReceive(fila_dados, &recebido, portMAX_DELAY) == pdPASS){
            for (int i = 0; i < recebido.num; i += EXAMPLE_ADC_DATA_LENGTH) {
                const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)(&recebido.result[i]);
                
                if (p->type1.channel < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                    if (do_calibration1 && p->type1.channel == 0 && j < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));
                        SAIDA.dados_corrente1[j++] = (float)CORRENTE;
                    }
                    else if (do_calibration1 && p->type1.channel == 4 && k < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));
                        SAIDA.dados_corrente2[k++] = (float)CORRENTE;
                    }
                    else if (do_calibration1 && p->type1.channel == 6 && n < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));
                        SAIDA.dados_corrente3[n++] = (float)CORRENTE;
                    }
                    else if (do_calibration1 && p->type1.channel == 3 && m < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));
                        SAIDA.dados_tensao1[m++] = (float)TENSAO;
                    }
                    else if (do_calibration1 && p->type1.channel == 5 && v < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));
                        SAIDA.dados_tensao2[v++] = (float)TENSAO;
                    }
                    else if (do_calibration1 && p->type1.channel == 7 && b < 100){
                        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));
                        SAIDA.dados_tensao3[b++] = (float)TENSAO;
                    }
                }
            }

            telemetria_trifasica_t pct;
            pct.c1 = rms(SAIDA.dados_corrente1, TAMANHO(SAIDA.dados_corrente1));
            pct.c2 = rms(SAIDA.dados_corrente2, TAMANHO(SAIDA.dados_corrente2));
            pct.c3 = rms(SAIDA.dados_corrente3, TAMANHO(SAIDA.dados_corrente3));
            pct.t1 = rms(SAIDA.dados_tensao1, TAMANHO(SAIDA.dados_tensao1));
            pct.t2 = rms(SAIDA.dados_tensao2, TAMANHO(SAIDA.dados_tensao2));
            pct.t3 = rms(SAIDA.dados_tensao3, TAMANHO(SAIDA.dados_tensao3));

            // Passa os dados brutos adiante para a Task de Conversão Física
            xQueueSend(fila_para_calculo, &pct, 0);
        }
    }
}

// ESTEIRA PARTE 2: Recebe os dados brutos e anexa as conversões reais na mesma struct
void Task_calculoVreal(void *pvParameters) {
    telemetria_trifasica_t pct_processando;

    while(1) {
        if (xQueueReceive(fila_para_calculo, &pct_processando, portMAX_DELAY) == pdPASS) {
            pct_processando.tr1 = VReal_tensao(pct_processando.t1);
            pct_processando.tr2 = VReal_tensao(pct_processando.t2);
            pct_processando.tr3 = VReal_tensao(pct_processando.t3);
            pct_processando.cr1 = VReal_corrente(pct_processando.c1);
            pct_processando.cr2 = VReal_corrente(pct_processando.c2);
            pct_processando.cr3 = VReal_corrente(pct_processando.c3);
            
            // Envia o pacote completo finalizado para a Task de Bluetooth
            xQueueSend(fila_para_bluetooth, &pct_processando, 0);
        }
    }
}

// ESTEIRA PARTE 3: Lê o pacote consolidado e transmite de forma controlada a cada 250ms
void Task_Bluetooth(void *pvParameters) {
    telemetria_trifasica_t dados_enviar;
    char buffer_texto[256];

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_spp_register_callback(esp_spp_cb);
    esp_spp_init(ESP_SPP_MODE_CB);

    while(1) {
        // Agora lê apenas UMA única fila de forma limpa e síncrona
        if (xQueueReceive(fila_para_bluetooth, &dados_enviar, portMAX_DELAY) == pdPASS) {
            if (bt_conn_handle != 0) {
                memset(buffer_texto, 0, sizeof(buffer_texto));

                int len = snprintf(buffer_texto, sizeof(buffer_texto),
                    "C1: %.2f | C2: %.2f | C3: %.2f | T1: %.2f | T2: %.2f | T3: %.2f\r\n"
                    "CR1: %.2f | CR2: %.2f | CR3: %.2f | TR1: %.2f | TR2: %.2f | TR3: %.2f\r\n",
                    dados_enviar.c1, dados_enviar.c2, dados_enviar.c3,
                    dados_enviar.t1, dados_enviar.t2, dados_enviar.t3,
                    dados_enviar.cr1, dados_enviar.cr2, dados_enviar.cr3,
                    dados_enviar.tr1, dados_enviar.tr2, dados_enviar.tr3);

                if (len > 0 && len < sizeof(buffer_texto)) {
                    esp_spp_write(bt_conn_handle, len, (uint8_t *)buffer_texto);
                }
            }
        }
        // Esse delay agora funciona perfeitamente, controlando a poluição visual do terminal do note
        vTaskDelay(pdMS_TO_TICKS(250));
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

    // Inicialização correta de todas as 3 filas da arquitetura
    fila_dados = xQueueCreate(30, sizeof(dados));
    fila_para_calculo = xQueueCreate(30, sizeof(telemetria_trifasica_t));
    fila_para_bluetooth = xQueueCreate(10, sizeof(telemetria_trifasica_t)); 

    // Validação robusta de alocação de memória RAM
    if (fila_dados == NULL || fila_para_calculo == NULL || fila_para_bluetooth == NULL) {
        ESP_LOGE(TAG_START, "Erro crítico: Falha ao criar as Filas em RAM!");
        return;
    }

    xTaskCreatePinnedToCore(Task_Adc, "TarefaNoCore1", 2048, NULL, 2, &s_task_handle1, 1);
    xTaskCreatePinnedToCore(Task_RMS, "TarefaNoCore0", 4096, NULL, 2, &s_task_handle2, 0);
    xTaskCreatePinnedToCore(Task_calculoVreal, "TarefaCalculo", 1024, NULL, 2, &s_task_handle4, 0);
    xTaskCreatePinnedToCore(Task_Bluetooth, "TarefaBT", 3072, NULL, 1, &s_task_handle3, 0); // Prioridade ligeiramente menor (1) para não engargalar o cálculo de potência
}