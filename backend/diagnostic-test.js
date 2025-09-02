// =============================================================================
// backend/diagnostic-test.js
// 모듈 로딩 문제 진단 스크립트
// =============================================================================

console.log('🔍 PulseOne 모듈 진단 시작...\n');

// 1. ConfigManager 테스트
console.log('1️⃣ ConfigManager 테스트');
try {
    const ConfigManager = require('./lib/config/ConfigManager');
    console.log('✅ ConfigManager 로드 성공');
    
    try {
        const instance = ConfigManager.getInstance();
        console.log('✅ ConfigManager 인스턴스 생성 성공');
        
        // 기본 메서드 테스트
        const testValue = instance.get('NODE_ENV', 'test');
        console.log(`✅ ConfigManager.get() 작동: NODE_ENV = ${testValue}`);
        
    } catch (instanceError) {
        console.error('❌ ConfigManager 인스턴스 생성 실패:', instanceError.message);
        console.error('   Stack:', instanceError.stack);
    }
    
} catch (loadError) {
    console.error('❌ ConfigManager 로드 실패:', loadError.message);
    console.error('   Stack:', loadError.stack);
}

console.log('\n' + '='.repeat(50) + '\n');

// 2. CollectorProxyService 테스트
console.log('2️⃣ CollectorProxyService 테스트');
try {
    const { getInstance: getCollectorProxy } = require('./lib/services/CollectorProxyService');
    console.log('✅ CollectorProxyService 모듈 로드 성공');
    
    try {
        const proxyInstance = getCollectorProxy();
        console.log('✅ CollectorProxyService 인스턴스 생성 성공');
        
        // 기본 메서드 테스트
        const config = proxyInstance.getCollectorConfig();
        console.log(`✅ getCollectorConfig() 작동: ${config.host}:${config.port}`);
        
        const isHealthy = proxyInstance.isCollectorHealthy();
        console.log(`✅ isCollectorHealthy() 작동: ${isHealthy}`);
        
    } catch (instanceError) {
        console.error('❌ CollectorProxyService 인스턴스 생성 실패:', instanceError.message);
        console.error('   Stack:', instanceError.stack?.split('\n').slice(0, 5).join('\n'));
    }
    
} catch (loadError) {
    console.error('❌ CollectorProxyService 로드 실패:', loadError.message);
    console.error('   Stack:', loadError.stack?.split('\n').slice(0, 5).join('\n'));
}

console.log('\n' + '='.repeat(50) + '\n');

// 3. ConfigSyncHooks 테스트 (hooks 폴더)
console.log('3️⃣ ConfigSyncHooks 테스트 (hooks 폴더)');
try {
    const { getInstance: getConfigSyncHooks } = require('./lib/hooks/ConfigSyncHooks');
    console.log('✅ ConfigSyncHooks (hooks) 모듈 로드 성공');
    
    try {
        const hooksInstance = getConfigSyncHooks();
        console.log('✅ ConfigSyncHooks 인스턴스 생성 성공');
        
        // 기본 메서드 테스트
        const isEnabled = hooksInstance.isHookEnabled();
        console.log(`✅ isHookEnabled() 작동: ${isEnabled}`);
        
        const registeredHooks = hooksInstance.getRegisteredHooks();
        console.log(`✅ getRegisteredHooks() 작동: ${registeredHooks.length}개 훅`);
        
    } catch (instanceError) {
        console.error('❌ ConfigSyncHooks 인스턴스 생성 실패:', instanceError.message);
        console.error('   Stack:', instanceError.stack?.split('\n').slice(0, 5).join('\n'));
    }
    
} catch (loadError) {
    console.error('❌ ConfigSyncHooks (hooks) 로드 실패:', loadError.message);
    
    // 4. ConfigSyncHooks 테스트 (hook 폴더) - 폴백
    console.log('\n4️⃣ ConfigSyncHooks 테스트 (hook 폴더) - 폴백');
    try {
        const { getInstance: getConfigSyncHooks } = require('./lib/hook/ConfigSyncHooks');
        console.log('✅ ConfigSyncHooks (hook) 모듈 로드 성공');
        
        try {
            const hooksInstance = getConfigSyncHooks();
            console.log('✅ ConfigSyncHooks 인스턴스 생성 성공');
            
            const isEnabled = hooksInstance.isHookEnabled();
            console.log(`✅ isHookEnabled() 작동: ${isEnabled}`);
            
            const registeredHooks = hooksInstance.getRegisteredHooks();
            console.log(`✅ getRegisteredHooks() 작동: ${registeredHooks.length}개 훅`);
            
        } catch (instanceError) {
            console.error('❌ ConfigSyncHooks 인스턴스 생성 실패:', instanceError.message);
            console.error('   Stack:', instanceError.stack?.split('\n').slice(0, 5).join('\n'));
        }
        
    } catch (fallbackError) {
        console.error('❌ ConfigSyncHooks (hook) 로드도 실패:', fallbackError.message);
    }
}

console.log('\n' + '='.repeat(50) + '\n');

// 5. 의존성 체크
console.log('5️⃣ 의존성 패키지 체크');
const dependencies = ['axios', 'https'];
dependencies.forEach(dep => {
    try {
        require(dep);
        console.log(`✅ ${dep} 사용 가능`);
    } catch (error) {
        console.error(`❌ ${dep} 없음:`, error.message);
    }
});

console.log('\n🏁 진단 완료!\n');