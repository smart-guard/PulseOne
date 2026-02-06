// backend/scripts/force_modbus.js
const ModbusRTU = require('modbus-serial');
const client = new ModbusRTU();

const address = parseInt(process.argv[2]);
const value = parseInt(process.argv[3]);
const type = process.argv[4] === 'register' ? 'register' : 'coil';

if (isNaN(address)) {
    console.log('Usage: node force_modbus.js <address> <value> [coil|register]');
    process.exit(1);
}

async function forceUpdate() {
    try {
        console.log(`📡 Connecting to Modbus simulator on localhost:50502...`);
        await client.connectTCP('simulator-modbus', { port: 50502 });
        await client.setID(1);

        if (type === 'coil') {
            console.log(`⚙️  Writing Coil: Address=${address}, Value=${value === 1}`);
            await client.writeCoil(address, value === 1);
        } else {
            console.log(`⚙️  Writing Register: Address=${address}, Value=${value}`);
            await client.writeRegister(address, value);
        }

        console.log(`✅ Successfully updated ${type} ${address} to ${value}`);
        client.close();
        process.exit(0);
    } catch (err) {
        console.error(`❌ Error force updating Modbus: ${err.message}`);
        process.exit(1);
    }
}

forceUpdate();
