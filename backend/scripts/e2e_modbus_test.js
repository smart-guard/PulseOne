const ModbusRTU = require('modbus-serial');
const client = new ModbusRTU();

async function run() {
    try {
        console.log('Connecting to simulator...');
        await client.connectTCP('simulator-modbus', { port: 50502 });
        await client.setID(1);
        console.log('Connected!');

        // 1. WLS.PV (Addr 100, BOOL) -> 1
        console.log('Setting WLS.PV (100) to 1');
        await client.writeCoil(100, true);
        await new Promise(r => setTimeout(r, 2000));

        // 2. WLS.SRS (Addr 101, BOOL) -> 0 -> 1 (Trigger Alarm 23)
        console.log('Toggling WLS.SRS (101) to 0 then 1');
        await client.writeCoil(101, false);
        await new Promise(r => setTimeout(r, 2000));
        await client.writeCoil(101, true);
        await new Promise(r => setTimeout(r, 2000));

        // 3. WLS.SCS (Addr 102, BOOL) -> 1 (Trigger Alarm 24)
        console.log('Setting WLS.SCS (102) to 1');
        await client.writeCoil(102, true);
        await new Promise(r => setTimeout(r, 2000));

        // 4. WLS.SSS (Addr 200, UINT16) -> 20 (Clear 3002) -> 150 (Trigger Rule 25 & 3002)
        console.log('Setting WLS.SSS (200) to 20 then 150');
        await client.writeRegister(200, 20);
        await new Promise(r => setTimeout(r, 2000));
        await client.writeRegister(200, 150);
        await new Promise(r => setTimeout(r, 2000));

        // 5. WLS.SBV (Addr 201, UINT16) -> 120 (Safe: 12.0V) -> 250 (High: 25.0V, Trigger Rule 26)
        console.log('Setting WLS.SBV (201) to 120 then 250');
        await client.writeRegister(201, 120);
        await new Promise(r => setTimeout(r, 2000));
        await client.writeRegister(201, 250);
        await new Promise(r => setTimeout(r, 2000));

        console.log('Sequence complete. Waiting for propagation...');
        await new Promise(r => setTimeout(r, 5000));

        // 6. Restore all to safe states
        console.log('Restoring all to safe states...');
        await client.writeCoil(100, false);
        await client.writeCoil(101, false);
        await client.writeCoil(102, false);
        await client.writeRegister(200, 80);
        await client.writeRegister(201, 130);

        console.log('Done.');
        client.close();
    } catch (e) {
        console.error('Error:', e);
    }
}

run();
