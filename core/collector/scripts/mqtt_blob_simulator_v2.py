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

connected = False

def on_connect(client, userdata, flags, rc):
    global connected
    if rc == 0:
        print(f"Connected successfully (rc={rc})")
        connected = True
    else:
        print(f"Connection failed with code {rc}")

def on_publish(client, userdata, mid):
    print(f"Message published (mid={mid})")

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.on_connect = on_connect
client.on_publish = on_publish

try:
    print(f"Connecting to {MQTT_HOST}:{MQTT_PORT}...")
    client.connect(MQTT_HOST, MQTT_PORT, 60)
except Exception as e:
    print(f"Connection failed: {e}")
    exit(1)

client.loop_start()

# Wait for connection
timeout = 5
start_wait = time.time()
while not connected and (time.time() - start_wait) < timeout:
    time.sleep(0.1)

if not connected:
    print("Failed to connect within timeout")
    exit(1)

# Single message to trigger alarm and provide file_ref
payload = {
    "temp": 150.0, # > 100 (High Alarm Limit)
    "file_ref": "file:///app/data/blobs/20260203_E2E_TEST.bin"
}

print(f"Sending alarm trigger to {TOPIC}...")
print(json.dumps(payload, indent=2))

msg = json.dumps(payload)
pub_info = client.publish(TOPIC, msg, qos=1)
pub_info.wait_for_publish()

print("Signal sent and confirmed. Waiting 5 seconds before disconnect...")
time.sleep(5)
client.loop_stop()
client.disconnect()
print("Done.")
