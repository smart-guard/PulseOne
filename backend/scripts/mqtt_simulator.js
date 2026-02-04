// scripts/mqtt_simulator.js
const mqtt = require('mqtt');

// Configuration
const BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883';
const TOPIC = 'vfd/auto_point';

console.log(`🚀 Starting MQTT Simulator`);
console.log(`   Broker: ${BROKER_URL}`);
console.log(`   Target Topic: ${TOPIC}`);

const client = mqtt.connect(BROKER_URL);

client.on('connect', () => {
    console.log('✅ Connected to Broker');

    // Publish simple telemetry to a new, unmapped topic
    // Expected: Collector should auto-create a point for 'vfd/auto_point'
    const payload = JSON.stringify({
        value: 123.45,
        status: "ok",
        timestamp: Date.now()
    });

    console.log(`📤 Publishing payload:`, payload);

    client.publish(TOPIC, payload, { qos: 1 }, (err) => {
        if (err) {
            console.error('❌ Publish failed:', err);
            process.exit(1);
        } else {
            console.log('✅ Publish success: vfd/auto_point');

            // 🔥 추가 테스트: 단일 토픽 자동 등록 (vfd/file)
            client.publish('vfd/file', 'blob://vfd_data_20260204.bin', { qos: 1 }, (err2) => {
                if (err2) {
                    console.error('❌ Publish failed (vfd/file):', err2);
                } else {
                    console.log('✅ Publish success: vfd/file');
                }
                client.end();
                process.exit(0);
            });
        }
    });
});

client.on('error', (err) => {
    console.error('❌ Connection Error:', err);
    process.exit(1);
});
