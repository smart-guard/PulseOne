const ModbusRTU = require('modbus-serial');
const client = new ModbusRTU();

async function triggerAlarm() {
    try {
        await client.connectTCP('127.0.0.1', { port: 50502 });
        await client.setID(1);

        console.log('🔗 Connected to Modbus Simulator');

        // Trigger WLS.SRS alarm (Address 101, Value 1)
        console.log('📤 Writing 1 to Address 101 (WLS.SRS)...');
        await client.writeCoil(101, true);

        console.log('✅ Trigger successful');
        client.close();
    } catch (e) {
        console.error('❌ Failed to trigger alarm:', e.message);
    }
}

triggerAlarm();
