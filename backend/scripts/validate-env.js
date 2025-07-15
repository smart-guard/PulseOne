// backend/scripts/validate-env.js
const fs = require('fs');
const path = require('path');
const dotenv = require('dotenv');

const ENV_PATH = path.resolve(__dirname, '../../config/.env');
const EXAMPLE_PATH = path.resolve(__dirname, '../../config/.env.example');

function parseEnvFile(filePath) {
  if (!fs.existsSync(filePath)) return {};
  const content = fs.readFileSync(filePath, 'utf-8');
  return dotenv.parse(content);
}

const actualEnv = parseEnvFile(ENV_PATH);
const exampleEnv = parseEnvFile(EXAMPLE_PATH);

let missing = [];

for (const key in exampleEnv) {
  if (!(key in actualEnv)) {
    missing.push(key);
  }
}

if (missing.length > 0) {
  console.error('\x1b[31m[ENV ERROR]\x1b[0m 다음 환경변수가 누락되었습니다:');
  missing.forEach(k => console.error(`  - ${k}`));
  process.exit(1);
} else {
  console.log('\x1b[32m[ENV OK]\x1b[0m .env 파일에 모든 필수 환경변수가 존재합니다.');
}
