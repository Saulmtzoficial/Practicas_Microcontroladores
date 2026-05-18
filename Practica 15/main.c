#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

int main() {

stdio_init_all();
// Inicializacion necesaria para el hardware del Pico W
if (cyw43_arch_init()) {
return-1;
}
while (true) {
// Encender LED integrado
cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
sleep_ms(500);
// Apagar LED integrado
cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
sleep_ms(500);
}
}