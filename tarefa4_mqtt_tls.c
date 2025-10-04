/*
Autor: STANLEY DE OLIVEIRA SOUZA
Descrição: Este projeto implementa um cliente MQTT seguro (TLS) rodando no Raspberry Pi Pico W, lendo botões e temperatura, publicando no broker Mosquitto.
*/
#include "pico/stdlib.h" 
#include "pico/cyw43_arch.h" 
#include "pico/unique_id.h" 
#include "hardware/gpio.h" 
#include "hardware/irq.h" 
#include "hardware/adc.h" 
#include "lwip/apps/mqtt.h" 
#include "lwip/apps/mqtt_priv.h" 
#include "lwip/dns.h" 
#include "lwip/altcp_tls.h"

#define ALUNOXX "aluno78"  //coloque aqui substitua aqui XX pelo final da sua matricula

// Temperatura
#ifndef TEMPERATURE_UNITS
#define TEMPERATURE_UNITS 'C'
#endif

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

typedef struct {
    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

#ifndef INFO_printf
#define INFO_printf printf
#endif
#ifndef ERROR_printf
#define ERROR_printf printf
#endif

#define TEMP_WORKER_TIME_S 10
#define MQTT_KEEP_ALIVE_S 60
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 1

#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "Bitdoglab"
#endif

#define MQTT_DEVICE_UNIQUE 0
#define BOTAO_A_PIN 5
#define BOTAO_B_PIN 6

static float read_onboard_temperature(const char unit) {
    const float conversionFactor = 3.3f / (1 << 12);
    float adc_val = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc_val - 0.706f) / 0.001721f;

    if (unit == 'F') return tempC * 9 / 5 + 32;
    return tempC;
}

static void pub_request_cb(__unused void *arg, err_t err) {
    if (err != 0) {
        ERROR_printf("pub_request_cb erro %d", err);
    }
}

static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
    return name;
}

static void publish_button_state(MQTT_CLIENT_DATA_T *state) {
    static int old_state = 0;
    int botao_estado = !gpio_get(BOTAO_A_PIN) || !gpio_get(BOTAO_B_PIN);
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/bitdoglab/botao", ALUNOXX);
    if (botao_estado != old_state) {
        old_state = botao_estado;
        char botao_state_str[2];
        snprintf(botao_state_str, sizeof(botao_state_str), "%d", botao_estado);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, topic), botao_state_str, strlen(botao_state_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        INFO_printf("Botao estado %s publicado em %s/bitdoglab/botao\n", botao_state_str, ALUNOXX);
    }
}

static void publish_temperature(MQTT_CLIENT_DATA_T *state) {
    static float old_temperature;
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/bitdoglab/temperatura", ALUNOXX);
    const char *temperature_key = full_topic(state, topic);
    float temperature = read_onboard_temperature(TEMPERATURE_UNITS);
    if (temperature != old_temperature) {
        old_temperature = temperature;
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);
        INFO_printf("Publicando %s para %s\n", temp_str, temperature_key);
        mqtt_publish(state->mqtt_client_inst, temperature_key, temp_str, strlen(temp_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)worker->user_data;
    publish_temperature(state);
    publish_button_state(state);
    async_context_add_at_time_worker_in_ms(context, worker, TEMP_WORKER_TIME_S * 1000);
}
static async_at_time_worker_t temperature_worker = { .do_work = temperature_worker_fn };

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;
        temperature_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &temperature_worker, 0);
    } else {
        panic("Falha na conexão MQTT");
    }
}

static void start_client(MQTT_CLIENT_DATA_T *state) {
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Usando TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Aviso: não usando TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        panic("Erro ao criar instância do cliente MQTT");
    }
    INFO_printf("Conectando ao servidor MQTT em %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("Erro de conexão MQTT");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    cyw43_arch_lwip_end();
}

static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    } else {
        panic("Falha na resolução de DNS");
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(5000);
    INFO_printf("Cliente MQTT iniciando...\n");

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    gpio_init(BOTAO_A_PIN);
    gpio_init(BOTAO_B_PIN);
    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_A_PIN);
    gpio_pull_up(BOTAO_B_PIN);

    static MQTT_CLIENT_DATA_T state;

    if (cyw43_arch_init()) {
        panic("Falha ao inicializar o CYW43");
    }

    char client_id_buf[64];
    if (MQTT_DEVICE_UNIQUE) {
        char unique_id_buf[5];
        pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
        for (int i = 0; i < sizeof(unique_id_buf) - 1; i++) unique_id_buf[i] = tolower(unique_id_buf[i]);
        snprintf(client_id_buf, sizeof(client_id_buf), "%s%s", MQTT_DEVICE_NAME, unique_id_buf);
    } else {
        strncpy(client_id_buf, MQTT_DEVICE_NAME, sizeof(client_id_buf));
        client_id_buf[sizeof(client_id_buf) - 1] = '\0';
    }
    INFO_printf("Nome do dispositivo %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S;
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;
#endif

#if LWIP_ALTCP && LWIP_ALTCP_TLS
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
            client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#else
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
#endif
#endif

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        panic("Falha ao conectar no Wi-Fi");
    }
    INFO_printf("Conectado ao Wi-Fi\n");

    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    if (err == ERR_OK) {
        start_client(&state);
    } else if (err != ERR_INPROGRESS) {
        panic("Falha na requisição DNS");
    }

    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst)) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));
    }

    INFO_printf("MQTT client exiting\n");
    return 0;
}