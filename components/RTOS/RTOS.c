#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"

// Includes do Bluetooth
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"  // ADICIONADO
#include "esp_spp_api.h"

// Include do NVS (Memória Não-Volátil)
#include "nvs_flash.h"      // ADICIONADO

#include "RTOS.h"

#define SPP_SERVER_NAME "ESP32_RMS_SERVER"
#define DEVICE_NAME "ESP32_Bancada_Trifasica"

// Struct para repassar os resultados calculados para a Task de Bluetooth
typedef struct {
    float c1, c2, c3;
    float t1, t2, t3;
} resultados_rms_t;

static adc_channel_t channel[6] = {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7};

static QueueHandle_t fila_dados;
static QueueHandle_t fila_bt_enviar; // NOVA: Fila para enviar resultados para o Bluetooth

static TaskHandle_t s_task_handle1 = NULL, s_task_handle2 = NULL, s_task_handle3 = NULL;
static const char *TAG_START = "INICIALIZADOR";
static const char *TAG_RMS = "TAREFA_RMS";
static const char *TAG_BT = "TAREFA_BT";

static dados dado = {0};
static Valor_saida SAIDA = {0};

static int TENSAO = 0;
static int CORRENTE = 0;  
static uint32_t bt_conn_handle = 0; // Handle da conexão ativa Bluetooth

// Callback do Bluetooth SPP
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
        case ESP_SPP_INIT_EVT:
            ESP_LOGI(TAG_BT, "SPP Inicializado, iniciando servidor...");
            esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER, 0, SPP_SERVER_NAME);
            break;
        case ESP_SPP_START_EVT:
            ESP_LOGI(TAG_BT, "Servidor SPP Iniciado. Nome do dispositivo: %s", DEVICE_NAME);
            esp_bt_gap_set_device_name(DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            break;
        case ESP_SPP_SRV_OPEN_EVT:
            ESP_LOGI(TAG_BT, "Notebook Conectado com sucesso!");
            bt_conn_handle = param->srv_open.handle; // Guarda o ID da conexão
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
    esp_rom_printf("\nWARNING: ADC Pool Overflow! A Task esta muito lenta.\n");
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
                if (xQueueSend(fila_dados, &dado, portMAX_DELAY) != pdPASS) {
                    printf("Falha ao enviar para a fila\n");
                }
            } else if (ret == ESP_ERR_TIMEOUT) {
                break;
            }
        }
    }
    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
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

            resultados_rms_t envio;
            envio.c1 = rms(SAIDA.dados_corrente1, TAMANHO(SAIDA.dados_corrente1));
            envio.c2 = rms(SAIDA.dados_corrente2, TAMANHO(SAIDA.dados_corrente2));
            envio.c3 = rms(SAIDA.dados_corrente3, TAMANHO(SAIDA.dados_corrente3));
            envio.t1 = rms(SAIDA.dados_tensao1, TAMANHO(SAIDA.dados_tensao1));
            envio.t2 = rms(SAIDA.dados_tensao2, TAMANHO(SAIDA.dados_tensao2));
            envio.t3 = rms(SAIDA.dados_tensao3, TAMANHO(SAIDA.dados_tensao3));

            // Envia os resultados prontos para a fila de transmissão via Bluetooth
            // Usa NULO (0) no timeout para não travar o cálculo de RMS se o BT estiver desconectado
            xQueueSend(fila_bt_enviar, &envio, 0);
        }
    }
}

// NOVA TAREFA: Responsável por gerenciar a transmissão sem fio
void Task_Bluetooth(void *pvParameters) {
    resultados_rms_t dados_enviar;
    char buffer_texto[128];

    // Inicialização do Controlador Bluetooth do ESP32
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_spp_register_callback(esp_spp_cb);
    esp_spp_init(ESP_SPP_MODE_CB);

    while(1) {
        // Aguarda a Task_RMS colocar um dado novo na fila
        if (xQueueReceive(fila_bt_enviar, &dados_enviar, portMAX_DELAY) == pdPASS) {
            // Só envia os dados se houver um notebook ativamente conectado
            if (bt_conn_handle != 0) {
                int len = snprintf(buffer_texto, sizeof(buffer_texto),
                                   "C1: %.2f | C2: %.2f | C3: %.2f | T1: %.2f | T2: %.2f | T3: %.2f\r\n", 
                                   dados_enviar.c1, dados_enviar.c2, dados_enviar.c3, 
                                   dados_enviar.t1, dados_enviar.t2, dados_enviar.t3);
                
                esp_spp_write(bt_conn_handle, len, (uint8_t *)buffer_texto);
            }
        }
        // Delay para controlar a taxa de atualização na tela (ex: ~250ms)
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void Start(void){
    // Inicialização da pilha de memória não-volátil (obrigatória para usar Bluetooth no ESP32)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    memset(dado.result, 0xcc, EXAMPLE_READ_LEN);

    fila_dados = xQueueCreate(30, sizeof(dados));
    fila_bt_enviar = xQueueCreate(5, sizeof(resultados_rms_t)); // Inicializa nova fila

    if (fila_dados == NULL || fila_bt_enviar == NULL) {
        ESP_LOGE(TAG_START, "Erro ao criar as Filas!");
        return;
    }

    xTaskCreatePinnedToCore(Task_Adc, "TarefaNoCore1", 4096, NULL, 2, &s_task_handle1, 1);
    xTaskCreatePinnedToCore(Task_RMS, "TarefaNoCore0", 8192, NULL, 2, &s_task_handle2, 0);
    
    // Criação da tarefa Bluetooth no Core 0 (Deixando o Core 1 focado estritamente no ADC)
    xTaskCreatePinnedToCore(Task_Bluetooth, "TarefaBT", 4096, NULL, 1, &s_task_handle3, 0);
}