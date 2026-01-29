
const net = require('net');

// Configuration matches the backend/simulator setup
const HOST = 'docker-simulator-modbus-1';
const PORT = 50502; // Standard Modbus TCP port for simulator

// Helper to create Modbus TCP Write Single Register frame
function createWriteFrame(transactionId, unitId, registerAddress, value) {
    const buffer = Buffer.alloc(12);

    // Header
    buffer.writeUInt16BE(transactionId, 0); // Transaction ID
    buffer.writeUInt16BE(0, 2);             // Protocol ID (0 for Modbus TCP)
    buffer.writeUInt16BE(6, 4);             // Length (UnitID + FuncCode + Addr + Data)

    // PDU
    buffer.writeUInt8(unitId, 6);           // Unit ID
    buffer.writeUInt8(6, 7);                // Function Code (6 = Write Single Register)
    buffer.writeUInt16BE(registerAddress, 8); // Register Address
    buffer.writeUInt16BE(value, 10);        // Value

    return buffer;
}

const client = new net.Socket();

const ALARMS = [
    // WLS.PV (ID 18) - High Limit > 100
    { name: 'WLS.PV_High', register: 4001, trigger: 150 },
    // WLS.SRS (ID 19) - Abnormal (High > 0)
    { name: 'WLS.SRS_Error', register: 4002, trigger: 1 },
    // WLS.SSS (ID 20) - Low Signal < 30
    { name: 'WLS.SSS_Low', register: 4003, trigger: 10 },
    // WLS.SBV (ID 21) - Low Battery < 20
    { name: 'WLS.SBV_Low', register: 4004, trigger: 10 }
];

client.connect(PORT, HOST, async () => {
    console.log(`Connected to Simulator at ${HOST}:${PORT}`);

    let transactionId = 1;

    for (const alarm of ALARMS) {
        console.log(`[${alarm.name}] Triggering alarm (Register ${alarm.register} -> ${alarm.trigger})...`);
        const frame = createWriteFrame(transactionId++, 1, alarm.register, alarm.trigger);
        client.write(frame);

        // Wait a bit to ensure processing
        await new Promise(resolve => setTimeout(resolve, 2000));
    }

    console.log('All triggers sent. Closing connection.');
    client.end();
});

client.on('error', (err) => {
    console.error('Connection Error:', err.message);
    process.exit(1);
});
