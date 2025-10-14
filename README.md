# Exemplo de ADC Contínuo com ESP-IDF

Este projeto demonstra o uso do driver ADC contínuo do ESP-IDF para realizar aquisições contínuas de sinais analógicos no ESP32, utilizando FreeRTOS para gerenciamento de tarefas.

## Funcionalidades

- Configuração do ADC contínuo na unidade ADC1, canal 6.
- Aquisição contínua de dados analógicos com taxa de amostragem de 6 kHz.
- Uso de uma tarefa FreeRTOS fixada no core 1 para leitura e processamento dos dados ADC.
- Utilização de callback para notificação da conclusão das conversões ADC.
- Impressão dos valores de ADC lidos no log do ESP-IDF.

## Componentes principais do código

- `continuous_adc_init()`: inicializa o driver do ADC contínuo com configuração de canais, atenuação e frequência de amostragem.
- `s_conv_done_cb()`: callback que notifica a tarefa que a conversão de ADC foi concluída.
- `Task_Adc()`: tarefa responsável por iniciar a aquisição contínua, ler e exibir os dados do ADC.
- `app_main()`: função principal que cria e inicia a tarefa do ADC, fixando-a no core 1.

## Requisitos

- ESP-IDF versão compatível com driver ADC contínuo (ESP-IDF v5.x recomendada).
- Placa ESP32 com suporte ADC na unidade 1, canal 6.

## Instruções para uso

1. Configure o ambiente ESP-IDF e conecte seu ESP32.
2. Compile o projeto com `idf.py build`.
3. Faça o flash no dispositivo com `idf.py flash monitor`.
4. Observe no monitor serial as leituras contínuas do ADC no canal configurado.

## Configurações possíveis

- Modifique o `channel` para alterar o canal de ADC.
- Ajuste `EXAMPLE_READ_LEN` para alterar o tamanho do buffer de leitura.
- Altere a frequência de amostragem em `dig_cfg.sample_freq_hz` conforme necessidade.

## Licença

Este código está licenciado sob a licença CC0 1.0 Universal (Domínio Público). Consulte o cabeçalho do arquivo fonte para mais detalhes.

---

Desenvolvido usando ESP-IDF com FreeRTOS, este exemplo é ideal para aplicações que necessitam de leituras rápidas e contínuas de sinais analógicos, como monitoramento de sensores analógicos ou aquisição de dados para processamento em tempo real.

