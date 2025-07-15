// scripts/validate-env.js
const fs = require('fs');
const path = require('path');
const dotenv = require('dotenv');

const REQUIRED_KEYS = [
  'DB_TYPE',
  'POSTGRES_MAIN_DB_HOST',
  'POSTGRES_MAIN_DB_PORT',
  'POSTGRES_MAIN_DB_USER',
  'POSTGRES_MAIN_DB_PASSWORD',
  'POSTGRES_MAIN_DB_NAME',
  'REDIS_MAIN_HOST',
  'REDIS_MAIN_PORT',
  'REDIS_MAIN_PASSWORD',
  'RPC_HOST',
  'RPC_PORT',
  'INFLUXDB_HOST',
  'INFLUXDB_PORT',
  'INFLUXDB_TOKEN',
  'INFLUXDB_ORG',
  'INFLUXDB_BUCKET',
  'BACKEND_PORT',
];

const stage = process.env.ENV_STAGE || 'dev';
const envPath = path.resolve(__dirname, `../../config/.env.${stage}`);

if (!fs.existsSync(envPath)) {
  console.error(`❌ .env.${stage} 파일이 존재하지 않습니다 (${envPath})`);
  process.exit(1);
}

dotenv.config({ path: envPath });

let missing = [];

for (const key of REQUIRED_KEYS) {
  if (!process.env[key]) {
    missing.push(key);
  }
}

if (missing.length > 0) {
  console.error('❌ 누락된 환경 변수:', missing.join(', '));
  process.exit(1);
}

console.log(`✅ ${envPath} 유효성 검사 통과`);
