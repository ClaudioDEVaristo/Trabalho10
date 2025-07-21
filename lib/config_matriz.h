#include "desenhos.h"
#include "hardware/pio.h"
#include "Trabalho10.pio.h"

#define OUT_PIN 7

// Função para definir a intensidade das cores da matriz de led
uint matrix_rgb(float r, float g, float b)
{
  unsigned char R, G, B;
  R = r * 255;
  G = g * 255;
  B = b * 255;
  return (G << 24) | (R << 16) | (B << 8);
}

// Função para converter a posição da matriz para uma posição do vetor.
int getid(int x, int y)
{
  if (y % 2 == 0)
  {
    return 24 - (y * 5 + x); 
  }
  else
  {
    return 24 - (y * 5 + (4 - x));
  }
}

void desenho(bool *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b)
{
    for (int y = 0; y < 5; y++) // Percorre as linhas da matriz
    {
        for (int x = 0; x < 5; x++) // Percorre as colunas da matriz
        {
            int real_index = getid(x, y); // Obtém o índice 
            valor_led = matrix_rgb(desenho[real_index] * r, desenho[real_index] * g, desenho[real_index] * b);
            pio_sm_put_blocking(pio, sm, valor_led);
        }
    }
}

void define_numero(char numero, PIO pio, uint sm){

    uint valorLed;

        switch (numero)
    {
    case 0:
        desenho(desenho0, valorLed, pio, sm, 0.0, 0.1, 0.0); // desenho do número 0
        break;
    case 1:
        desenho(desenho1, valorLed, pio, sm, 0.1, 0.1, 0.0); // desenho do número 1
        break;
    case 2:
        desenho(desenho2, valorLed, pio, sm, 0.1, 0.0, 0.0); // desenho do número 2
        break;
    case 3:
        desenho(apagado, valorLed, pio, sm, 0.1, 0.0, 0.0); // desenho do número 3
        break;
    }
};

//Configuração do PIO
PIO pio_config()
{
  int sm = 0;
  PIO pio = pio0;
  uint offset = pio_add_program(pio, &ws2812_program);

  ws2812_program_init(pio, sm, offset, OUT_PIN, 800000, false);

  return pio;
}