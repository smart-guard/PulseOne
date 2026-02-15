/**
 * BLE Beacon Simulator for PulseOne
 * Sends UDP packets to mimic discovered beacons.
 */
const dgram = require('dgram');
const client = dgram.createSocket('udp4');

const TARGET_HOST = process.env.COLLECTOR_HOST || 'localhost';
const PORT = 5555;

console.log(`🚀 BLE Simulator starting... Target: ${TARGET_HOST}:${PORT}`);

const BEACONS = [
    { uuid: "74278BDA-B644-4520-8F0C-720EAF059935", rssi: -65 },
    { uuid: "FDA50693-A4E2-4FB1-AFCF-C6EB07647825", rssi: -72 },
    { uuid: "B9407F30-F5F8-466E-AFF9-25556B57FE6D", rssi: -58 }
];

setInterval(() => {
    // Pick a random beacon and fluctuate RSSI
    const beacon = BEACONS[Math.floor(Math.random() * BEACONS.length)];
    const payload = JSON.stringify({
        uuid: beacon.uuid,
        rssi: beacon.rssi + Math.floor(Math.random() * 10 - 5)
    });

    const message = Buffer.from(payload);
    client.send(message, PORT, TARGET_HOST, (err) => {
        if (err) console.error(`[BLE-Sim] Send Error: ${err}`);
        else console.log(`[BLE-Sim] Discovered Beacon: ${payload}`);
    });
}, 3000);

process.on('SIGINT', () => {
    client.close();
    process.exit();
});
