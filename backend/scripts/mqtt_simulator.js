// scripts/mqtt_simulator.js
const mqtt = require('mqtt');

// 시뮬레이션 데이터 상태
let simData = {
    temp: 38.0,      // sensors/simulator/data -> temp
    status: "ok",    // sensors/simulator/data -> status
    direction: 1
};

const client = mqtt.connect(process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883');

client.on('connect', () => {
    console.log('🚀 MQTT Simulator connected to broker');

    // 2초마다 데이터 발행
    setInterval(() => {
        // 데이터 업데이트
        simData.temp += 0.7 * simData.direction;
        if (simData.temp > 45 || simData.temp < 35) {
            simData.direction *= -1;
            simData.status = (simData.temp > 42) ? "warning" : "ok";
        }

        const payload = JSON.stringify({
            temp: parseFloat(simData.temp.toFixed(2)),
            status: simData.status,
            timestamp: Date.now()
        });

        const topic = 'sensors/simulator/data';
        client.publish(topic, payload);

        // [디버그] 
        console.log(`[MQTT Simulator] Published to ${topic}: ${payload}`);
    }, 2000);
});

client.on('error', (err) => {
    console.error('[MQTT Simulator ERROR]', err);
});

console.log('🚀 MQTT Simulator started');
console.log(`    - Broker: ${process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883'}`);
console.log('    - Topic: sensors/simulator/data');
