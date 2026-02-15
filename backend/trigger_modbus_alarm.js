const ModbusRTU = require('modbus-serial');
const client = new ModbusRTU();

async function trigger() {
    try {
        console.log('Connecting to simulator-modbus:50502...');
        await client.connectTCP('simulator-modbus', { port: 50502 });
        await client.setID(1);
        
        console.log('Setting SSS to 10 (Clear)...');
        await client.writeRegister(200, 10);
        await new Promise(resolve => setTimeout(resolve, 5000));
        
        console.log('Setting SSS to 50 (Alarm)...');
        await client.writeRegister(200, 50);
        
        client.close();
        console.log('✅ Trigger completed');
    } catch (e) {
        console.error('❌ Trigger failed:', e.message);
    }
}

trigger();
