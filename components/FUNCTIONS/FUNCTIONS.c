#include <stdio.h>
#include <string.h>

#include <math.h>

float media(float*dados, int tamanho){

    float sum = 0.0f; // Use .0f para garantir que o compilador trate como float
    for(int i = 0; i < tamanho; i++){
        sum += dados[i];
    }
    return sum / tamanho;
};

float rms(float*dados, int tamanho){

    float mean = media(dados, tamanho);
    float sum_sq = 0.0f;
    
    for(int i = 0; i < tamanho; i++){
        float diff = dados[i] - mean;
        sum_sq += (diff * diff); // MUITO mais rápido que pow()
    }
    
    return sqrtf(sum_sq / tamanho); // Usando sqrtf() para aproveitar a FPU do ESP32
};

float VReal_tensao(float Vrms) {
    // 22.951219 * 38.33 = 879.719
    return (Vrms * 879.719f)/1000; 
}

float VReal_corrente(float Irms) {
    return (Irms * 145.161f)/1000;
}