
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
 * @file FUNCTIONS.h
 * @brief Esse componente possui funções de cálculo como valor rms e média, que serão usadas no programa.
 *
 */
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define TAMANHO(x) (sizeof(x) / sizeof((x)[0])) // Macro para obter tamanho da matriz

/**
 * @brief Protótipo da função de média.
 * @param[in] dados Ponteiro para a primeira posição de um vetor com os dados.
 * @param[in] tamanho Inteiro com o tamanho do vetor.
 * @return Retorna uma variável de ponto flutuante com o valor da média.
 */

float media(float*dados, int tamanho);

/**
 * @brief Protótipo da função de rms.
 * @param[in] dados Ponteiro para a primeira posição de um vetor com os dados.
 * @param[in] tamanho Inteiro com o tamanho do vetor.
 * @return Retorna uma variável de ponto flutuante com o valor da média.
 */

float rms(float*dados, int tamanho);


#endif //FUNCTIONS_H