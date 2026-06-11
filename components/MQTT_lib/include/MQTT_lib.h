#ifndef MQTT_LIB_H
#define MQTT_LIB_H

// SELEÇÃO DO MODO DE TRANSPORTE E SEGURANÇA
// Defina como 0: Modo convencional (Porta 1844, Usuário e Senha, Sem Criptografia)
// Defina como 1: Modo avançado industrial (Porta 8844, TLS-PSK Criptografado, Identity e Chave)
#define USE_MQTT_TLS_PSK 0

void mqtt_start(void);
void mqtt_subscribe(char *topic, int qos);
void mqtt_unsubscribe(char *topic);
void mqtt_publish(char *topic, char *payload, int qos, int retain);
int mqtt_connected(void);
void mqtt_disconect(void);

#endif // MQTT_LIB_H