import paho.mqtt.client as mqtt
import json
import time

# MQTT Broker (RabbitMQ)
MQTT_HOST = "localhost"
MQTT_PORT = 1883
MQTT_USER = "guest"
MQTT_PASS = "guest"

# Device/Point Information (From Database)
TOPIC = "sensors/simulator/data"

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.on_connect = on_connect

try:
    client.connect(MQTT_HOST, MQTT_PORT, 60)
except Exception as e:
    print(f"Connection failed: {e}")
    exit(1)

client.loop_start()

# Single message to trigger alarm and provide file_ref
payload = {
    "temp": 150.0, # > 100 (High Alarm Limit)
    "file_ref": "file:///app/data/blobs/20260203_E2E_TEST.bin"
}

print(f"Sending alarm trigger to {TOPIC}...")
print(json.dumps(payload, indent=2))

msg = json.dumps(payload)
client.publish(TOPIC, msg)

print("Signal sent. Waiting for Collector and Export Gateway to process...")
time.sleep(10)
client.loop_stop()
client.disconnect()
