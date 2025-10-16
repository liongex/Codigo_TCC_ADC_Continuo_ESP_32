
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

#define EXAMPLE_READ_LEN                    256

static adc_channel_t channel= ADC_CHANNEL_6;

static TaskHandle_t s_task_handle = NULL;
static const char *TAG = "EXAMPLE";
static uint8_t result[EXAMPLE_READ_LEN] = {0};

static int TENSAO;  

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data);
static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle);
void Task_Adc(void *pvParameters);

//PROTÓTIPO DA  CALIBRAÇÃO DO PWM
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle); // ADC calibration init
static void example_adc_calibration_deinit(adc_cali_handle_t handle);   

void app_main(void)
{
    memset(result, 0xcc, EXAMPLE_READ_LEN);

    xTaskCreatePinnedToCore(
        Task_Adc,           // Função da tarefa
        "TarefaNoCore1",        // Nome da tarefa
        2048,                   // Tamanho da stack (em palavras, não bytes)
        NULL,                   // Parâmetro para a tarefa
        2,                      // Prioridade
        &s_task_handle,   // Handle da tarefa
        1                       // Core onde a tarefa será fixada (0 ou 1)
    );


};

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){
    
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
};
static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle){
    
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = ADC_CHANNEL_6;
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
    continuous_adc_init(&channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_chan6_handle = NULL;    // ADC1 Calibration handle
    bool do_calibration1_chan6 = example_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_6, ADC_ATTEN_DB_6, &adc1_cali_chan6_handle); // ADC1 Calibration Init

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while(1){
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK) {
                ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);

            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                // Cast direto para acessar a amostra correta
                const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)(&result[i]);
                if (p->type1.channel < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                    ESP_LOGI(TAG, "Channel: %d, Value: %d",
                            p->type1.channel,
                            p->type1.data);
                    if (do_calibration1_chan6){
                            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan6_handle, p->type1.data,&TENSAO));            // ADC1 CALIBRAÇÃO
                            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_6, TENSAO); // ADC1 LOG CALIBRAÇÃO
                        }else{
                             ESP_LOGI(TAG, "NAO ATIVOU A CALIBRACAO"); // ADC1 LOG CALIBRAÇÃO
                        };

                        
                } else {
                    ESP_LOGW(TAG, "Invalid data [Ch%d_%d]",
                            p->type1.channel,
                            p->type1.data);
                }
            }

                vTaskDelay(1);
            } else if (ret == ESP_ERR_TIMEOUT) {
                //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }

        };

    };

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));

    if (do_calibration1_chan6)                                      // ADC1 Calibration Deinit
    {
        example_adc_calibration_deinit(adc1_cali_chan6_handle);     // ADC1 Calibration Deinit
    }
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
