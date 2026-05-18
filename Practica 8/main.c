#include <Arduino.h>

// LEDs binarios (4 bits)
#define LED_MSB 6
#define LED_B2 7
#define LED_B1 8
#define LED_LSB 9

// Display 7 segmentos (anodo comun)
#define SEG_A 28
#define SEG_B 27
#define SEG_C 21
#define SEG_D 20
#define SEG_E 19
#define SEG_F 26
#define SEG_G 22

// Boton
#define BTN_PIN 15
#define DEBOUNCE_MS 50

// Segmentos por digito hex (bit0=A...bit6=G, 1=ON)
const uint8_t SEG_TABLE[16] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x77, // A
    0x7C, // b
    0x39, // C
    0x5E, // d
    0x79, // E
    0x71  // F
};

const uint8_t SEG_PINS[7] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G};
const uint8_t LED_PINS[4] = {LED_MSB, LED_B2, LED_B1, LED_LSB};

uint8_t counter = 0;
bool last_btn_state = HIGH;

// Actualiza los 4 LEDs binarios
void actualizarLEDs(uint8_t valor) {
    for (int i = 0; i < 4; i++) {
        uint8_t bit = (valor >> (3 - i)) & 0x01;
        digitalWrite(LED_PINS[i], bit);
    }
}

// Actualiza display 7 segmentos (anodo comun = LOW activo)
void actualizarDisplay(uint8_t valor) {
    uint8_t patron = SEG_TABLE[valor & 0x0F];
    for (int i = 0; i < 7; i++) {
        digitalWrite(SEG_PINS[i], !((patron >> i) & 0x01));
    }
}

void setup() {
    for (int i = 0; i < 4; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
    
    for (int i = 0; i < 7; i++) {
        pinMode(SEG_PINS[i], OUTPUT);
        digitalWrite(SEG_PINS[i], HIGH); // todos apagados (anodo comun)
    }

    pinMode(BTN_PIN, INPUT_PULLUP);

    // Mostrar estado inicial (0)
    actualizarLEDs(counter);
    actualizarDisplay(counter);
}

void loop() {
    bool btn_state = digitalRead(BTN_PIN);

    if (btn_state == LOW && last_btn_state == HIGH) {
        delay(DEBOUNCE_MS);
        if (digitalRead(BTN_PIN) == LOW) {
            counter = (counter + 1) & 0x0F; // 0-15 y vuelve a 0
            actualizarLEDs(counter);        // LEDs binarios
            actualizarDisplay(counter);     // Display hex

            while (digitalRead(BTN_PIN) == LOW) {
                delay(10);
            }
            delay(DEBOUNCE_MS);
        }
    }

    last_btn_state = digitalRead(BTN_PIN);
    delay(10);
}