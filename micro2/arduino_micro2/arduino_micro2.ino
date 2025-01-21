#include <Arduino.h>
#include <uECC.h>
#include "klein64.h"

const uint8_t key8[8] = {0x12, 0x34, 0x56, 0x79, 0x90, 0xAB, 0xCD, 0xEF};  // La misma clave de cifrado

uint8_t cipher[8];  // Bloque cifrado
String decryptedMessage = "";  // Mensaje descifrado

// Variables para la generacion de clave y para compartirlas

uint8_t privateKey2[21];  // Clave privada de Micro 1
uint8_t publicKey1[40];   // Clave pública de Micro 1
uint8_t publicKey2[40]; // Almacenará la clave pública del micro 2
uint8_t sharedSecret[20];  // Clave compartida reducida a 8 bytes

// Funciones para la generación de las claves
// --------------------------------------------------------------------

static int RNG(uint8_t *dest, unsigned size) {
  // Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of 
  // random noise). This can take a long time to generate random data if the result of analogRead(0) 
  // doesn't change very frequently.
  while (size) {
    uint8_t val = 0;
    for (unsigned i = 0; i < 8; ++i) {
      int init = analogRead(A0);
      //int count = 0;
      int count = random(0, 256);
      
      /*
      while (analogRead(A0) == init) {
        ++count;
      }*/
      
      if (count == 0) {
         val = (val << 1) | (init & 0x01);
      } else {
         val = (val << 1) | (count & 0x01);
      }
    }
    *dest = val;
    ++dest;
    --size;
  }
  // NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
  return 1;
}

// Función para generar claves iniciales
void generateInitialKeys() {
  const struct uECC_Curve_t * curve = uECC_secp160r1();
  uECC_make_key(publicKey2, privateKey2, curve);         // Generar par de claves
}

// Funcion para enviar la clave publica al Micro 2
String jsonPublicKey() {
  // Crear una cadena JSON con la clave pública
  String publicKey2Str = "";
  for (int i = 0; i < 40; i++) {
    if (publicKey2[i] < 0x10) {
      publicKey2Str += "0";  // Asegurar siempre dos dígitos
    }
    publicKey2Str += String(publicKey2[i], HEX);
  }

  // Crear un objeto JSON y enviarlo
  String jsonMessage = "{\"publicKey\": \"" + publicKey2Str +
  "\", \"microName\": \"" + "micro2" +
  "\", \"typeMessage\": \"" + "KEYS" +
  "\"}";

  return jsonMessage;
}

bool waitForPublicKey1() {
  // Comprobar si hay datos disponibles por el puerto serial
  if (Serial.available()) {
    String publicKey1Str = Serial.readStringUntil('\n'); // Leer hasta un salto de línea
    publicKey1Str = publicKey1Str.substring(0, publicKey1Str.length());

    Serial.println("key: " + publicKey1Str);
    Serial.println("key len: " + publicKey1Str.length());
    // Verificar que la longitud sea válida (40 bytes en formato hexadecimal -> 80 caracteres)
    if (publicKey1Str.length() != 80) {
      Serial.println("Error: La clave publica recibida no tiene el tamano esperado.");
      Serial.println("key: " + publicKey1Str);
      Serial.println("key: " + publicKey1Str);
      Serial.println("key: " + publicKey1Str);
      Serial.println("key: " + publicKey1Str);
      Serial.println("key: " + publicKey1Str);
      Serial.println("key: " + publicKey1Str);
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      Serial.println("key len: " + publicKey1Str.length());
      return 0;
    }

    Serial.println("key okey: " + publicKey1Str);

    // Convertir la cadena al arreglo uint8_t
    for (int i = 0; i < 40; i++) {
      String byteHex = publicKey1Str.substring(i * 2, i * 2 + 2); // Obtener cada par de caracteres
      publicKey1[i] = strtol(byteHex.c_str(), NULL, 16);         // Convertir hex a uint8_t
    }
    return 1;
  }
  return 0; // Retornar cadena vacía si no hay datos disponibles
}

// Función para enviar la clave pública a Micro 2
void syncSharedKeys() {  
  bool isPublicKeySet = 0;
  String jsonKeyMessage = jsonPublicKey();

  Serial.println("Esperando por clave....");
  while (true) {
    isPublicKeySet = waitForPublicKey1();
    if (isPublicKeySet) {
      Serial.println(jsonKeyMessage);
      break;
    }
  }
  Serial.println("Clave obtenida");
}

// Función para calcular la clave compartida
void getSharedSecret() {
  uint8_t fullSharedSecret[20];  // Clave compartida completa
  const struct uECC_Curve_t *curve = uECC_secp160r1();

  // Calcular la clave compartida
  if (uECC_shared_secret(publicKey1, privateKey2, fullSharedSecret, curve)) {
    /*Serial.println("Clave compartida completa calculada:");
    for (int i = 0; i < 20; i++) {
      Serial.print(fullSharedSecret[i], HEX);
      Serial.print(" ");
    }
    Serial.println();*/

    // Optimizar la selección de la clave final (tomar los mejores 8 bytes)
    optimizeKeySelection(fullSharedSecret, sharedSecret);

    /*Serial.println("Clave optimizada seleccionada:");
    for (int i = 0; i < 8; i++) {
      Serial.print(sharedSecret[i], HEX);
      Serial.print(" ");
    }
    Serial.println();*/
  } else {
    Serial.println("Error calculando clave compartida");
  }
}

// Función para optimizar la selección de la clave final (basada en puntuación simple)
void optimizeKeySelection(uint8_t key[32], uint8_t optimizedKey[8]) {
  uint8_t tempKey[8];
  int bestScore = -1;

  // Probar diferentes combinaciones para la selección de los 8 bytes
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 8; j++) {
      // Generar una combinación tomando diferentes bloques de 32 bytes
      for (int k = 0; k < 8; k++) {
        tempKey[k] = key[(i + k) % 20];  // Ciclo a través de los bytes para obtener variabilidad
      }

      // Evaluar la puntuación
      int tempScore = score(tempKey);
      if (tempScore > bestScore) {
        bestScore = tempScore;
        memcpy(optimizedKey, tempKey, 8); // Guardar la mejor clave
      }
    }
  }
}

// Función de puntuación para evaluar la "calidad" de los bytes
int score(uint8_t key[8]) {
  int score = 0;
  for (int i = 0; i < 8; i++) {
    score += key[i] % 16;  // Añadir un criterio sencillo basado en el valor
  }
  return score;
}

// --------------------------------------------------------------------

void setup() {
  // Iniciar comunicación serial
  Serial.begin(9600);

  uECC_set_rng(&RNG);
  generateInitialKeys();    // Generar las claves iniciales
  syncSharedKeys();  // Enviar la clave pública al Micro 2 y obtener su clave
  getSharedSecret();
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

  // Verificar que la longitud del mensaje cifrado sea un múltiplo de 2
  if (encryptedMessage.length() % 2 != 0) {
    Serial.println("Error: El mensaje cifrado no tiene un tamaño múltiplo de 2 bytes.");
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
