#include <Arduino.h>

//--- DEFINICION DE PINES ---
// Segmentos A, B, C, D, E, F, G
const int segmentPins[] = {28, 27, 12, 13, 14, 22, 26};

// Catodos Comunes (Digitos)
const int pinDecenas = 11;
const int pinUnidades = 10;

// Boton
const int pinBoton = 15;

//--- MAPA DE NUMEROS (0-9) ---
// Configuracion para CATODO COMUN:
// 1 (HIGH) = Enciende el segmento
// 0 (LOW) = Apaga el segmento
// Orden: A, B, C, D, E, F, G
const byte numeros[10][7] = {
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}  // 9
};

//--- VARIABLES GLOBALES ---
int contador = 0;
int direccion = 1; // 1 = Ascendente, -1 = Descendente
unsigned long ultimoTiempoConteo = 0;
const int intervaloConteo = 200; // 200 ms entre incrementos

// Variables para el boton (anti-rebote por software)
int ultimoEstadoBoton = HIGH;
unsigned long ultimoTiempoRebote = 0;
const int delayRebote = 50; // 50 ms

void setup() {
    Serial.begin(115200);

    // Pines de segmentos como salida
    for (int i = 0; i < 7; i++) {
        pinMode(segmentPins[i], OUTPUT);
    }

    // Pines de catodos como salida
    pinMode(pinDecenas, OUTPUT);
    pinMode(pinUnidades, OUTPUT);

    // Boton con pull-up interno
    pinMode(pinBoton, INPUT_PULLUP);

    // Apagar displays al inicio (HIGH en catodo = apagado)
    digitalWrite(pinDecenas, HIGH);
    digitalWrite(pinUnidades, HIGH);

    Serial.println("---Sistema Iniciado---");
}

// Muestra 'numero' en el display seleccionado por 'pinDigito'
void dibujarDigito(int numero, int pinDigito) {
    // 1. Apagar ambos displays para evitar efecto fantasma
    digitalWrite(pinDecenas, HIGH);
    digitalWrite(pinUnidades, HIGH);

    // 2. Configurar segmentos
    for (int i = 0; i < 7; i++) {
        digitalWrite(segmentPins[i], numeros[numero][i]);
    }

    // 3. Activar unicamente el display deseado
    digitalWrite(pinDigito, LOW);
}

void loop() {
    unsigned long tiempoActual = millis();

    //--- 1. LOGICA DEL CONTADOR (no bloqueante) ---
    if (tiempoActual - ultimoTiempoConteo >= intervaloConteo) {
        contador += direccion;

        // Envolvimiento en limites 0-99
        if (contador > 99) contador = 0;
        if (contador < 0) contador = 99;

        Serial.print("Contador: ");
        Serial.print(contador);
        Serial.print(" | Direccion: ");
        Serial.println(direccion == 1 ? "ASC (+)" : "DESC (-)");

        ultimoTiempoConteo = tiempoActual;
    }

    //--- 2. LOGICA DEL BOTON (deteccion de flanco + anti rebote) ---
    int lecturaBoton = digitalRead(pinBoton);

    if (lecturaBoton != ultimoEstadoBoton) {
        if ((tiempoActual - ultimoTiempoRebote) > delayRebote) {
            if (lecturaBoton == LOW) {
                direccion *= -1; // Invierte la direccion
                Serial.println(">> BOTON PRESIONADO: Cambiando direccion <<");
            }
            ultimoTiempoRebote = tiempoActual;
        }
    }
    ultimoEstadoBoton = lecturaBoton;

    //--- 3. MULTIPLEXEO (refresco continuo de ambos displays) ---
    int decenas = contador / 10;
    int unidades = contador % 10;

    dibujarDigito(decenas, pinDecenas);
    delay(5); // ~5 ms por digito -> ~100 Hz de refresco
    dibujarDigito(unidades, pinUnidades);
    delay(5);
}