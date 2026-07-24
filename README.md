# CasioLink

Biblioteca Arduino para comunicación entre una ESP32-S3 y una Casio fx-9750GIII.

## Funciones principales

```cpp
Casio.begin(config);
Casio.setTransmitValue(valorADC);
Casio.update();

if (Casio.valueAvailable()) {
    uint16_t pwm = Casio.readValue();
}
```

## Rango numérico

La primera versión trabaja con enteros de 12 bits:

- mínimo: 0
- máximo: 4095

## Instalación

1. Comprima o utilice directamente la carpeta `CasioLink`.
2. En Arduino IDE seleccione:
   `Programa > Incluir librería > Añadir biblioteca .ZIP`
3. Abra:
   `Archivo > Ejemplos > CasioLink > ADC_PWM_12bits`

## Conexiones del ejemplo

- ADC: GPIO4
- PWM: GPIO5
- RX ESP32-S3: GPIO18
- TX ESP32-S3: GPIO17

Use adaptación de nivel eléctrico si la salida de la calculadora supera 3.3 V.
