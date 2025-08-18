import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/alarm-settings.css';

// 🔥 기존에 생성된 알람 룰들의 설정을 조정하는 인터페이스
interface ExistingAlarmRule {
  id: string;
  name: string;
  description: string;
  sourceType: 'data_point' | 'virtual_point' | 'device_status';
  sourceName: string;
  deviceName?: string;
  category: string;
  conditionType: 'threshold' | 'range' | 'deviation' | 'pattern' | 'timeout';
  
  // 🔥 조정 가능한 설정들 (읽기전용 기본 정보 + 수정 가능한 설정들)
  settings: {
    // 임계값 설정 (수정 가능)
    highHighLimit?: number;
    highLimit?: number;
    lowLimit?: number;
    lowLowLimit?: number;
    deadband?: number;
    targetValue?: number;
    tolerance?: number;
    timeWindow?: number;
    pattern?: string;
    
    // 동작 설정 (수정 가능)
    priority: 'low' | 'medium' | 'high' | 'critical';
    severity: 1 | 2 | 3 | 4 | 5;
    autoAcknowledge: boolean;
    autoReset: boolean;
    suppressDuration: number; // 중복 억제 시간 (초)
    maxOccurrences: number;   // 최대 발생 횟수
    escalationTime: number;   // 에스컬레이션 시간 (분)
    
    // 알림 설정 (수정 가능)
    emailEnabled: boolean;
    emailRecipients: string[];
    smsEnabled: boolean;
    smsRecipients: string[];
    soundEnabled: boolean;
    popupEnabled: boolean;
    webhookEnabled: boolean;
    webhookUrl: string;
    
    // 메시지 설정 (수정 가능)
    messageTemplate: string;
    emailTemplate: string;
    
    // 스케줄 설정 (수정 가능)
    schedule: {
      type: 'always' | 'business_hours' | 'custom';
      startTime?: string;
      endTime?: string;
      weekdays?: number[];
      holidays?: string[];
    };
    
    // 상태 (수정 가능)
    isEnabled: boolean;
  };
  
  // 🔥 읽기전용 정보 (통계 및 메타데이터)
  readonly: {
    triggerCount: number;
    lastTriggered?: Date;
    acknowledgeCount: number;
    avgResponseTime?: number; // 평균 대응 시간 (분)
    lastModified: Date;
    modifiedBy: string;
    createdAt: Date;
    createdBy: string;
  };
}

// 설정 그룹 인터페이스
interface SettingsGroup {
  id: string;
  name: string;
  description: string;
  rules: ExistingAlarmRule[];
}

const AlarmSettings: React.FC = () => {
  const [alarmRules, setAlarmRules] = useState<ExistingAlarmRule[]>([]);
  const [settingsGroups, setSettingsGroups] = useState<SettingsGroup[]>([]);
  const [selectedRule, setSelectedRule] = useState<ExistingAlarmRule | null>(null);
  const [showSettingsModal, setShowSettingsModal] = useState(false);
  const [showBulkModal, setShowBulkModal] = useState(false);
  const [selectedRules, setSelectedRules] = useState<string[]>([]);
  
  // 필터 상태
  const [filterCategory, setFilterCategory] = useState('all');
  const [filterPriority, setFilterPriority] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  const [viewMode, setViewMode] = useState<'list' | 'groups'>('list');

  // 설정 수정 상태
  const [pendingChanges, setPendingChanges] = useState<Record<string, Partial<ExistingAlarmRule['settings']>>>({});
  const [hasUnsavedChanges, setHasUnsavedChanges] = useState(false);

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 🔥 기존에 생성된 알람 룰들의 목업 데이터
    const mockRules: ExistingAlarmRule[] = [
      {
        id: 'alarm_001',
        name: '온도 4단계 경보',
        description: '생산라인 온도 4단계 임계값 모니터링',
        sourceType: 'data_point',
        sourceName: 'Temperature Sensor 1',
        deviceName: 'PLC-001',
        category: 'Safety',
        conditionType: 'threshold',
        settings: {
          highHighLimit: 100,
          highLimit: 80,
          lowLimit: 10,
          lowLowLimit: 5,
          deadband: 2,
          priority: 'critical',
          severity: 5,
          autoAcknowledge: false,
          autoReset: true,
          suppressDuration: 300,
          maxOccurrences: 10,
          escalationTime: 15,
          emailEnabled: true,
          emailRecipients: ['safety@company.com', 'manager@company.com'],
          smsEnabled: true,
          smsRecipients: ['+82-10-1234-5678'],
          soundEnabled: true,
          popupEnabled: true,
          webhookEnabled: true,
          webhookUrl: 'https://hooks.slack.com/services/safety',
          messageTemplate: '🌡️ 온도 알람: {sourceName} = {currentValue}°C (임계값: {threshold})',
          emailTemplate: '온도 센서에서 {severity} 수준의 알람이 발생했습니다.\n\n상세정보:\n- 센서: {sourceName}\n- 현재값: {currentValue}°C\n- 임계값: {threshold}°C\n- 발생시간: {timestamp}',
          schedule: {
            type: 'always'
          },
          isEnabled: true
        },
        readonly: {
          triggerCount: 127,
          lastTriggered: new Date(Date.now() - 2 * 60 * 60 * 1000),
          acknowledgeCount: 120,
          avgResponseTime: 8.5,
          lastModified: new Date(Date.now() - 1 * 24 * 60 * 60 * 1000),
          modifiedBy: 'safety_engineer',
          createdAt: new Date('2023-11-01'),
          createdBy: 'admin'
        }
      },
      {
        id: 'alarm_002',
        name: '압력 범위 경보',
        description: '압력이 정상 범위를 벗어났을 때',
        sourceType: 'data_point',
        sourceName: 'Pressure Gauge',
        deviceName: 'RTU-001',
        category: 'Process',
        conditionType: 'range',
        settings: {
          highLimit: 5.0,
          lowLimit: 1.0,
          deadband: 0.2,
          priority: 'medium',
          severity: 3,
          autoAcknowledge: false,
          autoReset: true,
          suppressDuration: 60,
          maxOccurrences: 50,
          escalationTime: 30,
          emailEnabled: true,
          emailRecipients: ['operator@company.com'],
          smsEnabled: false,
          smsRecipients: [],
          soundEnabled: true,
          popupEnabled: true,
          webhookEnabled: false,
          webhookUrl: '',
          messageTemplate: '⚡ 압력 알람: {sourceName} = {currentValue} bar (정상범위: {lowLimit}~{highLimit})',
          emailTemplate: '압력 게이지에서 알람이 발생했습니다.',
          schedule: {
            type: 'business_hours',
            startTime: '08:00',
            endTime: '18:00',
            weekdays: [1, 2, 3, 4, 5]
          },
          isEnabled: true
        },
        readonly: {
          triggerCount: 45,
          lastTriggered: new Date(Date.now() - 6 * 60 * 60 * 1000),
          acknowledgeCount: 42,
          avgResponseTime: 12.3,
          lastModified: new Date(Date.now() - 3 * 24 * 60 * 60 * 1000),
          modifiedBy: 'process_engineer',
          createdAt: new Date('2023-10-15'),
          createdBy: 'engineer1'
        }
      },
      {
        id: 'alarm_003',
        name: '생산 효율 저하',
        description: '생산 효율이 목표치 이하로 떨어졌을 때',
        sourceType: 'virtual_point',
        sourceName: 'Production Efficiency',
        category: 'Production',
        conditionType: 'threshold',
        settings: {
          lowLimit: 85,
          priority: 'low',
          severity: 2,
          autoAcknowledge: true,
          autoReset: true,
          suppressDuration: 900, // 15분
          maxOccurrences: 20,
          escalationTime: 60,
          emailEnabled: false,
          emailRecipients: [],
          smsEnabled: false,
          smsRecipients: [],
          soundEnabled: false,
          popupEnabled: true,
          webhookEnabled: false,
          webhookUrl: '',
          messageTemplate: '📊 생산효율 저하: {currentValue}% (목표: ≥{threshold}%)',
          emailTemplate: '',
          schedule: {
            type: 'always'
          },
          isEnabled: false // 🔥 현재 비활성화됨
        },
        readonly: {
          triggerCount: 23,
          lastTriggered: new Date(Date.now() - 24 * 60 * 60 * 1000),
          acknowledgeCount: 23,
          avgResponseTime: 45.2,
          lastModified: new Date(Date.now() - 5 * 24 * 60 * 60 * 1000),
          modifiedBy: 'production_manager',
          createdAt: new Date('2023-09-20'),
          createdBy: 'manager1'
        }
      },
      {
        id: 'alarm_004',
        name: 'PLC 통신 장애',
        description: 'PLC와의 통신이 끊어졌을 때',
        sourceType: 'device_status',
        sourceName: 'PLC Connection Status',
        deviceName: 'PLC-001',
        category: 'System',
        conditionType: 'pattern',
        settings: {
          pattern: 'disconnected',
          priority: 'critical',
          severity: 5,
          autoAcknowledge: false,
          autoReset: false,
          suppressDuration: 30,
          maxOccurrences: 5,
          escalationTime: 5, // 즉시 에스컬레이션
          emailEnabled: true,
          emailRecipients: ['it@company.com', 'manager@company.com'],
          smsEnabled: true,
          smsRecipients: ['+82-10-1234-5678', '+82-10-5678-1234'],
          soundEnabled: true,
          popupEnabled: true,
          webhookEnabled: true,
          webhookUrl: 'https://hooks.slack.com/services/it-emergency',
          messageTemplate: '🚨 PLC 통신 중단: {deviceName} - 즉시 확인 필요!',
          emailTemplate: 'CRITICAL: PLC 통신이 중단되었습니다.\n\n이것은 심각한 시스템 장애입니다. 즉시 현장을 확인하고 대응해주세요.',
          schedule: {
            type: 'always'
          },
          isEnabled: true
        },
        readonly: {
          triggerCount: 3,
          lastTriggered: new Date(Date.now() - 3 * 24 * 60 * 60 * 1000),
          acknowledgeCount: 3,
          avgResponseTime: 2.1,
          lastModified: new Date(Date.now() - 1 * 60 * 60 * 1000),
          modifiedBy: 'it_admin',
          createdAt: new Date('2023-11-05'),
          createdBy: 'it_admin'
        }
      }
    ];

    // 🔥 설정 그룹 생성
    const mockGroups: SettingsGroup[] = [
      {
        id: 'safety_group',
        name: '안전 관련 알람',
        description: '안전 및 위험 요소 관련 알람들',
        rules: mockRules.filter(rule => rule.category === 'Safety' || rule.category === 'System')
      },
      {
        id: 'production_group', 
        name: '생산 관련 알람',
        description: '생산성 및 공정 관련 알람들',
        rules: mockRules.filter(rule => rule.category === 'Production' || rule.category === 'Process')
      }
    ];

    setAlarmRules(mockRules);
    setSettingsGroups(mockGroups);
  };

  // 🔥 필터링된 알람 룰 목록
  const filteredRules = alarmRules.filter(rule => {
    const matchesCategory = filterCategory === 'all' || rule.category === filterCategory;
    const matchesPriority = filterPriority === 'all' || rule.settings.priority === filterPriority;
    const matchesStatus = filterStatus === 'all' || 
      (filterStatus === 'enabled' && rule.settings.isEnabled) || 
      (filterStatus === 'disabled' && !rule.settings.isEnabled);
    const matchesSearch = searchTerm === '' || 
      rule.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.sourceName.toLowerCase().includes(searchTerm.toLowerCase());
    
    return matchesCategory && matchesPriority && matchesStatus && matchesSearch;
  });

  // 🔥 설정 변경 핸들러
  const handleSettingChange = (ruleId: string, settingKey: string, value: any) => {
    setPendingChanges(prev => ({
      ...prev,
      [ruleId]: {
        ...prev[ruleId],
        [settingKey]: value
      }
    }));
    setHasUnsavedChanges(true);
  };

  // 🔥 설정 저장
  const handleSaveSettings = async (ruleId?: string) => {
    try {
      if (ruleId) {
        // 개별 룰 저장
        const changes = pendingChanges[ruleId];
        if (changes) {
          setAlarmRules(prev => prev.map(rule => 
            rule.id === ruleId 
              ? { 
                  ...rule, 
                  settings: { ...rule.settings, ...changes },
                  readonly: { ...rule.readonly, lastModified: new Date(), modifiedBy: 'current_user' }
                }
              : rule
          ));
          
          // API 호출
          // await alarmAPI.updateRuleSettings(ruleId, changes);
          
          setPendingChanges(prev => {
            const { [ruleId]: removed, ...rest } = prev;
            return rest;
          });
        }
      } else {
        // 모든 변경사항 저장
        setAlarmRules(prev => prev.map(rule => {
          const changes = pendingChanges[rule.id];
          return changes 
            ? { 
                ...rule, 
                settings: { ...rule.settings, ...changes },
                readonly: { ...rule.readonly, lastModified: new Date(), modifiedBy: 'current_user' }
              }
            : rule;
        }));
        
        // 배치 API 호출
        // await alarmAPI.batchUpdateSettings(pendingChanges);
        
        setPendingChanges({});
      }
      
      setHasUnsavedChanges(Object.keys(pendingChanges).length > (ruleId ? 1 : 0));
      
      // 성공 메시지
      alert(ruleId ? '설정이 저장되었습니다.' : '모든 변경사항이 저장되었습니다.');
      
    } catch (error) {
      console.error('설정 저장 실패:', error);
      alert('설정 저장에 실패했습니다.');
    }
  };

  // 🔥 변경사항 취소
  const handleCancelChanges = (ruleId?: string) => {
    if (ruleId) {
      setPendingChanges(prev => {
        const { [ruleId]: removed, ...rest } = prev;
        return rest;
      });
    } else {
      setPendingChanges({});
    }
    setHasUnsavedChanges(Object.keys(pendingChanges).length > (ruleId ? 1 : 0));
  };

  // 🔥 룰 활성화/비활성화
  const handleToggleRule = (ruleId: string) => {
    const rule = alarmRules.find(r => r.id === ruleId);
    if (rule) {
      handleSettingChange(ruleId, 'isEnabled', !rule.settings.isEnabled);
    }
  };

  // 🔥 일괄 설정 변경
  const handleBulkUpdate = (settings: Partial<ExistingAlarmRule['settings']>) => {
    selectedRules.forEach(ruleId => {
      Object.entries(settings).forEach(([key, value]) => {
        handleSettingChange(ruleId, key, value);
      });
    });
    setShowBulkModal(false);
  };

  // 🔥 우선순위별 색상
  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'critical': return 'priority-critical';
      case 'high': return 'priority-high';
      case 'medium': return 'priority-medium';
      case 'low': return 'priority-low';
      default: return 'priority-medium';
    }
  };

  // 🔥 현재 설정값 가져오기 (변경사항 반영)
  const getCurrentSettings = (rule: ExistingAlarmRule) => {
    const changes = pendingChanges[rule.id] || {};
    return { ...rule.settings, ...changes };
  };

  // 🔥 설정 모달 렌더링
  const renderSettingsModal = () => {
    if (!selectedRule) return null;
    
    const currentSettings = getCurrentSettings(selectedRule);
    
    return (
      <div className="modal-overlay">
        <div className="modal-container large">
          <div className="modal-header">
            <h2>알람 설정 조정: {selectedRule.name}</h2>
            <button
              className="modal-close-btn"
              onClick={() => setShowSettingsModal(false)}
            >
              <i className="fas fa-times"></i>
            </button>
          </div>

          <div className="modal-content">
            {/* 📊 읽기전용 정보 */}
            <div className="form-section readonly-section">
              <h3>기본 정보 (읽기전용)</h3>
              <div className="readonly-grid">
                <div className="readonly-item">
                  <label>데이터 소스</label>
                  <span>{selectedRule.sourceName}</span>
                </div>
                <div className="readonly-item">
                  <label>디바이스</label>
                  <span>{selectedRule.deviceName || 'N/A'}</span>
                </div>
                <div className="readonly-item">
                  <label>카테고리</label>
                  <span>{selectedRule.category}</span>
                </div>
                <div className="readonly-item">
                  <label>조건 타입</label>
                  <span>{selectedRule.conditionType}</span>
                </div>
                <div className="readonly-item">
                  <label>발생 횟수</label>
                  <span>{selectedRule.readonly.triggerCount}회</span>
                </div>
                <div className="readonly-item">
                  <label>평균 대응시간</label>
                  <span>{selectedRule.readonly.avgResponseTime?.toFixed(1) || 'N/A'}분</span>
                </div>
                <div className="readonly-item">
                  <label>마지막 발생</label>
                  <span>{selectedRule.readonly.lastTriggered?.toLocaleString() || 'N/A'}</span>
                </div>
                <div className="readonly-item">
                  <label>최근 수정</label>
                  <span>{selectedRule.readonly.lastModified.toLocaleString()} ({selectedRule.readonly.modifiedBy})</span>
                </div>
              </div>
            </div>

            {/* 🔧 임계값 설정 */}
            {(selectedRule.conditionType === 'threshold' || selectedRule.conditionType === 'range') && (
              <div className="form-section">
                <h3>임계값 설정</h3>
                <div className="form-row">
                  {currentSettings.highHighLimit !== undefined && (
                    <div className="form-group">
                      <label>매우 높음 (HH)</label>
                      <input
                        type="number"
                        value={currentSettings.highHighLimit || ''}
                        onChange={(e) => handleSettingChange(selectedRule.id, 'highHighLimit', Number(e.target.value))}
                        className="form-input"
                      />
                    </div>
                  )}
                  {currentSettings.highLimit !== undefined && (
                    <div className="form-group">
                      <label>높음 (H)</label>
                      <input
                        type="number"
                        value={currentSettings.highLimit || ''}
                        onChange={(e) => handleSettingChange(selectedRule.id, 'highLimit', Number(e.target.value))}
                        className="form-input"
                      />
                    </div>
                  )}
                  {currentSettings.lowLimit !== undefined && (
                    <div className="form-group">
                      <label>낮음 (L)</label>
                      <input
                        type="number"
                        value={currentSettings.lowLimit || ''}
                        onChange={(e) => handleSettingChange(selectedRule.id, 'lowLimit', Number(e.target.value))}
                        className="form-input"
                      />
                    </div>
                  )}
                  {currentSettings.lowLowLimit !== undefined && (
                    <div className="form-group">
                      <label>매우 낮음 (LL)</label>
                      <input
                        type="number"
                        value={currentSettings.lowLowLimit || ''}
                        onChange={(e) => handleSettingChange(selectedRule.id, 'lowLowLimit', Number(e.target.value))}
                        className="form-input"
                      />
                    </div>
                  )}
                </div>
                {currentSettings.deadband !== undefined && (
                  <div className="form-group">
                    <label>히스테리시스 (Deadband)</label>
                    <input
                      type="number"
                      value={currentSettings.deadband || ''}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'deadband', Number(e.target.value))}
                      className="form-input"
                      placeholder="0"
                    />
                    <small className="form-help">알람 chattering 방지를 위한 데드밴드</small>
                  </div>
                )}
              </div>
            )}

            {/* 🚨 우선순위 및 동작 설정 */}
            <div className="form-section">
              <h3>우선순위 및 동작</h3>
              <div className="form-row">
                <div className="form-group">
                  <label>우선순위</label>
                  <select
                    value={currentSettings.priority}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'priority', e.target.value)}
                    className="form-select"
                  >
                    <option value="low">Low</option>
                    <option value="medium">Medium</option>
                    <option value="high">High</option>
                    <option value="critical">Critical</option>
                  </select>
                </div>
                <div className="form-group">
                  <label>심각도 (1-5)</label>
                  <select
                    value={currentSettings.severity}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'severity', Number(e.target.value))}
                    className="form-select"
                  >
                    <option value={1}>1 (가장 낮음)</option>
                    <option value={2}>2 (낮음)</option>
                    <option value={3}>3 (보통)</option>
                    <option value={4}>4 (높음)</option>
                    <option value={5}>5 (가장 높음)</option>
                  </select>
                </div>
                <div className="form-group">
                  <label>에스컬레이션 시간 (분)</label>
                  <input
                    type="number"
                    value={currentSettings.escalationTime}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'escalationTime', Number(e.target.value))}
                    className="form-input"
                    placeholder="0"
                  />
                </div>
              </div>
              
              <div className="form-row">
                <div className="form-group">
                  <label>중복 억제 시간 (초)</label>
                  <input
                    type="number"
                    value={currentSettings.suppressDuration}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'suppressDuration', Number(e.target.value))}
                    className="form-input"
                    placeholder="0"
                  />
                  <small className="form-help">동일한 알람의 중복 발생을 억제할 시간</small>
                </div>
                <div className="form-group">
                  <label>최대 발생 횟수</label>
                  <input
                    type="number"
                    value={currentSettings.maxOccurrences}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'maxOccurrences', Number(e.target.value))}
                    className="form-input"
                    placeholder="0"
                  />
                  <small className="form-help">0 = 무제한</small>
                </div>
              </div>

              <div className="checkbox-options">
                <label className="checkbox-label">
                  <input
                    type="checkbox"
                    checked={currentSettings.autoAcknowledge}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'autoAcknowledge', e.target.checked)}
                  />
                  자동 승인
                </label>
                <label className="checkbox-label">
                  <input
                    type="checkbox"
                    checked={currentSettings.autoReset}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'autoReset', e.target.checked)}
                  />
                  자동 리셋
                </label>
                <label className="checkbox-label">
                  <input
                    type="checkbox"
                    checked={currentSettings.isEnabled}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'isEnabled', e.target.checked)}
                  />
                  알람 활성화
                </label>
              </div>
            </div>

            {/* 📧 알림 설정 */}
            <div className="form-section">
              <h3>알림 설정</h3>
              <div className="notification-options">
                <div className="checkbox-group">
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={currentSettings.emailEnabled}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'emailEnabled', e.target.checked)}
                    />
                    이메일 알림
                  </label>
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={currentSettings.smsEnabled}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'smsEnabled', e.target.checked)}
                    />
                    SMS 알림
                  </label>
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={currentSettings.soundEnabled}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'soundEnabled', e.target.checked)}
                    />
                    소리 알림
                  </label>
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={currentSettings.popupEnabled}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'popupEnabled', e.target.checked)}
                    />
                    팝업 알림
                  </label>
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={currentSettings.webhookEnabled}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'webhookEnabled', e.target.checked)}
                    />
                    웹훅 알림
                  </label>
                </div>
              </div>

              {currentSettings.emailEnabled && (
                <div className="form-group">
                  <label>이메일 수신자</label>
                  <input
                    type="text"
                    value={currentSettings.emailRecipients.join(', ')}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'emailRecipients', 
                      e.target.value.split(',').map(email => email.trim()).filter(email => email)
                    )}
                    className="form-input"
                    placeholder="이메일 주소들을 쉼표로 구분"
                  />
                </div>
              )}

              {currentSettings.smsEnabled && (
                <div className="form-group">
                  <label>SMS 수신자</label>
                  <input
                    type="text"
                    value={currentSettings.smsRecipients.join(', ')}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'smsRecipients', 
                      e.target.value.split(',').map(phone => phone.trim()).filter(phone => phone)
                    )}
                    className="form-input"
                    placeholder="전화번호들을 쉼표로 구분"
                  />
                </div>
              )}

              {currentSettings.webhookEnabled && (
                <div className="form-group">
                  <label>웹훅 URL</label>
                  <input
                    type="url"
                    value={currentSettings.webhookUrl}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'webhookUrl', e.target.value)}
                    className="form-input"
                    placeholder="https://hooks.slack.com/services/..."
                  />
                </div>
              )}
            </div>

            {/* 💬 메시지 설정 */}
            <div className="form-section">
              <h3>메시지 설정</h3>
              <div className="form-group">
                <label>알람 메시지 템플릿</label>
                <textarea
                  value={currentSettings.messageTemplate}
                  onChange={(e) => handleSettingChange(selectedRule.id, 'messageTemplate', e.target.value)}
                  className="form-textarea"
                  placeholder="알람 메시지 템플릿"
                  rows={3}
                />
                <small className="form-help">
                  사용 가능한 변수: {'{sourceName}'}, {'{currentValue}'}, {'{threshold}'}, {'{severity}'}, {'{timestamp}'}
                </small>
              </div>
              
              {currentSettings.emailEnabled && (
                <div className="form-group">
                  <label>이메일 메시지 템플릿</label>
                  <textarea
                    value={currentSettings.emailTemplate}
                    onChange={(e) => handleSettingChange(selectedRule.id, 'emailTemplate', e.target.value)}
                    className="form-textarea"
                    placeholder="이메일용 상세 메시지 템플릿"
                    rows={5}
                  />
                </div>
              )}
            </div>

            {/* ⏰ 스케줄 설정 */}
            <div className="form-section">
              <h3>스케줄 설정</h3>
              <div className="form-group">
                <label>실행 시간</label>
                <select
                  value={currentSettings.schedule.type}
                  onChange={(e) => handleSettingChange(selectedRule.id, 'schedule', { 
                    ...currentSettings.schedule, 
                    type: e.target.value as 'always' | 'business_hours' | 'custom' 
                  })}
                  className="form-select"
                >
                  <option value="always">항상</option>
                  <option value="business_hours">업무시간</option>
                  <option value="custom">사용자 정의</option>
                </select>
              </div>

              {currentSettings.schedule.type === 'business_hours' && (
                <div className="form-row">
                  <div className="form-group">
                    <label>시작 시간</label>
                    <input
                      type="time"
                      value={currentSettings.schedule.startTime || '08:00'}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'schedule', { 
                        ...currentSettings.schedule, 
                        startTime: e.target.value 
                      })}
                      className="form-input"
                    />
                  </div>
                  <div className="form-group">
                    <label>종료 시간</label>
                    <input
                      type="time"
                      value={currentSettings.schedule.endTime || '18:00'}
                      onChange={(e) => handleSettingChange(selectedRule.id, 'schedule', { 
                        ...currentSettings.schedule, 
                        endTime: e.target.value 
                      })}
                      className="form-input"
                    />
                  </div>
                </div>
              )}
            </div>
          </div>

          <div className="modal-footer">
            <button
              className="btn btn-outline"
              onClick={() => handleCancelChanges(selectedRule.id)}
              disabled={!pendingChanges[selectedRule.id]}
            >
              변경사항 취소
            </button>
            <button
              className="btn btn-outline"
              onClick={() => setShowSettingsModal(false)}
            >
              닫기
            </button>
            <button
              className="btn btn-primary"
              onClick={() => handleSaveSettings(selectedRule.id)}
              disabled={!pendingChanges[selectedRule.id]}
            >
              설정 저장
            </button>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div className="alarm-settings-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div>
          <h1 className="page-title">알람 설정 조정</h1>
          <div className="page-subtitle">
            기존 알람 룰들의 임계값, 알림, 스케줄 등 설정을 조정합니다
          </div>
        </div>
        <div className="page-actions">
          {hasUnsavedChanges && (
            <div className="unsaved-indicator">
              <i className="fas fa-exclamation-triangle"></i>
              저장되지 않은 변경사항이 있습니다
            </div>
          )}
          <button 
            className="btn btn-outline"
            onClick={() => handleCancelChanges()}
            disabled={!hasUnsavedChanges}
          >
            <i className="fas fa-undo"></i>
            모든 변경사항 취소
          </button>
          <button 
            className="btn btn-success"
            onClick={() => handleSaveSettings()}
            disabled={!hasUnsavedChanges}
          >
            <i className="fas fa-save"></i>
            모든 변경사항 저장
          </button>
          <button 
            className="btn btn-primary"
            onClick={() => setShowBulkModal(true)}
            disabled={selectedRules.length === 0}
          >
            <i className="fas fa-edit"></i>
            일괄 수정 ({selectedRules.length}개)
          </button>
        </div>
      </div>

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-row">
          <div className="filter-group">
            <label>보기 모드</label>
            <div className="view-toggle">
              <button 
                className={`toggle-btn ${viewMode === 'list' ? 'active' : ''}`}
                onClick={() => setViewMode('list')}
              >
                <i className="fas fa-list"></i> 목록
              </button>
              <button 
                className={`toggle-btn ${viewMode === 'groups' ? 'active' : ''}`}
                onClick={() => setViewMode('groups')}
              >
                <i className="fas fa-layer-group"></i> 그룹
              </button>
            </div>
          </div>
          
          <div className="filter-group">
            <label>카테고리</label>
            <select
              value={filterCategory}
              onChange={(e) => setFilterCategory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 카테고리</option>
              <option value="Safety">안전</option>
              <option value="Process">공정</option>
              <option value="Production">생산</option>
              <option value="System">시스템</option>
            </select>
          </div>

          <div className="filter-group">
            <label>우선순위</label>
            <select
              value={filterPriority}
              onChange={(e) => setFilterPriority(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 우선순위</option>
              <option value="critical">Critical</option>
              <option value="high">High</option>
              <option value="medium">Medium</option>
              <option value="low">Low</option>
            </select>
          </div>

          <div className="filter-group">
            <label>상태</label>
            <select
              value={filterStatus}
              onChange={(e) => setFilterStatus(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 상태</option>
              <option value="enabled">활성</option>
              <option value="disabled">비활성</option>
            </select>
          </div>

          <div className="filter-group flex-1">
            <label>검색</label>
            <div className="search-container">
              <input
                type="text"
                placeholder="알람명, 설명, 소스명 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="search-input"
              />
              <i className="fas fa-search search-icon"></i>
            </div>
          </div>
        </div>
      </div>

      {/* 통계 정보 */}
      <div className="stats-panel">
        <div className="stat-card">
          <div className="stat-value">{alarmRules.length}</div>
          <div className="stat-label">전체 알람 룰</div>
        </div>
        <div className="stat-card status-enabled">
          <div className="stat-value">{alarmRules.filter(r => r.settings.isEnabled).length}</div>
          <div className="stat-label">활성</div>
        </div>
        <div className="stat-card status-disabled">
          <div className="stat-value">{alarmRules.filter(r => !r.settings.isEnabled).length}</div>
          <div className="stat-label">비활성</div>
        </div>
        <div className="stat-card priority-critical">
          <div className="stat-value">{alarmRules.filter(r => r.settings.priority === 'critical').length}</div>
          <div className="stat-label">Critical</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{Object.keys(pendingChanges).length}</div>
          <div className="stat-label">변경 대기</div>
        </div>
      </div>

      {/* 알람 룰 목록 */}
      <div className="rules-list">
        <div className="rules-grid">
          {filteredRules.map(rule => {
            const currentSettings = getCurrentSettings(rule);
            const hasChanges = !!pendingChanges[rule.id];
            const isSelected = selectedRules.includes(rule.id);
            
            return (
              <div key={rule.id} className={`rule-card ${currentSettings.isEnabled ? 'enabled' : 'disabled'} ${hasChanges ? 'has-changes' : ''} ${isSelected ? 'selected' : ''}`}>
                <div className="rule-header">
                  <div className="rule-select">
                    <input
                      type="checkbox"
                      checked={isSelected}
                      onChange={(e) => {
                        if (e.target.checked) {
                          setSelectedRules(prev => [...prev, rule.id]);
                        } else {
                          setSelectedRules(prev => prev.filter(id => id !== rule.id));
                        }
                      }}
                    />
                  </div>
                  <div className="rule-title">
                    <h3>{rule.name}</h3>
                    <div className="rule-badges">
                      <span className={`priority-badge ${getPriorityColor(currentSettings.priority)}`}>
                        {currentSettings.priority.toUpperCase()}
                      </span>
                      <span className="category-badge">{rule.category}</span>
                      <span className={`status-badge ${currentSettings.isEnabled ? 'enabled' : 'disabled'}`}>
                        {currentSettings.isEnabled ? '활성' : '비활성'}
                      </span>
                      {hasChanges && <span className="changes-badge">변경사항</span>}
                    </div>
                  </div>
                  <div className="rule-actions">
                    <button
                      className={`toggle-btn ${currentSettings.isEnabled ? 'enabled' : 'disabled'}`}
                      onClick={() => handleToggleRule(rule.id)}
                      title={currentSettings.isEnabled ? '비활성화' : '활성화'}
                    >
                      <i className={`fas ${currentSettings.isEnabled ? 'fa-toggle-on' : 'fa-toggle-off'}`}></i>
                    </button>
                    <button
                      className="settings-btn"
                      onClick={() => {
                        setSelectedRule(rule);
                        setShowSettingsModal(true);
                      }}
                      title="설정 조정"
                    >
                      <i className="fas fa-cog"></i>
                    </button>
                    {hasChanges && (
                      <button
                        className="save-btn"
                        onClick={() => handleSaveSettings(rule.id)}
                        title="변경사항 저장"
                      >
                        <i className="fas fa-save"></i>
                      </button>
                    )}
                  </div>
                </div>

                <div className="rule-content">
                  <div className="rule-description">
                    {rule.description}
                  </div>

                  <div className="rule-source">
                    <label>데이터 소스:</label>
                    <span>{rule.sourceName}</span>
                    {rule.deviceName && <small>({rule.deviceName})</small>}
                  </div>

                  <div className="rule-condition">
                    <label>현재 설정:</label>
                    <div className="condition-display">
                      {rule.conditionType === 'threshold' && (
                        <span>
                          임계값: HH={currentSettings.highHighLimit}, H={currentSettings.highLimit}, 
                          L={currentSettings.lowLimit}, LL={currentSettings.lowLowLimit}
                          {currentSettings.deadband > 0 && `, DB=${currentSettings.deadband}`}
                        </span>
                      )}
                      {rule.conditionType === 'range' && (
                        <span>범위: {currentSettings.lowLimit} ~ {currentSettings.highLimit}</span>
                      )}
                      {rule.conditionType === 'pattern' && (
                        <span>패턴: {currentSettings.pattern}</span>
                      )}
                    </div>
                  </div>

                  <div className="rule-notifications">
                    <label>알림:</label>
                    <div className="notification-icons">
                      {currentSettings.emailEnabled && <i className="fas fa-envelope" title="이메일"></i>}
                      {currentSettings.smsEnabled && <i className="fas fa-sms" title="SMS"></i>}
                      {currentSettings.soundEnabled && <i className="fas fa-volume-up" title="소리"></i>}
                      {currentSettings.popupEnabled && <i className="fas fa-window-maximize" title="팝업"></i>}
                      {currentSettings.webhookEnabled && <i className="fas fa-link" title="웹훅"></i>}
                    </div>
                  </div>

                  <div className="rule-stats">
                    <div className="stat-item">
                      <span>발생:</span>
                      <span>{rule.readonly.triggerCount}회</span>
                    </div>
                    <div className="stat-item">
                      <span>평균대응:</span>
                      <span>{rule.readonly.avgResponseTime?.toFixed(1) || 'N/A'}분</span>
                    </div>
                    <div className="stat-item">
                      <span>억제시간:</span>
                      <span>{currentSettings.suppressDuration}초</span>
                    </div>
                    <div className="stat-item">
                      <span>최근 수정:</span>
                      <span>{rule.readonly.lastModified.toLocaleDateString()}</span>
                    </div>
                  </div>
                </div>
              </div>
            );
          })}
        </div>

        {filteredRules.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-cog empty-icon"></i>
            <div className="empty-title">설정할 알람 룰이 없습니다</div>
            <div className="empty-description">
              필터 조건을 변경하거나 새로운 알람 룰을 생성해보세요.
            </div>
          </div>
        )}
      </div>

      {/* 설정 모달 */}
      {showSettingsModal && renderSettingsModal()}

      <style jsx>{`
        .alarm-settings-container {
          max-width: none;
          width: 100%;
        }

        .page-header {
          display: flex;
          justify-content: space-between;
          align-items: flex-start;
          margin-bottom: 2rem;
        }

        .page-subtitle {
          color: #6b7280;
          font-size: 0.875rem;
          margin-top: 0.25rem;
        }

        .page-actions {
          display: flex;
          gap: 0.75rem;
          align-items: center;
        }

        .unsaved-indicator {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          color: #f59e0b;
          font-size: 0.875rem;
          background: #fef3c7;
          padding: 0.5rem 0.75rem;
          border-radius: 0.375rem;
          border: 1px solid #fbbf24;
        }

        .view-toggle {
          display: flex;
          background: #f3f4f6;
          border-radius: 0.375rem;
          padding: 0.125rem;
        }

        .toggle-btn {
          padding: 0.5rem 0.75rem;
          border: none;
          background: transparent;
          border-radius: 0.25rem;
          cursor: pointer;
          font-size: 0.875rem;
          transition: all 0.2s;
        }

        .toggle-btn.active {
          background: white;
          color: #3b82f6;
          box-shadow: 0 1px 2px rgba(0, 0, 0, 0.05);
        }

        .rule-card {
          position: relative;
          border: 2px solid #e5e7eb;
          border-radius: 0.75rem;
          padding: 1.5rem;
          transition: all 0.2s;
          background: white;
        }

        .rule-card.has-changes {
          border-color: #f59e0b;
          background: #fffbeb;
        }

        .rule-card.selected {
          border-color: #3b82f6;
          background: #eff6ff;
        }

        .rule-header {
          display: flex;
          align-items: flex-start;
          gap: 0.75rem;
          margin-bottom: 1rem;
        }

        .rule-select {
          margin-top: 0.25rem;
        }

        .rule-select input[type="checkbox"] {
          width: 1.125rem;
          height: 1.125rem;
        }

        .changes-badge {
          font-size: 0.75rem;
          background: #f59e0b;
          color: white;
          padding: 0.125rem 0.5rem;
          border-radius: 0.375rem;
          text-transform: uppercase;
          font-weight: 600;
        }

        .settings-btn {
          background: none;
          border: none;
          padding: 0.5rem;
          border-radius: 0.375rem;
          cursor: pointer;
          transition: all 0.2s;
          color: #6b7280;
        }

        .settings-btn:hover {
          background: #3b82f6;
          color: white;
        }

        .save-btn {
          background: #10b981;
          border: none;
          padding: 0.5rem;
          border-radius: 0.375rem;
          cursor: pointer;
          color: white;
          transition: all 0.2s;
        }

        .save-btn:hover {
          background: #059669;
        }

        .condition-display {
          font-family: 'Courier New', monospace;
          font-size: 0.8rem;
          background: #f3f4f6;
          padding: 0.5rem;
          border-radius: 0.25rem;
          margin-top: 0.25rem;
        }

        .readonly-section {
          background: #f9fafb;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1rem;
        }

        .readonly-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 0.75rem;
        }

        .readonly-item {
          display: flex;
          flex-direction: column;
          gap: 0.25rem;
        }

        .readonly-item label {
          font-size: 0.75rem;
          font-weight: 500;
          color: #6b7280;
          text-transform: uppercase;
        }

        .readonly-item span {
          font-size: 0.875rem;
          color: #1f2937;
          font-weight: 500;
        }

        .modal-container.large {
          max-width: 1000px;
          max-height: 90vh;
        }

        .modal-content {
          max-height: 70vh;
          overflow-y: auto;
        }

        @media (max-width: 768px) {
          .page-header {
            flex-direction: column;
            gap: 1rem;
          }
          
          .page-actions {
            width: 100%;
            justify-content: flex-start;
          }
          
          .readonly-grid {
            grid-template-columns: 1fr;
          }
        }
      `}</style>
    </div>
  );
};

export default AlarmSettings;