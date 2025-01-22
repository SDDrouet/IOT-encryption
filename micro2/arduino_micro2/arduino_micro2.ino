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

// Estructura para mantener el estado de las claves
struct KeyState {
    const uint8_t key8[KEY_SIZE];
    uint8_t privateKey2[PRIVATE_KEY_SIZE];
    uint8_t publicKey1[PUBLIC_KEY_SIZE];
    uint8_t publicKey2[PUBLIC_KEY_SIZE];
    uint8_t sharedSecret[KEY_SIZE];
    uint16_t nonce;
    bool keysVerified;
    
    KeyState() : key8{0x12, 0x34, 0x56, 0x79, 0x90, 0xAB, 0xCD, 0xEF}, 
                nonce(0), keysVerified(false) {
        memset(privateKey2, 0, PRIVATE_KEY_SIZE);
        memset(publicKey1, 0, PUBLIC_KEY_SIZE);
        memset(publicKey2, 0, PUBLIC_KEY_SIZE);
        memset(sharedSecret, 0, KEY_SIZE);
    }
};

static KeyState keyState;
static uint8_t cipher[8];
static String encryptedMessage = "";

// Funciones existentes sin cambios...

// Función mejorada de RNG usando precompilación
static int RNG(uint8_t *dest, unsigned size) {
    while (size--) {
        uint8_t val = 0;
        for (uint8_t i = 0; i < 8; ++i) {
            int init = analogRead(A0);
            uint8_t count = random(0, 5);
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
    result.reserve(length * 3);  // Pre-reservar espacio (2 chars por byte + espacio)
    
    char hexBuffer[3] = {0};
    for (size_t i = 0; i < length; i++) {
        byteToHexStr(data[i], hexBuffer);
        result += hexBuffer;
        if (i < length - 1) result += ' ';
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

// Función para calcular la clave compartida
bool calculateSharedSecret(uint8_t *tempSecret) {
    const struct uECC_Curve_t *curve = uECC_secp160r1();
    return uECC_shared_secret(keyState.publicKey1, keyState.privateKey2, tempSecret, curve) == 1;
}

// Función optimizada para seleccionar la mejor clave
void optimizeKeySelection(const uint8_t *fullSecret, uint8_t *optimizedKey) {
    uint32_t bestScore = 0;
    uint8_t bestIndex = 0;
    
    // Evaluar cada posible posición de inicio
    for (uint8_t i = 0; i <= SHARED_SECRET_SIZE - KEY_SIZE; i++) {
        uint32_t score = 0;
        for (uint8_t j = 0; j < KEY_SIZE; j++) {
            score += fullSecret[i + j];
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    
    // Copiar los mejores 8 bytes
    memcpy(optimizedKey, &fullSecret[bestIndex], KEY_SIZE);
}

// Función optimizada para convertir string hex a bytes
void hexStringToBytes(const String &hexString, uint8_t *output, size_t outputSize) {
    Serial.println("keystr: " + hexString);
    size_t hexLength = hexString.length();
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
void printAllKeys(const uint8_t *tempSecret) {
    Serial.println(F("\n-------- DEBUG: KEYS STATUS --------"));
    Serial.print(F("Private Key 2: "));
    Serial.println(getHexString(keyState.privateKey2, PRIVATE_KEY_SIZE));
    
    Serial.print(F("Public Key 2: "));
    Serial.println(getHexString(keyState.publicKey2, PUBLIC_KEY_SIZE));
    
    Serial.print(F("Public Key 1 (received): "));
    Serial.println(getHexString(keyState.publicKey1, PUBLIC_KEY_SIZE));
    
    if (tempSecret != nullptr) {
        Serial.print(F("Full Shared Secret: "));
        Serial.println(getHexString(tempSecret, SHARED_SECRET_SIZE));
    }
    
    Serial.print(F("Optimized Shared Secret: "));
    Serial.println(getHexString(keyState.sharedSecret, KEY_SIZE));
    Serial.println(F("----------------------------------\n"));
}


// Función optimizada para descifrar mensajes
String decryptMessage(const String &encryptedMessage) {
    String result;
    size_t messageLength = encryptedMessage.length();
    
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
        
        klein64_decrypt(block, keyState.key8, decrypted);
        
        for (uint8_t k = 0; k < 8; k++) {
            if (decrypted[k]) result += (char)decrypted[k];
        }
    }
    
    return result;
}

// Nueva función para encriptar mensajes usando Klein
void encryptMessage(const String &message, const uint8_t *key) {
    int messageLength = message.length();
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
        for (int k = 0; k < 8; k++) {
            if (cipher[k] < 0x10) {
                encryptedMessage += "0";
            }
            encryptedMessage += String(cipher[k], HEX);
            if (k < 7) encryptedMessage += " ";
        }
    }
}

// Nueva función para enviar el nonce encriptado
void sendEncryptedNonce() {
    // Generar nonce aleatorio
    keyState.nonce = random(1000, 9999);
    
    // Convertir nonce a string
    String nonceStr = String(keyState.nonce);

    // Encriptar el nonce usando el secreto compartido
    encryptMessage(nonceStr, keyState.sharedSecret);

    // Obtener la clave pública en formato hexadecimal
    String publicKeyLocal = getHexString(keyState.publicKey2, PUBLIC_KEY_SIZE);

    // Crear una copia de la clave pública
    String publicKeyLocalCopy = publicKeyLocal;

    Serial.print(F("{\"publicKey\": \"" ));
    Serial.print(publicKeyLocalCopy);
    Serial.print(F("\", \"encryptedNonce\": \""));
    Serial.print(encryptedMessage);
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
        unsigned long timeout = millis() + 5000;  // 5 segundos de timeout
        Serial.println(F("waiting response..."));
        while (millis() < timeout && !Serial.available()) {
            delay(10);
        }
        
        if (Serial.available()) {
            String response = Serial.readStringUntil('\n');
            verified = verifyNonceResponse(response);
            
            if (verified) {
                Serial.println(F("Key verification successful!"));
                keyState.keysVerified = true;
                return true;
            } else {
                Serial.println(F("Verification failed, regenerating shared secret..."));
                // Recalcular secreto compartido
                uint8_t tempSecret[SHARED_SECRET_SIZE];
                if (calculateSharedSecret(tempSecret)) {
                    optimizeKeySelection(tempSecret, keyState.sharedSecret);
                }
            }
        }
    }
    
    Serial.println(F("Key verification failed after maximum attempts"));
    return false;
}

void setup() {
    Serial.begin(115200);
    uECC_set_rng(&RNG);
    randomSeed(analogRead(A0));
    
    Serial.println(F("\nInitializing key generation..."));
    
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
    
    // Calcular secreto compartido y verificar
    uint8_t tempSecret[SHARED_SECRET_SIZE];
    if (calculateSharedSecret(tempSecret)) {
        optimizeKeySelection(tempSecret, keyState.sharedSecret);
        printAllKeys(tempSecret);
        
        // Realizar verificación de claves
        if (!performKeyVerification()) {
            Serial.println(F("Failed to establish secure connection"));
            return;
        }
    } else {
        Serial.println(F("Error: Failed to calculate shared secret"));
        return;
    }
}

void loop() {
    if (Serial.available() && keyState.keysVerified) {
        String encryptedMessage = Serial.readStringUntil('\n');
        String decrypted = decryptMessage(encryptedMessage);
        
        if (decrypted.length() > 0) {
            Serial.print(F("{\"decryptedMessage\": \""));
            Serial.print(decrypted);
            Serial.println(F("\", \"microName\": \"destination\"}"));
        }
    }
}