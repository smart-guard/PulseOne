const mqtt = require('mqtt');
const crypto = require('crypto');

// Configuration
const BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://rabbitmq:1883';
const BLOB_TOPIC_BASE = 'sensors/simulator/blob';
const DATA_TOPIC = 'sensors/simulator/data';
const FILE_SIZE = 1024 * 5; // 5KB dummy file
const CHUNK_SIZE = 2048; // 2KB chunks

console.log(`🚀 Starting E2E MQTT Export Test`);
console.log(`   Broker: ${BROKER_URL}`);

const client = mqtt.connect(BROKER_URL);

function generateDummyFile(size) {
    return crypto.randomBytes(size);
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

client.on('connect', async () => {
    console.log('✅ Connected to Broker');

    try {
        // 1. Generate Dummy File
        const fileId = `test_file_${Date.now()}`;
        const fileContent = generateDummyFile(FILE_SIZE);
        const totalChunks = Math.ceil(FILE_SIZE / CHUNK_SIZE);

        console.log(`\n📦 STARTING BLOB TRANSFER (ID: ${fileId})`);
        console.log(`   Total Size: ${FILE_SIZE} bytes`);
        console.log(`   Total Chunks: ${totalChunks}`);

        // 2. Send Chunks
        for (let i = 0; i < totalChunks; i++) {
            const start = i * CHUNK_SIZE;
            const end = Math.min(start + CHUNK_SIZE, FILE_SIZE);
            const chunk = fileContent.slice(start, end);

            const topic = `${BLOB_TOPIC_BASE}/${fileId}/${totalChunks}/${i}`;

            await new Promise((resolve, reject) => {
                client.publish(topic, chunk, { qos: 1 }, (err) => {
                    if (err) reject(err);
                    else {
                        console.log(`   -> Sent Chunk ${i + 1}/${totalChunks} to ${topic} (${chunk.length} bytes)`);
                        resolve();
                    }
                });
            });

            await sleep(100); // Slight delay for stability
        }
        console.log(`✅ BLOB TRANSFER COMPLETE`);

        // 3. Send Standard Telemetry (to trigger normal pipeline)
        console.log(`\n📊 SENDING STANDARD TELEMETRY`);
        const telemetryPayload = JSON.stringify({
            temp: 45.5,
            status: "warning",
            timestamp: Date.now(),
            file_ref: `file://${fileId}` // Just a marker
        });

        await new Promise((resolve, reject) => {
            client.publish(DATA_TOPIC, telemetryPayload, { qos: 1 }, (err) => {
                if (err) reject(err);
                else {
                    console.log(`   -> Sent data to ${DATA_TOPIC}: ${telemetryPayload}`);
                    resolve();
                }
            });
        });

        console.log(`\n✨ TEST SUCCESS: All messages published.`);
        console.log(`   Now check Collector logs for "Reassembled blob" and "Saved blob".`);
        console.log(`   Check /app/data/blobs/ in Collector container.`);

        client.end();
        process.exit(0);

    } catch (err) {
        console.error('❌ TEST FAILED:', err);
        client.end();
        process.exit(1);
    }
});

client.on('error', (err) => {
    console.error('❌ MQTT Client Error:', err);
    process.exit(1);
});
