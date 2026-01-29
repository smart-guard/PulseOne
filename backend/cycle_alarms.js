
const net = require('net');

// Use localhost when running from the host machine to access the mapped port
// If running inside docker, change to 'docker-simulator-modbus-1'
const HOST = 'docker-simulator-modbus-1';
const PORT = 50502;

function createWriteFrame(transactionId, unitId, registerAddress, value) {
    const buffer = Buffer.alloc(12);
    // Header
    buffer.writeUInt16BE(transactionId, 0);
    buffer.writeUInt16BE(0, 2);
    buffer.writeUInt16BE(6, 4);
    // PDU
    buffer.writeUInt8(unitId, 6);
    buffer.writeUInt8(6, 7);
    buffer.writeUInt16BE(registerAddress, 8);
    buffer.writeUInt16BE(value, 10);
    return buffer;
}

const ALARMS = [
    { name: 'WLS.PV_Digital', register: 100, normal: 0, trigger: 1 },
    { name: 'WLS.SRS_High_Limit', register: 101, normal: 50, trigger: 150 }, // Limit > 100.0
    { name: 'WLS.SSS_High_Limit', register: 200, normal: 10, trigger: 50 },  // Limit > 30.0
    { name: 'WLS.SBV_High_Limit', register: 201, normal: 20, trigger: 100 }  // Limit > 50.0
];

const client = new net.Socket();

async function runCycle() {
    let transactionId = 1;

    while (true) {
        console.log('\n--- Starting Alarm Cycle ---');

        // 1. Trigger Alarms
        console.log('>>> TRIGGERING ALARMS');
        for (const alarm of ALARMS) {
            console.log(`[${alarm.name}] Setting Register ${alarm.register} to ${alarm.trigger} (ALARM)`);
            const frame = createWriteFrame(transactionId++, 1, alarm.register, alarm.trigger);
            client.write(frame);
            await new Promise(r => setTimeout(r, 100));
        }

        // Wait for connection to pick up and alarm to trigger
        console.log('Waiting 10 seconds for alarms to be detected...');
        await new Promise(r => setTimeout(r, 10000));

        // 2. Clear Alarms (Return to Normal)
        console.log('<<< CLEARING ALARMS');
        for (const alarm of ALARMS) {
            console.log(`[${alarm.name}] Setting Register ${alarm.register} to ${alarm.normal} (NORMAL)`);
            const frame = createWriteFrame(transactionId++, 1, alarm.register, alarm.normal);
            client.write(frame);
            await new Promise(r => setTimeout(r, 100));
        }

        console.log('Waiting 10 seconds before next cycle...');
        await new Promise(r => setTimeout(r, 10000));
    }
}

client.connect(PORT, HOST, async () => {
    console.log(`Connected to Simulator at ${HOST}:${PORT}`);
    runCycle().catch(err => console.error(err));
});

client.on('error', (err) => {
    console.error('Connection Error:', err.message);
    process.exit(1);
});

client.on('close', () => {
    console.log('Connection closed');
    process.exit(0);
});
