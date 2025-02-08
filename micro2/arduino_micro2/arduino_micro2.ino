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
#define uECC_OPTIMIZATION_LEVEL 3 // Nivel básico de optimización
#define uECC_SQUARE_FUNC 1

// Estructura para mantener el estado de las claves
struct KeyState {
    uint8_t privateKey2[PRIVATE_KEY_SIZE];
    uint8_t publicKey1[PUBLIC_KEY_SIZE];
    uint8_t publicKey2[PUBLIC_KEY_SIZE];
    uint8_t fullSharedSecret[SHARED_SECRET_SIZE];
    uint8_t sharedSecret[KEY_SIZE];
    const struct uECC_Curve_t *curve = uECC_secp160r1();
    uint16_t nonce;
    bool keysVerified;
    
    KeyState() : nonce(0), keysVerified(false) {
        memset(privateKey2, 0, PRIVATE_KEY_SIZE);
        memset(publicKey1, 0, PUBLIC_KEY_SIZE);
        memset(publicKey2, 0, PUBLIC_KEY_SIZE);
        memset(fullSharedSecret, 0, SHARED_SECRET_SIZE);
        memset(sharedSecret, 0, KEY_SIZE);
    }
};

static KeyState keyState;
static uint8_t cipher[8];
static String encryptedMessage = "";

unsigned long startTime;
unsigned long finalTime;

// Funciones existentes sin cambios...

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

// Función helper para convertir bytes a hexadecimal
static void byteToHexStr(const uint8_t byte, char* output) {
    static const char hex[] = "0123456789ABCDEF";
    output[0] = hex[byte >> 4];
    output[1] = hex[byte & 0x0F];
}

// Función optimizada para convertir array de bytes a string hexadecimal
String getHexString(const uint8_t *data, const size_t length) {
    String result;
    result.reserve(length * 2);
    char hexBuffer[3] = {0};
    for (size_t i = 0; i < length; i++) {
        sprintf(hexBuffer, "%02X", data[i]);
        result += hexBuffer;
    }
    return result;
}

// Función para generar claves iniciales
bool generateInitialKeys() {
    const struct uECC_Curve_t *curve = uECC_secp160r1();
    uint8_t attempts = 0;
    const uint8_t MAX_ATTEMPTS = 5;
    
    while (attempts++ < MAX_ATTEMPTS) {
        if (uECC_make_key(keyState.publicKey2, keyState.privateKey2, curve)) {
            return true;
        }
        delay(10);  // Pequeña pausa entre intentos
    }
    return false;
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


// Función optimizada para convertir string hex a bytes
void hexStringToBytes(const String &hexString, uint8_t *output, size_t outputSize) {
    Serial.println("keystr: " + hexString);
    size_t hexLength = getMessageLength(hexString);
    size_t bytesToProcess = outputSize;
    
    for (size_t i = 0; i < bytesToProcess; i++) {
        char highNibble = hexString[i * 2];
        char lowNibble = hexString[i * 2 + 1];
        
        highNibble = (highNibble <= '9') ? highNibble - '0' : (highNibble & 0x7) + 9;
        lowNibble = (lowNibble <= '9') ? lowNibble - '0' : (lowNibble & 0x7) + 9;
        
        output[i] = (highNibble << 4) | lowNibble;
    }
}

// Función para imprimir todas las claves (debug)
void printAllKeys() {
    Serial.println(F("\n-------- DEBUG: KEYS STATUS --------"));
    Serial.print(F("Private Key 2: "));
    Serial.println(getHexString(keyState.privateKey2, PRIVATE_KEY_SIZE));
    
    Serial.print(F("Public Key 2: "));
    Serial.println(getHexString(keyState.publicKey2, PUBLIC_KEY_SIZE));
    
    Serial.print(F("Public Key 1 (received): "));
    Serial.println(getHexString(keyState.publicKey1, PUBLIC_KEY_SIZE));
    
    Serial.print(F("Full Shared Secret: "));
    Serial.println(getHexString(keyState.fullSharedSecret, SHARED_SECRET_SIZE));
    
    Serial.print(F("Optimized Shared Secret: "));
    Serial.println(getHexString(keyState.sharedSecret, KEY_SIZE));
    Serial.println(F("----------------------------------\n"));
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
        // Convertir 16 caracteres hex a 8 bytes
        for (size_t j = 0; j < 8; j++) {
            char highNibble = encryptedMessage[i + j * 2];
            char lowNibble = encryptedMessage[i + j * 2 + 1];
            
            highNibble = (highNibble <= '9') ? highNibble - '0' : (highNibble & 0x7) + 9;
            lowNibble = (lowNibble <= '9') ? lowNibble - '0' : (lowNibble & 0x7) + 9;
            
            block[j] = (highNibble << 4) | lowNibble;
        }
        
        klein64_decrypt(block, keyState.sharedSecret, decrypted);
        
        for (uint8_t k = 0; k < 8; k++) {
            if (decrypted[k]) result += (char)decrypted[k];
        }
    }
    
    return result;
}

// Nueva función para encriptar mensajes usando Klein
void encryptMessage(const String &message, const uint8_t *key) {
    int messageLength = getMessageLength(message);
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

// Nueva función para enviar el nonce encriptado
void sendPublicKey() {
    // Obtener la clave pública en formato hexadecimal
    String publicKeyLocal = getHexString(keyState.publicKey2, PUBLIC_KEY_SIZE);

    Serial.print(F("{\"publicKey\": \"" ));
    Serial.print(publicKeyLocal);
    Serial.println(F("\", \"microName\": \"micro2\", \"typeMessage\": \"KEY\"}"));
}

// Nueva función para enviar el nonce encriptado
void sendEncryptedNonce() {
    // Generar nonce aleatorio
    keyState.nonce = random(1000, 9999);
    
    // Convertir nonce a string
    String nonceStr = String(keyState.nonce);

    Serial.print(F("nonce1: "));
    Serial.println(nonceStr);

    // Encriptar el nonce usando el secreto compartido
    encryptMessage(nonceStr, keyState.sharedSecret);

    Serial.print(F("{\"encryptedNonce\": \"" ));
    Serial.print(encryptedMessage);
    Serial.println(F("\", \"microName\": \"micro2\", \"typeMessage\": \"NONCE\"}"));
}

// Nueva función para enviar el nonce encriptado
void sendVerification(bool verified) {
    Serial.print(F("{\"verify\": \"" ));
    if (verified) {
      Serial.print(F("1"));
    } else {
      Serial.print(F("0"));
    }
    Serial.println(F("\", \"microName\": \"micro2\", \"typeMessage\": \"VERIFY\"}"));
}


// Nueva función para verificar la respuesta del nonce
bool verifyNonceResponse(const String &response) {
    // Desencriptar la respuesta usando el secreto compartido
    String decrypted = decryptMessage(response);
    
    // Verificar si el nonce recibido es igual al nonce original - 1
    int receivedNonce = decrypted.toInt();
    return (receivedNonce == (keyState.nonce - 1));
}

bool performKeyVerification() {
    uint8_t attempts = 0;
    bool verified = false;
    while (attempts++ < MAX_RETRIES && !verified) {
        // Enviar nonce encriptado
        sendEncryptedNonce();
        
        // Esperar respuesta
        unsigned long timeout = millis() + 500000;  // 5 segundos de timeout
        Serial.println(F("waiting response..."));
        while (millis() < timeout && !Serial.available()) {
            delay(10);
        }
        
        if (Serial.available()) {
            String response = Serial.readStringUntil('\n');
            verified = verifyNonceResponse(response);
            sendVerification(verified);
            if (verified) {
                Serial.println(F("Key verification successful!"));
                keyState.keysVerified = true;
                return true;
            } else {
                Serial.println(F("Verification failed, regenerating shared secret..."));
                // Recalcular secreto compartido
                memset(keyState.fullSharedSecret, 0, SHARED_SECRET_SIZE);
                memset(keyState.sharedSecret, 0, KEY_SIZE);
                int r = 0;
                while (r == 0) {
                    Serial.println(F("Regenerando..."));
                    memset(keyState.fullSharedSecret, 0, SHARED_SECRET_SIZE);
                    memset(keyState.sharedSecret, 0, KEY_SIZE);

                    r = uECC_shared_secret(keyState.publicKey1, keyState.privateKey2, keyState.fullSharedSecret, keyState.curve);
                    optimizeKeySelection(keyState.fullSharedSecret, keyState.sharedSecret);

                    printAllKeys();
                }
            }
        }
    }
    
    Serial.println(F("Key verification failed after maximum attempts"));
    return false;
}

void sendAnalytics(unsigned long time, String typeMessure, String unitMessure) {
  // Imprimir el mensaje JSON usando F() para optimizar memoria
  Serial.print(F("{"));
  Serial.print(F("\"time\": "));
  Serial.print(time);
  Serial.print(F(", "));
  Serial.print(F("\"microName\": \"Microcontrolador 2\", "));
  Serial.print(F("\"medition\": \""));
  Serial.print(typeMessure);
  Serial.print(F("\", "));
  Serial.print(F("\"UnitMessure\": \""));
  Serial.print(unitMessure);
  Serial.print(F("\", "));
  Serial.print(F("\"typeMessage\": \"ANALYTICS\""));
  Serial.println(F("}"));
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
    
    // Generar claves iniciales
    if (!generateInitialKeys()) {
        Serial.println(F("Error: Failed to generate keys"));
        return;
    }
    
    // Esperar y procesar la clave pública externa
    Serial.println(F("Waiting for external public key..."));
    while (!Serial.available()) delay(10);
    String publicKeyStr = Serial.readStringUntil('\n');
    hexStringToBytes(publicKeyStr, keyState.publicKey1, PUBLIC_KEY_SIZE);
    
    int r = uECC_shared_secret(keyState.publicKey1, keyState.privateKey2, keyState.fullSharedSecret, keyState.curve);
    if (r == 1) {
        optimizeKeySelection(keyState.fullSharedSecret, keyState.sharedSecret);
        printAllKeys();
        sendPublicKey();
        // Realizar verificación de claves
        if (!performKeyVerification()) {
            Serial.println(F("Error: Failed to establish secure connection"));
            return;
        }
        Serial.println(F("Secure Communication: Ready"));
    } else {
        Serial.println(F("Error: Failed to calculate shared secret"));
        return;
    }
}

void loop() {
    if (Serial.available() && keyState.keysVerified) {
        String encryptedMessage = Serial.readStringUntil('\n');

        startTime = micros();
        String decrypted = decryptMessage(encryptedMessage);
        finalTime = micros() - startTime;
        sendAnalytics(finalTime, "Decryption Time", "us");
        
        if (decrypted != "") {
            Serial.print(F("Mensaje: "));
            Serial.println(decrypted);
        }
    }
}