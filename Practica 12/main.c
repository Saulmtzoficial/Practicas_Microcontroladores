/*
 * Practica 12 - Control LED RGB mediante senales PWM
 * Framework: pico-sdk | Placa: Raspberry Pi Pico W
 *
 * Entradas ADC (potenciometros 5k + 1k):
 * Red -> GP28 (ADC2)
 * Green -> GP26 (ADC0)
 * Blue -> GP27 (ADC1)
 * 
 * Salidas PWM (LED RGB catodo comun, res. 220 Ohm):
 * Red -> GP15
 * Green -> GP14
 * Blue -> GP13
 *
 * Protocolo salida:
 * "DATA:r,g,b,MODE\n"
 * "Canales de color:\n"
 * "R: XX% | G: XX% | B: XX%\n"
 *
 * Protocolo entrada:
 * "MODE:MANUAL\n" | "MODE:SERIAL\n"
 * "RGB:r,g,b\n" (solo en modo SERIAL)
 * "GET\n"
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define POT_R_PIN 28
#define POT_G_PIN 26
#define POT_B_PIN 27
#define LED_R_PIN 15
#define LED_G_PIN 14
#define LED_B_PIN 13
#define ADC_MAX 4095u
#define PWM_WRAP 255u

#define SAMPLE_MS 80u
#define CMD_BUF_SIZE 64
#define ADC_PROMEDIO 8

/* 1 = catodo comun | 0 = anodo comun */
#define LED_CATODO_COMUN 1

typedef enum { MODE_MANUAL = 0, MODE_SERIAL = 1 } CtrlMode;
static CtrlMode g_mode = MODE_MANUAL;
static uint8_t g_red = 0;
static uint8_t g_green = 0;
static uint8_t g_blue = 0;

/* ---------- Helpers PWM ---------- */
static void pwm_pin_init(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, PWM_WRAP);
    pwm_set_enabled(slice, true);
}

static void pwm_set_level(uint pin, uint8_t level) {
    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
#if LED_CATODO_COMUN
    pwm_set_chan_level(slice, channel, level);
#else
    pwm_set_chan_level(slice, channel, PWM_WRAP - level);
#endif
}

static void apply_color(void) {
    pwm_set_level(LED_R_PIN, g_red);
    pwm_set_level(LED_G_PIN, g_green);
    pwm_set_level(LED_B_PIN, g_blue);
}

/* ---------- Lectura ADC con promedio ---------- */
static uint8_t read_adc_channel(uint channel) {
    adc_select_input(channel);
    sleep_us(10);
    uint32_t suma = 0;
    for (int i = 0; i < ADC_PROMEDIO; i++) {
        suma += adc_read();
        sleep_us(10);
    }
    uint16_t prom = (uint16_t)(suma / ADC_PROMEDIO);
    return (uint8_t)((prom * 255u) / ADC_MAX);
}

/* ---------- Parser de comandos serial ---------- */
static void parse_command(const char *cmd) {
    if (strncmp(cmd, "MODE:", 5) == 0) {
        const char *arg = cmd + 5;
        if (strcmp(arg, "MANUAL") == 0) {
            g_mode = MODE_MANUAL;
            printf("ACK:MODE:MANUAL\n");
        } else if (strcmp(arg, "SERIAL") == 0) {
            g_mode = MODE_SERIAL;
            printf("ACK:MODE:SERIAL\n");
        } else {
            printf("ERR:UNKNOWN_MODE:%s\n", arg);
        }
        return;
    }
    
    if (strncmp(cmd, "RGB:", 4) == 0) {
        int r, g, b;
        if (sscanf(cmd + 4, "%d,%d,%d", &r, &g, &b) == 3) {
            if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                if (g_mode == MODE_SERIAL) {
                    g_red = (uint8_t)r;
                    g_green = (uint8_t)g;
                    g_blue = (uint8_t)b;
                    apply_color();
                    printf("ACK:RGB:%d,%d,%d\n", r, g, b);
                } else {
                    printf("ERR:NOT_IN_SERIAL_MODE\n");
                }
            } else {
                printf("ERR:VALUE_OUT_OF_RANGE\n");
            }
        } else {
            printf("ERR:INVALID_RGB_FORMAT\n");
        }
        return;
    }
    
    if (strcmp(cmd, "GET") == 0) {
        printf("ACK:GET:%d,%d,%d,%s\n",
               g_red, g_green, g_blue,
               g_mode == MODE_MANUAL ? "MANUAL" : "SERIAL");
        return;
    }
    printf("ERR:UNKNOWN_CMD:%s\n", cmd);
}

/* ---------- main ---------- */
int main(void) {
    stdio_init_all();
    sleep_ms(2000); /* Esperar enumeracion USB */
    
    adc_init();
    adc_gpio_init(POT_R_PIN);
    adc_gpio_init(POT_G_PIN);
    adc_gpio_init(POT_B_PIN);
    
    pwm_pin_init(LED_R_PIN);
    pwm_pin_init(LED_G_PIN);
    pwm_pin_init(LED_B_PIN);
    
    apply_color();
    
    char cmd_buf[CMD_BUF_SIZE];
    uint8_t cmd_idx = 0;
    uint32_t last_ms = 0;
    
    printf("INIT:RGB_CONTROLLER:READY\n");
    printf("INFO:PINS:R=GP15,G=GP14,B=GP13\n");
    printf("INFO:POTS:R=GP28,G=GP26,B=GP27\n");
    printf("INFO:MODE:MANUAL\n");
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        /* Lectura serial no bloqueante */
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            char ch = (char)c;
            if (ch == '\n' || ch == '\r') {
                if (cmd_idx > 0) {
                    cmd_buf[cmd_idx] = '\0';
                    parse_command(cmd_buf);
                    cmd_idx = 0;
                }
            } else if (cmd_idx < CMD_BUF_SIZE - 1) {
                cmd_buf[cmd_idx++] = ch;
            }
        }
        
        /* Muestreo periodico */
        if (now - last_ms >= SAMPLE_MS) {
            last_ms = now;
            if (g_mode == MODE_MANUAL) {
                g_red = read_adc_channel(2);   /* GP28 = ADC2 */
                g_green = read_adc_channel(0); /* GP26 = ADC0 */
                g_blue = read_adc_channel(1);  /* GP27 = ADC1 */
                apply_color();
            }
            
            uint8_t pct_r = (uint8_t)((g_red * 100u) / 255u);
            uint8_t pct_g = (uint8_t)((g_green * 100u) / 255u);
            uint8_t pct_b = (uint8_t)((g_blue * 100u) / 255u);
            
            printf("DATA:%d,%d,%d,%s\n",
                   g_red, g_green, g_blue,
                   g_mode == MODE_MANUAL ? "MANUAL" : "SERIAL");
            printf("Canales de color:\n");
            printf("R: %3d%% | G: %3d%% | B: %3d%%\n", pct_r, pct_g, pct_b);
        }
        sleep_ms(1);
    }
    return 0;
}