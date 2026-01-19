/**
 * mock_gateway.js
 * Simulates an Export Gateway for verification purposes in Docker.
 * - Publishes heartbeats to 'gateway:status:{id}'
 * - Subscribes to 'cmd:gateway:{id}' and 'config:reload'
 */
const redis = require('redis');

// Configuration
const GATEWAY_ID = process.env.GATEWAY_ID || 1;
const HEARTBEAT_INTERVAL = 3000;
const REDIS_URL = process.env.REDIS_URL || 'redis://redis:6379';

async function startMockGateway() {
    console.log(`🚀 Starting Mock Gateway [ID: ${GATEWAY_ID}]...`);
    console.log(`📡 Connecting to Redis: ${REDIS_URL}`);

    const client = redis.createClient({ url: REDIS_URL });
    const subscriber = client.duplicate();

    client.on('error', (err) => console.error('Redis Client Error', err));
    subscriber.on('error', (err) => console.error('Redis Subscriber Error', err));

    await client.connect();
    await subscriber.connect();

    console.log(`✅ Redis Connected.`);

    // 1. Heartbeat Loop
    setInterval(async () => {
        const status = {
            id: GATEWAY_ID,
            status: 'online',
            cpu: (Math.random() * 20 + 5).toFixed(1),
            ram: (Math.random() * 100 + 400).toFixed(1),
            ups: (Math.random() * 1000 + 5000).toFixed(0),
            last_heartbeat: new Date().toISOString()
        };

        await client.set(`gateway:status:${GATEWAY_ID}`, JSON.stringify(status), {
            EX: 30
        });
        console.log(`💓 Heartbeat sent [ID: ${GATEWAY_ID}] - ${new Date().toLocaleTimeString()}`);
    }, HEARTBEAT_INTERVAL);

    // 2. Command Subscription
    const channels = [`cmd:gateway:${GATEWAY_ID}`, 'config:reload', 'target:reload'];
    console.log(`🎧 Subscribing to channels: ${channels.join(', ')}`);

    await subscriber.subscribe(channels, (message, channel) => {
        console.log(`\n🔔 [${channel}] COMMAND RECEIVED:`);
        try {
            const cmd = JSON.parse(message);
            console.log(JSON.stringify(cmd, null, 2));

            if (cmd.command === 'DEPLOY' || channel === 'config:reload') {
                console.log('✅ Configuration reload triggered!');
            }
        } catch (e) {
            console.log('Raw message:', message);
        }
    });

    process.on('SIGINT', async () => {
        await client.quit();
        await subscriber.quit();
        console.log('Mock Gateway Stopped.');
        process.exit();
    });
}

startMockGateway().catch(err => {
    console.error('❌ Failed to start Mock Gateway:', err);
    process.exit(1);
});
