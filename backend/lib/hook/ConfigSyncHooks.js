// =============================================================================
// backend/lib/hooks/ConfigSyncHooks.js
// 설정 변경 시 Collector 동기화 자동 처리
// =============================================================================

const { getInstance: getCollectorProxy } = require('../services/CollectorProxyService');

class ConfigSyncHooks {
  constructor() {
    this.hooks = new Map();
    this.isEnabled = process.env.CONFIG_SYNC_ENABLED !== 'false';
    console.log(`🔄 ConfigSyncHooks initialized (enabled: ${this.isEnabled})`);
  }

  // =============================================================================
  // 🔥 디바이스 설정 변경 후크
  // =============================================================================

  async afterDeviceCreate(device) {
    if (!this.isEnabled) return;
    
    try {
      console.log(`🔄 Device created hook: ${device.id} (${device.name})`);
      
      const proxy = getCollectorProxy();
      
      // 1. 전체 설정 재로드 (새 디바이스 추가)
      await proxy.reloadAllConfigs();
      
      // 2. 필요시 워커 시작
      if (device.is_enabled) {
        try {
          await proxy.startDevice(device.id.toString());
          console.log(`✅ Device worker started for new device: ${device.id}`);
        } catch (error) {
          console.warn(`⚠️ Failed to start worker for new device ${device.id}:`, error.message);
        }
      }
      
    } catch (error) {
      console.error(`❌ Device create hook failed for device ${device.id}:`, error.message);
    }
  }

  async afterDeviceUpdate(oldDevice, newDevice) {
    if (!this.isEnabled) return;
    
    try {
      console.log(`🔄 Device updated hook: ${newDevice.id} (${newDevice.name})`);
      
      const proxy = getCollectorProxy();
      const deviceId = newDevice.id.toString();
      
      // 1. 설정 동기화
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
      
    } catch (error) {
      console.error(`❌ Device update hook failed for device ${newDevice.id}:`, error.message);
    }
  }

  async afterDeviceDelete(device) {
    if (!this.isEnabled) return;
    
    try {
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
      
      // 2. 전체 설정 재로드 (삭제된 디바이스 제거)
      await proxy.reloadAllConfigs();
      
    } catch (error) {
      console.error(`❌ Device delete hook failed for device ${device.id}:`, error.message);
    }
  }

  // =============================================================================
  // 🔥 알람 규칙 변경 후크
  // =============================================================================

  async afterAlarmRuleCreate(alarmRule) {
    if (!this.isEnabled) return;
    
    try {
      console.log(`🔄 Alarm rule created hook: ${alarmRule.id} (${alarmRule.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('alarm_rule', alarmRule.id, {
        action: 'create',
        target_type: alarmRule.target_type,
        target_id: alarmRule.target_id,
        is_enabled: alarmRule.is_enabled
      });
      
    } catch (error) {
      console.error(`❌ Alarm rule create hook failed for rule ${alarmRule.id}:`, error.message);
    }
  }

  async afterAlarmRuleUpdate(oldRule, newRule) {
    if (!this.isEnabled) return;
    
    try {
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
      
    } catch (error) {
      console.error(`❌ Alarm rule update hook failed for rule ${newRule.id}:`, error.message);
    }
  }

  async afterAlarmRuleDelete(alarmRule) {
    if (!this.isEnabled) return;
    
    try {
      console.log(`🔄 Alarm rule deleted hook: ${alarmRule.id} (${alarmRule.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('alarm_rule', alarmRule.id, {
        action: 'delete',
        target_type: alarmRule.target_type,
        target_id: alarmRule.target_id
      });
      
    } catch (error) {
      console.error(`❌ Alarm rule delete hook failed for rule ${alarmRule.id}:`, error.message);
    }
  }

  // =============================================================================
  // 🔥 가상포인트 변경 후크
  // =============================================================================

  async afterVirtualPointCreate(virtualPoint) {
    if (!this.isEnabled) return;
    
    try {
      console.log(`🔄 Virtual point created hook: ${virtualPoint.id} (${virtualPoint.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('virtual_point', virtualPoint.id, {
        action: 'create',
        is_enabled: virtualPoint.is_enabled,
        calculation_interval: virtualPoint.calculation_interval,
        calculation_trigger: virtualPoint.calculation_trigger
      });
      
    } catch (error) {
      console.error(`❌ Virtual point create hook failed for point ${virtualPoint.id}:`, error.message);
    }
  }

  async afterVirtualPointUpdate(oldPoint, newPoint) {
    if (!this.isEnabled) return;
    
    try {
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
      
    } catch (error) {
      console.error(`❌ Virtual point update hook failed for point ${newPoint.id}:`, error.message);
    }
  }

  async afterVirtualPointDelete(virtualPoint) {
    if (!this.isEnabled) return;
    
    try {
      console.log(`🔄 Virtual point deleted hook: ${virtualPoint.id} (${virtualPoint.name})`);
      
      const proxy = getCollectorProxy();
      await proxy.notifyConfigChange('virtual_point', virtualPoint.id, {
        action: 'delete'
      });
      
    } catch (error) {
      console.error(`❌ Virtual point delete hook failed for point ${virtualPoint.id}:`, error.message);
    }
  }

  // =============================================================================
  // 🔥 데이터포인트 변경 후크
  // =============================================================================

  async afterDataPointUpdate(oldPoint, newPoint) {
    if (!this.isEnabled) return;
    
    try {
      console.log(`🔄 Data point updated hook: ${newPoint.id} (${newPoint.point_name})`);
      
      // 데이터포인트가 변경되면 해당 디바이스 재시작
      if (newPoint.device_id) {
        const proxy = getCollectorProxy();
        await proxy.restartDevice(newPoint.device_id.toString());
        console.log(`✅ Device restarted for data point change: ${newPoint.device_id}`);
      }
      
    } catch (error) {
      console.error(`❌ Data point update hook failed for point ${newPoint.id}:`, error.message);
    }
  }

  // =============================================================================
  // 🔥 훅 등록/해제 시스템
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

  // =============================================================================
  // 🔥 유틸리티 메서드
  // =============================================================================

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
}

// 싱글톤 인스턴스
let instance = null;

function getInstance() {
  if (!instance) {
    instance = new ConfigSyncHooks();
    
    // 기본 후크들 등록
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