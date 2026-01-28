const net = require('net');

// Holding Register 2002 (Address 0x07D2) - Point 7 (Active_Alarms)
// We set it to 1 to trigger "Test Alarm" (High Limit > 0)
// Then set it to 0 to clear.

const client = new net.Socket();
const HOST = 'localhost';
const PORT = 50502;

client.connect(PORT, HOST, function () {
    console.log('Connected to Modbus Simulator');

    // 1. Write Register 2002 -> 0 (Ensure Clear)
    console.log('Writing Register 2002 -> 0');
    // TransId: 00 01, Proto: 00 00, Len: 00 06, Unit: 01, Func: 06, Addr: 07 D2, Val: 00 00
    const bufOff = Buffer.from([
        0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x01, 0x06, 0x07, 0xD2, 0x00, 0x00
    ]);
    client.write(bufOff);

    // 2. Wait 2 seconds and Write Register 2002 -> 1 (Trigger Alarm)
    setTimeout(() => {
        console.log('Writing Register 2002 -> 1');
        // TransId: 00 02 ... Val: 00 01
        const bufOn = Buffer.from([
            0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x01, 0x06, 0x07, 0xD2, 0x00, 0x01
        ]);
        client.write(bufOn);
    }, 2000);
});

client.on('data', function (data) {
    console.log('Received:', data.toString('hex'));
});

client.on('close', function () {
    console.log('Connection closed');
});

client.on('error', function (err) {
    console.error('Error:', err.message);
});

// Close after 5 seconds
setTimeout(() => {
    client.destroy();
}, 5000);
