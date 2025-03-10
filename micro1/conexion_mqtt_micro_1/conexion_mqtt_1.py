import serial
import json
import paho.mqtt.client as mqtt
import threading

# Configuración del puerto serial
<<<<<<< HEAD
serial_port = serial.Serial('COM2', baudrate=9600, timeout=1)
=======
SERIAL_PORT = 'COM6'  # Cambia al puerto donde está conectado tu Arduino
BAUDRATE = 57600
serial_port = serial.Serial(SERIAL_PORT, baudrate=BAUDRATE, timeout=1)
>>>>>>> addKeyGenerator

# Configuración del broker MQTT
BROKER_ADDRESS = "localhost"
BROKER_PORT = 1883
TOPIC_ENCRIPTACION = "topic/encriptacion"
TOPIC_KEY = "topic/micro2/key"
TOPIC_NONCE = "topic/micro2/nonce"
TOPIC_VERIFY = "topic/micro2/verify"
TOPIC_SEND = "topic/analisis"

mqtt_client = mqtt.Client()

# Callback cuando se recibe un mensaje en el tópico TOPIC_ENCRIPTACION
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode('utf-8'))

        if msg.topic == TOPIC_KEY:
            if serial_port.is_open:
                publicKey = data.get("publicKey", None)
                message = publicKey + "\n";
                serial_port.write(message.encode('utf-8'))
                serial_port.flush()

        elif msg.topic == TOPIC_NONCE:
            if serial_port.is_open:
                nonce = data.get("encryptedNonce", None)
                message = nonce + "\n";
                serial_port.write(message.encode('utf-8'))
                serial_port.flush()
        elif msg.topic == TOPIC_VERIFY:
            if serial_port.is_open:
                verify = data.get("verify", None)
                message = verify + "\n";
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
        (TOPIC_KEY, 0),
        (TOPIC_NONCE, 0),       # Suscripción con QoS 0
        (TOPIC_VERIFY, 0)
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
                    if arduino_data == "" or arduino_data == "0":
                        continue
                    try:
                        # Parsear el mensaje como JSON
                        json_data = json.loads(arduino_data)
                        print("---------------------------------------")
                        print("json")
                        print(json_data)
                        print("---------------------------------------")                        
                        typeMessage = json_data.get("typeMessage", None)
                        if typeMessage == "ENCRYPTED MESSAGE":
                            mqtt_client.publish("topic/encriptacion", arduino_data)
                        elif typeMessage == "ANALYTICS":
                            mqtt_client.publish("topic/analisis", arduino_data)
                        elif typeMessage == "KEYS":
                            mqtt_client.publish("topic/micro1/key", arduino_data)
                        elif typeMessage == "NONCE":
                            mqtt_client.publish("topic/micro1/nonce", arduino_data)
                        
                    except json.JSONDecodeError:
                        print(arduino_data)
                        continue
                except UnicodeDecodeError:
                    print("Error al decodificar el mensaje")
                    continue
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
