// mqtt_simulator.js — MQTT Publisher Simulator
// Broker: ${MQTT_BROKER_URL} (defaults to mqtt://rabbitmq:1883)
// Topic: vfd/ subtopics — matches MQTT-Test-Device config (topic: vfd/#)
//
// Uses the mqtt npm package (usually already installed in backend)

'use strict';

const mqtt = require('mqtt');

const BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://rabbitmq:1883';
const PUBLISH_INTERVAL_MS = 3000;

console.log(`[MQTT Sim] Connecting to broker: ${BROKER_URL}`);

const client = mqtt.connect(BROKER_URL, {
    clientId: `pulseone_simulator_${Math.random().toString(16).slice(2, 8)}`,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
});

client.on('connect', () => {
    console.log(`[MQTT Sim] Connected to ${BROKER_URL}`);

    setInterval(() => {
        const now = new Date().toISOString();
        const temp = (20 + Math.random() * 10).toFixed(2);
        const speed = (1000 + Math.random() * 500).toFixed(1);
        const current = (10 + Math.random() * 5).toFixed(2);
        const voltage = (380 + Math.random() * 20).toFixed(1);
        const status = Math.random() > 0.1 ? 'running' : 'fault';

        const payloads = [
            { topic: 'vfd/motor1/temperature', payload: JSON.stringify({ value: parseFloat(temp), unit: '°C', ts: now }) },
            { topic: 'vfd/motor1/speed', payload: JSON.stringify({ value: parseFloat(speed), unit: 'rpm', ts: now }) },
            { topic: 'vfd/motor1/current', payload: JSON.stringify({ value: parseFloat(current), unit: 'A', ts: now }) },
            { topic: 'vfd/motor1/voltage', payload: JSON.stringify({ value: parseFloat(voltage), unit: 'V', ts: now }) },
            { topic: 'vfd/motor1/status', payload: JSON.stringify({ value: status, ts: now }) },
        ];

        for (const { topic, payload } of payloads) {
            client.publish(topic, payload, { qos: 0 }, (err) => {
                if (err) console.error(`[MQTT Sim] Publish error on ${topic}: ${err.message}`);
            });
        }

        console.log(`[MQTT Sim] Published: temp=${temp}°C speed=${speed}rpm current=${current}A status=${status}`);
    }, PUBLISH_INTERVAL_MS);
});

client.on('error', (err) => {
    console.error(`[MQTT Sim] Connection error: ${err.message}`);
});

client.on('reconnect', () => {
    console.log('[MQTT Sim] Reconnecting...');
});

client.on('close', () => {
    console.log('[MQTT Sim] Disconnected');
});
