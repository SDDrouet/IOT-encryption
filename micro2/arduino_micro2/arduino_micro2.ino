#include <Arduino.h>
#include <uECC.h>
#include "klein64.h"

#define uECC_OPTIMIZATION_LEVEL 3 // Nivel básico de optimización
#define uECC_SQUARE_FUNC 1

const uint8_t key8[8] = {0x12, 0x34, 0x56, 0x79, 0x90, 0xAB, 0xCD, 0xEF};  // La misma clave de cifrado

uint8_t cipher[8];  // Bloque cifrado
String decryptedMessage = "";  // Mensaje descifrado

// Variables para la generacion de clave y para compartirlas

uint8_t privateKey2[21] = {0};  // Clave privada de Micro 1
uint8_t publicKey1[40] = {0};   // Clave pública de Micro 1
uint8_t publicKey2[40] = {0}; // Almacenará la clave pública del micro 2
uint8_t sharedSecret[8] = {0};  // Clave compartida reducida a 8 bytes
int nonce = 0;

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
      int count = random(0, 5);
      
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
  const struct uECC_Curve_t *curve = uECC_secp160r1();
  int isKeysOK = 0;
  while (true) {
    isKeysOK = uECC_make_key(publicKey2, privateKey2, curve);         // Generar par de claves

    if (isKeysOK == 1) {
      Serial.println("Claves generadas");
      break;
    } else {
      Serial.println("Error: generando par de claves");
    }
  }
}

String getHexString(const uint8_t *data, size_t length) {
  String result = "";
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      result += "0"; // Asegurar que siempre tenga dos dígitos
    }
    result += String(data[i], HEX); // Convertir a HEX y concatenar
    if (i < length - 1) {
      result += " ";
    }
  }
  return result;
}


// Funcion para enviar la clave publica al Micro 2
String jsonPublicKey() {
  // Crear una cadena JSON con la clave pública
  String publicKey2Str = getHexString(publicKey2, sizeof(publicKey2));
  nonce = 50;//random(0, 1000);

  // Crear un objeto JSON y enviarlo
  String jsonMessage = "{\"publicKey\": \"" + publicKey2Str +
  "\", \"nonce\": \"" + nonce +
  "\", \"microName\": \"" + "micro2" +
  "\", \"typeMessage\": \"" + "KEYS" +
  "\"}";

  return jsonMessage;
}

String waitForPublicKey1() {
  if (Serial.available() > 0) {
    String publicKey1Str = Serial.readStringUntil('\n');
    return publicKey1Str;
  }

  return "";
}

void confirmSuccessKeyShared() {
  while (true) {
    if (Serial.available() > 0) {
      String nonceMicro1 = Serial.readStringUntil('\n');
      String nonceMicro2 = String(nonce - 1); 

      if (nonceMicro1 == nonceMicro2) {
        Serial.println("Sincronización exitosa, Listo para compartir datos");
        break;
      } else {
        String message = "Nonce no valido: n1-" + nonceMicro1 + "-  n2-" + nonceMicro2 + "-";
        Serial.println(message);
      }
    }
  }
}

void convertPublicKey(String publicKey) {
  for (int i = 0; i < 40; i++) {
    String byteHex = publicKey.substring(i * 2, i * 2 + 2); // Obtener cada par de caracteres
    publicKey1[i] = strtol(byteHex.c_str(), NULL, 16);         // Convertir hex a uint8_t
  }
  
}

// Función para enviar la clave pública a Micro 2
void syncSharedKeys() {  
  String publicKeyStr = "";
  Serial.println("Esperando por clave....");
  while (true) {
    publicKeyStr = waitForPublicKey1();
    if (publicKeyStr != "") {
      convertPublicKey(publicKeyStr);
      Serial.println("Public Key 1 obtenida");
      break;
    }
  }
}

// Función para calcular la clave compartida
void getSharedSecret() {
  const struct uECC_Curve_t *curve = uECC_secp160r1();

  Serial.println("privatekey2: " + getHexString(privateKey2, sizeof(privateKey2)));
  Serial.println("publickey2: " + getHexString(publicKey2, sizeof(publicKey2)));
  Serial.println("publicKey1: " + getHexString(publicKey1, sizeof(publicKey1)));


  
  uint8_t fullSharedSecret[20] = {0};  // Clave compartida completa
  int isKeysOK = 0;
  
  while (isKeysOK == 0) {
    //isKeysOK = uECC_shared_secret(publicKey1, privateKey2, fullSharedSecret, curve);

    if (isKeysOK == 1) {
      //Serial.println("privatekey2:" + getHexString(privateKey2, sizeof(privateKey2)));
      //Serial.println("publickey2:" + getHexString(publicKey2, sizeof(publicKey2)));
      //Serial.println("publicKey1:" + getHexString(publicKey1, sizeof(publicKey1)));
      //optimizeKeySelection(fullSharedSecret, sharedSecret);
    } else {
      Serial.println("Error calculando clave compartida, volviendo a calcular");
    }
  }
}

// Función para optimizar la selección de la clave final (basada en puntuación simple)
void optimizeKeySelection(uint8_t key[20], uint8_t optimizedKey[8]) {
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

  Serial.println("Shared secret obtenido");
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
  String jsonMessage = "";
  Serial.begin(115200);
  uECC_set_rng(&RNG);

  generateInitialKeys();    // Generar las claves iniciales
  syncSharedKeys();
  Serial.println("privatekey2:" + getHexString(privateKey2, sizeof(privateKey2)));
  Serial.println("publickey2:" + getHexString(publicKey2, sizeof(publicKey2)));
  Serial.println("publicKey1:" + getHexString(publicKey1, sizeof(publicKey1)));
  /*
  Serial.println("privatekey2:" + getHexString(privateKey2, sizeof(privateKey2)));
  Serial.println("publickey2:" + getHexString(publicKey2, sizeof(publicKey2)));
  Serial.println("publicKey1:" + getHexString(publicKey1, sizeof(publicKey1)));
  Serial.println("sharedSecret:" + getHexString(sharedSecret, sizeof(sharedSecret)));*/
  //jsonMessage = jsonPublicKey();
  //Serial.println(jsonMessage);
  Serial.println("Fin de Setup");
}

void getSecret(uint8_t *fullSharedSecret, size_t fullSharedSecretSize) {
  const struct uECC_Curve_t *curve = uECC_secp160r1();

  int isKeysOK = uECC_shared_secret(publicKey1, privateKey2, fullSharedSecret, curve);

  Serial.println("privatekey2:" + getHexString(privateKey2, sizeof(privateKey2)));
  Serial.println("publickey2:" + getHexString(publicKey2, sizeof(publicKey2)));
  Serial.println("publicKey1:" + getHexString(publicKey1, sizeof(publicKey1)));
  Serial.println("fullSharedSecret:" + getHexString(fullSharedSecret, fullSharedSecretSize));
}


void loop() {
  //getSharedSecret();
  uint8_t fullSharedSecret[20]; // Suponiendo que este es el tamaño del secreto compartido
  size_t fullSharedSecretSize = sizeof(fullSharedSecret); // Tamaño del arreglo

  getSecret(fullSharedSecret, fullSharedSecretSize);


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





