const net = require('net');

const HOST = 'localhost';
const PORT = 50502;
const UNIT_ID = 1;

// Function 06: Write Single Holding Register
// Address 100 on Modbus is often 0-indexed or 1-indexed. Collector usually uses 0-indexed.
// Target Point 101 -> Address 100
const ADDR = 100;

const client = new net.Socket();

client.connect(PORT, HOST, function () {
    console.log('Connected to Modbus Simulator');

    // Trans ID: 00 01 | Proto: 00 00 | Len: 00 06 | Unit: 01 | Fn: 06 | Addr: 00 64 (100) | Value: 00 7B (123)
    const bufOff = Buffer.from([
        0x00, 0x01, 0x00, 0x00, 0x00, 0x06, UNIT_ID, 0x06, 0x00, 0x64, 0x03, 0x09
    ]);

    console.log(`Writing Address ${ADDR} -> Value 123`);
    client.write(bufOff);
});

client.on('data', function (data) {
    console.log('Received:', data.toString('hex'));
    client.destroy();
});

client.on('error', function (err) {
    console.error('Error:', err.message);
});

setTimeout(() => {
    client.destroy();
}, 2000);
