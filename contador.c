#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "pico/bootrom.h"
#include "build/pio_matrix.pio.h"

#define NUM_PIXELS 25    // 5x5
#define OUT_PIN    7     // Pino de saída de dados para os LEDs WS2812
#define LED_G      11    // LED RGB - Verde
#define LED_B      12    // LED RGB - Azul
#define LED_R      13    // LED RGB - Vermelho
#define BTN_A      5     // Botão A
#define BTN_B      6     // Botão B

volatile int numero_atual = 0;
volatile uint32_t ultimo_tempo_a = 0;
volatile uint32_t ultimo_tempo_b = 0;
const uint32_t debounce_delay = 200000; // 200 ms em microssegundos

//Converte o comando para 24bits para enviar para o ws2812 
uint32_t matrix_rgb(double r, double g, double b) {
    double brilho = 0.01; // Brilho em 1%
    unsigned char R = (unsigned char)(r * 255 * brilho);
    unsigned char G = (unsigned char)(g * 255 * brilho);
    unsigned char B_ = (unsigned char)(b * 255 * brilho);
    return ( (uint32_t)(G) << 24 )
         | ( (uint32_t)(R) << 16 )
         | ( (uint32_t)(B_) <<  8 );
}
//Pisca o led
void piscar_led_rgb() {
    static bool estado = false;
    gpio_put(LED_R, estado);
    estado = !estado;
}

bool timer_callback(struct repeating_timer *t) {
    piscar_led_rgb();
    return true;
}

void exibir_numero(PIO pio, uint sm, int numero) {
    static uint32_t numeros[10][NUM_PIXELS] = {
        {0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0}, // 0
        {0,1,1,1,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,1,0, 0,0,1,0,0}, // 1        
        {1,1,1,1,1, 0,0,0,0,1, 0,1,1,1,0, 1,0,0,0,0, 1,1,1,1,1}, // 2
        {1,1,1,1,1, 1,0,0,0,0, 0,1,1,1,0, 1,0,0,0,0, 1,1,1,1,1}, // 3
        {0,0,0,0,1, 1,0,0,0,0, 1,1,1,1,1, 1,0,0,0,1, 1,0,0,0,1}, // 4
        {1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1}, // 5
        {1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1}, // 6
        {1,0,0,0,0, 0,0,0,1,0, 0,0,1,0,0, 0,1,0,0,0, 1,1,1,1,1}, // 7
        {1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1}, // 8
        {1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1}  // 9
    };
    for (int i = 0; i < NUM_PIXELS; i++) {
        int mirrored_index = (i / 5) * 5 + (4 - (i % 5));
        pio_sm_put_blocking(pio, sm, matrix_rgb(numeros[numero][mirrored_index], numeros[numero][mirrored_index], numeros[numero][mirrored_index]));
    }
}

//verifica pressionamento dos botões A e B
void gpio_callback(uint gpio, uint32_t events) {
    uint32_t agora = time_us_32();
    if (gpio == BTN_A && (agora - ultimo_tempo_a) > debounce_delay) {
        numero_atual = (numero_atual + 1) % 10;
        ultimo_tempo_a = agora;
    }
    if (gpio == BTN_B && (agora - ultimo_tempo_b) > debounce_delay) {
        numero_atual = (numero_atual - 1 + 10) % 10;
        ultimo_tempo_b = agora;
    }
}

int main() {
    //Inicia os pinos
    stdio_init_all();
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, OUT_PIN);
    
    //Inicia led vermelho
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);
    
    //Inicia botões como pull up
    gpio_init(BTN_A);
    gpio_init(BTN_B);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_A);
    gpio_pull_up(BTN_B);
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    
    //Adiciona repetição do led vermelho para 200ms (aprox 5x por segundo)
    struct repeating_timer timer;
    add_repeating_timer_ms(200, timer_callback, NULL, &timer);
    
    while (true) {
        exibir_numero(pio, sm, numero_atual);
        sleep_ms(50);
    }
    return 0;
}
