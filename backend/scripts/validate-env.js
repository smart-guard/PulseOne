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
  // 'REDIS_MAIN_PASSWORD', // ← 이것을 OPTIONAL로 이동
  'RPC_HOST',
  'RPC_PORT',
  'INFLUXDB_HOST',
  'INFLUXDB_PORT',
  'INFLUXDB_TOKEN',
  'INFLUXDB_ORG',
  'INFLUXDB_BUCKET',
  'BACKEND_PORT',
];

// 선택적 환경변수 (빈 문자열도 허용)
const OPTIONAL_KEYS = [
  'REDIS_MAIN_PASSWORD',
  'REDIS_SECONDARY_PASSWORD',
  'NODE_ENV',
  'LOG_LEVEL'
];

const stage = process.env.ENV_STAGE || 'dev';
const envPath = path.resolve(__dirname, `../../config/.env.${stage}`);

if (!fs.existsSync(envPath)) {
  console.error(`❌ .env.${stage} 파일이 존재하지 않습니다 (${envPath})`);
  process.exit(1);
}

dotenv.config({ path: envPath });

let missing = [];

// 필수 환경변수 검사 (빈 문자열도 누락으로 간주)
for (const key of REQUIRED_KEYS) {
  if (!process.env[key] || process.env[key].trim() === '') {
    missing.push(key);
  }
}

// 선택적 환경변수는 존재하기만 하면 OK (빈 문자열도 허용)
for (const key of OPTIONAL_KEYS) {
  if (process.env[key] === undefined) {
    console.warn(`⚠️ 선택적 환경변수 ${key}가 설정되지 않음`);
  }
}

if (missing.length > 0) {
  console.error('❌ 누락된 환경 변수:', missing.join(', '));
  process.exit(1);
}

console.log(`✅ ${envPath} 유효성 검사 통과`);
console.log(`   - 필수 변수: ${REQUIRED_KEYS.length}개 확인됨`);
console.log(`   - 선택적 변수: ${OPTIONAL_KEYS.length}개`);