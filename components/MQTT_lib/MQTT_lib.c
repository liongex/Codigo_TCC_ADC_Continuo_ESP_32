#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"  
#include "MQTT_lib.h"

static const char *TAG = "Biblioteca MQTT";

// Handle para o client MQTT
static esp_mqtt_client_handle_t client;

// Grupo de eventos e variáveis para checar conexão mqtt
static EventGroupHandle_t status_mqtt_event_group;
#define MQTT_CONNECTED BIT0

static void mqtt_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id){
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(status_mqtt_event_group, MQTT_CONNECTED);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(status_mqtt_event_group, MQTT_CONNECTED);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED from msg_id = %d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED from msg_id = %d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("topic: %.*s\n", event->topic_len, event->topic);
        printf("message: %.*s\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "ERROR %s", strerror(event->error_handle->esp_transport_sock_errno));
        break;
    default:
        ESP_LOGI(TAG, "EVENTO DESCONHECIDO id:%d", event->event_id);
        break;
    }    
}

#if USE_MQTT_TLS_PSK
// Converte a chave hexadecimal string do usuário para bytes puros em memória
static const uint8_t psk_key_bytes[] = { 0xAB, 0xCD, 0x44, 0xEF, 0x12, 0x34, 0x56, 0x78 };

static const struct psk_key_hint psk_config_bancada = {
    .key = psk_key_bytes,
    .key_size = sizeof(psk_key_bytes), 
    .hint = "isac" 
};
#endif
void mqtt_start(void){

    status_mqtt_event_group = xEventGroupCreate();

    #if USE_MQTT_TLS_PSK
    // =========================================================================
    // CONFIGURAÇÃO MODO SEGURO: MQTTS via TLS-PSK (Porta 8844)
    // =========================================================================
    esp_mqtt_client_config_t esp_mqtt_client_cfg = {
        .network.disable_auto_reconnect = false,
        .broker.address.uri = "mqtts://192.168.0.9", 
        .broker.address.port = 8844,
        .broker.verification.psk_hint_key = &psk_config_bancada, 
        .session.keepalive = 10,
        .session.last_will = {
            .topic = "casa/temp",
            .msg = "{\"status\":\"Offline por Falha Critica\"}",
            .msg_len = strlen("{\"status\":\"Offline por Falha Critica\"}"),
            .retain = 0
        }
    };
    #else
    // =========================================================================
    // CONFIGURAÇÃO MODO CONVENCIONAL: TCP Puro com Autenticação Simples (Porta 1844)
    // =========================================================================
    esp_mqtt_client_config_t esp_mqtt_client_cfg = {
        .network.disable_auto_reconnect = false,
        .broker.address.uri = "mqtt://192.168.0.9", 
        .broker.address.port = 1844,
        .credentials.username = "isac",
        .credentials.authentication.password = "isac",
        .session.keepalive = 10,
        .session.last_will = {
            .topic = "casa/temp",
            .msg = "{\"status\":\"Offline\"}",
            .msg_len = strlen("{\"status\":\"Offline\"}"),
            .retain = 0
        }
    };
    #endif

    client = esp_mqtt_client_init(&esp_mqtt_client_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    #if USE_MQTT_TLS_PSK
    // Renderizado como texto simples para evitar LaTeX desnecessário
    ESP_LOGI(TAG, "Conexao iniciada em MODO SEGURO (MQTTS via TLS-PSK) na porta 8844.");
    #else
    // Renderizado como texto simples para evitar LaTeX desnecessário
    ESP_LOGI(TAG, "Conexao iniciada em MODO CONVENCIONAL (TCP / User-Pass) na porta 1844.");
    #endif
}

void mqtt_subscribe(char *topic, int qos){
    // TODO > O QUE FAZER QUANDO A INSCRICAO DER ERRO
    int msg_id = esp_mqtt_client_subscribe(client, topic, qos);
    ESP_LOGI(TAG, "ID da inscrição = %d", msg_id);
}

void mqtt_unsubscribe(char *topic){

    int msg_id = esp_mqtt_client_unsubscribe(client, topic);
    ESP_LOGI(TAG, "ID da desinscrição = %d", msg_id);
}

void mqtt_publish(char *topic, char *payload, int qos, int retain){

    int msg_id = esp_mqtt_client_publish(client, topic, payload, strlen(payload), qos, retain);
    ESP_LOGI(TAG, "ID da publicação = %d", msg_id);
}

int mqtt_connected(void){
    EventBits_t bits = xEventGroupGetBits(status_mqtt_event_group);

    if(bits & MQTT_CONNECTED){
        return 1;
    }else {
        return 0;
    }
}