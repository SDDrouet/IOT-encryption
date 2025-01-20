#include <Arduino.h>
#include "klein64.h"

const uint8_t key8[8] = {0x12, 0x34, 0x56, 0x79, 0x90, 0xAB, 0xCD, 0xEF};  // La misma clave de cifrado

uint8_t cipher[8];  // Bloque cifrado
String decryptedMessage = "";  // Mensaje descifrado

void setup() {
  // Iniciar comunicación serial
  Serial.begin(9600);
}

void loop() {
  // Comprobar si hay datos disponibles por el puerto serial
  if (Serial.available()) {
    String encryptedMessage = Serial.readStringUntil('\n');  // Leer el mensaje cifrado hasta un salto de línea
    encryptedMessage = encryptedMessage.substring(0, encryptedMessage.length());
    decryptedMessage = decryptMessage(encryptedMessage, key8);  // Desencriptar el mensaje

    String jsonMessage = "{\"decryptedMessage\": \"" + decryptedMessage +
    "\", \"microName\": \"" + "destination" +
    "\"}";

    // Mostrar el mensaje descifrado
    Serial.println(jsonMessage);
  }
}

String decryptMessage(String encryptedMessage, const uint8_t key8[8]) {
  String message = "";

  // Eliminar espacios si los hay
  encryptedMessage.replace(" ", "");

  // Verificar que la longitud del mensaje cifrado sea un múltiplo de 16
  if (encryptedMessage.length() % 16 != 0) {
    Serial.println("Error: El mensaje cifrado no tiene un tamaño múltiplo de 8 bytes.");
    return "";
  }

  // Dividir el mensaje cifrado en bloques de 16 caracteres (8 bytes)
  for (int i = 0; i < encryptedMessage.length(); i += 16) {  // 16 caracteres para 8 bytes
    uint8_t block[8] = {0};

    // Extraer 8 bloques de 2 caracteres hexadecimales cada uno
    for (int j = 0; j < 8; j++) {
      String byteHex = encryptedMessage.substring(i + j * 2, i + j * 2 + 2);  // Obtener cada byte en formato hex
      block[j] = strtol(byteHex.c_str(), NULL, 16);  // Convertir el hex a byte
    }

    // Desencriptar el bloque
    klein64_decrypt(block, key8, cipher);

    // Reconstruir el mensaje original (convertir los bytes a caracteres)
    for (int k = 0; k < 8; k++) {
      if (cipher[k] != 0) {  // Solo agregar caracteres que no sean cero (espacios)
        message += (char)cipher[k];
      }
    }
  }

  return message;  // Retornar el mensaje desencriptado
}
