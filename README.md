# ESP32 Continuous ADC & RMS Monitor

Este projeto implementa um sistema de aquisição de dados analógicos de alta velocidade e precisão para o ESP32. Utilizando o modo **ADC Contínuo (DMA)** e o framework **ESP-IDF**, o firmware é capaz de amostrar simultaneamente 6 canais analógicos para monitoramento de Tensão e Corrente, calculando o valor RMS (Root Mean Square) em tempo real.

A arquitetura foi desenhada com foco em estabilidade e desempenho, dividindo a carga de trabalho entre os dois núcleos do processador através de tarefas do **FreeRTOS** sincronizadas por filas (Queues).

## 🚀 Principais Funcionalidades

*   **Amostragem de Alta Frequência via DMA:** Configurado para uma taxa global de 36 kHz, garantindo 6.000 amostras por segundo para cada um dos 6 canais, sem bloquear o processador.
*   **Calibração de Hardware:** Integração com os esquemas nativos de calibração do ESP32 (`Curve Fitting` e `Line Fitting`) para converter dados brutos do ADC em valores reais de tensão (mV) compensando as não-linearidades do silício.
*   **Processamento Multicore:**
    *   **Core 1:** Dedicado à coleta de dados críticos em tempo real e alimentação de filas (Buffer).
    *   **Core 0:** Dedicado ao processamento matemático (Média, offset e RMS) e logs de saída.
*   **Cálculo RMS:** Funções customizadas para processar blocos de 100 amostras por canal e extrair o valor Root Mean Square preciso.
*   **Proteção contra Overflow:** Monitoramento ativo do *pool* de memória do ADC contínuo com sistema de interrupção e *flags* (ISRs) para alertar perdas de frames em caso de lentidão do processador.

## 📌 Mapeamento de Canais e Pinos

O sistema monitora 3 fases/sensores de tensão e 3 fases/sensores de corrente, mapeados estritamente na unidade `ADC_UNIT_1`.

| Grandeza Monitorada | Variável Interna | Canal ADC | GPIO Padrão (ESP32) |
| :--- | :--- | :--- | :--- |
| Corrente 1 | `dados_corrente1` | `ADC_CHANNEL_0` | GPIO 36 (VP) |
| Tensão 1 | `dados_tensao1` | `ADC_CHANNEL_3` | GPIO 39 (VN) |
| Corrente 2 | `dados_corrente2` | `ADC_CHANNEL_4` | GPIO 32 |
| Tensão 2 | `dados_tensao2` | `ADC_CHANNEL_5` | GPIO 33 |
| Corrente 3 | `dados_corrente3` | `ADC_CHANNEL_6` | GPIO 34 |
| Tensão 3 | `dados_tensao3` | `ADC_CHANNEL_7` | GPIO 35 |

> **Nota de Hardware:** Devido a restrições do ADC contínuo, a inicialização dos canais no código respeita obrigatoriamente a ordem crescente dos registradores `{0, 3, 4, 5, 6, 7}` e lê 2 bytes (16-bits) por amostra (`type1`), garantindo o alinhamento correto da memória DMA.

## 🛠️ Pré-requisitos e Dependências

*   **Hardware:** Placa de desenvolvimento baseada no SoC ESP32 (padrão).
*   **Framework:** [ESP-IDF v5.0+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) (Requer as APIs modernas de `esp_adc/adc_continuous.h` e `esp_adc/adc_cali.h`).
*   **Sensores:** Transformadores de Corrente (TCs) e Transformadores de Potencial (TPs) ou divisores de tensão com saída máxima limitada a ~3.3V, adequados para a atenuação de 2.5dB (`ADC_ATTEN_DB_2_5`).

## ⚙️ Arquitetura de Software

A lógica principal está dividida em duas tarefas principais (`Tasks`) do FreeRTOS:

1.  `Task_Adc`: Aguarda um sinal (Notification) da interrupção de hardware indicando que um *frame* de 1200 bytes (100 amostras x 6 canais x 2 bytes) está pronto. Ela empurra esses bytes brutos para a `fila_dados`.
2.  `Task_RMS`: Lê a fila continuamente, aplica o *casting* nos bytes para decodificar os 16-bits, identifica o ID do canal, realiza a calibração (`adc_cali_raw_to_voltage`), agrupa os dados nas matrizes da `struct` correspondente e, finalmente, calcula a métrica RMS imprimindo no terminal.

## 💻 Como Compilar e Rodar

Clone o repositório e utilize as ferramentas de linha de comando do ESP-IDF:
```bash
# Compile o projeto
idf.py build

# Faça o flash para a placa (substitua PORT pela sua porta serial, ex: /dev/ttyUSB0 ou COM3)
idf.py -p PORT flash

# Abra o monitor serial para visualizar as leituras
idf.py -p PORT monitor

---

Desenvolvido usando ESP-IDF com FreeRTOS, este exemplo é ideal para aplicações que necessitam de leituras rápidas e contínuas de sinais analógicos, como monitoramento de sensores analógicos ou aquisição de dados para processamento em tempo real.

