// -- Inclusão de bibliotecas
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "pico/bootrom.h"

// -- Definição de constantes
// GPIO
#define button_B 6 // Botão B GPIO 6
#define matriz_leds 7 // Matriz de LEDs GPIO 7
#define NUM_LEDS 25 // Número de LEDs na matriz
#define buzzer_A 21 // Buzzer A GPIO 21
#define buzzer_B 10 // Buzzer B GPIO 10
#define LED_Green 11 // LED Verde GPIO 11
#define LED_Red 13 // LED Vermelho GPIO 13
#define joystick_Y 26 // VRY do Joystick GPIO 26
#define joystick_X 27 // VRX do Joystick GPIO 27

// Display I2C
#define display_i2c_port i2c1 // Define a porta I2C
#define display_i2c_sda 14 // Define o pino SDA na GPIO 14
#define display_i2c_scl 15 // Define o pino SCL na GPIO 15
#define display_i2c_endereco 0x3C // Define o endereço do I2C
ssd1306_t ssd; // Inicializa a estrutura do display

typedef struct{
    float volume_chuva;
    float nivel_agua;
} VolumeNivel_data_t;

QueueHandle_t xQueueEstacaoModo; // Fila para guardar modo
QueueHandle_t xQueueVolumeNivel; // Fila para guardar o volume e o nivel

// --- Funções necessária para a manipulação da matriz de LEDs

// Estrutura do pixel GRB (Padrão do WS2812)
struct pixel_t {
    uint8_t G, R, B; // Define variáveis de 8-bits (0 a 255) para armazenar a cor
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Permite declarar variáveis utilizando apenas "npLED_t"

// Declaração da Array que representa a matriz de LEDs
npLED_t leds[NUM_LEDS];

// Variáveis para máquina PIO
PIO np_pio;
uint sm;

// Função para definir a cor de um LED específico
void cor(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

// Função para desligar todos os LEDs
void desliga() {
    for (uint i = 0; i < NUM_LEDS; ++i) {
        cor(i, 0, 0, 0);
    }
}

// Função para enviar o estado atual dos LEDs ao hardware  - buffer de saída
void buffer() {
    for (uint i = 0; i < NUM_LEDS; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
}

// Função para converter a posição da matriz para uma posição do vetor.
int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24-(y * 5 + x); // Linha par (esquerda para direita).
    } else {
        return 24-(y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
    }
}

// --- Final das funções necessária para a manipulação da matriz de LEDs

// Tarefa/função para o Joystick
void vJoystickTask(void *params){
    // Inicialização do ADC
    adc_init();
    adc_gpio_init(joystick_X);
    adc_gpio_init(joystick_Y);

    // Variáveis utilizadas nas filas
    bool modo_alerta = false;
    VolumeNivel_data_t VolNiv;

    while (true){
        adc_select_input(0); // Seleciona o ADC0 referente ao VRY do Joystick (GPIO 26)
        uint16_t value_vry = adc_read(); // Ler o valor do ADC selecionado (ADC0 - VRY) e guarda
        adc_select_input(1); // Seleciona o ADC1 referente ao VRX do Joystick (GPIO 27)
        uint16_t value_vrx = adc_read(); // Ler o valor do ADC selecionado (ADC1 - VRX) e guarda

        // Cálculos de volume de chuva e nível de água
        VolNiv.volume_chuva = (value_vry/4095.0) * 100.0;
        VolNiv.nivel_agua = (value_vrx/4095.0) * 100.0;

        // Decisão se o modo alerta é ativado
        if((VolNiv.volume_chuva >= 80) || (VolNiv.nivel_agua >= 70)){
            modo_alerta = true;
        }else{
            modo_alerta = false;
        }

        // Debug
        printf("Volume de chuva: %.1f | Nivel da agua: %.1f | Modo alerta: %d\n", VolNiv.volume_chuva, VolNiv.nivel_agua, modo_alerta);
        xQueueSend(xQueueEstacaoModo, &modo_alerta, 0); // Envia o valor para fila
        xQueueSend(xQueueVolumeNivel, &VolNiv, 0); // Envia o valor para fila
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Tarefa/função para exibição no display OLED
void vDisplayTask(void *params){
    // Inicialização dp Display I2C
    i2c_init(display_i2c_port, 400 * 1000); // Inicializa o I2C usando 400kHz
    gpio_set_function(display_i2c_sda, GPIO_FUNC_I2C); // Define o pino SDA (GPIO 14) na função I2C
    gpio_set_function(display_i2c_scl, GPIO_FUNC_I2C); // Define o pino SCL (GPIO 15) na função I2C
    gpio_pull_up(display_i2c_sda); // Ativa o resistor de pull up para o pino SDA (GPIO 14)
    gpio_pull_up(display_i2c_scl); // Ativa o resistor de pull up para o pino SCL (GPIO 15)
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, display_i2c_endereco, display_i2c_port); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
    ssd1306_fill(&ssd, false); // Limpa o display
    ssd1306_send_data(&ssd); // Atualiza o display

    // Variáveis utilizadas nas filas
    bool modo_alerta;
    VolumeNivel_data_t VolNiv;

    // Strings para volume e nivel
    char str_vol[3];
    char str_niv[3];

    while(true){
        ssd1306_rect(&ssd, 0, 0, 127, 63, true, false); // Borda principal
        ssd1306_line(&ssd, 1, 12, 126, 12, true); // Desenha uma linha horizontal
        ssd1306_line(&ssd, 1, 24, 126, 24, true); // Desenha uma linha horizontal

        // Cabeçalho
        ssd1306_draw_string(&ssd, "EMB Semaforo", 16, 3); // Desenha uma string
        ssd1306_draw_string(&ssd, "Modo:", 12, 15); // Desenha uma string

        // Exibe informações de acordo com o modo
        if(xQueueReceive(xQueueEstacaoModo, &modo_alerta, portMAX_DELAY)){
            if(modo_alerta){
                ssd1306_draw_string(&ssd, "Alerta", 60, 15); // Desenha uma string
                ssd1306_draw_string(&ssd, "ABRIGUE-SE", 22, 51); // Desenha uma string
            }else{
                ssd1306_draw_string(&ssd, "Normal", 60, 15); // Desenha uma string
                ssd1306_draw_string(&ssd, "          ", 22, 51); // Desenha uma string
            }
        }

        ssd1306_draw_string(&ssd, "Vol. Chuva:   %", 4, 27); // Desenha uma string
        ssd1306_draw_string(&ssd, "Niv.  Agua:   %", 4, 38); // Desenha uma string

        // Exibe os valores de volume e nivel
        if(xQueueReceive(xQueueVolumeNivel, &VolNiv, portMAX_DELAY)){

            sprintf(str_vol, "%.0f", VolNiv.volume_chuva); // Converte float em string
            sprintf(str_niv, "%.0f", VolNiv.nivel_agua); // Converte float em string

            if(VolNiv.volume_chuva > 99.5){
                ssd1306_draw_string(&ssd, str_vol, 92, 27); // Desenha uma string
            }else if(VolNiv.volume_chuva > 9.9){
                ssd1306_draw_string(&ssd, str_vol, 100, 27); // Desenha uma string
            }else{
                ssd1306_draw_string(&ssd, str_vol, 108, 27); // Desenha uma string
            }

            if(VolNiv.nivel_agua > 99.5){
                ssd1306_draw_string(&ssd, str_niv, 92, 38); // Desenha uma string
            }else if(VolNiv.nivel_agua > 9.9){
                ssd1306_draw_string(&ssd, str_niv, 100, 38); // Desenha uma string
            }else{
                ssd1306_draw_string(&ssd, str_niv, 108, 38); // Desenha uma string
            }
        }

        ssd1306_send_data(&ssd); // Atualiza o display
    }
}

// Tarefa/função para o LED RGB
void vLEDTask(void *params){
    // Inicialização dos LEDs
    gpio_init(LED_Green); // Inicia a GPIO 11 do LED Verde
    gpio_set_dir(LED_Green, GPIO_OUT); // Define a direção da GPIO 11 do LED Verde como saída
    gpio_put(LED_Green, false); // Estado inicial do LED apagado
    gpio_init(LED_Red); // Inicia a GPIO 13 do LED Vermelho
    gpio_set_dir(LED_Red, GPIO_OUT); // Define a direção da GPIO 13 do LED Vermelho como saída
    gpio_put(LED_Red, false); // Estado inicial do LED apagado

    // Variável utilizada na fila
    bool modo_alerta;

    while(true){
        // Recebe informação do modo da fila
        if(xQueueReceive(xQueueEstacaoModo, &modo_alerta, portMAX_DELAY)){
            if(modo_alerta){
                gpio_put(LED_Green, false);
                gpio_put(LED_Red, true);
            }else{
                gpio_put(LED_Green, true);
                gpio_put(LED_Red, false);
            }
        }
    }
}

// Tarfea/função para a matriz de LEDs
void vMatrizTask(void *params){
    // Inicialização do PIO
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, true);
    uint offset = pio_add_program(pio0, &ws2818b_program);
    ws2818b_program_init(np_pio, sm, offset, matriz_leds, 800000);
    desliga(); // Para limpar o buffer dos LEDs
    buffer(); // Atualiza a matriz de LEDs

    // Variável utilizada na fila
    bool modo_alerta;

    while(true){
        desliga();
        // Recebe a informação do modo da fila
        if(xQueueReceive(xQueueEstacaoModo, &modo_alerta, portMAX_DELAY)){
            if(modo_alerta){
                // Frame "!"
                int frame0[5][5][3] = {
                    {{0, 0, 0}, {0, 0, 0}, {150, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {150, 0, 0}, {0, 0, 0}, {0, 0, 0}},    
                    {{0, 0, 0}, {0, 0, 0}, {150, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {150, 0, 0}, {0, 0, 0}, {0, 0, 0}}
                };
                for (int linha = 0; linha < 5; linha++)
                {
                    for (int coluna = 0; coluna < 5; coluna++)
                    {
                    int posicao = getIndex(linha, coluna);
                    cor(posicao, frame0[coluna][linha][0], frame0[coluna][linha][1], frame0[coluna][linha][2]);
                    }
                };
                buffer();
            }else{
                // Frame "N"
                int frame1[5][5][3] = {
                    {{0, 150, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 150, 0}},
                    {{0, 150, 0}, {0, 150, 0}, {0, 0, 0}, {0, 0, 0}, {0, 150, 0}},
                    {{0, 150, 0}, {0, 0, 0}, {0, 150, 0}, {0, 0, 0}, {0, 150, 0}},
                    {{0, 150, 0}, {0, 0, 0}, {0, 0, 0}, {0, 150, 0}, {0, 150, 0}},
                    {{0, 150, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 150, 0}}
                };
                for (int linha = 0; linha < 5; linha++)
                {
                    for (int coluna = 0; coluna < 5; coluna++)
                    {
                    int posicao = getIndex(linha, coluna);
                    cor(posicao, frame1[coluna][linha][0], frame1[coluna][linha][1], frame1[coluna][linha][2]);
                    }
                };
                buffer();
            }
        }
    }
}

// Tarefa/função para os buzzers
void vBuzzerTask(void *params){

    // Inicialização dos Buzzers PWM
    gpio_set_function(buzzer_A, GPIO_FUNC_PWM); // Define a função da porta GPIO como PWM
    gpio_set_function(buzzer_B, GPIO_FUNC_PWM); // Define a função da porta GPIO como PWM

    uint freq = 1000; // Frequência do buzzer
    uint clock_div = 4; // Divisor do clock
    uint wrap = (125000000 / (clock_div * freq)) - 1; // Define o valor do wrap para frequência escolhida

    uint slice_A = pwm_gpio_to_slice_num(buzzer_A); // Define o slice do buzzer A
    uint slice_B = pwm_gpio_to_slice_num(buzzer_B); // Define o slice do buzzer B

    pwm_set_clkdiv(slice_A, clock_div); // Define o divisor do clock para o buzzer A
    pwm_set_clkdiv(slice_B, clock_div); // Define o divisor do clock para o buzzer B
    pwm_set_wrap(slice_A, wrap); // Define o valor do wrap para o buzzer A
    pwm_set_wrap(slice_B, wrap); // Define o valor do wrap para o buzzer B
    pwm_set_chan_level(slice_A, pwm_gpio_to_channel(buzzer_A), wrap / 40); // Duty cycle para definir o Volume do buzzer A
    pwm_set_chan_level(slice_B, pwm_gpio_to_channel(buzzer_B), wrap / 40); // Duty cycle para definir o volume do buzzer B

    // Variável utilizada na fila
    bool modo_alerta;

    while(true){
        // Recebe a informaão do modo da fila
        if(xQueueReceive(xQueueEstacaoModo, &modo_alerta, portMAX_DELAY)){
            if(modo_alerta){
                pwm_set_enabled(slice_A, true);
                pwm_set_enabled(slice_B, true);
                vTaskDelay(pdMS_TO_TICKS(100));
                pwm_set_enabled(slice_A, false);
                pwm_set_enabled(slice_B, false);
                vTaskDelay(pdMS_TO_TICKS(100));
            }else{
                pwm_set_enabled(slice_A, false);
                pwm_set_enabled(slice_B, false);
            }
        }
    }
}

// Interrupção para o botão B para bootsel
void gpio_irq_handler(uint gpio, uint32_t events){
    reset_usb_boot(0, 0);
}

// Função principal
int main(){
    // Ativa BOOTSEL via botão
    gpio_init(button_B);
    gpio_set_dir(button_B, GPIO_IN);
    gpio_pull_up(button_B);
    gpio_set_irq_enabled_with_callback(button_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();

    // Cria a fila para compartilhamento de valor do joystick
    xQueueEstacaoModo = xQueueCreate(5, sizeof(bool));
    xQueueVolumeNivel = xQueueCreate(5, sizeof(VolumeNivel_data_t));

    // Criação das tasks
    xTaskCreate(vJoystickTask, "Joystick", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vLEDTask, "LED RGB", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vMatrizTask, "Matriz de LEDs", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}
