const mqtt = require('mqtt');

// RabbitMQ (MQTT) Broker URL - inside docker network
const client = mqtt.connect('mqtt://rabbitmq:1883');

client.on('connect', () => {
    console.log('Connected to MQTT Broker');

    // KST Helper (UTC+9)
    const kst = new Date(new Date().getTime() + (9 * 60 * 60 * 1000));
    const kstString = kst.toISOString().replace('T', ' ').substring(0, 19);

    // JSON Payload for auto-discovery
    const payload = JSON.stringify({
        new_metric_1: 123.45,
        new_metric_2: 67.89,
        discovery_timestamp: kstString
    });

    const topic = 'vfd/test';
    console.log(`Publishing to ${topic}...`);
    client.publish(topic, payload, { qos: 1 }, (err) => {
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
