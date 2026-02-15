const ModbusRTU = require("modbus-serial");
const client = new ModbusRTU();

async function trigger() {
    try {
        await client.connectTCP("localhost", { port: 50502 });
        await client.setID(1);
        
        console.log("Setting SSS to 10 (Clear)...");
        await client.writeRegister(200, 10);
        await new Promise(resolve => setTimeout(resolve, 5000));
        
        console.log("Setting SSS to 50 (Alarm)...");
        await client.writeRegister(200, 50);
        
        client.close();
        console.log("Done");
    } catch (e) {
        console.error(e);
    }
}

trigger();
