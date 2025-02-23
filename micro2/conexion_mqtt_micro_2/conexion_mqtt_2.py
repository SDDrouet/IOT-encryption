import serial
import json
import paho.mqtt.client as mqtt
import threading

# Configuración del puerto serial
SERIAL_PORT = 'COM5'  # Cambia al puerto donde está conectado tu Arduino
BAUDRATE = 57600
serial_port = serial.Serial(SERIAL_PORT, baudrate=BAUDRATE, timeout=1)

# Configuración del broker MQTT
BROKER_ADDRESS = "localhost"
BROKER_PORT = 1883
TOPIC_ENCRIPTACION = "topic/encriptacion"
TOPIC_KEY = "topic/micro1/key"
TOPIC_NONCE = "topic/micro1/nonce"
TOPIC_SEND = "topic/analisis"

mqtt_client = mqtt.Client()

# Callback cuando se recibe un mensaje en el tópico TOPIC_ENCRIPTACION
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode('utf-8'))

        if msg.topic == TOPIC_ENCRIPTACION:
            if serial_port.is_open:
                message = data.get("encryptedMessage", None)
                serial_port.write((message + '\n').encode('utf-8'))
        elif msg.topic == TOPIC_NONCE:
            if serial_port.is_open:
                print(f"Enviando clave al puerto {SERIAL_PORT}:\n{data}")
                message = data.get("encryptedNonce", None) + '\n';
                serial_port.write(message.encode('utf-8'))
                serial_port.flush()
        elif msg.topic == TOPIC_KEY:
            if serial_port.is_open:
                print(f"Enviando clave al puerto {SERIAL_PORT}:\n{data}")
                message = data.get("publicKey", None) + '\n';
                serial_port.write(message.encode('utf-8'))
                serial_port.flush()

    except json.JSONDecodeError:
        print("Error al decodificar el mensaje")
        return

# Configuración inicial del cliente MQTT
def setup_mqtt():
    mqtt_client.on_message = on_message
    mqtt_client.connect(BROKER_ADDRESS, BROKER_PORT)
    mqtt_client.subscribe([
        (TOPIC_ENCRIPTACION, 0),  # Suscripción con QoS 0
        (TOPIC_KEY, 0),       # Suscripción con QoS 0
        (TOPIC_NONCE, 0)       # Suscripción con QoS 0
    ])

    print("Conexión establecida con el broker MQTT")
    print("Esperando mensajes...")

# Hilo para manejar el loop de MQTT
def mqtt_loop():
    mqtt_client.loop_forever()

# Loop principal para procesar datos del puerto serial
def serial_loop():
    try:
        while True:
            if serial_port.in_waiting > 0:
                try:
                    arduino_data = serial_port.readline().decode('utf-8').strip()
                    
                    try:
                        # Parsear el mensaje como JSON
                        json_data = json.loads(arduino_data)
                        typeMessage = json_data.get("typeMessage", None)
                        print("----------------------")
                        print("Enviando mensaje a mqtt: ")
                        print(arduino_data)
                        print("----------------------")
                        if typeMessage == "ANALYTICS":
                            mqtt_client.publish("topic/analisis", arduino_data)
                        elif typeMessage == "KEY":
                            mqtt_client.publish("topic/micro2/key", arduino_data)
                        elif typeMessage == "NONCE":
                            mqtt_client.publish("topic/micro2/nonce", arduino_data)
                        elif typeMessage == "VERIFY":
                            mqtt_client.publish("topic/micro2/verify", arduino_data)                         
                    except json.JSONDecodeError:
                        print(arduino_data)
                        continue
                except UnicodeDecodeError:
                    print("Error al decodificar el mensaje")
                    continue                
                mqtt_client.publish(TOPIC_SEND, arduino_data)
    except KeyboardInterrupt:
        print("Cerrando programa...")
        serial_port.close()
        mqtt_client.disconnect()

# Configurar MQTT y lanzar el hilo de loop
setup_mqtt()
mqtt_thread = threading.Thread(target=mqtt_loop)
mqtt_thread.daemon = True
mqtt_thread.start()

# Ejecutar el loop de comunicación serial
serial_loop()
