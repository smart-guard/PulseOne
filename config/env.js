const path = require('path');
const dotenvSafe = require('dotenv-safe');

// 현재 실행 환경: default = 'dev'
const ENV_STAGE = process.env.ENV_STAGE || 'dev';

// 경로 정의
const envPath = path.resolve(__dirname, `../../config/.env.${ENV_STAGE}`);
const examplePath = path.resolve(__dirname, '../../config/.env.example');

// 환경 변수 로드 및 검증
dotenvSafe.config({
  path: envPath,
  example: examplePath,
  allowEmptyValues: false,
});

// 필요한 값 변환 처리
const toInt = (v, def) => (v ? parseInt(v, 10) : def);

// 설정 객체 내보내기
module.exports = {
  ENV_STAGE,

  // Redis
  REDIS_MAIN_HOST: process.env.REDIS_MAIN_HOST,
  REDIS_MAIN_PORT: toInt(process.env.REDIS_MAIN_PORT, 6379),
  REDIS_MAIN_PASSWORD: process.env.REDIS_MAIN_PASSWORD || '',
  REDIS_REPLICAS: process.env.REDIS_REPLICAS?.split(',') || [],
  REDIS_SECONDARY_HOST: process.env.REDIS_SECONDARY_HOST,
  REDIS_SECONDARY_PORT: toInt(process.env.REDIS_SECONDARY_PORT, 6379),
  REDIS_SECONDARY_PASSWORD: process.env.REDIS_SECONDARY_PASSWORD || '',

  // PostgreSQL
  POSTGRES_MAIN_DB_HOST: process.env.POSTGRES_MAIN_DB_HOST,
  POSTGRES_MAIN_DB_PORT: toInt(process.env.POSTGRES_MAIN_DB_PORT, 5432),
  POSTGRES_MAIN_DB_USER: process.env.POSTGRES_MAIN_DB_USER,
  POSTGRES_MAIN_DB_PASSWORD: process.env.POSTGRES_MAIN_DB_PASSWORD,
  POSTGRES_MAIN_DB_NAME: process.env.POSTGRES_MAIN_DB_NAME,

  POSTGRES_LOG_DB_HOST: process.env.POSTGRES_LOG_DB_HOST,
  POSTGRES_LOG_DB_PORT: toInt(process.env.POSTGRES_LOG_DB_PORT, 5432),
  POSTGRES_LOG_DB_USER: process.env.POSTGRES_LOG_DB_USER,
  POSTGRES_LOG_DB_PASSWORD: process.env.POSTGRES_LOG_DB_PASSWORD,
  POSTGRES_LOG_DB_NAME: process.env.POSTGRES_LOG_DB_NAME,

  // InfluxDB
  INFLUXDB_HOST: process.env.INFLUXDB_HOST,
  INFLUXDB_PORT: toInt(process.env.INFLUXDB_PORT, 8086),
  INFLUXDB_TOKEN: process.env.INFLUXDB_TOKEN,
  INFLUXDB_ORG: process.env.INFLUXDB_ORG,
  INFLUXDB_BUCKET: process.env.INFLUXDB_BUCKET,

  // RPC
  RPC_HOST: process.env.RPC_HOST,
  RPC_PORT: toInt(process.env.RPC_PORT, 4000),

  // RabbitMQ
  RABBITMQ_HOST: process.env.RABBITMQ_HOST,
  RABBITMQ_PORT: toInt(process.env.RABBITMQ_PORT, 5672),
  RABBITMQ_MANAGEMENT_PORT: toInt(process.env.RABBITMQ_MANAGEMENT_PORT, 15672),

  // App
  BACKEND_PORT: toInt(process.env.BACKEND_PORT, 3000),

  // Optional
  DB_TYPE: process.env.DB_TYPE || 'postgres',
  SQLITE_PATH: process.env.SQLITE_PATH || './db.sqlite',
};
