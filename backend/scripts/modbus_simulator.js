// modbus_simulator.js — Modbus TCP Server (Modbus Standard)
// DB 레지스터 맵 기준 (device_id=2, HMI-001, endpoint=simulator-modbus:50502)
//
// Modbus 표준:
//   FC01 Read Coils        — CO:100(WLS.PV), CO:101(WLS.SRS), CO:102(WLS.SCS)
//   FC03 Read Holding Regs — HR:200(WLS.SSS), HR:201(WLS.SBV)
//
// Collector 수정 내용 (ModbusDriver.cpp):
//   address_string prefix 파싱으로 FC 결정 (CO:→FC01, HR:→FC03)
//   더 이상 BOOL → HOLDING_REGISTER fallback 하지 않음

'use strict';

const net = require('net');

const PORT = parseInt(process.env.MODBUS_PORT || '50502');

// ── Register storage ───────────────────────────────────────────────────────

// Coil: FC01 대상 (CO:xxx)
const coils = new Uint8Array(1024);

// Holding Registers: FC03 대상 (HR:xxx)
const holdingRegs = new Uint16Array(1024);

// ── Initial values ─────────────────────────────────────────────────────────
// CO:100 — WLS.PV  (BOOL)
coils[100] = 1;
// CO:101 — WLS.SRS (BOOL)
coils[101] = 0;
// CO:102 — WLS.SCS (BOOL)
coils[102] = 0;

// HR:200 — WLS.SSS (UINT16) Setpoint
holdingRegs[200] = 80;
// HR:201 — WLS.SBV (UINT16) Backup Value
holdingRegs[201] = 360;

// ── Realistic value simulation ─────────────────────────────────────────────
setInterval(() => {
    const pv = Math.random() > 0.2 ? 1 : 0;
    coils[100] = pv;           // WLS.PV
    coils[101] = pv;           // WLS.SRS
    coils[102] = pv;           // WLS.SCS

    holdingRegs[200] = 60 + Math.floor(Math.random() * 40);   // 60–100
    holdingRegs[201] = 300 + Math.floor(Math.random() * 120); // 300–420

    console.log(`[Modbus] CO:100(PV)=${coils[100]} HR:200(SSS)=${holdingRegs[200]} HR:201(SBV)=${holdingRegs[201]}`);
}, 2000);

// ── Response builders ──────────────────────────────────────────────────────

function buildException(txId, protoId, unitId, fc, code) {
    const b = Buffer.alloc(9);
    b.writeUInt16BE(txId, 0);
    b.writeUInt16BE(protoId, 2);
    b.writeUInt16BE(3, 4);
    b.writeUInt8(unitId, 6);
    b.writeUInt8(fc | 0x80, 7);
    b.writeUInt8(code, 8);
    return b;
}

// FC01 — Read Coils
function handleFC01(txId, protoId, unitId, data) {
    if (data.length < 4) return buildException(txId, protoId, unitId, 1, 3);
    const start = data.readUInt16BE(0);
    const qty = data.readUInt16BE(2);
    if (qty < 1 || qty > 2000) return buildException(txId, protoId, unitId, 1, 3);
    if (start + qty > coils.length) return buildException(txId, protoId, unitId, 1, 2);

    const byteCount = Math.ceil(qty / 8);
    const b = Buffer.alloc(9 + byteCount);
    b.writeUInt16BE(txId, 0);
    b.writeUInt16BE(protoId, 2);
    b.writeUInt16BE(3 + byteCount, 4);
    b.writeUInt8(unitId, 6);
    b.writeUInt8(1, 7);
    b.writeUInt8(byteCount, 8);
    b.fill(0, 9);
    for (let i = 0; i < qty; i++) {
        if (coils[start + i]) b[9 + Math.floor(i / 8)] |= (1 << (i % 8));
    }
    return b;
}

// FC03 — Read Holding Registers
function handleFC03(txId, protoId, unitId, data) {
    if (data.length < 4) return buildException(txId, protoId, unitId, 3, 3);
    const start = data.readUInt16BE(0);
    const qty = data.readUInt16BE(2);
    if (qty < 1 || qty > 125) return buildException(txId, protoId, unitId, 3, 3);
    if (start + qty > holdingRegs.length) return buildException(txId, protoId, unitId, 3, 2);

    const byteCount = qty * 2;
    const b = Buffer.alloc(9 + byteCount);
    b.writeUInt16BE(txId, 0);
    b.writeUInt16BE(protoId, 2);
    b.writeUInt16BE(3 + byteCount, 4);
    b.writeUInt8(unitId, 6);
    b.writeUInt8(3, 7);
    b.writeUInt8(byteCount, 8);
    for (let i = 0; i < qty; i++) {
        b.writeUInt16BE(holdingRegs[start + i], 9 + i * 2);
    }
    return b;
}

// FC05 — Write Single Coil
function handleFC05(txId, protoId, unitId, data) {
    if (data.length < 4) return buildException(txId, protoId, unitId, 5, 3);
    const addr = data.readUInt16BE(0);
    const val = data.readUInt16BE(2); // 0xFF00 = ON, 0x0000 = OFF
    if (addr >= coils.length) return buildException(txId, protoId, unitId, 5, 2);
    coils[addr] = (val === 0xFF00) ? 1 : 0;
    console.log(`[Modbus] FC05 WriteCoil addr=${addr} val=${coils[addr]}`);
    // Echo response
    const b = Buffer.alloc(12);
    b.writeUInt16BE(txId, 0); b.writeUInt16BE(protoId, 2);
    b.writeUInt16BE(6, 4); b.writeUInt8(unitId, 6);
    b.writeUInt8(5, 7); b.writeUInt16BE(addr, 8);
    b.writeUInt16BE(val, 10);
    return b;
}

// FC06 — Write Single Register
function handleFC06(txId, protoId, unitId, data) {
    if (data.length < 4) return buildException(txId, protoId, unitId, 6, 3);
    const addr = data.readUInt16BE(0);
    const val = data.readUInt16BE(2);
    if (addr >= holdingRegs.length) return buildException(txId, protoId, unitId, 6, 2);
    holdingRegs[addr] = val;
    console.log(`[Modbus] FC06 WriteHR addr=${addr} val=${val}`);
    // Echo response
    const b = Buffer.alloc(12);
    b.writeUInt16BE(txId, 0); b.writeUInt16BE(protoId, 2);
    b.writeUInt16BE(6, 4); b.writeUInt8(unitId, 6);
    b.writeUInt8(6, 7); b.writeUInt16BE(addr, 8);
    b.writeUInt16BE(val, 10);
    return b;
}

// ── TCP Server ─────────────────────────────────────────────────────────────

const server = net.createServer((socket) => {
    const addr = `${socket.remoteAddress}:${socket.remotePort}`;
    console.log(`[Modbus] Connected: ${addr}`);
    socket.setNoDelay(true);

    let buf = Buffer.alloc(0);

    socket.on('data', (chunk) => {
        buf = Buffer.concat([buf, chunk]);
        while (buf.length >= 6) {
            const pduLen = buf.readUInt16BE(4);
            const total = 6 + pduLen;
            if (buf.length < total) break;

            const txId = buf.readUInt16BE(0);
            const protoId = buf.readUInt16BE(2);
            const unitId = buf.readUInt8(6);
            const fc = buf.readUInt8(7);
            const data = buf.slice(8, total);
            buf = buf.slice(total);

            let resp;
            switch (fc) {
                case 1: resp = handleFC01(txId, protoId, unitId, data); break;  // Read Coils
                case 3: resp = handleFC03(txId, protoId, unitId, data); break;  // Read Holding Registers
                case 5: resp = handleFC05(txId, protoId, unitId, data); break;  // Write Single Coil
                case 6: resp = handleFC06(txId, protoId, unitId, data); break;  // Write Single Register
                default: resp = buildException(txId, protoId, unitId, fc, 1);   // Illegal Function
            }
            socket.write(resp);
        }
    });

    socket.on('close', () => console.log(`[Modbus] Disconnected: ${addr}`));
    socket.on('error', (e) => console.error(`[Modbus] Socket error: ${e.message}`));
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[Modbus] Listening on 0.0.0.0:${PORT} (Modbus Standard)`);
    console.log(`[Modbus] FC01 Coil: CO:100=WLS.PV, CO:101=WLS.SRS, CO:102=WLS.SCS`);
    console.log(`[Modbus] FC03 HR  : HR:200=WLS.SSS, HR:201=WLS.SBV`);
});

server.on('error', (e) => { console.error(`[Modbus] Server error: ${e.message}`); process.exit(1); });
