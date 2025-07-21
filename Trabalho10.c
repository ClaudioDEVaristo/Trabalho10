#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include <math.h>
#include "Trabalho10.pio.h"
#include "config_matriz.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
#include "lwip/tcp.h"
#include "html_body.h"
#include "html_index.h"
#include "html_temp.h"

#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa

// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 2
#define I2C_SCL_DISP 3
#define endereco 0x76

#define I2C_BAUDRATE 100000

#define led_r 13
#define led_g 11
#define led_b 12

#define valor_min 20
#define valor_max 80

#define WIFI_SSID "Crau crau do mal"
#define WIFI_PASSWORD "23092003"

typedef struct {
    uint8_t min;
    uint8_t max;
    volatile bool estado_bomba;
    uint8_t nivel_atual_Umid;
    absolute_time_t alarm_a;
    float umidade;
} nivel_temperatura;

nivel_temperatura nv = {valor_min, valor_max, false, 30, 0};

struct http_state{
    char response[8192];
    size_t len;
    size_t sent;
};

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);


// Função para calcular a altitude a partir da pressão atmosférica
double calculate_altitude(double pressure)
{
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
   // Fim do trecho para modo BOOTSEL com botão B

    stdio_init_all();

    gpio_init(led_r);
    gpio_set_dir(led_r, GPIO_OUT);
    gpio_init(led_g);
    gpio_set_dir(led_g, GPIO_OUT);
    gpio_init(led_b);
    gpio_set_dir(led_b, GPIO_OUT);

    PIO pio = pio_config(); //Config do PIO

    // Inicializa I2C0
    i2c_init(i2c0, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa I2C1
    i2c_init(i2c1, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    // Inicializa o BMP280
    bmp280_init(I2C_PORT_DISP);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT_DISP, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

    printf("Iniciando Wi-Fi\n");
    printf("Aguarde...\n");

    if (cyw43_arch_init()){
        printf("WiFi => FALHA");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)){
        printf("WiFi => ERRO");
        return 1;
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    printf("WiFi => OK\n");
    printf("ip: %s\n", ip_str);

    start_http_server();

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_tmp1[5];  // Buffer para armazenar a string
    char str_alt[5];  // Buffer para armazenar a string  
    char str_tmp2[5];  // Buffer para armazenar a string
    char str_umi[5];  // Buffer para armazenar a string      

    bool cor = true;
    while (1)
    {
        cyw43_arch_poll(); // mantém o Wi-Fi funcionando
        sleep_ms(10);      // espera um pouco (evita CPU 100%)

        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT_DISP, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // Cálculo da altitude
        double altitude = calculate_altitude(pressure);

        printf("Pressao = %.3f kPa\n", pressure / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temperature / 100.0);
        printf("Altitude estimada: %.2f m\n", altitude);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);

            nv.nivel_atual_Umid = data.humidity;
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }


        sprintf(str_tmp1, "%.1fC", temperature / 100.0);  // Converte o inteiro em string
        sprintf(str_alt, "%.0fm", altitude);  // Converte o inteiro em string
        sprintf(str_tmp2, "%.1fC", data.temperature);  // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", data.humidity);  // Converte o inteiro em string        

        sleep_ms(1000);
    }

    return 0;
}

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len){
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;

    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
        return ERR_OK;
    }

    size_t remaining = hs->len - hs->sent;
    size_t chunk = remaining > 1024 ? 1024 : remaining;
    err_t err = tcp_write(tpcb, hs->response + hs->sent, chunk, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        tcp_output(tpcb);
    } else {
        printf("Erro ao enviar chunk restante: %d\n", err);
    }

    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    if (!p){
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs){
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    if (strstr(req, "GET /api/data")) {
        char json_payload[128];
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{\"min\":%d,\"max\":%d,\"nivel_atual_Umid\":%d,\"estado_bomba\":%s}\r\n",
            nv.min, nv.max, nv.nivel_atual_Umid, nv.estado_bomba ? "true" : "false");
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            json_len, json_payload);

    } else if (strstr(req, "POST /api/limites")){
        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");
        if (min_str && max_str){
            min_str += 4; // Pular "min="
            max_str += 4; // Pular "max="

            nv.min = atoi(min_str);
            nv.max = atoi(max_str);

            if (nv.min >= nv.max){
                hs->len = snprintf(hs->response, sizeof(hs->response),
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n");
            }
        }
    } else if (strstr(req, "GET / ") || strstr(req, "GET /index")){
       size_t html_len = strlen(HTML_INDEX);

        int hdr_len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n", html_len);

        memcpy(hs->response + hdr_len, HTML_INDEX, html_len);

        hs->len = hdr_len + html_len;
    }

    else if (strstr(req, "GET /umidade")){
       size_t html_len = strlen(HTML_BODY);

        int hdr_len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n", html_len);

        memcpy(hs->response + hdr_len, HTML_BODY, html_len);

        hs->len = hdr_len + html_len;
        hs->sent = 0;

        tcp_arg(tpcb, hs);
        tcp_sent(tpcb, http_sent); // chama http_sent() após cada envio

        // envia apenas o primeiro pedaço (até 1024 bytes)
        size_t chunk = hs->len > 1024 ? 1024 : hs->len;
        tcp_write(tpcb, hs->response, chunk, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        hs->sent = chunk;

        pbuf_free(p);
        return ERR_OK;
    }

    else if (strstr(req, "GET /temperatura")){
       size_t html_len = strlen(HTML_TEMP);

        int hdr_len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n", html_len);

        memcpy(hs->response + hdr_len, HTML_TEMP, html_len);

        hs->len = hdr_len + html_len;
        hs->sent = 0;

        tcp_arg(tpcb, hs);
        tcp_sent(tpcb, http_sent); // chama http_sent() após cada envio

        // envia apenas o primeiro pedaço (até 1024 bytes)
        size_t chunk = hs->len > 1024 ? 1024 : hs->len;
        tcp_write(tpcb, hs->response, chunk, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        hs->sent = chunk;

        pbuf_free(p);
        return ERR_OK;
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);

    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}


static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err){
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void start_http_server(void){
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb){
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK){
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}