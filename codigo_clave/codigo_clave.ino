#include <uECC.h>

#define uECC_OPTIMIZATION_LEVEL 3 // Nivel básico de optimización
#define uECC_SQUARE_FUNC 1

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
}

// Función de puntuación para evaluar la "calidad" de los bytes
int score(uint8_t key[8]) {
  int score = 0;
  for (int i = 0; i < 8; i++) {
    score += key[i] % 16;  // Añadir un criterio sencillo basado en el valor
  }
  return score;
}

void printHex(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print("0"); // Asegurar que siempre tenga dos dígitos
    }
    Serial.print(data[i], HEX);
    if (i < length - 1) {
      Serial.print(" ");
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.print("Testing ECC\n");
  uECC_set_rng(&RNG);
}

void loop() {
  const struct uECC_Curve_t *curve = uECC_secp160r1();
  uint8_t private1[21] = {0};
  uint8_t private2[21] = {0};

  uint8_t public1[40] = {0};
  uint8_t public2[40] = {0};

  uint8_t secret1[20] = {0};
  uint8_t secret2[20] = {0};

  uint8_t secretKey1[8] = {0};
  uint8_t secretKey2[8] = {0};

  int isKeysOK = 0;


  // Generar las claves para el primer par (private1 y public1)
  unsigned long a = millis();
  while (true) {
    isKeysOK = uECC_make_key(public1, private1, curve);

    if (isKeysOK == 1) {
      break;
    }

    Serial.println("Regenerando claves para el segundo par...");
  }
  unsigned long b = millis();
  Serial.print("Clave publica 1: ");
  printHex(public1, sizeof(public1));
  Serial.print("Clave privada 1: ");
  printHex(private1, sizeof(private1));
  Serial.println("----------------------------------------------------------------------------------");


  isKeysOK = 0;
  // Generar las claves para el segundo par (private2 y public2)
  a = millis();
  while (true) {
    int isKeysOK = uECC_make_key(public2, private2, curve);

    if (isKeysOK == 1) {
      break;
    }

    Serial.println("Regenerando claves para el segundo par...");
  }
  b = millis();

    Serial.print("Clave publica 2: ");
  printHex(public2, sizeof(public2));
  Serial.print("Clave privada 2: ");
  printHex(private2, sizeof(private2));
  Serial.println("----------------------------------------------------------------------------------");

  while (true) {
  // Generar secretos compartidos
  a = millis();
  int r = uECC_shared_secret(public2, private1, secret1, curve);
  b = millis();
  Serial.print("Secreto compartido 1 calculado en ");
  Serial.println(b - a);
  if (!r) {
    Serial.println("Error al calcular el secreto compartido (1)");
    return;
  }
  isKeysOK = 0;
    
  a = millis();
  r = uECC_shared_secret(public1, private2, secret2, curve);
  b = millis();
  Serial.print("Secreto compartido 2 calculado en ");
  Serial.println(b - a);
  if (!r) {
    Serial.println("Error al calcular el secreto compartido (2)");
    return;
  }

  // Comparar secretos compartidos
  if (memcmp(secret1, secret2, 20) != 0) {
    Serial.println("¡Los secretos compartidos no son identicos!");
  } else {
    isKeysOK = 1;
    Serial.println("Los secretos compartidos son identicos");
  }

  // Imprimir los secretos compartidos
  Serial.print("Secreto compartido 1: ");
  printHex(secret1, sizeof(secret1));
  Serial.print("Secreto compartido 2: ");
  printHex(secret2, sizeof(secret2));

  
  Serial.println("----------------------------------------------------------------------------------");
  Serial.print("Optimizando clave: ");
  optimizeKeySelection(secret1, secretKey1);
  optimizeKeySelection(secret2, secretKey2);

    // Comparar secretos compartidos
  if (memcmp(secretKey1, secretKey2, 8) != 0) {
    Serial.println("¡Los secretos optimizados no son identicos!");
  } else {
    isKeysOK = 1;
    Serial.println("Los secretos optimizados son identicos");
  }

  // Imprimir los secretos optimizados
  Serial.print("Secreto optimizado 1: ");
  printHex(secretKey1, sizeof(secretKey1));
  Serial.print("Secreto optimizado 2: ");
  printHex(secretKey2, sizeof(secretKey2));

  Serial.println("----------------------------------------------------------------------------------");
  

  if (isKeysOK == 1) {
    break;
  }
  }
  delay(300); // Evitar inundar el puerto serie
  
}