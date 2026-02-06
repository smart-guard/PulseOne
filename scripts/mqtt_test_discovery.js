const mqtt = require('mqtt');

// RabbitMQ (MQTT) Broker URL
const client = mqtt.connect('mqtt://localhost:1883');

client.on('connect', () => {
    console.log('Connected to MQTT Broker');

    // JSON Payload for auto-discovery
    const payload = JSON.stringify({
        frequency: 60.5,
        voltage: 220.1,
        current: 5.2,
        status: "RUNNING",
        is_active: true
    });

    console.log('Publishing to vfd/device1...');
    client.publish('vfd/device1', payload, { qos: 1 }, (err) => {
        if (err) {
            console.error('Publish Error:', err);
        } else {
            console.log('Published successfully');
        }
        client.end();
    });
});

client.on('error', (err) => {
    console.error('Connection Error:', err);
});
