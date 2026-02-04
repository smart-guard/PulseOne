const aedes = require('aedes')();
const server = require('net').createServer(aedes.handle);
const port = 1883;
const fs = require('fs');

// --- Broker Setup ---
server.listen(port, function () {
    console.log('🚀 MQTT Broker Simulator started and listening on port', port);
    startSimulation();
});

aedes.on('client', function (client) {
    console.log('Client Connected: \x1b[33m' + (client ? client.id : client) + '\x1b[0m', 'to broker', aedes.id);
});

aedes.on('subscribe', function (subscriptions, client) {
    console.log('Client: \x1b[33m' + (client ? client.id : client) + '\x1b[0m subscribed to topics: ' + subscriptions.map(s => s.topic).join('\n'));
});

// --- Simulation Logic ---
async function startSimulation() {
    console.log('⏳ Waiting 5s for Collector to connect...');
    await new Promise(r => setTimeout(r, 5000));

    const DATA_TOPIC = 'vfd/data';
    const FILE_TOPIC = 'vfd/file';

    // 1. Publish Telemetry (Auto-Discovery)
    const telemetryPayload = JSON.stringify({
        value: 160.0,
        status: "critical", // > 150
        timestamp: Date.now()
    });

    console.log(`📤 Publishing Telemetry to ${DATA_TOPIC}:`, telemetryPayload);
    aedes.publish({ topic: DATA_TOPIC, payload: telemetryPayload, qos: 1, retain: false });

    // 2. Publish File Chunking
    const FILE_SIZE = 1024;
    const CHUNK_SIZE = 256;
    const buffer = Buffer.alloc(FILE_SIZE, 'X'); // 'X' content
    const totalChunks = Math.ceil(FILE_SIZE / CHUNK_SIZE);
    const filename = 'broker_sim_file.bin';
    const fileId = `f_${Date.now()}`;

    console.log(`📦 Starting File Transfer: ${filename} to ${FILE_TOPIC}`);

    for (let i = 0; i < totalChunks; i++) {
        const start = i * CHUNK_SIZE;
        const end = Math.min(start + CHUNK_SIZE, FILE_SIZE);
        const chunk = buffer.slice(start, end);

        // Protocol: JSON wrapper with base64 data
        const chunkPayload = JSON.stringify({
            file_id: fileId,
            filename: filename,
            chunk_index: i,
            total_chunks: totalChunks,
            data: chunk.toString('base64'),
            file_ref: ''
        });

        console.log(`   -> Sending chunk ${i + 1}/${totalChunks}`);
        aedes.publish({ topic: FILE_TOPIC, payload: chunkPayload, qos: 1, retain: false });

        await new Promise(r => setTimeout(r, 200));
    }

    console.log('✅ Simulation Complete. Broker keeping alive for Collector to fetch...');
}
