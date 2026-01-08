// scripts/validate-env.js - 기존 환경변수 구조에 맞춘 버전
const fs = require('fs');
const path = require('path');

console.log('🔍 PulseOne 환경변수 유효성 검사...');

// 기존 환경변수 구조에 맞춘 필수 변수들
const REQUIRED_KEYS = [
  // 데이터베이스 기본 설정
  'DATABASE_TYPE',
  'DATABASE_ENABLED',
  
  // SQLite 설정 (기본 데이터베이스)
  'SQLITE_ENABLED',
  'SQLITE_DB_PATH',
  'SQLITE_TIMEOUT_MS',
  
  // Redis 설정
  'REDIS_PRIMARY_ENABLED',
  'REDIS_PRIMARY_HOST', 
  'REDIS_PRIMARY_PORT',
  'REDIS_PRIMARY_DB',
  
  // 백엔드 기본 설정
  'NODE_ENV',
  'BACKEND_PORT'
];

// 선택적 환경변수 (있으면 좋은 것들)
const OPTIONAL_KEYS = [
  // PostgreSQL (선택적)
  'POSTGRESQL_ENABLED',
  'POSTGRESQL_HOST',
  'POSTGRESQL_PORT',
  'POSTGRESQL_DATABASE',
  'POSTGRESQL_USERNAME',
  'POSTGRESQL_PASSWORD',
  
  // MySQL (선택적)
  'MYSQL_ENABLED',
  'MYSQL_HOST',
  'MYSQL_PORT',
  'MYSQL_DATABASE',
  
  // Redis 추가 설정
  'REDIS_PRIMARY_PASSWORD',
  'REDIS_POOL_SIZE',
  'REDIS_KEY_PREFIX',
  'REDIS_TEST_MODE',
  
  // 기타
  'LOG_LEVEL',
  'ENV_STAGE',
  'BUILD_TYPE'
];

let missing = [];
let present = [];
let warnings = [];

console.log('📋 환경변수 로딩 상태:');
console.log(`   - Working Directory: ${process.cwd()}`);
console.log(`   - NODE_ENV: ${process.env.NODE_ENV || '(not set)'}`);
console.log(`   - ENV_STAGE: ${process.env.ENV_STAGE || '(not set)'}`);

// 필수 환경변수 검사
console.log('\n🔍 필수 환경변수 검사:');
for (const key of REQUIRED_KEYS) {
  if (!process.env[key] || process.env[key].trim() === '') {
    missing.push(key);
    console.log(`   ❌ ${key}: 누락됨`);
  } else {
    present.push(key);
    // 값이 너무 길면 자르기
    const value = process.env[key].length > 50 
      ? process.env[key].substring(0, 47) + '...'
      : process.env[key];
    console.log(`   ✅ ${key}: ${value}`);
  }
}

// 선택적 환경변수 확인
console.log('\n📝 선택적 환경변수 상태:');
let optional_present = [];
for (const key of OPTIONAL_KEYS) {
  if (process.env[key] !== undefined && process.env[key] !== '') {
    optional_present.push(key);
    const value = process.env[key].length > 30 
      ? process.env[key].substring(0, 27) + '...'
      : process.env[key];
    console.log(`   ✅ ${key}: ${value}`);
  } else {
    console.log(`   ⚪ ${key}: (미설정)`);
  }
}

// 데이터베이스 타입별 검증
console.log('\n🗄️ 데이터베이스 설정 검증:');
const dbType = process.env.DATABASE_TYPE;
console.log(`   📊 DATABASE_TYPE: ${dbType}`);

if (dbType === 'SQLITE') {
  if (process.env.SQLITE_ENABLED === 'true') {
    console.log('   ✅ SQLite 설정 활성화됨');
    if (process.env.SQLITE_DB_PATH) {
      console.log(`   📁 SQLite 경로: ${process.env.SQLITE_DB_PATH}`);
    }
  } else {
    warnings.push('DATABASE_TYPE이 SQLITE인데 SQLITE_ENABLED가 false입니다');
  }
} else if (dbType === 'POSTGRESQL') {
  if (process.env.POSTGRESQL_ENABLED === 'true') {
    console.log('   ✅ PostgreSQL 설정 활성화됨');
  } else {
    warnings.push('DATABASE_TYPE이 POSTGRESQL인데 POSTGRESQL_ENABLED가 false입니다');
  }
}

// Redis 연결 정보 확인
console.log('\n🔴 Redis 설정 검증:');
if (process.env.REDIS_PRIMARY_ENABLED === 'true') {
  console.log('   ✅ Redis 활성화됨');
  console.log(`   🌐 Redis 연결: ${process.env.REDIS_PRIMARY_HOST}:${process.env.REDIS_PRIMARY_PORT}`);
  console.log(`   📁 Redis DB: ${process.env.REDIS_PRIMARY_DB}`);
  if (process.env.REDIS_KEY_PREFIX) {
    console.log(`   🏷️ Key Prefix: ${process.env.REDIS_KEY_PREFIX}`);
  }
} else {
  warnings.push('REDIS_PRIMARY_ENABLED가 false로 설정되어 있습니다');
}

// 결과 출력
console.log('\n📊 검증 결과:');
console.log(`   - 필수 변수: ${present.length}/${REQUIRED_KEYS.length}개 확인됨`);
console.log(`   - 선택적 변수: ${optional_present.length}/${OPTIONAL_KEYS.length}개 설정됨`);

// 경고 출력
if (warnings.length > 0) {
  console.log('\n⚠️ 경고사항:');
  warnings.forEach(warning => {
    console.log(`   - ${warning}`);
  });
}

// 누락된 필수 변수 처리
if (missing.length > 0) {
  console.log('\n❌ 누락된 필수 환경변수:');
  missing.forEach(key => {
    console.log(`   - ${key}`);
  });
  
  console.log('\n💡 해결방법:');
  console.log('   1. config/.env 파일 확인');
  console.log('   2. config/database.env 파일 확인');
  console.log('   3. docker-compose.dev.yml의 env_file 섹션 확인');
  
  // 개발환경에서는 경고만 출력하고 계속 진행
  if (process.env.NODE_ENV === 'development') {
    console.log('\n⚠️ 개발환경에서는 누락된 환경변수가 있어도 계속 진행합니다.');
    console.log('✅ 환경변수 검증 완료 (개발모드 - 경고 포함)');
    process.exit(0);
  } else {
    console.log('\n❌ 운영환경에서는 모든 필수 환경변수가 필요합니다.');
    process.exit(1);
  }
} else {
  console.log('\n🎉 모든 필수 환경변수가 올바르게 설정되었습니다!');
  console.log('✅ 환경변수 검증 완료');
  process.exit(0);
}