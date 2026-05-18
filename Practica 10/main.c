#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define WIFI_SSID "TU_SSID"
#define WIFI_PASS "TU_PASSWORD"

#define POT_PIN A0  // GP26
#define LED_PIN 14  // GP14
#define BTN_PIN 15  // GP15

#define ADC_BITS 12
#define ADC_MAX 4095
#define ADC_VREF 3.3f
#define BLINK_MIN_MS 50
#define BLINK_MAX_MS 1000
#define BUF_SIZE 60
#define SAMPLE_MS 400

WebServer server(80);

float voltBuf[BUF_SIZE];
int bufIdx = 0;
int bufCount = 0;
bool paused = false;
bool ledState = false;
uint16_t rawADC = 0;
float voltage = 0.0f;
uint32_t lastBlinkMs = 0;
uint32_t lastSampleMs = 0;
uint32_t lastBtnMs = 0;

uint32_t adcToBlinkInterval(uint16_t adc) {
    float norm = (float)adc / (float)ADC_MAX;
    return (uint32_t)(BLINK_MAX_MS - norm * (BLINK_MAX_MS - BLINK_MIN_MS));
}

void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=UTF-8", "");
    
    // ¡OJO AQUÍ, REY! El HTML original se perdió en el copy-paste.
    // Pega tu código HTML dentro de las comillas de abajo:
    server.sendContent("<!DOCTYPE html><html><head><title>Monitor ADC</title></head><body>");
    server.sendContent("<h1>Monitor de Potenciometro</h1>");
    server.sendContent("</body></html>");
    
    server.sendContent("");
}

void handleData() {
    String json = "{\"paused\":" + String(paused ? "true" : "false");
    json += ",\"voltage\":" + String(voltage, 3);
    json += ",\"adc\":" + String(rawADC);
    json += ",\"interval\":" + String(adcToBlinkInterval(rawADC));
    json += ",\"data\":[";
    
    for (int i = 0; i < bufCount; i++) {
        int idx = (bufIdx - bufCount + i + BUF_SIZE) % BUF_SIZE;
        json += String(voltBuf[idx], 3);
        if (i < bufCount - 1) json += ",";
    }
    json += "]}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    analogReadResolution(ADC_BITS);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    pinMode(BTN_PIN, INPUT_PULLUP);
    
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        Serial.print("."); 
    }
    
    Serial.print("\nIP: http://"); 
    Serial.println(WiFi.localIP());
    
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
}

void loop() {
    server.handleClient();
    uint32_t now = millis();
    
    // Boton HOLD
    if (digitalRead(BTN_PIN) == LOW && (now - lastBtnMs > 250)) {
        paused = !paused;
        lastBtnMs = now;
        if (paused) digitalWrite(LED_PIN, HIGH);
    }
    
    if (!paused) {
        // Muestreo ADC
        if (now - lastSampleMs >= SAMPLE_MS) {
            rawADC = (uint16_t)analogRead(POT_PIN);
            voltage = (rawADC * ADC_VREF) / (float)ADC_MAX;
            voltBuf[bufIdx] = voltage;
            bufIdx = (bufIdx + 1) % BUF_SIZE;
            if (bufCount < BUF_SIZE) bufCount++;
            lastSampleMs = now;
        }
        
        // Parpadeo LED
        uint32_t interval = adcToBlinkInterval(rawADC);
        if (now - lastBlinkMs >= interval) {
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
            lastBlinkMs = now;
        }
    }
}