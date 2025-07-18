#!/usr/bin/env node

/**
 * 🔧 PulseOne 초기화 헬퍼 스크립트
 * 기존 앱과 독립적으로 실행 가능한 간단한 초기화 도구
 */

const path = require('path');
require('dotenv').config({ path: path.join(__dirname, '../../config/.env.dev') });

const DatabaseInitializer = require('./database-initializer');

/**
 * 🎯 명령행 인수 처리
 */
const args = process.argv.slice(2);
const command = args[0];

/**
 * 📋 사용법 표시
 */
function showUsage() {
    console.log(`
🔧 PulseOne 초기화 헬퍼 v2.0

사용법:
  node scripts/init-helper.js <command>

명령어:
  status    현재 초기화 상태 확인
  init      전체 자동 초기화 실행
  menu      인터랙티브 메뉴 표시
  backup    백업 생성
  reset     데이터베이스 재설정 (주의!)

예시:
  npm run db:status     # 상태 확인
  npm run db:init       # 인터랙티브 메뉴
  node scripts/init-helper.js init  # 자동 초기화
    `);
}

/**
 * 🔍 상태 확인
 */
async function checkStatus() {
    console.log('🔍 데이터베이스 상태 확인 중...\n');
    
    const initializer = new DatabaseInitializer();
    await initializer.checkDatabaseStatus();
    
    console.log('📊 상태 요약:');
    console.log(`   🐘 PostgreSQL 시스템 테이블: ${initializer.initStatus.systemTables ? '✅' : '❌'}`);
    console.log(`   🏢 테넌트 스키마: ${initializer.initStatus.tenantSchemas ? '✅' : '❌'}`);
    console.log(`   📊 샘플 데이터: ${initializer.initStatus.sampleData ? '✅' : '❌'}`);
    console.log(`   🔴 Redis: ${initializer.initStatus.redis ? '✅' : '❌'}`);
    console.log(`   📈 InfluxDB: ${initializer.initStatus.influxdb ? '✅' : '❌'}`);
    console.log(`   🎯 전체 초기화: ${initializer.isFullyInitialized() ? '✅ 완료' : '❌ 필요'}\n`);
}

/**
 * 🚀 자동 초기화
 */
async function autoInit() {
    console.log('🚀 자동 초기화를 시작합니다...\n');
    
    const initializer = new DatabaseInitializer();
    
    // 상태 확인
    await initializer.checkDatabaseStatus();
    
    if (initializer.isFullyInitialized() && process.env.SKIP_IF_INITIALIZED === 'true') {
        console.log('✅ 데이터베이스가 이미 초기화되어 있습니다.');
        return;
    }
    
    // 초기화 실행
    await initializer.performInitialization();
    console.log('🎉 자동 초기화가 완료되었습니다!');
}

/**
 * 💾 백업 생성
 */
async function createBackup() {
    console.log('💾 백업을 생성합니다...\n');
    
    const initializer = new DatabaseInitializer();
    await initializer.createBackup();
    
    console.log('✅ 백업이 완료되었습니다!');
}

/**
 * 🗑️ 데이터베이스 재설정
 */
async function resetDatabase() {
    console.log('⚠️  데이터베이스 재설정은 모든 데이터를 삭제합니다!');
    console.log('이 작업은 인터랙티브 메뉴를 통해 수행하세요.');
    console.log('실행: npm run db:init');
}

/**
 * 🎛️ 인터랙티브 메뉴
 */
async function showMenu() {
    const initializer = new DatabaseInitializer();
    await initializer.run();
}

/**
 * 🚀 메인 실행
 */
async function main() {
    try {
        switch (command) {
            case 'status':
                await checkStatus();
                break;
                
            case 'init':
                await autoInit();
                break;
                
            case 'menu':
                await showMenu();
                break;
                
            case 'backup':
                await createBackup();
                break;
                
            case 'reset':
                await resetDatabase();
                break;
                
            default:
                showUsage();
        }
    } catch (error) {
        console.error('❌ 오류 발생:', error.message);
        if (process.env.LOG_LEVEL === 'debug') {
            console.error(error.stack);
        }
        process.exit(1);
    }
}

// 직접 실행 시에만 실행
if (require.main === module) {
    main();
}

module.exports = {
    checkStatus,
    autoInit,
    createBackup,
    showMenu
};