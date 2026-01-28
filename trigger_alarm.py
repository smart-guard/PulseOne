
from pymodbus.client import ModbusTcpClient
import sys

client = ModbusTcpClient('127.0.0.1', port=5020)
client.connect()
result = client.write_register(20, 1)
client.close()

if result.isError():
    print(f"Error: {result}")
    sys.exit(1)
else:
    print("Successfully wrote 1 to address 20")
