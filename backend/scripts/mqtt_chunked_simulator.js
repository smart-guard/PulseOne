const mqtt = require('mqtt');
const fs = require('fs');
const path = require('path');

// Configuration
// Connect to RabbitMQ via internal docker network hostname 'rabbitmq'
const BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883';
const DATA_TOPIC = 'vfd/data';
const FILE_TOPIC = 'vfd/file';

console.log(`🚀 Starting MQTT Chunked Transfer Simulator`);
console.log(`   Broker: ${BROKER_URL}`);

const client = mqtt.connect(BROKER_URL);

// Dummy file creation (1KB)
const FILE_SIZE = 1024;
const CHUNK_SIZE = 256;
const DUMMY_FILE_PATH = '/tmp/test_chunk_export.bin';
const buffer = Buffer.alloc(FILE_SIZE, 'A'); // Fill with 'A'
fs.writeFileSync(DUMMY_FILE_PATH, buffer);

client.on('connect', async () => {
    console.log('✅ Connected to Broker');

    // 1. Generate fileId First
    const fileId = `file_${Date.now()}`;
    const filename = 'chunked_export.bin';
    const totalChunks = Math.ceil(FILE_SIZE / CHUNK_SIZE);

    // 2. Publish Telemetry with file_ref (Triggers Auto-Discovery for 'vfd/data')
    const telemetryPayload = JSON.stringify({
        value: 155.5, // > 150 to trigger alarm later
        status: "critical",
        timestamp: Date.now(),
        file_ref: fileId // Metadata for file association
    });
    console.log(`📤 Sending Telemetry to ${DATA_TOPIC}: ${telemetryPayload}`);
    client.publish(DATA_TOPIC, telemetryPayload, { qos: 1 });

    // 3. Publish File in Chunks (Triggers Auto-Discovery for 'vfd/file' + Reassembly)
    console.log(`📦 Starting File Transfer: ${filename} (${FILE_SIZE} bytes) to ${FILE_TOPIC}`);

    for (let i = 0; i < totalChunks; i++) {
        const start = i * CHUNK_SIZE;
        const end = Math.min(start + CHUNK_SIZE, FILE_SIZE);
        const chunk = buffer.slice(start, end);

        // PulseOne Protocol: Topic-based Blob Transfer
        // Topic: vfd/blob/{id}/{total}/{index}
        const blobTopic = `vfd/blob/${fileId}/${totalChunks}/${i}`;

        console.log(`   -> Sending chunk ${i + 1}/${totalChunks} to ${blobTopic}`);
        client.publish(blobTopic, chunk, { qos: 1 });

        await new Promise(r => setTimeout(r, 200)); // Small delay to ensure order
    }

    console.log('✅ File Transfer Complete. Waiting for processing...');

    setTimeout(() => {
        client.end();
        process.exit(0);
    }, 2000);
});

client.on('error', (err) => {
    console.error('❌ Connection Error:', err);
    process.exit(1);
});
