/*
 * MIT License
 *
 * Copyright (c) 2026 Isac L Feliciano
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

 /**
 * @file ADC.h
 * @brief Esse componente implementa uma interface para inicializar e configurar o ADC do esp32 no modo contínuo.
 *
 */

#ifndef ADC_H
#define ADC_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"


#define EXAMPLE_ADC_DATA_LENGTH               2 // Garente que apenas 2 Bytes sejam lidos por vez.
#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1 // Declara a utilização do ADC1 do esp21
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1 // Garante que apenas o ADC1 será utilizado para a conversão
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_2_5 // Atenuação de 2.5dB
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH // Garente o uso de 12bits para cada amostra
#define EXAMPLE_READ_LEN                    1200 // Tamanho para cada frame, considerando 100 amostras por canal, 2 bytes por amostra e 6 canais.


/**
 * @brief Struct criada para armazenar os valores de saída do ADC
 */

typedef struct{
    uint8_t result[EXAMPLE_READ_LEN];
    uint32_t num;

}dados;

/**
 * @brief Struct criado para armazenar as amostras processadas de tensão e corrente para cada canal.
 */

typedef struct{ 
    float dados_tensao1[100];   // DEVERÁ SER LIDO NO CANAL 3
    float dados_tensao2[100];   // DEVERÁ SER LIDO NO CANAL 5
    float dados_tensao3[100];   // DEVERÁ SER LIDO NO CANAL 7
    float dados_corrente1[100]; // DEVERÁ SER LIDO NO CANAL 0
    float dados_corrente2[100]; // DEVERÁ SER LIDO NO CANAL 4
    float dados_corrente3[100]; // DEVERÁ SER LIDO NO CANAL 6

}Valor_saida;

/**
 * @brief Protótipo da função de inicialização do ADC
 * @param[in] channel Ponteiro para um array de canais de ADC que você deseja configurar.
 * @param[in] channel_num Número de canais no array passado em channel.
 * @param[in] out_handle Ponteiro para um handle que será preenchido pela função.
 */

void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle);

/**
 * @brief Protótipo da função de inicialização de calibração do ADC
 * @param[in] unit Especifica qual unidade de ADC será calibrada.
 * @param[in] channel Indica o canal específico dentro da unidade de ADC.
 * @param[in] atten Define a atenuação aplicada ao sinal de entrada.
 * @param[in] out_handle Ponteiro para um handle de calibração que será preenchido pela função.
 */

bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);

/**
 * @brief Protótipo da função de inicialização de calibração do ADC
 * @param[in] handle É o handle de calibração que você recebeu ao chamar example_adc_calibration_init.
 * 
 */

void example_adc_calibration_deinit(adc_cali_handle_t handle);   

#endif //ADC_H