const mqtt = require('mqtt');

// Configuration
const BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883';
const TOPIC_DATA = 'vfd/data';
const TOPIC_FILE = 'vfd/file';
const FILE_REF = 'file:///app/data/blobs/20260204_TEST_EXPORT.bin';

const client = mqtt.connect(BROKER_URL);

client.on('connect', () => {
    console.log('🚀 MQTT File Upload Simulator connected');

    // Create a payload that mimics a high-frequency data burst with a file reference
    // We send a value that triggers an alarm (e.g., Point 2002 -> 150)
    // AND we include the 'file_ref' metadata.

    // Note: The Collector's MQTTWorker looks for JSON paths. 
    // We assume there's a mapping for this. 
    // Based on previous Modbus tests, Point 2002 triggers alarm 202.
    // However, for MQTT, we need to know the TOPIC -> POINT mapping.
    // Let's assume a generic mapping or try to inject directly if possible.
    // A safer bet for "File Export" verification is to emulate the specific blob format.

    // 1. Publish Telemetry Data (Triggers Alarm)
    const payloadData = JSON.stringify({
        value: 156.0, // > 150 Trigger
        status: "critical",
        file_ref: FILE_REF,
        timestamp: Date.now()
    });

    // 2. Publish File Event (Multi-topic test)
    const payloadFile = JSON.stringify({
        filename: "test_export.bin",
        file_ref: FILE_REF,
        size: 1024
    });

    console.log(`Publishing to ${TOPIC_DATA}:`, payloadData);
    client.publish(TOPIC_DATA, payloadData, { qos: 1 });

    console.log(`Publishing to ${TOPIC_FILE}:`, payloadFile);
    client.publish(TOPIC_FILE, payloadFile, { qos: 1 }, (err) => {
        if (err) {
            console.error('Publish failed:', err);
            process.exit(1);
        } else {
            console.log('✅ Publish success');
            client.end();
            process.exit(0);
        }
    });
});

client.on('error', (err) => {
    console.error('MQTT Error:', err);
    process.exit(1);
});
