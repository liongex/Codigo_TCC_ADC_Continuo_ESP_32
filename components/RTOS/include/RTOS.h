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
 * @file RTOS.h
 * @brief Esse componente implementa uma interface para inicializar e controlar as tarefas usando o freeRTOS.
 *
 */

#ifndef RTOS_H
#define RTOS_H

#include "ADC.h"
#include "wifi.h"
#include "MQTT_lib.h"
#include "FUNCTIONS.h"

/**
 * @brief Protótipo da função de callback para o caso de um frame completo.
 * @param[in] handle handle (referência/ponteiro opaco) para o driver do ADC contínuo que está chamando o callback.
 * @param[in] channel_num Estrutura que contém os dados do evento disparado pelo ADC.
 * @param[in] user_data Ponteiro genérico para dados definidos pelo usuário.
 * @return O retorno da função é true se mustYield == pdTRUE, ou seja, se for necessário realizar um context switch imediato.
 */

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data);

/**
 * @brief Protótipo da função de callback para o caso de um frame completo.
 * @param[in] handle handle (referência/ponteiro opaco) para o driver do ADC contínuo que está chamando o callback.
 * @param[in] channel_num Estrutura que contém os dados do evento disparado pelo ADC.
 * @param[in] user_data Ponteiro genérico para dados definidos pelo usuário.
 * @return Retorna false pois não estamos acordando nenhuma task diretamente aqui.
 */

static bool IRAM_ATTR s_pool_ovf_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data);

/**
 * @brief Protótipo da tarefa de recebimento de amostras do ADC.
 */

void Task_Adc(void *pvParameters);

/**
 * @brief Protótipo da tarefa de cálculo do RMS
 */

void Task_RMS(void *pvParameters);

/**
 * @brief Protótipo da função de inicialização do sistema
 */
void Start(void);

#endif //RTOS_H