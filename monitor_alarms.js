const Redis = require('ioredis');
const redis = new Redis({
    host: 'localhost',
    port: 6379
});

console.log('📡 Starting Redis Monitor...');

redis.psubscribe('*', (err, count) => {
    if (err) {
        console.error('❌ Failed to subscribe: %s', err.message);
        process.exit(1);
    } else {
        console.log('✅ Subscribed to all channels. Waiting for messages...');
    }
});

redis.on('pmessage', (pattern, channel, message) => {
    if (channel.includes('alarm')) {
        console.log(`\n🔔 Alarm Event on channel [${channel}]:`);
        try {
            const data = JSON.parse(message);
            console.log(JSON.stringify(data, null, 2));
        } catch (e) {
            console.log(message);
        }
    }
});

// 자동 종료 (30초 후)
setTimeout(() => {
    console.log('\n👋 Monitoring complete.');
    process.exit(0);
}, 30000);
