// backend/scripts/modbus_simulator.js
const ModbusRTU = require('modbus-serial');

// 시뮬레이션 데이터 상태
let simData = {
    temp: 34.0,      // Address 1004 (Float)
    current: 29.5,   // Address 1003 (Float)
    direction: 1
};

// 주기적으로 데이터 업데이트 (알람 범위를 왔다갔다함)
setInterval(() => {
    simData.temp += 0.5 * simData.direction;
    simData.current += 0.2 * simData.direction;

    if (simData.temp > 40 || simData.temp < 30) {
        simData.direction *= -1;
    }
}, 2000);

const vector = {
    getInputRegister: (addr, unitID) => addr,
    getHoldingRegister: (addr, unitID) => {
        // [테스트용] 1003 (Current), 1004 (Temp) - FLOAT32 (2 registers each)
        // FLOAT는 보통 2개의 레지스터를 차지하지만, 여기선 간단히 정수형으로 변환해 리턴하거나 
        // 상위/하위 바이트 분할이 필요할 수 있음. 
        // Collector의 FLOAT32 파싱 방식에 맞춰야 함. (Big Endian 가정)

        if (addr === 1003) { // Sim_Current (Address 1 = 1003)
            return Math.floor(simData.current * 10); // 295 -> 29.5 (Scaling factor 0.1 적용 가정)
        }
        if (addr === 1004) { // Sim_Temp (Address 2 = 1004)
            return Math.floor(simData.temp * 10); // 340 -> 34.0 (Scaling factor 0.1 적용 가정)
        }

        // 구 버전 호환용 (0-9)
        if (addr < 10) {
            return Math.floor(Math.random() * 1000);
        }
        return addr;
    },
    getCoil: (addr, unitID) => (addr % 2 === 0),
    setRegister: (addr, value, unitID) => {
        console.log(`[Modbus Simulator] Write Register: Addr=${addr}, Value=${value}`);
        return;
    },
    setCoil: (addr, value, unitID) => {
        console.log(`[Modbus Simulator] Write Coil: Addr=${addr}, Value=${value}`);
        return;
    }
};

const serverTCP = new ModbusRTU.ServerTCP(vector, {
    host: '0.0.0.0',
    port: parseInt(process.env.MODBUS_PORT) || 50502,
    debug: false,
    unitID: 1
});

serverTCP.on('socketError', (err) => {
    console.error('[Modbus Simulator Socket ERROR]', err);
});

serverTCP.on('error', (err) => {
    console.error('[Modbus Simulator ERROR]', err);
});

serverTCP.on('initialized', () => {
    console.log(`🚀 Modbus TCP Simulator initialized and listening on port ${parseInt(process.env.MODBUS_PORT) || 50502}`);
});

serverTCP.on('connection', (client) => {
    // console.log(`[Modbus Simulator] New connection from ${client.remoteAddress}:${client.remotePort}`);
});

console.log('🚀 Modbus TCP Simulator started');
console.log('    - Slave ID: 1');
console.log('    - Test Registers: 1003 (Current), 1004 (Temp)');

// Keep the process alive
setInterval(() => { }, 10000);
