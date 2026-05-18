/* Pines:
 * LM35 -> GP26 (ADC0) - alimentar con 3.3V
 * LED Verde -> GP15
 * LED Rojo -> GP14
 * Buzzer (PWM) -> GP13
 * Transistor (Fan) -> GP22
 */

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define PIN_LED_VERDE 15
#define PIN_LED_ROJO 14
#define PIN_BUZZER 13
#define PIN_FAN 22
#define PIN_ADC 26 // ADC0

#define TEMP_LIMITE 70.0f
#define MUESTRAS 32
#define INTERVALO_MS 500
#define BUZZER_FREQ 1000
#define ADC_VREF 3.3f
#define ADC_MAX 4095.0f

static uint pwm_slice;
static uint pwm_chan;

void buzzer_init(void) {
    gpio_set_function(PIN_BUZZER, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(PIN_BUZZER);
    pwm_chan = pwm_gpio_to_channel(PIN_BUZZER);
    uint16_t wrap = 999;
    float div = 125000000.0f / ((wrap + 1) * BUZZER_FREQ);
    pwm_set_clkdiv(pwm_slice, div);
    pwm_set_wrap(pwm_slice, wrap);
    pwm_set_chan_level(pwm_slice, pwm_chan, 0);
    pwm_set_enabled(pwm_slice, true);
}

static inline void buzzer_on(void) {
    pwm_set_chan_level(pwm_slice, pwm_chan, 500); 
}

static inline void buzzer_off(void) {
    pwm_set_chan_level(pwm_slice, pwm_chan, 0); 
}

float leer_temperatura(void) {
    buzzer_off();
    sleep_us(500);
    uint32_t suma = 0;
    for (int i = 0; i < MUESTRAS; i++) {
        suma += adc_read();
        sleep_us(100);
    }
    float raw = (float)(suma / MUESTRAS);
    float voltaje = (raw / ADC_MAX) * ADC_VREF;
    return voltaje * 100.0f;
}

int main(void) {
    stdio_init_all();
    sleep_ms(3000);

    gpio_init(PIN_LED_VERDE); 
    gpio_set_dir(PIN_LED_VERDE, GPIO_OUT);
    
    gpio_init(PIN_LED_ROJO); 
    gpio_set_dir(PIN_LED_ROJO, GPIO_OUT);
    
    gpio_init(PIN_FAN); 
    gpio_set_dir(PIN_FAN, GPIO_OUT);

    adc_init();
    adc_gpio_init(PIN_ADC);
    adc_select_input(0);
    buzzer_init();

    gpio_put(PIN_LED_VERDE, 0);
    gpio_put(PIN_LED_ROJO, 0);
    gpio_put(PIN_FAN, 0);

    for (int i = 0; i < 3; i++) {
        gpio_put(PIN_LED_VERDE, 1); 
        gpio_put(PIN_LED_ROJO, 1);
        sleep_ms(150);
        gpio_put(PIN_LED_VERDE, 0); 
        gpio_put(PIN_LED_ROJO, 0);
        sleep_ms(150);
    }

    printf("=== PRACTICA 11 - CONTROL DE TEMPERATURA ===\n");
    printf("Umbral: %.1f C | Formato: TEMP:|ESTADO:|TIME:\n", (double)TEMP_LIMITE);

    bool blink = false;

    while (true) {
        float temperatura = leer_temperatura();
        uint32_t t_ms = to_ms_since_boot(get_absolute_time());

        if (temperatura >= TEMP_LIMITE) {
            gpio_put(PIN_FAN, 1);
            gpio_put(PIN_LED_VERDE, 0);
            blink = !blink;
            gpio_put(PIN_LED_ROJO, blink ? 1 : 0);
            if (blink) buzzer_on();
            printf("TEMP:%.2f|ESTADO:ALARMA|TIME:%lu\n", (double)temperatura, (unsigned long)t_ms);
        } else {
            gpio_put(PIN_FAN, 0);
            gpio_put(PIN_LED_ROJO, 0);
            gpio_put(PIN_LED_VERDE, 1);
            buzzer_off();
            blink = false;
            printf("TEMP:%.2f|ESTADO:NORMAL|TIME:%lu\n", (double)temperatura, (unsigned long)t_ms);
        }
        sleep_ms(INTERVALO_MS);
    }
    return 0;
}