#include <string.h>

//Includes FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

//Includes ESP-IDF
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

//Include LwIP
#include "lwip/err.h"
#include "lwip/sys.h"

// Nossa Biblioteca
#include "wifi.h"

// Credenciais do Wi-Fi definidas no menuconfig
#define EXAMPLE_ESP_WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASSWORD   CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY   CONFIG_ESP_MAXIMUM_RETRY

// Definição dos bits para o grupo de eventos (BIT0 e BIT1 já são macros, não precisamos realizar deslocamento de bits)
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

//Sincronizar as tarefas conforme os eventos WiFi ocorrem
static EventGroupHandle_t s_wifi_event_group;

// TAG para info
static const char *TAG = "Biblioteca Wi-Fi";

// Quantidade de tentativas para conexão
static int s_retry_num = 0;

//Tentando se reconectar
static bool reconnect = false;
int wifi_connect_status = 0;
static esp_netif_t *esp_netif;

// Motivo e tratamento da desconexão
char *get_wifi_disconnection_string(wifi_err_reason_t wifi_err_reason)
{
    switch (wifi_err_reason)
    {
    case WIFI_REASON_UNSPECIFIED:
        return "WIFI_REASON_UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:
        return "WIFI_REASON_AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:
        return "WIFI_REASON_AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE:
        return "WIFI_REASON_ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY:
        return "WIFI_REASON_ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED:
        return "WIFI_REASON_NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED:
        return "WIFI_REASON_NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE:
        return "WIFI_REASON_ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
        return "WIFI_REASON_ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
        return "WIFI_REASON_DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
        return "WIFI_REASON_DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC:
        return "WIFI_REASON_BSS_TRANSITION_DISASSOC";
    case WIFI_REASON_IE_INVALID:
        return "WIFI_REASON_IE_INVALID";
    case WIFI_REASON_MIC_FAILURE:
        return "WIFI_REASON_MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        return "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        return "WIFI_REASON_IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID:
        return "WIFI_REASON_GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
        return "WIFI_REASON_PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID:
        return "WIFI_REASON_AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
        return "WIFI_REASON_UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP:
        return "WIFI_REASON_INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED:
        return "WIFI_REASON_802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
        return "WIFI_REASON_CIPHER_SUITE_REJECTED";
    case WIFI_REASON_TDLS_PEER_UNREACHABLE:
        return "WIFI_REASON_TDLS_PEER_UNREACHABLE";
    case WIFI_REASON_TDLS_UNSPECIFIED:
        return "WIFI_REASON_TDLS_UNSPECIFIED";
    case WIFI_REASON_SSP_REQUESTED_DISASSOC:
        return "WIFI_REASON_SSP_REQUESTED_DISASSOC";
    case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT:
        return "WIFI_REASON_NO_SSP_ROAMING_AGREEMENT";
    case WIFI_REASON_BAD_CIPHER_OR_AKM:
        return "WIFI_REASON_BAD_CIPHER_OR_AKM";
    case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION:
        return "WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION";
    case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS:
        return "WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS";
    case WIFI_REASON_UNSPECIFIED_QOS:
        return "WIFI_REASON_UNSPECIFIED_QOS";
    case WIFI_REASON_NOT_ENOUGH_BANDWIDTH:
        return "WIFI_REASON_NOT_ENOUGH_BANDWIDTH";
    case WIFI_REASON_MISSING_ACKS:
        return "WIFI_REASON_MISSING_ACKS";
    case WIFI_REASON_EXCEEDED_TXOP:
        return "WIFI_REASON_EXCEEDED_TXOP";
    case WIFI_REASON_STA_LEAVING:
        return "WIFI_REASON_STA_LEAVING";
    case WIFI_REASON_END_BA:
        return "WIFI_REASON_END_BA";
    case WIFI_REASON_UNKNOWN_BA:
        return "WIFI_REASON_UNKNOWN_BA";
    case WIFI_REASON_TIMEOUT:
        return "WIFI_REASON_TIMEOUT";
    case WIFI_REASON_PEER_INITIATED:
        return "WIFI_REASON_PEER_INITIATED";
    case WIFI_REASON_AP_INITIATED:
        return "WIFI_REASON_AP_INITIATED";
    case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT:
        return "WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT";
    case WIFI_REASON_INVALID_PMKID:
        return "WIFI_REASON_INVALID_PMKID";
    case WIFI_REASON_INVALID_MDE:
        return "WIFI_REASON_INVALID_MDE";
    case WIFI_REASON_INVALID_FTE:
        return "WIFI_REASON_INVALID_FTE";
    case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED:
        return "WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED";
    case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED:
        return "WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "WIFI_REASON_BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:
        return "WIFI_REASON_NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:
        return "WIFI_REASON_AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:
        return "WIFI_REASON_ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "WIFI_REASON_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
        return "WIFI_REASON_CONNECTION_FAIL";
    case WIFI_REASON_AP_TSF_RESET:
        return "WIFI_REASON_AP_TSF_RESET";
    case WIFI_REASON_ROAMING:
        return "WIFI_REASON_ROAMING";
    case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG:
        return "WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG";
    case WIFI_REASON_SA_QUERY_TIMEOUT:
        return "WIFI_REASON_SA_QUERY_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        return "WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY";
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        return "WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD";
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
        return "WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD";
    }
    return "UNKNOWN";
}

//Função para tratamento dos eventos
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){

    // Verificar se o evento gerado é Wi-Fi e qual o motivo
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        esp_wifi_connect();
    }else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        
        // Estrutura para o evento da desconexão no modo station
        wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = event_data;

        // Visualizando o motivo da desconexão
        ESP_LOGW(TAG, "Motivo da Desconexão %d: %s", wifi_event_sta_disconnected->reason, get_wifi_disconnection_string(wifi_event_sta_disconnected->reason));
        
        if(reconnect){
            //Tentar novamente conexão se o motivo for:
//            if(wifi_event_sta_disconnected->reason == WIFI_REASON_NO_AP_FOUND ||
//            wifi_event_sta_disconnected->reason == WIFI_REASON_ASSOC_LEAVE ||
//            wifi_event_sta_disconnected->reason == WIFI_REASON_AUTH_EXPIRE){

                // Tentando reconectar
                if(s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY){
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "Tentando se conectar %d", s_retry_num);
                }else{
                    // Caso não consiga conexão indica ao grupo de eventos
                    ESP_LOGI(TAG, "Falha na conexão");
                    wifi_connect_status = 0;
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
//            }
        }    

    }else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        
        // Verificando se o evento é referente ao IP
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Pegamos um IP " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connect_status = 1;
        s_retry_num = 0;

        //Depois de ter conseguido IP a conexão foi realizada com sucesso. Indica ao grupo de eventos
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){

    // Verificar se o evento gerado é Wi-Fi e qual o motivo
    if(event_id == WIFI_EVENT_AP_STACONNECTED){
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "MAC do Station: "MACSTR" CONECTADO, AID = %d", MAC2STR(event->mac), event->aid);
    } else if(event_id == WIFI_EVENT_AP_STADISCONNECTED){
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "MAC do Station: "MACSTR" DESCONECTADO, AID = %d", MAC2STR(event->mac), event->aid);
    }
}

void wifi_init(){

    // Init Phase - Wi-Fi Driver and LwIP
    ESP_ERROR_CHECK(esp_netif_init());                       // LwIP
    ESP_ERROR_CHECK(esp_event_loop_create_default());        // Event loop for event task   
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();     // Driver Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Manipulador de eventos WiFi
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Realizando o storage na RAM. Deixa a conexão um pouco mais rápida.
    // É uma outra forma de configurar, ao invés de modo persistente na NVS.
    // Obs: Perdendo a conexão é preciso realizar a configurações novamente (executa essa mesma rotina).
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}

void wifi_init_sta(void){

    wifi_init();
    reconnect = true;
        
    s_wifi_event_group = xEventGroupCreate(); 
    esp_netif = esp_netif_create_default_wifi_sta();    // Cria LwIP para config station

    // Configuração da estrutura do WiFi modo sta com as credenciais e autenticações
    wifi_config_t wifi_config = {
        .sta ={
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg ={
                .capable = true,
                .required = false,
            },
        },
    };

    // Definição do modo station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //Configuração do Wifi a partir da estrutura previamente definida
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    // Start no WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Configuração e inicialização do Wi-Fi finalizado!!!");

    // Esperando os eventos do Wi-Fi para seguir com tratamento
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if(bits & WIFI_CONNECTED_BIT){
        ESP_LOGI(TAG, "CONECTADO A SSID: %s password: %s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASSWORD);

    }else if(bits & WIFI_FAIL_BIT){
        ESP_LOGI(TAG, "FALHA AO SE CONECTAR A SSID: %s password: %s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASSWORD);

    }else {
        ESP_LOGI(TAG, "EVENTO INESPERADO!!!");
    }
}

void wifi_init_ap(const char *ssid, const char *pass){
    
    // Função de configuração
    wifi_init();
    ESP_LOGI(TAG, "INICIALIZANDO O WI-FI!");
    esp_netif = esp_netif_create_default_wifi_ap();     // LwIP com configurações básicas para o modo AP.

    // Wi-Fi Configuration Phase
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL, NULL));
    
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char *)wifi_config.ap.password, pass, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 5;
    wifi_config.ap.beacon_interval = 100;   // Deve ser múltiplo de 100. Faixa de 100 a 600000. Unit: TU(time unit, 1 TU = 1024 us).
    wifi_config.ap.channel = 1;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    // Wi-Fi start Phase
    esp_wifi_start();

    ESP_LOGI(TAG, "ESP32 COMO AP PRONTO! CONECTAR AO SSID: %s, PASSWORD: %s E CANAL: %d", 
            wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);


}

void wifi_disconnect(){
    reconnect = false;
    esp_wifi_stop();
    esp_netif_destroy(esp_netif);
}

int wifi_conect_status(){
    return wifi_connect_status;
}