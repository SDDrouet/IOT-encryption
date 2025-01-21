import serial
import json
import paho.mqtt.client as mqtt

# Configuración del puerto serial
serial_port = serial.Serial('COM2', baudrate=9600, timeout=1)

# Configuración del broker MQTT
mqtt_client = mqtt.Client(callback_api_version=2)
mqtt_client.connect("localhost", 1883)
print("Conexión establecida con el broker MQTT")
print("Esperando mensajes...")

def send_encrypted_message(json_data):
    # Extraer el campo "encryptedMessage"
    encrypted_message = json_data.get("encryptedMessage", None)
    
    if encrypted_message:
        # Enviar solo el encryptedMessage al broker MQTT
        mqtt_client.publish("topic/encriptacion", encrypted_message)
    else:
        print("Error: 'encryptedMessage' no encontrado en el JSON recibido")

def send_key_message(json_data):
    key_message = json_data.get("publicKey", None)
    print(key_message)
    if key_message:
        mqtt_client.publish("topic/micro1/key", key_message)
    else:
        print("Error: 'publicKey' no encontrado en el JSON recibido")

num = 0

while True:
    if serial_port.in_waiting > 0:
        # Leer y decodificar datos desde el puerto serie
        data = serial_port.readline().decode('utf-8').strip()
        try:
            # Parsear el mensaje como JSON
            json_data = json.loads(data)
            typeMessage = json_data.get("typeMessage", None)
            if typeMessage == "ENCRYPTED MESSAGE":
                send_encrypted_message(json_data)
            elif typeMessage == "ANALYTICS":
                mqtt_client.publish("topic/analisis", data)
            elif typeMessage == "KEYS":
                print("num de msg: ", num)
                num += 1
                send_key_message(json_data)

            
        except json.JSONDecodeError:
            print("Error: El mensaje recibido no es un JSON válido")
