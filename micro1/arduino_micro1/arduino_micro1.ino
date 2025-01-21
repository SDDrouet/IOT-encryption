#include <Arduino.h>
#include <uECC.h>
#include "klein64.h"

// Variables para controlar el tiempo de lectura
unsigned long time_now = 0;
int period = 1000; // Intervalo de 1 segundo


// Variables para el cifrado Klein
// Definir clave de ejemplo (usa una clave de 8 bytes)
const uint8_t key8[8] = {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF};
uint8_t cipher[8];
String encryptedMessage = "";


// Variables para la generacion de clave y para compartirlas

uint8_t privateKey1[21];  // Clave privada de Micro 1
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
  uECC_make_key(publicKey1, privateKey1, curve);         // Generar par de claves
}

// Funcion para enviar la clave publica al Micro 2
String jsonPublicKey() {
  // Crear una cadena JSON con la clave pública
  String publicKey1Str = "";
  for (int i = 0; i < 40; i++) {
    if (publicKey1[i] < 0x10) {
      publicKey1Str += "0";  // Asegurar siempre dos dígitos
    }
    publicKey1Str += String(publicKey1[i], HEX);
  }

  // Crear un objeto JSON y enviarlo
  String jsonMessage = "{\"publicKey\": \"" + publicKey1Str +
  "\", \"microName\": \"" + "source" +
  "\", \"typeMessage\": \"" + "KEYS" +
  "\"}";

  return jsonMessage;
}

bool waitForPublicKey2() {
  // Comprobar si hay datos disponibles por el puerto serial
  if (Serial.available()) {
    String publicKey2Str = Serial.readStringUntil('\n'); // Leer hasta un salto de línea

    // Limpiar espacios y caracteres innecesarios
    publicKey2Str.trim();

    // Verificar que la longitud sea válida (40 bytes en formato hexadecimal -> 80 caracteres)
    if (publicKey2Str.length() != 80) {
      Serial.println("Error: La clave pública recibida no tiene el tamaño esperado.");
      return "";
    }

    // Convertir la cadena al arreglo uint8_t
    for (int i = 0; i < 40; i++) {
      String byteHex = publicKey2Str.substring(i * 2, i * 2 + 2); // Obtener cada par de caracteres
      publicKey2[i] = strtol(byteHex.c_str(), NULL, 16);         // Convertir hex a uint8_t
    }
    return true;
  }
  return false; // Retornar cadena vacía si no hay datos disponibles
}

// Función para enviar la clave pública a Micro 2
void syncSharedKeys() {  
  int syncPeriod = 5000;
  int syncTimeNow = 0;
  bool isPublicKeySet = false;
  String jsonKeyMessage = jsonPublicKey();

  while (true) {
    if (millis() >= syncTimeNow + syncPeriod) {
      syncTimeNow += syncPeriod;
      Serial.println(jsonKeyMessage);
    }

    isPublicKeySet = waitForPublicKey2();
    if (isPublicKeySet) {
      break;
    }
  }
}

// Función para calcular la clave compartida
void getSharedSecret() {
  uint8_t fullSharedSecret[32];  // Clave compartida completa
  const struct uECC_Curve_t *curve = uECC_secp256r1();

  // Calcular la clave compartida
  if (uECC_shared_secret(publicKey2, privateKey1, fullSharedSecret, curve)) {
    Serial.println("Clave compartida completa calculada:");
    for (int i = 0; i < 32; i++) {
      Serial.print(fullSharedSecret[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Optimizar la selección de la clave final (tomar los mejores 8 bytes)
    optimizeKeySelection(fullSharedSecret, sharedSecret);

    Serial.println("Clave optimizada seleccionada:");
    for (int i = 0; i < 8; i++) {
      Serial.print(sharedSecret[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  } else {
    Serial.println("Error calculando clave compartida");
  }
}

// Función para optimizar la selección de la clave final (basada en puntuación simple)
void optimizeKeySelection(uint8_t key[32], uint8_t optimizedKey[8]) {
  uint8_t tempKey[8];
  int bestScore = -1;

  // Probar diferentes combinaciones para la selección de los 8 bytes
  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 8; j++) {
      // Generar una combinación tomando diferentes bloques de 32 bytes
      for (int k = 0; k < 8; k++) {
        tempKey[k] = key[(i + k) % 32];  // Ciclo a través de los bytes para obtener variabilidad
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

// ------------------------------------------------------------------------


// Funciones para realizar el proceso de encriptacion con klein
//------------------------------------------------------------------------

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
//------------------------------------------------------------------------


void setup() {
  // Iniciar comunicación serial
  Serial.begin(9600);
  uECC_set_rng(&RNG);

  generateInitialKeys();    // Generar las claves iniciales
  syncSharedKeys();  // Enviar la clave pública al Micro 2 y obtener su clave
  getSharedSecret();
  time_now = millis();
}

void loop() {
  // Enviar mensaje cada cierto tiempo
  if (millis() >= time_now + period) {
    time_now += period;

    String dataValue = String(analogRead(A0));
    String message = "Dato micro1: " + dataValue;

    // Cifrar el mensaje
    //encryptMessage(message, key8);

    // Crear un objeto JSON y enviarlo
    String jsonMessage = "{\"encryptedMessage\": \"" + encryptedMessage +
    "\", \"decryptedMessage\": \"" + message +
    "\", \"microName\": \"" + "source" +
    "\", \"typeMessage\": \"" + "ENCRYPTED MESSAGE" +
    "\"}";
    //Serial.println(jsonMessage);
    Serial.println("MICRO 1: En loop principal");
  }
}
