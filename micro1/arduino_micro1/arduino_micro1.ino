#include <Arduino.h>
#include "klein64.h"

// Variables para controlar el tiempo de lectura
unsigned long time_now = 0;
int period = 1000; // Intervalo de 1 segundo


// Variables para el cifrado Klein
// Definir clave de ejemplo (usa una clave de 8 bytes)
const uint8_t key8[8] = {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF};

uint8_t cipher[8];
String encryptedMessage = "";

void setup() {
  // Iniciar comunicación serial
  Serial.begin(9600);

  time_now = millis();
}

void loop() {
  // Enviar mensaje cada cierto tiempo
  if (millis() >= time_now + period) {
    time_now += period;

    String dataValue = String(analogRead(A0));
    String message = "Dato micro1: " + dataValue;

    // Cifrar el mensaje
    encryptMessage(message, key8);

    // Crear un objeto JSON y enviarlo
    String jsonMessage = "{\"encryptedMessage\": \"" + encryptedMessage +
    "\", \"decryptedMessage\": \"" + message +
    "\", \"microName\": \"" + "source" +
    "\"}";
    Serial.println(jsonMessage);
  }
}


// Función para cifrar el mensaje y concatenar los bloques cifrados
void encryptMessage(String message, const uint8_t key8[8]) {
  int messageLength = message.length();
  
  // Procesar mensaje en bloques de 8 bytes
  encryptedMessage = "";  // Limpiar mensaje cifrado

  for (int i = 0; i < messageLength; i += 8) {
    uint8_t block[8] = {0};  // Inicializar el bloque con ceros
    
    // Copiar los 8 bytes del mensaje al bloque
    for (int j = 0; j < 8 && (i + j) < messageLength; j++) {
      block[j] = (uint8_t)message[i + j];
    }

    // Cifrar el bloque
    klein64_encrypt(block, key8, cipher);

    // Concatenar el bloque cifrado al mensaje cifrado final
    for (int k = 0; k < 8; k++) {
      String byteHex = String(cipher[k], HEX);  // Convertir a hexadecimal

      // Si el valor es de un solo dígito, agregar un cero antes
      if (byteHex.length() < 2) {
        byteHex = "0" + byteHex;
      }

      encryptedMessage += byteHex;
      encryptedMessage += " ";  // Separador para cada byte
    }
  }
}
