#include <stdio.h>
#include <string.h>

#include <math.h>

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