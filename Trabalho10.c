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
#include "html_pressao.h"

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
#define buzz 10 //GPIO para o buzzer

float valor_min_temp = 20.0;
float valor_max_temp = 30.0;
float valor_min_press = 90.0;
float valor_max_press = 110.0;
float valor_min_umid = 40.0;
float valor_max_umid = 80.0;

const uint16_t period = 20000; //  20ms = 20000 ticks
const float DIVIDER_PWM = 125.0; //Divisor de clock
uint16_t pwm_level = 500;
uint slice_num;

#define WIFI_SSID "Crau crau do mal"
#define WIFI_PASSWORD "23092003"

typedef struct {
    uint8_t min;
    uint8_t max;
    volatile bool estado_bomba;
    uint8_t nivel_atual_Umid;
    float nivel_atual_Temp; // Adicionado para armazenar temperatura
    float nivel_atual_Press; //Armazena Pressão
    absolute_time_t alarm_a;
    float umidade;
} nivel_temperatura;

nivel_temperatura nv = {0, 0, false, 30, 0.0, 0, 0.0};

struct http_state{
    char response[8192];
    size_t len;
    size_t sent;
};

PIO pio;

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);

void setup_pwm(){ //Inicialização do PWM
  gpio_set_function(buzz, GPIO_FUNC_PWM);
  slice_num = pwm_gpio_to_slice_num(buzz);

  pwm_set_clkdiv(slice_num, 125.0); // 125 MHz / 125 = 1 MHz
  pwm_set_wrap(slice_num, 250);     // 1 MHz / 250 = 4 kHz (frequência audível)
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzz), 0); //Inicia o BUZZ desligado
  pwm_set_enabled(slice_num, true);// Inicia o PWM
}

// Pisca a cor ligada ao pino `pin` durante `seconds` segundos, num ciclo
// de 500 ms ligado / 500 ms desligado.
void blink_color(uint pin, int seconds) {
    int cycles = seconds;  // 1 ciclo = 1 s (500 ms on + 500 ms off)
    for (int i = 0; i < cycles; i++) {
        gpio_put(pin, 1);
        pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzz), 250); //Aciona o buzz com Duty-C de 50%
        sleep_ms(500);

        gpio_put(pin, 0);
        pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzz), 0); //Desliga o buzz
        sleep_ms(500);
    }
}

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
    setup_pwm();

    gpio_init(led_r);
    gpio_set_dir(led_r, GPIO_OUT);
    gpio_init(led_g);
    gpio_set_dir(led_g, GPIO_OUT);
    gpio_init(led_b);
    gpio_set_dir(led_b, GPIO_OUT);

    pio = pio_config(); //Config do PIO

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
            nv.nivel_atual_Temp = data.temperature; // Armazena a temperatura do AHT20
            nv.nivel_atual_Press = pressure / 1000.0;
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }

        if (nv.nivel_atual_Temp < valor_min_temp || nv.nivel_atual_Temp > valor_max_temp) {
        // código para alerta de temperatura fora do intervalo
            define_numero(0, pio, 0);
            blink_color(led_r, 2);
        }
        else if (nv.nivel_atual_Press < valor_min_press || nv.nivel_atual_Press > valor_max_press) {
        // código para alerta de pressão
            define_numero(1, pio, 0);
            blink_color(led_g, 2);
        }
        else if (nv.nivel_atual_Umid < valor_min_umid || nv.nivel_atual_Umid > valor_max_umid) {
        // código para alerta de umidade
            define_numero(2, pio, 0);
            blink_color(led_b, 2);
        }
        else{
            define_numero(3, pio, 0);
            gpio_put(led_r, 0);
            gpio_put(led_g, 0);
            gpio_put(led_b, 0);
        }

        sprintf(str_tmp1, "%.1fC", temperature / 100.0);  // Converte o inteiro em string
        sprintf(str_alt, "%.0fm", altitude);  // Converte o inteiro em string
        sprintf(str_tmp2, "%.1fC", data.temperature);  // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", data.humidity);  // Converte o inteiro em string        

        sleep_ms(3000);
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
    char json_payload[256];
    int json_len = snprintf(json_payload, sizeof(json_payload),
        "{"  
          "\"min_umid\":%.1f,"
          "\"max_umid\":%.1f,"
          "\"nivel_atual_Umid\":%d,"
          "\"min_temp\":%.1f,"
          "\"max_temp\":%.1f,"
          "\"nivel_atual_Temp\":%.1f,"
          "\"min_press\":%.1f,"
          "\"max_press\":%.1f,"
          "\"nivel_atual_Press\":%.3f,"
          "\"estado_bomba\":%s"
        "}\r\n",
        valor_min_umid, valor_max_umid,
        nv.nivel_atual_Umid,
        valor_min_temp, valor_max_temp,
        nv.nivel_atual_Temp,
        valor_min_press, valor_max_press,
        nv.nivel_atual_Press,
        nv.estado_bomba ? "true" : "false"
    );
    hs->len = snprintf(hs->response, sizeof(hs->response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        json_len, json_payload
    );
}

        else if (strstr(req, "POST /api/limites/temperatura")){

        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");

        if (min_str && max_str){
        valor_min_temp = atof(min_str + 4);
        valor_max_temp = atof(max_str + 4);
        printf("Novos limites de temperatura: %.2f ~ %.2f\n", valor_min_temp, valor_max_temp);
        }

        hs->len = snprintf(hs->response, sizeof(hs->response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n");

        } else if (strstr(req, "POST /api/limites/pressao")){

        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");

        if (min_str && max_str){
        valor_min_press = atof(min_str + 4);
        valor_max_press = atof(max_str + 4);
        printf("Novos limites de Pressao: %.2f ~ %.2f\n", valor_min_press, valor_max_press);
        }

        hs->len = snprintf(hs->response, sizeof(hs->response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n");

        } else if (strstr(req, "POST /api/limites/umidade")){

        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");

        if (min_str && max_str){
        valor_min_umid = atof(min_str + 4);
        valor_max_umid = atof(max_str + 4);
        printf("Novos limites de umidade: %.1f ~ %.1f\n", valor_min_umid, valor_max_umid);
        }

        hs->len = snprintf(hs->response, sizeof(hs->response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n");

        }  else if (strstr(req, "GET / ") || strstr(req, "GET /index")){

        size_t html_len = strlen(HTML_INDEX);

        int hdr_len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n", html_len);

        memcpy(hs->response + hdr_len, HTML_INDEX, html_len);

        hs->len = hdr_len + html_len;

    } else if (strstr(req, "GET /umidade")){

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

    } else if (strstr(req, "GET /temperatura")){
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

    }else if (strstr(req, "GET /pressao")){
       size_t html_len = strlen(HTML_PRESSAO);

        int hdr_len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n", html_len);

        memcpy(hs->response + hdr_len, HTML_PRESSAO, html_len);

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