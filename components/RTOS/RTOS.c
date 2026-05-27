#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"

#include "RTOS.h"

static adc_channel_t channel[6]= {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7};

//Vetor com os canais utilizados e fila para receber os dados obtidos do ADC1 e permitir que sejam enviadas para a tarefa de processamento
static QueueHandle_t fila_dados;

// Handles para as taregas e TAG para controle de erros
static TaskHandle_t s_task_handle1 = NULL, s_task_handle2 = NULL;
static const char *TAG_START = "INICIALIZADOR";
static const char *TAG_RMS = "TAREFA_RMS";

//Declaração e inicialização de variáveis da structs criadas.
static dados dado = {0};
static Valor_saida SAIDA = {0};

//Declaração de variáveis para receber valor da amostra processado
static int TENSAO = 0;
static int CORRENTE = 0;  


static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle1, &mustYield);

    return (mustYield == pdTRUE);
};

static bool IRAM_ATTR s_pool_ovf_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    
    // Log seguro para ser executado dentro de uma interrupção (ISR)
    esp_rom_printf("WARNING: ADC Pool Overflow! A Task esta muito lenta e perdemos amostras.\n");
    
    // Retorna false pois não estamos acordando nenhuma task diretamente aqui
    return pdFALSE;
};

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
                //ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, dado.num);
                if (xQueueSend(fila_dados, &dado, portMAX_DELAY) != pdPASS) {
                printf("Falha ao enviar para a fila\n");
                };
            } else if (ret == ESP_ERR_TIMEOUT) {
                //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        };

    };

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
};

void Task_RMS(void *pvParameters){

    dados recebido;

    
    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_handle = NULL;    // ADC1 Calibration handle
    bool do_calibration1 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_6, ADC_ATTEN_DB_2_5, &adc1_cali_handle); // ADC1 Calibration Init

    while(1){
        
        int j =0, k = 0, n = 0, m = 0, v = 0, b = 0; 

        if (xQueueReceive(fila_dados, &recebido, portMAX_DELAY) == pdPASS){
             for (int i = 0; i <recebido.num; i += EXAMPLE_ADC_DATA_LENGTH) {
                // Cast direto para acessar a amostra correta
                const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)(&recebido.result[i]);
                //ESP_LOGI(TAG_RMS, "Channel: %d, Value: %d", p->type1.channel, p->type1.data);
                if (p->type1.channel < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                    //ESP_LOGI(TAG_RMS, "Channel: %d, Value: %d", p->type1.channel, p->type1.data);
                    if (do_calibration1 && p->type1.channel == 0 && j < 100){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_corrente1[j] = (float)CORRENTE;
                            //ESP_LOGI(TAG_RMS, "ADC%d Channel[%d] Cali Voltage: %f V", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_corrente1[j]);
                            j++; 
                        }else{
                            //ESP_LOGI(TAG_RMS, "NAO ATIVOU A CALIBRACAO CANAL 0"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 4 && k < 100){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_corrente2[k] = (float)CORRENTE;
                            //ESP_LOGI(TAG_RMS, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_corrente2[k]);
                            k++; 
                        }else{
                            //ESP_LOGI(TAG_RMS, "NAO ATIVOU A CALIBRACAO CANAL 4"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 6 && n < 100){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_corrente3[n] = (float)CORRENTE;
                            //ESP_LOGI(TAG_RMS, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_corrente3[n]);
                            n++; 
                        }else{
                            //ESP_LOGI(TAG_RMS, "NAO ATIVOU A CALIBRACAO CANAL 6"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 3 && m < 100){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_tensao1[m] = (float)TENSAO;
                            //ESP_LOGI(TAG_RMS, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_tensao1[m], m);
                            m++; 
                        }else{
                            //ESP_LOGI(TAG_RMS, "NAO ATIVOU A CALIBRACAO CANAL 3"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 5 && v < 100){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_tensao2[v] = (float)TENSAO;
                            //ESP_LOGI(TAG_RMS, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_tensao2[v]);
                            v++; 
                        }else{
                            //ESP_LOGI(TAG_RMS, "NAO ATIVOU A CALIBRACAO CANAL 5"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 7 && b < 100){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_tensao3[b] = (float)TENSAO;
                           // ESP_LOGI(TAG_RMS, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_tensao3[b]);
                           b++; 
                        }else{
                            //ESP_LOGI(TAG_RMS, "NAO ATIVOU A CALIBRACAO CANAL 7"); // ADC1 LOG CALIBRAÇÃO
                        };

                } else {
                    ESP_LOGW(TAG_RMS, "Invalid data [Ch%d_%d]",
                            p->type1.channel,
                            p->type1.data);
                }
            }

            ESP_LOGI(TAG_RMS, "Valor Corrente 1 RMS0 = %fmV", rms(SAIDA.dados_corrente1,TAMANHO(SAIDA.dados_corrente1))); 
            ESP_LOGI(TAG_RMS, "Valor Corrente 2 RMS4 = %fmV", rms(SAIDA.dados_corrente2,TAMANHO(SAIDA.dados_corrente2))); 
            ESP_LOGI(TAG_RMS, "Valor Corrente 3 RMS6 = %fmV", rms(SAIDA.dados_corrente3,TAMANHO(SAIDA.dados_corrente3))); 
            ESP_LOGI(TAG_RMS, "Valor Tensao 1 RMS3 = %fmV", rms(SAIDA.dados_tensao1,TAMANHO(SAIDA.dados_tensao1))); 
            ESP_LOGI(TAG_RMS, "Valor Tensao 2 RMS5 = %fmV", rms(SAIDA.dados_tensao2,TAMANHO(SAIDA.dados_tensao2))); 
            ESP_LOGI(TAG_RMS, "Valor Tensao 3 RMS7 = %fmV", rms(SAIDA.dados_tensao3,TAMANHO(SAIDA.dados_tensao3))); 

        
        }else{
            ESP_LOGI(TAG_RMS, "VALOR PERDIDO NA FILA");
        }
    }
    
    if (do_calibration1)                                      // ADC1 Calibration Deinit
    {
        example_adc_calibration_deinit(adc1_cali_handle);     // ADC1 Calibration Deinit
    }
    
};

void Start(void){
    memset(dado.result, 0xcc, EXAMPLE_READ_LEN); //Preencher o vetor de amostras brutas para facilitar debug

    fila_dados = xQueueCreate(50, sizeof(dados)); //Criação da fila para recebimento das amostras brutas
    if (fila_dados == NULL) {
        ESP_LOGI(TAG_START, "Erro ao criar a Fila!");
        return;
    }

// Criação da tarefa do ADC, fixada ao núcleo 1 do ESP32
    xTaskCreatePinnedToCore(
        Task_Adc,               // Função da tarefa
        "TarefaNoCore1",        // Nome da tarefa
        4096,                   // Tamanho da stack (em palavras, não bytes)
        NULL,                   // Parâmetro para a tarefa
        2,                      // Prioridade
        &s_task_handle1,        // Handle da tarefa
        1                       // Core onde a tarefa será fixada (0 ou 1)
    );
// Criação da tarefa de cálculo do RMS, fixada ao núcleo 0 do ESP32 
     xTaskCreatePinnedToCore(
        Task_RMS,               // Função da tarefa
        "TarefaNoCore0",        // Nome da tarefa
        8192,                   // Tamanho da stack (em palavras, não bytes)
        NULL,                   // Parâmetro para a tarefa
        2,                      // Prioridade
        &s_task_handle2,        // Handle da tarefa
        0                       // Core onde a tarefa será fixada (0 ou 1)
    );
};

