import serial
import paho.mqtt.client as mqtt
import threading

# Configuración del puerto serial
SERIAL_PORT = 'COM4'  # Cambia al puerto donde está conectado tu Arduino
BAUDRATE = 9600
serial_port = serial.Serial(SERIAL_PORT, baudrate=BAUDRATE, timeout=1)

# Configuración del broker MQTT
BROKER_ADDRESS = "localhost"
BROKER_PORT = 1883
TOPIC_RECEIVE = "topic/encriptacion"
TOPIC_SEND = "topic/analisis"

mqtt_client = mqtt.Client()

# Callback cuando se recibe un mensaje en el tópico TOPIC_RECEIVE
def on_message(client, userdata, msg):
    data = msg.payload.decode('utf-8')
    print("***************************************");
    # Enviar el mensaje al puerto serial
    if serial_port.is_open:
        serial_port.write((data + '\n').encode('utf-8'))
        print(f"Enviado al puerto {SERIAL_PORT}:\n{data}")

# Configuración inicial del cliente MQTT
def setup_mqtt():
    mqtt_client.on_message = on_message
    mqtt_client.connect(BROKER_ADDRESS, BROKER_PORT)
    mqtt_client.subscribe(TOPIC_RECEIVE)
    print("Conexión establecida con el broker MQTT")
    print("Esperando mensajes en el tópico:", TOPIC_RECEIVE)

# Hilo para manejar el loop de MQTT
def mqtt_loop():
    mqtt_client.loop_forever()

# Loop principal para procesar datos del puerto serial
def serial_loop():
    try:
        while True:
            if serial_port.in_waiting > 0:
                arduino_data = serial_port.readline().decode('utf-8').strip()
                print(f"Recibido del puerto {SERIAL_PORT}:\n{arduino_data}")
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
