#include "pico/stdlib.h" // Biblioteca para GPIO y temporizacion

int main() {
    const uint LED_PIN = 3; // Define el pin donde se conecta el LED
    const uint BOTON = 4;   // Define el pin donde se conecta el boton

    gpio_init(LED_PIN); // Inicializa el pin del LED
    gpio_init(BOTON);   // Inicializa el pin del boton
    
    gpio_set_dir(LED_PIN, GPIO_OUT); // Configura el pin del LED como salida
    gpio_set_dir(BOTON, GPIO_IN);    // Configura el pin del boton como entrada

    while (true) { // Bucle infinito para mantener el programa en ejecucion
        if (gpio_get(BOTON)) { // Si el boton esta presionado (devuelve 1)
            sleep_ms(50); // Pausa 50ms para evitar rebotes mecanicos (debounce)
            if (gpio_get(BOTON)) { // Vuelve a verificar si sigue presionado
                gpio_put(LED_PIN, 1); // Enciende el LED (3.3 V en el pin)
            }
        } else {
            gpio_put(LED_PIN, 0); // Apaga el LED (0V) si el boton no esta presionado
        }
        sleep_ms(10); // Pequeno delay para evitar sobrecarga en el microcontrolador
    }
}