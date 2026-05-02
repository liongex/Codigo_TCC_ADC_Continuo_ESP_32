
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1 // Declara a utilização do ADC1 do esp21
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1 // Garante que apenas o ADC1 será utilizado para a conversão
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_2_5 // Atenuação de 2.5dB
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH // Garente o uso de 12bits para cada amostra

#define EXAMPLE_READ_LEN                    2400 // Tamanho para cada frame, considerando 100 amostras por canal, 4 bytes por amostra e 6 canais.

#define TAMANHO(x) (sizeof(x) / sizeof((x)[0])) // Macro para obter tamanho da matriz

//Struct criado para armazenar as amostras brutas retiradas do ADC
typedef struct{
    uint8_t result[EXAMPLE_READ_LEN];
    uint32_t num;

}dados;

//Struct criado para armazenar as amostras processadas de tensão e corrente para cada canal
typedef struct{ 
    float dados_tensao1[100];   // DEVERÁ SER LIDO NO CANAL 3
    float dados_tensao2[100];   // DEVERÁ SER LIDO NO CANAL 5
    float dados_tensao3[100];   // DEVERÁ SER LIDO NO CANAL 7
    float dados_corrente1[100]; // DEVERÁ SER LIDO NO CANAL 0
    float dados_corrente2[100]; // DEVERÁ SER LIDO NO CANAL 4
    float dados_corrente3[100]; // DEVERÁ SER LIDO NO CANAL 6

}Valor_saida;

//Vetor com os canais utilizados e fila para receber os dados obtidos do ADC1 e permitir que sejam enviadas para a tarefa de processamento
static adc_channel_t channel[6]= {ADC_CHANNEL_0, ADC_CHANNEL_6, ADC_CHANNEL_4, ADC_CHANNEL_3, ADC_CHANNEL_5, ADC_CHANNEL_7};
static QueueHandle_t fila_dados;

// Handles para as taregas e TAG para controle de erros
static TaskHandle_t s_task_handle1 = NULL, s_task_handle2 = NULL;
static const char *TAG = "ADC_CONTINUO";

//Declaração e inicialização de variáveis da structs criadas.
static dados dado = {0};
static Valor_saida SAIDA = {0};

//Declaração de variáveis para receber valor da amostra processado
static int TENSAO = 0;
static int CORRENTE = 0;  

//Protótipo da função de interrupção para cada frame completo
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data);
//Pro tótipo da função de inicialização do ADC
static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle);
//Protótipo da tarefa de recebimento de amostras do ADC.
void Task_Adc(void *pvParameters);
//Protótipo da tarefa de cálculo do RMS
void Task_RMS(void *pvParameters);
//Protótipos das funções de cálculo do rms, media e tamanho
float media(float*dados, int tamanho);
float rms(float*dados, int tamanho);

//Protótipo da função de calibração do ADC
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle); // ADC calibration init
static void example_adc_calibration_deinit(adc_cali_handle_t handle);   

void app_main(void)
{
    memset(dado.result, 0xcc, EXAMPLE_READ_LEN); //Preencher o vetor de amostras brutas para facilitar debug

    fila_dados = xQueueCreate(50, sizeof(dados)); //Criação da fila para recebimento das amostras brutas
    if (fila_dados == NULL) {
        ESP_LOGI(TAG, "Erro ao criar a Fila!");
        return;
    }

// Criação da tarefa do ADC, fixada ao núcleo 1 do ESP32
    xTaskCreatePinnedToCore(
        Task_Adc,               // Função da tarefa
        "TarefaNoCore1",        // Nome da tarefa
        8192,                   // Tamanho da stack (em palavras, não bytes)
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

//DECLARAÇÃO DA FUNÇÕES

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle1, &mustYield);

    return (mustYield == pdTRUE);
};

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle){
    
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 48000,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 36000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i];
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;

   
};

void Task_Adc(void *pvParameters){

    esp_err_t ret;

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while(1){
            ret = adc_continuous_read(handle, dado.result, EXAMPLE_READ_LEN, &dado.num, 0);
            if (ret == ESP_OK) {
                ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, dado.num);
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
             for (int i = 0; i <recebido.num; i += SOC_ADC_DIGI_DATA_BYTES_PER_CONV ) {
                // Cast direto para acessar a amostra correta
                const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)(&recebido.result[i]);
                ESP_LOGI(TAG, "Channel: %d, Value: %d", p->type1.channel, p->type1.data);
                if (p->type1.channel < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                    //ESP_LOGI(TAG, "Channel: %d, Value: %d", p->type1.channel, p->type1.data);
                    if (do_calibration1 && p->type1.channel == 0 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_corrente1[j] = (float)CORRENTE;
                            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %f V", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_corrente1[j]);
                            j++; 
                        }else{
                            //ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO CANAL 0"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 4 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_corrente2[k] = (float)CORRENTE;
                            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_corrente2[k]);
                            k++; 
                        }else{
                            //ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO CANAL 4"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 6 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &CORRENTE));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_corrente3[n] = (float)CORRENTE;
                            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_corrente3[n]);
                            n++; 
                        }else{
                            //ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO CANAL 6"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 3 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_tensao1[m] = (float)TENSAO;
                            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_tensao1[m], m);
                            m++; 
                        }else{
                            //ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO CANAL 3"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 5 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_tensao2[v] = (float)TENSAO;
                            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_tensao2[v]);
                            v++; 
                        }else{
                            //ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO CANAL 5"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1 && p->type1.channel == 7 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            SAIDA.dados_tensao3[b] = (float)TENSAO;
                           // ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %f mV", ADC_UNIT_1 + 1, p->type1.channel, SAIDA.dados_tensao3[b]);
                           b++; 
                        }else{
                            //ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO CANAL 7"); // ADC1 LOG CALIBRAÇÃO
                        };

                } else {
                    ESP_LOGW(TAG, "Invalid data [Ch%d_%d]",
                            p->type1.channel,
                            p->type1.data);
                }
            }

       // ESP_LOGI(TAG, "Valor Tensao 1 RMS0 = %fmV", rms(SAIDA.dados_tensao1,TAMANHO(SAIDA.dados_tensao1))); 
        //ESP_LOGI(TAG, "Valor Tensao 2 RMS4 = %fmV", rms(SAIDA.dados_tensao2,TAMANHO(SAIDA.dados_tensao2))); 
        //ESP_LOGI(TAG, "Valor Corrente 3 RMS6 = %fmV", rms(SAIDA.dados_corrente3,TAMANHO(SAIDA.dados_corrente3))); 
        //ESP_LOGI(TAG, "Valor Corrente 1 RMS3 = %fmV", rms(SAIDA.dados_corrente1,TAMANHO(SAIDA.dados_corrente1))); 
       // ESP_LOGI(TAG, "Valor Corrente 2 RMS5 = %fmV", rms(SAIDA.dados_corrente2,TAMANHO(SAIDA.dados_corrente2))); 
        //ESP_LOGI(TAG, "Valor Corrente 3 RMS7 = %fmV", rms(SAIDA.dados_corrente3,TAMANHO(SAIDA.dados_corrente3))); 

        
        }else{
            ESP_LOGI(TAG, "VALOR PERDIDO NA FILA");
        }
    }
    
    if (do_calibration1)                                      // ADC1 Calibration Deinit
    {
        example_adc_calibration_deinit(adc1_cali_handle);     // ADC1 Calibration Deinit
    }
    
};

float media(float*dados, int tamanho){
    float mean = 0;
    for(int i=0; i< tamanho; i++){
        //ESP_LOGI(TAG, "VALOR  dado = %f  e iteracao = %d e media = %f" ,dados[i],i,mean);
        mean = mean + dados[i];
        
    }
    //ESP_LOGI(TAG, "VALOR  mean = %f " ,mean/tamanho);
    return (mean/tamanho);
};

float rms(float*dados, int tamanho){
    float mean = media(dados,tamanho);
    float rms = 0;
    for(int i = 0; i<tamanho; i++){
        rms = rms + pow((dados[i]-mean),2);
    };
    return sqrt(rms/tamanho);
};

// DECLARAÇÃO DAS FUNÇÕES DE CALIBRAÇÃO DO ADC
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
};
static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
};
