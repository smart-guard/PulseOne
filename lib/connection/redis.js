const redis = require('redis');
const { env } = require('../config');

const redisClient = redis.createClient({
  url: `redis://${env.REDIS_HOST || 'localhost'}:6379`,
});

redisClient.connect();
module.exports = redisClient;
