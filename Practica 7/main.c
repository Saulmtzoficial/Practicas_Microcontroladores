#include <Arduino.h>

// --- Configuracion de pines ---
// MSB -> GP6, luego GP7, GP8, LSB -> GP9
#define LED_MSB 6 // bit 3
#define LED_B2 7  // bit 2
#define LED_B1 8  // bit 1
#define LED_LSB 9 // bit 0 (LSB)

#define BTN_PIN 15 // Boton con pull-up interno (activo en LOW)
#define DEBOUNCE_MS 50

const uint8_t leds[4] = {LED_MSB, LED_B2, LED_B1, LED_LSB};
uint8_t counter = 0;
bool last_btn_state = HIGH;

void actualizarLEDs(uint8_t valor) {
    for (int i = 0; i < 4; i++) {
        uint8_t bit = (valor >> (3 - i)) & 0x01;
        digitalWrite(leds[i], bit);
    }
}

void setup() {
    for (int i = 0; i < 4; i++) {
        pinMode(leds[i], OUTPUT);
        digitalWrite(leds[i], LOW);
    }
    pinMode(BTN_PIN, INPUT_PULLUP);
}

void loop() {
    bool btn_state = digitalRead(BTN_PIN);

    if (btn_state == LOW && last_btn_state == HIGH) {
        delay(DEBOUNCE_MS);

        if (digitalRead(BTN_PIN) == LOW) {
            counter = (counter + 1) & 0x0F;
            actualizarLEDs(counter);

            while (digitalRead(BTN_PIN) == LOW) {
                delay(10);
            }
            delay(DEBOUNCE_MS);
        }
    }

    last_btn_state = btn_state;
    delay(10);
}