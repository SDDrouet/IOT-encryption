#include <Arduino.h>
#include <uECC.h>
#include "klein64.h"

// Constantes de configuración
#define KEY_SIZE 8
#define PRIVATE_KEY_SIZE 21
#define PUBLIC_KEY_SIZE 40
#define SHARED_SECRET_SIZE 20
#define HEX_CHARS_PER_BYTE 2
#define MAX_RETRIES 5

// Estructura para almacenar el estado de claves
struct KeyState {
    uint8_t privateKey1[PRIVATE_KEY_SIZE];
    uint8_t publicKey1[PUBLIC_KEY_SIZE];
    uint8_t publicKey2[PUBLIC_KEY_SIZE];
    uint8_t fullSharedSecret[SHARED_SECRET_SIZE];
    uint8_t sharedSecret[KEY_SIZE];
    const struct uECC_Curve_t *curve = uECC_secp160r1();
    uint16_t nonce;
    bool keysVerified;

    KeyState() : nonce(0), keysVerified(false) {
        memset(privateKey1, 0, PRIVATE_KEY_SIZE);
        memset(publicKey1, 0, PUBLIC_KEY_SIZE);
        memset(publicKey2, 0, PUBLIC_KEY_SIZE);
        memset(fullSharedSecret, 0, SHARED_SECRET_SIZE);
        memset(sharedSecret, 0, KEY_SIZE);
    }
};

static KeyState keyState;
static uint8_t cipher[8];
static String encryptedMessage = "";

long time_now;
long period = 200;

unsigned long startTime;
unsigned long finalTime;

// Función mejorada de RNG usando precompilación
static int RNG(uint8_t *dest, unsigned size) {
    while (size--) {
        uint8_t val = 0;
        for (uint8_t i = 0; i < 8; ++i) {
            int init = analogRead(A0);
            uint8_t count = random(0, 3);
            val = (val << 1) | (count ? (count & 0x01) : (init & 0x01));
        }
        *dest++ = val;
    }
    return 1;
}

// Función para generar claves iniciales
bool generateInitialKeys() {
    uint8_t attempts = 0;
    const uint8_t MAX_ATTEMPTS = 5;

    while (attempts++ < MAX_ATTEMPTS) {
        if (uECC_make_key(keyState.publicKey1, keyState.privateKey1, keyState.curve)) {
            return true;
        }
        delay(10);  // Pequeña pausa entre intentos
    }
    return false;
}

// Función para convertir array de bytes a string hexadecimal
String getHexString(const uint8_t *data, size_t length) {
    String result;
    result.reserve(length * 2);
    char buffer[3] = {0};
    for (size_t i = 0; i < length; i++) {
        sprintf(buffer, "%02X", data[i]);
        result += buffer;
    }
    return result;
}

// Función para convertir string hexadecimal a array de bytes
void hexStringToBytes(const String &hexString, uint8_t *output, size_t outputSize) {
    for (size_t i = 0; i < outputSize; i++) {
        char highNibble = hexString[i * 2];
        char lowNibble = hexString[i * 2 + 1];
        highNibble = (highNibble <= '9') ? highNibble - '0' : (highNibble & 0x7) + 9;
        lowNibble = (lowNibble <= '9') ? lowNibble - '0' : (lowNibble & 0x7) + 9;
        output[i] = (highNibble << 4) | lowNibble;
    }
}

// Función para enviar la clave pública
void sendPublicKey() {
    String publicKeyHex = getHexString(keyState.publicKey1, PUBLIC_KEY_SIZE);
    Serial.print(F("{\"publicKey\": \"" ));
    Serial.print(publicKeyHex);
    Serial.println(F("\", \"microName\": \"micro1\", \"typeMessage\": \"KEYS\"}"));
}

// Función para recibir la clave pública del otro microcontrolador
void receivePublicKey() {
    while (!Serial.available()) delay(10);
    String publicKeyHex = Serial.readStringUntil('\n');
    hexStringToBytes(publicKeyHex, keyState.publicKey2, PUBLIC_KEY_SIZE);
}

// Nueva función para encriptar mensajes usando Klein
void encryptMessage(const String &message, const uint8_t *key) {
    int messageLength = strlen(message.c_str());
    encryptedMessage = "";
    
    for (int i = 0; i < messageLength; i += 8) {
        uint8_t block[8] = {0};
        
        // Copiar los bytes del mensaje al bloque
        for (int j = 0; j < 8 && (i + j) < messageLength; j++) {
            block[j] = (uint8_t)message[i + j];
        }

        // Cifrar el bloque
        klein64_encrypt(block, key, cipher);

        // Convertir a hexadecimal
        encryptedMessage += getHexString(cipher, 8);
    }
}

size_t getMessageLength(const String &message) {
    size_t messageLength = 0;

    // Recorrer manualmente los caracteres de la cadena
    for (size_t i = 0; !(message[i] == '\0' || message[i] == '\n' || message[i] == '\r'); i++) {
        messageLength++;
    }

    return messageLength;
}

// Función optimizada para descifrar mensajes
String decryptMessage(const String &encryptedMessage) {
    String result;
    size_t messageLength = getMessageLength(encryptedMessage);
    
    if (messageLength % 16 != 0) return result;  // Validación rápida
    
    result.reserve(messageLength / 2);  // Pre-reservar espacio
    uint8_t block[8] = {0};
    uint8_t decrypted[8] = {0};
    
    for (size_t i = 0; i < messageLength; i += 16) {
        Serial.print(i);
        // Convertir 16 caracteres hex a 8 bytes
        for (size_t j = 0; j < 8; j++) {
            char highNibble = encryptedMessage[i + j * 2];
            char lowNibble = encryptedMessage[i + j * 2 + 1];
            
            highNibble = (highNibble <= '9') ? highNibble - '0' : (highNibble & 0x7) + 9;
            lowNibble = (lowNibble <= '9') ? lowNibble - '0' : (lowNibble & 0x7) + 9;
            
            block[j] = (highNibble << 4) | lowNibble;
        }
        Serial.println(F(""));
        
        klein64_decrypt(block, keyState.sharedSecret, decrypted);
        
        for (uint8_t k = 0; k < 8; k++) {
            if (decrypted[k]) result += (char)decrypted[k];
        }
    }
    
    return result;
}

// Nueva función para enviar el nonce encriptado
void sendEncryptedNonce(String encryptedNonce) {
    Serial.print(F("{\"encryptedNonce\": \"" ));
    Serial.print(encryptedNonce);
    Serial.println(F("\", \"microName\": \"micro1\", \"typeMessage\": \"NONCE\"}"));
}

// Función de puntuación basada en entropía y distribución
int score(const uint8_t key[8]) {
    int score = 0;
    for (int i = 0; i < 8; i++) {
        score += (key[i] * (i + 1)) % 256;  // Pondera posiciones
    }
    return score;
}

// Función hash determinista basada en suma ponderada
uint32_t simpleHash(const uint8_t key[20]) {
    uint32_t hash = 0;
    for (int i = 0; i < 20; i++) {
        hash = (hash * 33) ^ key[i];  // Multiplicación y XOR para más entropía
    }
    return hash;
}

// Algoritmo heurístico de selección basado en IA
void optimizeKeySelection(const uint8_t key[20], uint8_t optimizedKey[8]) {
    uint8_t tempKey[8];
    int bestScore = -1;

    // Determinar índice inicial con base en un hash (simulación de IA)
    uint32_t startIndex = simpleHash(key) % 20;

    // Probar diferentes combinaciones usando una búsqueda heurística
    for (int i = 0; i < 5; i++) {  // 5 intentos de selección "inteligente"
        uint8_t tempKey[8];

        // Selección heurística de 8 bytes no consecutivos
        for (int j = 0; j < 8; j++) {
            tempKey[j] = key[(startIndex + (j * 3)) % 20];  // Patrón distribuido
        }

        // Evaluar la calidad de la clave
        int tempScore = score(tempKey);
        if (tempScore > bestScore) {
            bestScore = tempScore;
            memcpy(optimizedKey, tempKey, 8);
        }

        // Cambiar punto de inicio con una mutación ligera (simulación de IA)
        startIndex = (startIndex + 7) % 20;
    }
}


// Función para imprimir todas las claves (debug)
void printAllKeys() {
    Serial.println(F("\n-------- DEBUG: KEYS STATUS --------"));
    Serial.print(F("Private Key 1: "));
    Serial.println(getHexString(keyState.privateKey1, PRIVATE_KEY_SIZE));
    
    Serial.print(F("Public Key 1: "));
    Serial.println(getHexString(keyState.publicKey1, PUBLIC_KEY_SIZE));
    
    Serial.print(F("Public Key 2 (received): "));
    Serial.println(getHexString(keyState.publicKey2, PUBLIC_KEY_SIZE));
    
    Serial.print(F("Full Shared Secret: "));
    Serial.println(getHexString(keyState.fullSharedSecret, SHARED_SECRET_SIZE));
    
    Serial.print(F("Optimized Shared Secret: "));
    Serial.println(getHexString(keyState.sharedSecret, KEY_SIZE));
    Serial.println(F("----------------------------------\n"));
}

void performKeyExchange() {
    // Enviar clave pública
    sendPublicKey();
    Serial.println(F("Waiting for external public key..."));

    // Recibir clave pública
    receivePublicKey();

    String publicKey2Hex = getHexString(keyState.publicKey2, PUBLIC_KEY_SIZE);
    Serial.print(F("publicKey2: "));
    Serial.println(publicKey2Hex);
    
    int r = uECC_shared_secret(keyState.publicKey2, keyState.privateKey1, keyState.fullSharedSecret, keyState.curve);

    if (r == 1) {
        optimizeKeySelection(keyState.fullSharedSecret, keyState.sharedSecret);
        printAllKeys();
    } else {
        Serial.println(F("Failed to calculate shared secret"));
        return;
    }

    
    Serial.println(F("Waiting for nonce..."));
    // Procesar el intercambio del nonce
    while (!keyState.keysVerified) {
        // Esperar respuesta cifrada del otro micro
        while (!Serial.available()) delay(10);
        String response = Serial.readStringUntil('\n');

        String decrypted = decryptMessage(response);

        // Validar nonce recibido y verificar
        int receivedNonce = decrypted.toInt() - 1;
        if (receivedNonce) {
            // Generar respuesta para cerrar el intercambio
            Serial.print(F("nounce res:"));
            Serial.println(receivedNonce);

            String responseNonce = String(receivedNonce);
            encryptMessage(responseNonce, keyState.sharedSecret);
            
            sendEncryptedNonce(encryptedMessage);

            Serial.println(F("Nonce send. Waiting for verification..."));
            
            while (!Serial.available()) delay(10);
            int responseVerify = Serial.readStringUntil('\n').toInt();

            keyState.keysVerified = responseVerify == 1;
            
        } else {
            Serial.println(F("Key exchange failed, retrying"));
        }
    }
}

void sendAnalytics(unsigned long time, String typeMessure, String unitMessure) {
  // Crear mensaje JSON manualmente
    String jsonMessage = "{";
    jsonMessage += "\"time\": " + String(time) + ", ";
    jsonMessage += "\"microName\": \"Microcontrolador 1\", ";
    jsonMessage += "\"medition\": \"" + typeMessure + "\", ";
    jsonMessage += "\"UnitMessure\": \"" + unitMessure + "\", ";
    jsonMessage += "\"typeMessage\": \"ANALYTICS\"";
    jsonMessage += "}";

    // Enviar JSON
    Serial.println(jsonMessage);
}

// Funcion de prueba para medir generacion de claves
void calculateKeyTimeGeneration() {
    const struct uECC_Curve_t *curve = uECC_secp160r1();
    uint8_t private1[21] = {0};
    uint8_t private2[21] = {0};

    uint8_t public1[40] = {0};
    uint8_t public2[40] = {0};

    uint8_t secret1[20] = {0};
    uint8_t secret2[20] = {0};

    uint8_t secretKey1[8] = {0};
    uint8_t secretKey2[8] = {0};

    //Calculando clave publica y privada 1
    startTime = millis();
    uECC_make_key(public1, private1, curve);
    finalTime = millis() - startTime;
    sendAnalytics(finalTime, "Public Private Key Time", "ms");

    //Calculando clave publica y privada 2
    startTime = millis();
    uECC_make_key(public2, private2, curve);
    finalTime = millis() - startTime;
    sendAnalytics(finalTime, "Public Private Key Time", "ms");

    //Calculando clave compartida 1
    startTime = millis();
    uECC_shared_secret(public2, private1, secret1, curve);
    finalTime = millis() - startTime;
    sendAnalytics(finalTime, "UECC Shared Key Time", "ms");

    //Calculando clave compartida 2
    startTime = millis();
    uECC_shared_secret(public2, private1, secret1, curve);
    finalTime = millis() - startTime;
    sendAnalytics(finalTime, "UECC Shared Key Time", "ms");

    //Calculando clave compartida Optimizada
    startTime = micros();
    optimizeKeySelection(secret1, secretKey1);
    finalTime = micros() - startTime;
    sendAnalytics(finalTime, "Optimized Shared Key Time", "us");

    //Calculando clave compartida Optimizada
    startTime = micros();
    optimizeKeySelection(secret2, secretKey2);
    finalTime = micros() - startTime;
    sendAnalytics(finalTime, "Optimized Shared Key Time", "us");
}

void setup() {
    Serial.begin(57600);
    uECC_set_rng(&RNG);
    randomSeed(analogRead(A0));

    Serial.println(F("\nInitializing key generation..."));

    //Calcular tiempo de generacion de claves
    //while(true) calculateKeyTimeGeneration();

    if (!generateInitialKeys()) {
        Serial.println(F("Failed to generate keys"));
        return;
    }

    performKeyExchange();

    if (keyState.keysVerified) {
      Serial.println(F("Secure Communication: Ready"));
    } else {
      Serial.println(F("Error: Failed to establish secure connection"));
    }

    time_now = millis();
}

void loop() {
  // Enviar mensaje cada cierto tiempo
  if (millis() >= time_now + period) {
    time_now += period;

    String dataValue = String(analogRead(A0));
    String message = "Dato micro1: " + dataValue;

    // Cifrar el mensaje
    startTime = micros();
    encryptMessage(message, keyState.sharedSecret);
    finalTime = micros() - startTime;
    sendAnalytics(finalTime, "Encryption Time", "us");

    // Crear un objeto JSON y enviarlo
    String jsonMessage = "{\"encryptedMessage\": \"" + encryptedMessage +
    "\", \"microName\": \"" + "source" +
    "\", \"typeMessage\": \"" + "ENCRYPTED MESSAGE" +
    "\"}";

    Serial.println(jsonMessage);
  }
}
