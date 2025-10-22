
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

#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_6
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

#define EXAMPLE_READ_LEN                    600

typedef struct{
    uint8_t result[EXAMPLE_READ_LEN];
    uint32_t num;

}dados;

typedef struct{
    float dados_tensao1[100];
    float dados_tensao2[100];
    float dados_tensao3[100];
}Valor_tensao;

static adc_channel_t channel[3]= {ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_3};
static QueueHandle_t fila_dados;


static TaskHandle_t s_task_handle1 = NULL, s_task_handle2 = NULL;
static const char *TAG = "EXAMPLE";
static dados dado = {0};
static Valor_tensao dados_tensao = {0};

static int TENSAO;  

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data);
static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle);
void Task_Adc(void *pvParameters);
void Task_RMS(void *pvParameters);
float media(float*dados, int tamanho);
float rms(float media,float*dados, int tamanho);

//PROTÓTIPO DA  CALIBRAÇÃO DO PWM
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle); // ADC calibration init
static void example_adc_calibration_deinit(adc_cali_handle_t handle);   

void app_main(void)
{
    memset(dado.result, 0xcc, EXAMPLE_READ_LEN);

    fila_dados = xQueueCreate(15, sizeof(dados));
    if (fila_dados == NULL) {
        ESP_LOGI(TAG, "Erro ao criar a Fila!");
        return;
    }


    xTaskCreatePinnedToCore(
        Task_Adc,           // Função da tarefa
        "TarefaNoCore1",        // Nome da tarefa
        2048,                   // Tamanho da stack (em palavras, não bytes)
        NULL,                   // Parâmetro para a tarefa
        2,                      // Prioridade
        &s_task_handle1,   // Handle da tarefa
        1                       // Core onde a tarefa será fixada (0 ou 1)
    );
     xTaskCreatePinnedToCore(
        Task_RMS,           // Função da tarefa
        "TarefaNoCore0",        // Nome da tarefa
        8192,                   // Tamanho da stack (em palavras, não bytes)
        NULL,                   // Parâmetro para a tarefa
        2,                      // Prioridade
        &s_task_handle2,   // Handle da tarefa
        0                       // Core onde a tarefa será fixada (0 ou 1)
    );


};

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle1, &mustYield);

    return (mustYield == pdTRUE);
};
static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle){
    
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 3600,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 18000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
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
    uint32_t ret_num = 0;

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
            ret = adc_continuous_read(handle, dado.result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK) {
                ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);
                dado.num=ret_num;
                if (xQueueSend(fila_dados, &dado, portMAX_DELAY) != pdPASS) {
                printf("Falha ao enviar para a fila\n");
                };
            } else if (ret == ESP_ERR_TIMEOUT) {
                //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        };
         vTaskDelay(pdMS_TO_TICKS(1));

    };

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
};

void Task_RMS(void *pvParameters){

    dados recebido;

    

    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_chan6_handle = NULL;    // ADC1 Calibration handle
    bool do_calibration1_chan6 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_6, ADC_ATTEN_DB_6, &adc1_cali_chan6_handle); // ADC1 Calibration Init
    adc_cali_handle_t adc1_cali_chan7_handle = NULL;    // ADC1 Calibration handle
    bool do_calibration1_chan7 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_7, ADC_ATTEN_DB_6, &adc1_cali_chan7_handle);
    adc_cali_handle_t adc1_cali_chan4_handle = NULL;    // ADC1 Calibration handle
    bool do_calibration1_chan4 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_4, ADC_ATTEN_DB_6, &adc1_cali_chan4_handle);

    while(1){
        int j =0, k = 0, n = 0; 

        if (xQueueReceive(fila_dados, &recebido, portMAX_DELAY) == pdPASS){
            
             for (int i = 0; i <recebido.num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                // Cast direto para acessar a amostra correta
                const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)(&recebido.result[i]);
                if (p->type1.channel < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                    //ESP_LOGI(TAG, "Channel: %d, Value: %d",
                         //   p->type1.channel,
                         //   p->type1.data);
                    if (do_calibration1_chan6 && p->type1.channel == 6 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan6_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            dados_tensao.dados_tensao1[j] = (float)TENSAO;
                          //  ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %f V", ADC_UNIT_1 + 1, p->type1.channel, dados_tensao.dados_tensao1[j]);
                            j++; 
                        }else{
                            // ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1_chan7 && p->type1.channel == 7 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan7_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            dados_tensao.dados_tensao2[k] = (float)TENSAO;
                            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, p->type1.channel, TENSAO);
                            k++; 
                        }else{
                           //  ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO"); // ADC1 LOG CALIBRAÇÃO
                        };
                    if (do_calibration1_chan4 && p->type1.channel == 3 ){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan4_handle, p->type1.data, &TENSAO));     // ADC1 CALIBRAÇÃO
                            dados_tensao.dados_tensao3[n] = (float)TENSAO;
                           // ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, p->type1.channel, TENSAO);
                            n++; 
                        }else{
                            // ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO"); // ADC1 LOG CALIBRAÇÃO
                        };

                vTaskDelay(pdMS_TO_TICKS(1));
                } else {
                    ESP_LOGW(TAG, "Invalid data [Ch%d_%d]",
                            p->type1.channel,
                            p->type1.data);
                }
            }
        ESP_LOGI(TAG, "Valor RMS6 = %fmV", rms(media(dados_tensao.dados_tensao1,(sizeof(dados_tensao.dados_tensao1)/sizeof(dados_tensao.dados_tensao1[0]))),dados_tensao.dados_tensao1,(sizeof(dados_tensao.dados_tensao1)/sizeof(dados_tensao.dados_tensao1[0])))); 
        ESP_LOGI(TAG, "Valor RMS7 = %fmV", rms(media(dados_tensao.dados_tensao2,(sizeof(dados_tensao.dados_tensao2)/sizeof(dados_tensao.dados_tensao2[0]))),dados_tensao.dados_tensao2,(sizeof(dados_tensao.dados_tensao2)/sizeof(dados_tensao.dados_tensao2[0])))); 
        ESP_LOGI(TAG, "Valor RMS3 = %fmV", rms(media(dados_tensao.dados_tensao3,(sizeof(dados_tensao.dados_tensao3)/sizeof(dados_tensao.dados_tensao3[0]))),dados_tensao.dados_tensao3,(sizeof(dados_tensao.dados_tensao3)/sizeof(dados_tensao.dados_tensao3[0])))); 
        
         
        }else{
            ESP_LOGI(TAG, "VALOR PERDIDO NA FILA");
        }
    }
    
    if (do_calibration1_chan6)                                      // ADC1 Calibration Deinit
    {
        example_adc_calibration_deinit(adc1_cali_chan6_handle);     // ADC1 Calibration Deinit
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

float rms(float media,float*dados, int tamanho){
    float rms = 0;
    for(int i = 0; i<tamanho; i++){
        rms = rms + pow((dados[i]-media),2);
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
