// ======================= BIBLIOTECAS ========================
#include <stdio.h>
#include "pico/stdlib.h"
#include <hardware/pio.h>
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "animacao_matriz.pio.h"  // Programa PIO compilado para controle dos LEDs

// ======================= CONSTANTES ========================
#define LED_STATUS_AZUL     12    // Pino do LED azul de status

#define QTD_LEDS            25    // Número de LEDs na matriz 5x5
#define PINO_MATRIZ         7     // Pino de dados da matriz
#define BOTAO_INCREMENTO    6     // Pino do botão para aumentar número
#define BOTAO_DECREMENTO    5     // Pino do botão para diminuir número

// ====================== VARIÁVEIS GLOBAIS ====================
PIO controlador_pio;              // Controlador PIO (0 ou 1)
uint maquina_estado;              // Índice da máquina de estado PIO
volatile uint numero_atual = 0;   // Número exibido (0-9)
volatile uint32_t ultimo_toque = 0; // Temporização para debounce
const float fator_brilho = 0.8;   // 80% do brilho máximo (20% de redução)

// ====================== CONFIGURAÇÕES FIXAS ==================
// Padrões dos números (0-9) representados na matriz 5x5
const uint8_t formato_numerico[10][QTD_LEDS] = {
    // 0         1         2         3         4          (Posições na matriz)
    {0,1,1,1,0,
     0,1,0,1,0, 
     0,1,0,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0}, // 0

    {0,0,1,0,0,
     0,0,1,0,0, 
     0,0,1,0,0, 
     0,0,1,0,0, 
     0,0,1,0,0}, // 1

    {0,1,1,1,0,
     0,0,0,1,0, 
     0,1,1,1,0, 
     0,1,0,0,0, 
     0,1,1,1,0}, // 2

    {0,1,1,1,0,
     0,0,0,1,0, 
     0,1,1,1,0, 
     0,0,0,1,0, 
     0,1,1,1,0}, // 3

    {0,1,0,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0, 
     0,0,0,1,0, 
     0,0,0,1,0}, // 4

    {0,1,1,1,0,
     0,1,0,0,0, 
     0,1,1,1,0, 
     0,0,0,1,0, 
     0,1,1,1,0}, // 5

    {0,1,0,0,0,
     0,1,0,0,0, 
     0,1,1,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0}, // 6

    {0,1,1,1,0,
     0,0,0,1,0, 
     0,0,0,1,0, 
     0,0,0,1,0, 
     0,0,0,1,0}, // 7

    {0,1,1,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0}, // 8

    {0,1,1,1,0,
     0,1,0,1,0, 
     0,1,1,1,0, 
     0,0,0,1,0, 
     0,0,0,1,0}  // 9
};

// Mapeamento lógico-físico dos LEDs (corrige disposição da matriz)
const uint8_t ordem_leds[QTD_LEDS] = {
    0,1,2,3,4,9,8,7,6,5,10,11,12,13,14,19,18,17,16,15,20,21,22,23,24
};

// ===================== FUNÇÕES PRINCIPAIS ====================

// Gera cor vermelha com intensidade ajustada pelo brilho
uint32_t gerar_cor_vermelha() {
    return (uint8_t)(50 * fator_brilho) << 16;
}

// Atualiza a matriz com o número especificado
void atualizar_matriz(uint numero) {
    for (uint8_t posicao = 0; posicao < QTD_LEDS; posicao++) {
        // Determina se o LED atual deve estar aceso ou apagado
        uint8_t led_aceso = formato_numerico[numero][ordem_leds[24 - posicao]];
        
        // Envia o comando de cor para o LED
        uint32_t cor = led_aceso ? gerar_cor_vermelha() : 0;
        pio_sm_put_blocking(controlador_pio, maquina_estado, cor);
    }
}

// Manipulador de interrupção para os botões
void tratar_interrupcao_botao(uint gpio, uint32_t eventos) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    
    // Filtro de debounce: ignora eventos com menos de 200ms do último
    if (tempo_atual - ultimo_toque > 200) {
        if (gpio == BOTAO_INCREMENTO) {
            numero_atual = (numero_atual + 1) % 10;  // Ciclo 0-9
        } 
        else if (gpio == BOTAO_DECREMENTO) {
            numero_atual = (numero_atual + 9) % 10;  // Ciclo 9-0 (equivale a -1)
        }
        
        atualizar_matriz(numero_atual);     // Atualiza display
        ultimo_toque = tempo_atual;         // Registra novo evento
    }
}

// ===================== PROGRAMA PRINCIPAL ====================
int main() {
    // Configuração inicial do sistema
    controlador_pio = pio0;  // Usa controlador PIO 0
    bool clock_01 = set_sys_clock_khz(128000, false);  // Clock a 128MHz
    
    stdio_init_all();  // Inicializa USB/Serial para debug
    printf("Sistema Iniciado\n");
    if (clock_01) printf("Clock operando em %ld Hz\n", clock_get_hz(clk_sys));

    // ------ Configuração do PIO para controle da matriz ------
    uint offset = pio_add_program(controlador_pio, &animacao_matriz_program);
    maquina_estado = pio_claim_unused_sm(controlador_pio, true);
    animacao_matriz_program_init(controlador_pio, maquina_estado, offset, PINO_MATRIZ);

    // ------ Configuração dos LEDs de status ------
    gpio_init(LED_STATUS_AZUL);
    gpio_set_dir(LED_STATUS_AZUL, GPIO_OUT);
    gpio_put(LED_STATUS_AZUL, 0);

    // ------ Configuração dos botões com pull-up ------
    gpio_init(BOTAO_INCREMENTO);
    gpio_pull_up(BOTAO_INCREMENTO);
    gpio_set_irq_enabled_with_callback(BOTAO_INCREMENTO, GPIO_IRQ_EDGE_FALL, true, &tratar_interrupcao_botao);
    
    gpio_init(BOTAO_DECREMENTO);
    gpio_pull_up(BOTAO_DECREMENTO);
    gpio_set_irq_enabled_with_callback(BOTAO_DECREMENTO, GPIO_IRQ_EDGE_FALL, true, &tratar_interrupcao_botao);

    // ------ Inicialização da matriz ------
    atualizar_matriz(0);  // Começa exibindo o número 0

    // ------ Loop principal ------
    while (true) {
        // Pisca LED azul de status (5Hz) para indicar funcionamento
        gpio_put(LED_STATUS_AZUL, !gpio_get(LED_STATUS_AZUL));
        sleep_ms(100);  // 100ms ligado + 100ms desligado
    }
}