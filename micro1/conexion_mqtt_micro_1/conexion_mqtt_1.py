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

while True:
    if serial_port.in_waiting > 0:
        # Leer y decodificar datos desde el puerto serie
        data = serial_port.readline().decode('utf-8').strip()
        
        try:
            # Parsear el mensaje como JSON
            json_data = json.loads(data)
            
            # Extraer el campo "encryptedMessage"
            encrypted_message = json_data.get("encryptedMessage", None)
            
            if encrypted_message:
                # Enviar solo el encryptedMessage al broker MQTT
                mqtt_client.publish("topic/encriptacion", encrypted_message)
                mqtt_client.publish("topic/analisis", data)
                print(f"Enviado a topic/encriptacion: {encrypted_message}")
            else:
                print("Error: 'encryptedMessage' no encontrado en el JSON recibido")
        except json.JSONDecodeError:
            print("Error: El mensaje recibido no es un JSON válido")
