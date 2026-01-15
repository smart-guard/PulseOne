// backend/scripts/modbus_simulator.js
const ModbusRTU = require('modbus-serial');
const vector = {
    getInputRegister: (addr, unitID) => addr,
    getHoldingRegister: (addr, unitID) => {
        // Return dynamic values for registers 0-9
        if (addr < 10) {
            return Math.floor(Math.random() * 1000);
        }
        return addr;
    },
    getCoil: (addr, unitID) => (addr % 2 === 0),
    setRegister: (addr, value, unitID) => {
        console.log(`[Simulator] Write Register: Addr=${addr}, Value=${value}`);
        return;
    },
    setCoil: (addr, value, unitID) => {
        console.log(`[Simulator] Write Coil: Addr=${addr}, Value=${value}`);
        return;
    }
};

const serverTCP = new ModbusRTU.ServerTCP(vector, {
    host: '0.0.0.0',
    port: 50502,
    debug: true,
    unitID: 1
});

serverTCP.on('socketError', (err) => {
    console.error('[Simulator Socket ERROR]', err);
});

serverTCP.on('error', (err) => {
    console.error('[Simulator ERROR]', err);
});

serverTCP.on('initialized', () => {
    console.log('🚀 Modbus TCP Simulator initialized and listening on port 50502');
});

serverTCP.on('connection', (client) => {
    console.log(`[Simulator] New connection from ${client.remoteAddress}:${client.remotePort}`);
});

console.log('🚀 Modbus TCP Simulator started');
console.log('    - Slave ID: 1');
console.log('    - Holding Registers: 0-10 (Random values)');

// Keep the process alive
setInterval(() => {
    // console.log('[Simulator] Still alive...');
}, 10000);
