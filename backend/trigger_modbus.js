const ModbusRTU = require('modbus-serial');
const client = new ModbusRTU();

async function trigger() {
    try {
        const host = process.env.MODBUS_HOST || '127.0.0.1';
        await client.connectTCP(host, { port: 50502 });
        client.setID(1);
        console.log('Connected to Simulator. Writing Register 100 to 1...');
        await client.writeRegister(100, 1);
        console.log('Successfully wrote Register 100 to 1.');
        await new Promise(resolve => setTimeout(resolve, 2000));
        await client.close();
    } catch (e) {
        console.error('Trigger Error:', e);
    }
}

trigger();
