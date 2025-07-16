const redis = require('redis');
const env = require('../../../config/env');

const redisClient = redis.createClient({
  url: `redis://${env.REDIS_MAIN_HOST || 'localhost'}:6379`,
});

redisClient.connect();
module.exports = redisClient;
