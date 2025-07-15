const fs = require('fs');
const path = require('path');
const dotenv = require('dotenv');

// 환경 변수 파일 경로 우선순위
const ENV_PATHS = [
  path.resolve(__dirname, '../../config/.env'),
  path.resolve(process.cwd(), '.env')
];

// 가장 먼저 발견되는 .env 파일을 로드
for (const envPath of ENV_PATHS) {
  if (fs.existsSync(envPath)) {
    dotenv.config({ path: envPath });
    break;
  }
}

// 필수 환경변수 목록 (config/.env.example과 연동됨)
const requiredKeys = [
  'REDIS_HOST',
  'POSTGRES_HOST',
  'POSTGRES_USER',
  'POSTGRES_PASSWORD',
  'POSTGRES_DB',
  'DB_TYPE',
  'RPC_HOST',
  'RPC_PORT',
  'INFLUX_URL',
  'INFLUX_TOKEN',
  'INFLUX_ORG',
  'INFLUX_BUCKET'
];

// 누락된 항목 검사
const missing = requiredKeys.filter((key) => !process.env[key]);
if (missing.length) {
  throw new Error(`다음 환경변수가 누락되었습니다: ${missing.join(', ')}`);
}

// 환경 설정 객체 내보내기
module.exports = {
  REDIS_HOST: process.env.REDIS_HOST,
  POSTGRES_HOST: process.env.POSTGRES_HOST,
  POSTGRES_USER: process.env.POSTGRES_USER,
  POSTGRES_PASSWORD: process.env.POSTGRES_PASSWORD,
  POSTGRES_DB: process.env.POSTGRES_DB,
  DB_TYPE: process.env.DB_TYPE,
  RPC_HOST: process.env.RPC_HOST,
  RPC_PORT: parseInt(process.env.RPC_PORT, 10),
  INFLUX_URL: process.env.INFLUX_URL,
  INFLUX_TOKEN: process.env.INFLUX_TOKEN,
  INFLUX_ORG: process.env.INFLUX_ORG,
  INFLUX_BUCKET: process.env.INFLUX_BUCKET
};
