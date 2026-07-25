#include <Arduino.h>

#define TX_PIN 17  
#define RX_PIN 16  
#define ADC_PIN 5  // Cable de señal analógica en el GPIO 5 (ADC1)

HardwareSerial CasioSerial(2); 

// Variable global para almacenar la lectura analógica limpia
volatile int valor_adc_actual = 0;

// Envía un byte y congela al ESP32 por unos milisegundos para que la Casio lo procese
void enviar_datos_casio_lento(uint8_t *buffer, int longitud) {
    for (int i = 0; i < longitud; i++) {
        CasioSerial.write(buffer[i]);
        CasioSerial.flush(); // Fuerza a que el byte salga físicamente por el pin TX
        delay(5);            // Espera 5 milisegundos completos entre bytes individuales
    }
}

uint8_t to_bcd(int val) {
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

// Función matemática de Checksum exacta
uint8_t calcular_checksum(uint8_t *buffer, int longitud) {
    uint16_t suma = 0;
    for (int i = 0; i < longitud; i++) {
        suma += buffer[i];
    }
    uint8_t resultado_resta = (uint8_t)(suma - 0x3A);
    return (uint8_t)((~resultado_resta) + 1);
}

// Función principal corregida con indexación explícita
void Asig_value_packet(int value, uint8_t *packet) {
    packet[0] = 0x3A; 
    packet[1] = 0x00; 
    packet[2] = 0x01; 
    packet[3] = 0x00; 
    packet[4] = 0x01; 

    for (int i = 5; i <= 12; i++) {
        packet[i] = 0x00;
    }

    if (value >= 0 && value <= 9) {
        packet[5] = to_bcd(value); 
        packet[14] = 0x00; // N(15)
    } 
    else if (value >= 10 && value <= 99) {
        packet[5] = to_bcd(value); 
        packet[14] = 0x01; // N(15)
    } 
    else if (value >= 100 && value <= 999) {
        packet[5] = to_bcd(value / 100);   
        packet[6] = to_bcd(value % 100);   
        packet[14] = 0x02; // N(15)
    } 
    else if (value >= 1000 && value <= 9999) {
        packet[5] = to_bcd(value / 100);   
        packet[6] = to_bcd(value % 100);   
        packet[14] = 0x03; // N(15)
    }

    packet[13] = 0x01; // N(14) siempre es 1
    packet[15] = calcular_checksum(packet, 15); // N(16) Checksum
}

bool esperar_byte(uint8_t byte_esperado, uint32_t timeout_ms) {
    uint32_t inicio = millis();
    while (millis() - inicio < timeout_ms) {
        if (CasioSerial.available() > 0) {
            if (CasioSerial.read() == byte_esperado) {
                return true;
            }
        }
        yield();
    }
    return false;
}

bool recibir_request_packet() {
    uint32_t inicio = millis();
    int index = 0;
    while (millis() - inicio < 500 && index < 50) {
        if (CasioSerial.available() > 0) {
            CasioSerial.read(); 
            index++;
        }
        yield();
    }
    return (index == 50);
}

void enviar_variable_A() {
    uint8_t var_desc[50];
    memset(var_desc, 0xFF, 50); 
    
    // CORREGIDO: ":VAL" en ASCII exacto (3A 56 41 45)
    var_desc[0] = 0x3A; 
    var_desc[1] = 0x56; 
    var_desc[2] = 0x41; 
    var_desc[3] = 0x45; // Letra 'E' fija corregida (era 0x4L)
    var_desc[4] = 0x00; 
    var_desc[5] = 0x56; 
    var_desc[6] = 0x4D; // "VM"
    var_desc[7] = 0x00; 
    var_desc[8] = 0x01; 
    var_desc[9] = 0x00; 
    var_desc[10] = 0x01; 
    var_desc[11] = 0x41; // Variable 'A'
    
    const char* var_text = "Variable";
    for(int i = 0; i < 8; i++) var_desc[19 + i] = var_text[i];
    
    var_desc[27] = 0x52; // 'R' para Real
    var_desc[28] = 0x0A; 
    
    var_desc[49] = calcular_checksum(var_desc, 49);
    enviar_datos_casio_lento(var_desc, 50);
}

void enviar_end_packet() {
    uint8_t end_pack[50];
    memset(end_pack, 0xFF, 50);
    
    end_pack[0] = 0x3A; 
    end_pack[1] = 0x45; 
    end_pack[2] = 0x4E; 
    end_pack[3] = 0x44; // ":END"
    end_pack[49] = 0x56; // Checksum de cierre fijo estipulado por Casio
    
    enviar_datos_casio_lento(end_pack, 50);
}

void setup() {
    Serial.begin(115200); 
    CasioSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    
    pinMode(ADC_PIN, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    Serial.println("ESP32-S3 emulando Docklight sin errores de sintaxis. Listo...");
}

void loop() {
    // Tomar lectura constante en background sin bloquear la UART
    valor_adc_actual = analogReadMilliVolts(ADC_PIN);

    if (CasioSerial.available() > 0) {
        uint8_t byte_recibido = CasioSerial.read();
        
        if (byte_recibido == 0x15) {
            Serial.println("\n[Casio conectada]");
            
            delay(20); // Tiempo holgado para responder presente
            CasioSerial.write(0x13);
            CasioSerial.flush();
            
            if (recibir_request_packet()) {
                delay(20);
                CasioSerial.write(0x06); // Enviar ACK inicial
                CasioSerial.flush();
                
                if (esperar_byte(0x06, 500)) {
                    // Transmitir descripción corregida (:VAL)
                    enviar_variable_A();
                    
                    if (esperar_byte(0x06, 1000)) {
                        int valor_final = valor_adc_actual;
                        Serial.printf(">>> TRANSMITIENDO: %d mV <<<\n", valor_final);
                        
                        // Buffer con tamaño explícito de 16 bytes obligatorios
                        uint8_t value_packet[16];
                        Asig_value_packet(valor_final, value_packet);
                        
                        // Transmitir los 16 bytes con pausas
                        enviar_datos_casio_lento(value_packet, 16);
                        
                        if (esperar_byte(0x06, 1000)) {
                            enviar_end_packet();
                            Serial.println(">>> Transmisión completada satisfactoriamente <<<");
                        } else {
                            Serial.println("Error: Timeout o rechazo del Value Packet.");
                        }
                    } else {
                        Serial.println("Error: Timeout o rechazo de la Descripción.");
                    }
                }
            }
        }
    }
    yield();
}
