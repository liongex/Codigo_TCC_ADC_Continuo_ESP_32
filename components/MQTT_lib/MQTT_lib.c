#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "mqtt_client.h"
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

void mqtt_start(void){

    status_mqtt_event_group = xEventGroupCreate();
    //Configuração da estrutura do cliente MQTT
    esp_mqtt_client_config_t esp_mqtt_client_cfg = {
        .network.disable_auto_reconnect = false,                    // Habilitar reconexão 
        .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
        .broker.address.port = 1883,
        .session.keepalive = 10,                                    // Padrão é 120 segundos
        .session.last_will = {
            .topic = "ELE0629/Weather/Status",
            .msg = "ADEUS",
            .msg_len = strlen("Offline"),
            .retain = 0
        }
    };

    client = esp_mqtt_client_init(&esp_mqtt_client_cfg);

    //Event handler para conexão MQTT
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    //Primeira mensagem: enviar para UFRN/Lab/status o valor Online
    ESP_LOGI(TAG, "Primeira publicação. Setando Online!");
    mqtt_publish("ELE0629/Weather/Status", "Online", 0, 0);
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