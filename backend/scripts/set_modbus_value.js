const ModbusRTU = require('modbus-serial');
const client = new ModbusRTU();

const PORT = parseInt(process.env.MODBUS_PORT) || 50502;
const HOST = 'localhost';
const UNIT_ID = 1;
const ADDR = 200;
const VALUE = 150;

console.log(`Connecting to ${HOST}:${PORT}...`);

client.connectTCP(HOST, { port: PORT })
    .then(() => {
        console.log("Connected");
        return client.setID(UNIT_ID);
    })
    .then(() => {
        console.log(`Writing ${VALUE} to register ${ADDR}...`);
        return client.writeRegister(ADDR, VALUE);
    })
    .then(() => {
        console.log("Write success!");
        client.close();
        process.exit(0);
    })
    .catch((e) => {
        console.error("Error:", e.message);
        client.close();
        process.exit(1);
    });
