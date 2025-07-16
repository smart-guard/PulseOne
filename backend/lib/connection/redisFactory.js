// lib/connection/redisFactory.js
const Redis = require('ioredis');
const config = require('../../config');

function createRedisClient({ host, port, password }) {
  return new Redis({ host, port, password });
}

module.exports = {
  main: () => createRedisClient(config.redis.main),
  secondary: () => createRedisClient(config.redis.secondary),
  replicas: () => config.redis.replicas.map(cfg => createRedisClient(cfg)),
};