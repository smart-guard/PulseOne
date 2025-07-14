// app.js
require('dotenv').config();
const express = require('express');
const redis = require('redis');

const app = express();

app.get('/health', (req, res) => res.json({ status: 'ok', ts: new Date() }));

// Redis 클라이언트 연결
const client = redis.createClient({ url: `redis://${process.env.REDIS_HOST}:6379` });

client.connect().then(() => console.log('Redis connected')).catch(console.error);

// 테스트 및 외부에서 불러올 수 있도록 export
module.exports = app;
