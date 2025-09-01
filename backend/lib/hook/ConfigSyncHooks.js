// =============================================================================
// backend/lib/hooks/ConfigSyncHooks.js
// 설정 변경 시 Collector 동기화 자동 처리 (에러 처리 개선 버전)
// =============================================================================

const { getInstance: getCollectorProxy } = require('../services/CollectorProxyService');

class ConfigSyncHooks {
  constructor() {
    this.hooks = new Map();
    this.isEnabled = process.env.CONFIG_SYNC_ENABLED !== 'false';
    
    // 🔥 NEW: 에러 처리 정책 설정
    this.errorPolicy = {
      // 'throw' | 'log' | 'ignore'
      onSyncFailure: process.env.SYNC_ERROR_POLICY || 'throw',
      
      // Critical operations은 항상 throw (데이터 불일치 방지)
      criticalOperations: ['device_update', 'device_delete'],
      
      // 재시도 설정
      maxRetries: 2,
      retryDelayMs: 1000
    };
    
    console.log(`🔄 ConfigSyncHooks initialized (enabled: ${this.isEnabled}, errorPolicy: ${this.errorPolicy.onSyncFailure})`);
  }

  // =============================================================================
  // 🔥 NEW: 중앙집중식 에러 처리 메소드
  // =============================================================================

  async handleSyncOperation(operationType, operation, ...args) {
    if (!this.isEnabled) return { success: true, skipped: true };
    
    const isCritical = this.errorPolicy.criticalOperations.includes(operationType);
    
    for (let attempt = 0; attempt <= this.errorPolicy.maxRetries; attempt++) {
      try {
        await operation(...args);
        return { success: true, attempts: attempt + 1 };
        
      } catch (error) {
        console.error(`❌ Sync operation failed (attempt ${attempt + 1}): ${operationType}`, error.message);
        
        // 마지막 시도가 실패한 경우
        if (attempt === this.errorPolicy.maxRetries) {
          const errorInfo = {
            operation: operationType,
            error: error.message,
            attempts: attempt + 1
          };
          
          // 에러 처리 정책에 따라 처리
          if (this.errorPolicy.onSyncFailure === 'throw' || isCritical) {
            // Critical 작업이거나 정책이 throw인 경우 예외 전파
            const syncError = new Error(`Sync failed: ${operationType} - ${error.message}`);
            syncError.name = 'SyncError';
            syncError.details = errorInfo;
            throw syncError;
            
          } else if (this.errorPolicy.onSyncFailure === 'log') {
            // 로그만 남기고 계속 진행
            console.warn(`⚠️ Sync failed but continuing: ${JSON.stringify(errorInfo)}`);
            return { success: false, error: errorInfo, continued: true };
            
          } else {
            // 'ignore' - 완전히 무시
            return { success: false, error: errorInfo, ignored: true };
          }
        }
        
        // 재시도 대기
        if (attempt < this.errorPolicy.maxRetries) {
          await new Promise(resolve => setTimeout(resolve, this.errorPolicy.retryDelayMs));
        }
      }
    }
  }

  // =============================================================================
  // 🔥 개선된 디바이스 설정 변경 후크들
  // =============================================================================

  async afterDeviceCreate(device) {
    return await this.handleSyncOperation('device_create', async () => {
      console.log(`🔄 Device created hook: ${device.id} (${device.name})`);
      
      const proxy = getCollectorProxy();
      
      // 1. 전체 설정 재로드
      await proxy.reloadAllConfigs();
      
      // 2. 필요시 워커 시작
      if (device.is_enabled) {
        try {
          await proxy.startDevice(device.id.toString());
          console.log(`✅ Device worker started for new device: ${device.id}`);
        } catch (error) {
          // Worker 시작 실패는 non-critical
          console.warn(`⚠️ Failed to start worker for new device ${device.id}:`, error.message);
        }
      }
    });
  }

  async afterDeviceUpdate(oldDevice, newDevice) {
    // 🚨 Critical Operation - 항상 성공해야 함
    return await this.handleSyncOperation('device_update', async () => {
      console.log(`🔄 Device updated hook: ${newDevice.id} (${newDevice.name})`);
      
      const proxy = getCollectorProxy();
      const deviceId = newDevice.id.toString();
      
      // 1. 설정 동기화 (Critical)
      await proxy.syncDeviceSettings(deviceId, {
        name: newDevice.name,
        protocol_type: newDevice.protocol_type,
        endpoint: newDevice.endpoint,
        polling_interval: newDevice.polling_interval,
        is_enabled: newDevice.is_enabled,
        settings: newDevice.settings
      });
      
      // 2. 상태 변경 처리
      if (oldDevice.is_enabled !== newDevice.is_enabled) {
        if (newDevice.is_enabled) {
          await proxy.startDevice(deviceId);
          console.log(`✅ Device worker started: ${deviceId}`);
        } else {
          await proxy.stopDevice(deviceId);
          console.log(`✅ Device worker stopped: ${deviceId}`);
        }
      } else if (newDevice.is_enabled) {
        // 활성 상태에서 설정만 변경된 경우 재시작
        await proxy.restartDevice(deviceId);
        console.log(`✅ Device worker restarted for config change: ${deviceId}`);
      }
    });
  }

  async afterDeviceDelete(device) {
    // 🚨 Critical Operation - 항상 성공해야 함
    return await this.handleSyncOperation('device_delete', async () => {
      console.log(`🔄 Device deleted hook: ${device.id} (${device.name})`);
      
      const proxy = getCollectorProxy();
      const deviceId = device.id.toString();
      
      // 1. 워커 중지
      try {
        await proxy.stopDevice(deviceId);
        console.log(`✅ Device worker stopped for deleted device: ${deviceId}`);
      } catch (error) {
        // 이미 중지되었거나 존재하지 않는 경우 무시
        console.log(`ℹ️ Device worker was not running: ${deviceId}`);
      }
      
      // 2. 전체 설정 재로드
      await proxy.reloadAllConfigs();
    });
  }

  // =============================================================================
  // 🔥 알람 규칙 변경 후크들 (Non-Critical)
  // =============================================================================

  async afterAlarmRuleCreate(alarmRule) {
    return await this.handleSyncOperation('alarm_create', async () => {
      console.log(`🔄 Alarm rule created hook: ${alarmRule.id} (${alarmRule.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('alarm_rule', alarmRule.id, {
        action: 'create',
        target_type: alarmRule.target_type,
        target_id: alarmRule.target_id,
        is_enabled: alarmRule.is_enabled
      });
    });
  }

  async afterAlarmRuleUpdate(oldRule, newRule) {
    return await this.handleSyncOperation('alarm_update', async () => {
      console.log(`🔄 Alarm rule updated hook: ${newRule.id} (${newRule.name})`);
      
      const proxy = getCollectorProxy();
      
      // 변경사항 분석
      const changes = {
        action: 'update',
        target_type: newRule.target_type,
        target_id: newRule.target_id,
        enabled_changed: oldRule.is_enabled !== newRule.is_enabled,
        condition_changed: oldRule.condition_config !== newRule.condition_config,
        severity_changed: oldRule.severity !== newRule.severity
      };
      
      await proxy.notifyConfigChange('alarm_rule', newRule.id, changes);
    });
  }

  async afterAlarmRuleDelete(alarmRule) {
    return await this.handleSyncOperation('alarm_delete', async () => {
      console.log(`🔄 Alarm rule deleted hook: ${alarmRule.id} (${alarmRule.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('alarm_rule', alarmRule.id, {
        action: 'delete',
        target_type: alarmRule.target_type,
        target_id: alarmRule.target_id
      });
    });
  }

  // =============================================================================
  // 🔥 가상포인트 변경 후크들 (Non-Critical)
  // =============================================================================

  async afterVirtualPointCreate(virtualPoint) {
    return await this.handleSyncOperation('virtual_point_create', async () => {
      console.log(`🔄 Virtual point created hook: ${virtualPoint.id} (${virtualPoint.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('virtual_point', virtualPoint.id, {
        action: 'create',
        is_enabled: virtualPoint.is_enabled,
        calculation_interval: virtualPoint.calculation_interval,
        calculation_trigger: virtualPoint.calculation_trigger
      });
    });
  }

  async afterVirtualPointUpdate(oldPoint, newPoint) {
    return await this.handleSyncOperation('virtual_point_update', async () => {
      console.log(`🔄 Virtual point updated hook: ${newPoint.id} (${newPoint.name})`);
      
      const proxy = getCollectorProxy();
      
      const changes = {
        action: 'update',
        enabled_changed: oldPoint.is_enabled !== newPoint.is_enabled,
        formula_changed: oldPoint.formula !== newPoint.formula,
        interval_changed: oldPoint.calculation_interval !== newPoint.calculation_interval,
        trigger_changed: oldPoint.calculation_trigger !== newPoint.calculation_trigger
      };
      
      await proxy.notifyConfigChange('virtual_point', newPoint.id, changes);
    });
  }

  async afterVirtualPointDelete(virtualPoint) {
    return await this.handleSyncOperation('virtual_point_delete', async () => {
      console.log(`🔄 Virtual point deleted hook: ${virtualPoint.id} (${virtualPoint.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('virtual_point', virtualPoint.id, {
        action: 'delete'
      });
    });
  }

  // =============================================================================
  // 🔥 데이터포인트 변경 후크 (Non-Critical)
  // =============================================================================

  async afterDataPointUpdate(oldPoint, newPoint) {
    return await this.handleSyncOperation('datapoint_update', async () => {
      console.log(`🔄 Data point updated hook: ${newPoint.id} (${newPoint.point_name})`);
      
      // 데이터포인트가 변경되면 해당 디바이스 재시작
      if (newPoint.device_id) {
        const proxy = getCollectorProxy();
        await proxy.restartDevice(newPoint.device_id.toString());
        console.log(`✅ Device restarted for data point change: ${newPoint.device_id}`);
      }
    });
  }

  // =============================================================================
  // 🔥 설정 관리 메소드들 (기존 + 개선)
  // =============================================================================

  setErrorPolicy(policy) {
    if (['throw', 'log', 'ignore'].includes(policy)) {
      this.errorPolicy.onSyncFailure = policy;
      console.log(`🔄 Sync error policy changed to: ${policy}`);
    } else {
      console.error(`❌ Invalid error policy: ${policy}. Must be 'throw', 'log', or 'ignore'`);
    }
  }

  getErrorPolicy() {
    return { ...this.errorPolicy };
  }

  // 기존 메소드들 유지
  setEnabled(enabled) {
    this.isEnabled = enabled;
    console.log(`🔄 ConfigSyncHooks ${enabled ? 'enabled' : 'disabled'}`);
  }

  isHookEnabled() {
    return this.isEnabled;
  }

  getRegisteredHooks() {
    return Array.from(this.hooks.keys());
  }

  clearAllHooks() {
    this.hooks.clear();
    console.log('🔄 All hooks cleared');
  }

  // =============================================================================
  // 기존 후크 등록 시스템 유지
  // =============================================================================
  
  registerHook(entityType, eventType, callback) {
    const key = `${entityType}.${eventType}`;
    if (!this.hooks.has(key)) {
      this.hooks.set(key, []);
    }
    this.hooks.get(key).push(callback);
    console.log(`🎣 Hook registered: ${key}`);
  }

  async executeHooks(entityType, eventType, ...args) {
    const key = `${entityType}.${eventType}`;
    const callbacks = this.hooks.get(key) || [];
    
    for (const callback of callbacks) {
      try {
        await callback(...args);
      } catch (error) {
        console.error(`❌ Hook execution failed for ${key}:`, error.message);
      }
    }
  }
}

// 싱글톤 인스턴스
let instance = null;

function getInstance() {
  if (!instance) {
    instance = new ConfigSyncHooks();
    
    // 기본 후크들 등록 (모든 원본 후크 포함)
    instance.registerHook('device', 'create', instance.afterDeviceCreate.bind(instance));
    instance.registerHook('device', 'update', instance.afterDeviceUpdate.bind(instance));
    instance.registerHook('device', 'delete', instance.afterDeviceDelete.bind(instance));
    
    instance.registerHook('alarm_rule', 'create', instance.afterAlarmRuleCreate.bind(instance));
    instance.registerHook('alarm_rule', 'update', instance.afterAlarmRuleUpdate.bind(instance));
    instance.registerHook('alarm_rule', 'delete', instance.afterAlarmRuleDelete.bind(instance));
    
    instance.registerHook('virtual_point', 'create', instance.afterVirtualPointCreate.bind(instance));
    instance.registerHook('virtual_point', 'update', instance.afterVirtualPointUpdate.bind(instance));
    instance.registerHook('virtual_point', 'delete', instance.afterVirtualPointDelete.bind(instance));
    
    instance.registerHook('data_point', 'update', instance.afterDataPointUpdate.bind(instance));
  }
  
  return instance;
}

module.exports = {
  getInstance,
  ConfigSyncHooks
};