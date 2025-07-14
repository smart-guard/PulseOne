require('dotenv').config();
const express = require('express');
const app = express();

app.get('/health', (req, res) => res.json({ status: 'ok', ts: new Date() }));

// 예시: Redis 연결
const redis = require('redis');
const client = redis.createClient({ url: `redis://${process.env.REDIS_HOST}:6379` });
client.connect().then(() => console.log('Redis connected')).catch(console.error);

app.listen(3000, '0.0.0.0', () => {
    console.log('Backend listening on port 3000');
  });
