// backend/scripts/modbus_simulator.js
const ModbusRTU = require('modbus-serial');

// 시뮬레이션 데이터 상태 (나이키빌딩 소방수신기 시뮬레이션)
let simData = {
    // Digital (Coils)
    pv: 0,   // WLS.PV (접점 감지)
    srs: 0,  // WLS.SRS (인식 상태)
    scs: 0,  // WLS.SCS (통신 상태)

    // Analog (Holdings)
    sss: 85, // WLS.SSS (신호 강도: -5 ~ 100)
    sbv: 36, // WLS.SBV (배터리 전압: 20 ~ 100)

    direction: 1
};

// 주기적으로 데이터 업데이트
setInterval(() => {
    // 알람 시뮬레이션을 위해 10초마다 디지털 값 토글
    if (Math.random() > 0.8) simData.pv = simData.pv === 0 ? 1 : 0;

    // 아날로그 값 변동
    simData.sss += 1 * simData.direction;
    if (simData.sss > 95 || simData.sss < 30) simData.direction *= -1;

    simData.sbv = 36 + Math.random(); // 배터리 전압은 비교적 일정하게 유지
}, 2000);

const vector = {
    getCoil: (addr, unitID) => {
        if (addr === 100) return simData.pv === 1;
        if (addr === 101) return simData.srs === 1;
        if (addr === 102) return simData.scs === 1;
        return false;
    },
    getHoldingRegister: (addr, unitID) => {
        if (addr === 200) return Math.floor(simData.sss);
        if (addr === 201) return Math.floor(simData.sbv * 10); // 3.6V -> 36
        return 0;
    },
    getInputRegister: (addr, unitID) => addr,
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

serverTCP.on('initialized', () => {
    console.log(`🚀 Nike Building Modbus TCP Simulator initialized on port ${parseInt(process.env.MODBUS_PORT) || 50502}`);
});

console.log('🚀 Modbus TCP Simulator started');
console.log('    - Digital (Coils): 100(PV), 101(SRS), 102(SCS)');
console.log('    - Analog (Holdings): 200(SSS), 201(SBV)');

// Keep the process alive
setInterval(() => { }, 10000);
