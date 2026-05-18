#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Para usar atoi()
#include "pico/stdlib.h"
#include "hardware/adc.h" 
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"

#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "ble/att_server.h"
#include "ble/sm.h"
#include "gap.h"
#include "hci.h"
#include "l2cap.h"

#include "lampara.h"

// ── PINES Y CONFIGURACIÓN ─────────────────────────────────
#define RELAY_GPIO       15
#define RELAY_ACTIVE_LOW 1
#define LED_EXT_GPIO     2

#define PIN_LDR          26      // Pin ADC donde está conectada la LDR
#define ADC_CANAL        0       // Canal ADC (GP26 = canal 0)

// ¡Nuevos umbrales calibrados al milímetro!
#define UMBRAL_ENCENDER  290     // Menos de esto = ENCIENDE
#define UMBRAL_APAGAR    500     // Más de esto = APAGA

// ── ESTADOS Y MODOS ───────────────────────────────────────
typedef enum {
    MODO_MANUAL,
    MODO_AUTO,
    MODO_TIMER
} modo_lampara_t;

static const char* nombres_modos[] = {"MANUAL", "AUTOMÁTICO", "TEMPORIZADOR"};

static modo_lampara_t modo_actual = MODO_MANUAL;
static bool lamp_state = false;
static int32_t tiempo_restante = 0; 

static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_timer_source_t loop_timer; 

// ── FUNCIONES DE HARDWARE ─────────────────────────────────
static void hardware_init(void) {
    gpio_init(RELAY_GPIO);
    gpio_set_dir(RELAY_GPIO, GPIO_OUT);
    gpio_put(RELAY_GPIO, RELAY_ACTIVE_LOW ? 1 : 0);

    gpio_init(LED_EXT_GPIO);
    gpio_set_dir(LED_EXT_GPIO, GPIO_OUT);
    gpio_put(LED_EXT_GPIO, 0);

    adc_init();
    adc_gpio_init(PIN_LDR);
    adc_select_input(ADC_CANAL);
}

static void set_lamp(bool state) {
    if (lamp_state == state) return; 
    
    lamp_state = state;
    gpio_put(RELAY_GPIO, RELAY_ACTIVE_LOW ? !state : state);
    gpio_put(LED_EXT_GPIO, state);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, state);
    printf("💡 ACCIÓN -> Lámpara: %s\n", state ? "ENCENDIDA" : "APAGADA");
}

// ── EL CORAZÓN DEL SISTEMA ─────────────────────────────────
static void timer_handler(btstack_timer_source_t *ts) {
    btstack_run_loop_set_timer(ts, 1000);
    btstack_run_loop_add_timer(ts);

    uint16_t valor_adc = adc_read();
    float voltaje = valor_adc * 3.3f / 65535.0f;
    
    printf("📡 [INFO] LDR: %5d (%.2fV) | Modo: %-12s | Foco: %s\n", 
           valor_adc, voltaje, nombres_modos[modo_actual], lamp_state ? "ON" : "OFF");

    if (modo_actual == MODO_TIMER) {
        if (tiempo_restante > 0) {
            tiempo_restante--;
            if (tiempo_restante <= 0) {
                printf("⏰ ¡Pum! Temporizador terminado. Apagando todo.\n");
                set_lamp(false);
                modo_actual = MODO_MANUAL; 
            }
        }
    } 
    else if (modo_actual == MODO_AUTO) {
        // ¡Aquí está la magia nueva!
        if (valor_adc < UMBRAL_ENCENDER && !lamp_state) {
            printf("🌙 Valor LDR bajó de %d. Encendiendo...\n", UMBRAL_ENCENDER);
            set_lamp(true);
        } else if (valor_adc > UMBRAL_APAGAR && lamp_state) {
            printf("☀️ Valor LDR subió de %d. Apagando...\n", UMBRAL_APAGAR);
            set_lamp(false);
        }
    }
}

// ── CONFIGURACIÓN BLE ─────────────────────────────────────
static const uint8_t adv_data[] = {
    0x02, 0x01, 0x06,
    0x0E, 0x09, 'P','i','c','o','W','-','L','a','m','p','a','r','a',
};
static const uint8_t adv_data_len = sizeof(adv_data);

static uint16_t att_read_callback(hci_con_handle_t connection_handle,
    uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    UNUSED(connection_handle);
    if (att_handle == ATT_CHARACTERISTIC_12345678_1234_5678_1234_56789ABCDEF1_01_VALUE_HANDLE) {
        if (buffer) buffer[0] = lamp_state ? '1' : '0';
        return 1;
    }
    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle,
    uint16_t att_handle, uint16_t transaction_mode, uint16_t offset,
    uint8_t *buffer, uint16_t buffer_size) {
    UNUSED(connection_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);

    if (att_handle == ATT_CHARACTERISTIC_12345678_1234_5678_1234_56789ABCDEF1_01_VALUE_HANDLE && buffer_size > 0) {
        char comando[20];
        uint16_t len = buffer_size < 19 ? buffer_size : 19;
        memcpy(comando, buffer, len);
        comando[len] = '\0';

        printf("\n📱 -> Comando recibido de la app: %s\n", comando);

        if (comando[0] == 'M' && comando[1] == ':') {
            modo_actual = MODO_MANUAL;
            set_lamp(comando[2] == '1');
            printf("👉 Cambiando a Modo MANUAL\n");
        } 
        else if (comando[0] == 'A' && comando[1] == ':') {
            modo_actual = MODO_AUTO;
            printf("👉 Cambiando a Modo AUTOMÁTICO. Dejando que la LDR decida...\n");
        } 
        else if (comando[0] == 'T' && comando[1] == ':') {
            modo_actual = MODO_TIMER;
            tiempo_restante = atoi(&comando[2]) + 1; 
            set_lamp(true); 
            printf("👉 Cambiando a Modo TEMPORIZADOR. Cuenta regresiva: %d segundos\n", tiempo_restante);
        }
    }
    return 0;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                bd_addr_t null_addr;
                memset(null_addr, 0, 6);
                gap_advertisements_set_params(0x0030, 0x00a0, 0, 0, null_addr, 0x07, 0x00);
                gap_advertisements_set_data(adv_data_len, (uint8_t *)adv_data);
                gap_advertisements_enable(1);
                printf("✅ BLE encendido y transmitiendo...\n");
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = HCI_CON_HANDLE_INVALID;
            set_lamp(false);
            modo_actual = MODO_MANUAL; 
            gap_advertisements_enable(1);
            printf("\n❌ Celular desconectado. Foco apagado y vuelto a Manual.\n");
            break;
        case HCI_EVENT_LE_META:
            if (hci_event_le_meta_get_subevent_code(packet) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                printf("\n🔥 ¡Celular conectado con éxito!\n");
            }
            break;
        default:
            break;
    }
}

int main(void) {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("Error: cyw43 init failed.\n");
        return -1;
    }

    hardware_init();

    l2cap_init();
    sm_init();
    att_server_init(profile_data, att_read_callback, att_write_callback);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    att_server_register_packet_handler(packet_handler);

    hci_power_control(HCI_POWER_ON);

    btstack_run_loop_set_timer_handler(&loop_timer, timer_handler);
    btstack_run_loop_set_timer(&loop_timer, 1000);
    btstack_run_loop_add_timer(&loop_timer);

    printf("\n==========================================\n");
    printf("   🌟 LÁMPARA INTELIGENTE PICO W V2 🌟    \n");
    printf("==========================================\n");
    printf("Inicializando sistemas...\n\n");

    btstack_run_loop_execute();

    return 0;
}