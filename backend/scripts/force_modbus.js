// backend/scripts/force_modbus.js
const ModbusRTU = require('modbus-serial');
const client = new ModbusRTU();

const addr = parseInt(process.argv[2]) || 100;
const value = parseInt(process.argv[3]) || 0;

console.log(`Attempting to write ${value} to address ${addr}...`);

client.connectTCP("localhost", { port: 50502 })
    .then(() => {
        if (addr >= 200) {
            return client.writeRegister(addr, value);
        } else {
            return client.writeCoil(addr, value === 1);
        }
    })
    .then(() => {
        console.log(`Success: Address ${addr} is now ${value}`);
        process.exit(0);
    })
    .catch((err) => {
        console.error("Error connecting or writing:", err.message);
        process.exit(1);
    });
