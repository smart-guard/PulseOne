import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/alarm-settings.css';

interface AlarmRule {
  id: string;
  name: string;
  description: string;
  sourceType: 'data_point' | 'virtual_point' | 'device_status';
  sourceId: string;
  sourceName: string;
  factory: string;
  category: string;
  
  // 알람 조건
  conditionType: 'threshold' | 'range' | 'deviation' | 'pattern' | 'timeout';
  highLimit?: number;
  lowLimit?: number;
  targetValue?: number;
  tolerance?: number;
  timeWindow?: number; // seconds
  pattern?: string;
  
  // 우선순위 및 동작
  priority: 'low' | 'medium' | 'high' | 'critical';
  severity: 1 | 2 | 3 | 4 | 5;
  autoAcknowledge: boolean;
  autoReset: boolean;
  escalationTime?: number; // minutes
  
  // 알림 설정
  emailEnabled: boolean;
  emailRecipients: string[];
  smsEnabled: boolean;
  smsRecipients: string[];
  soundEnabled: boolean;
  popupEnabled: boolean;
  
  // 메시지 템플릿
  messageTemplate: string;
  emailTemplate?: string;
  
  // 상태 및 일정
  isEnabled: boolean;
  schedule?: {
    type: 'always' | 'business_hours' | 'custom';
    startTime?: string;
    endTime?: string;
    weekdays?: number[];
  };
  
  // 통계
  triggerCount: number;
  lastTriggered?: Date;
  acknowledgeCount: number;
  
  createdAt: Date;
  updatedAt: Date;
  createdBy: string;
}

interface DataSource {
  id: string;
  name: string;
  type: 'data_point' | 'virtual_point' | 'device_status';
  deviceName?: string;
  unit?: string;
  currentValue?: any;
  dataType: 'number' | 'boolean' | 'string';
}

const AlarmSettings: React.FC = () => {
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [dataSources, setDataSources] = useState<DataSource[]>([]);
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [isEditing, setIsEditing] = useState(false);
  const [filterCategory, setFilterCategory] = useState('all');
  const [filterPriority, setFilterPriority] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');

  // 새 알람 룰 폼 데이터
  const [formData, setFormData] = useState<Partial<AlarmRule>>({
    name: '',
    description: '',
    sourceType: 'data_point',
    factory: '',
    category: '',
    conditionType: 'threshold',
    priority: 'medium',
    severity: 3,
    autoAcknowledge: false,
    autoReset: true,
    emailEnabled: false,
    smsEnabled: false,
    soundEnabled: true,
    popupEnabled: true,
    emailRecipients: [],
    smsRecipients: [],
    messageTemplate: '{sourceName}에서 알람이 발생했습니다. 현재값: {currentValue}',
    isEnabled: true,
    schedule: {
      type: 'always'
    }
  });

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 목업 데이터 소스
    const mockDataSources: DataSource[] = [
      {
        id: 'dp_001',
        name: 'Temperature Sensor 1',
        type: 'data_point',
        deviceName: 'PLC-001',
        unit: '°C',
        currentValue: 25.4,
        dataType: 'number'
      },
      {
        id: 'dp_002',
        name: 'Pressure Gauge',
        type: 'data_point',
        deviceName: 'RTU-001',
        unit: 'bar',
        currentValue: 3.2,
        dataType: 'number'
      },
      {
        id: 'vp_001',
        name: 'Production Efficiency',
        type: 'virtual_point',
        unit: '%',
        currentValue: 87.5,
        dataType: 'number'
      },
      {
        id: 'ds_001',
        name: 'PLC Connection Status',
        type: 'device_status',
        deviceName: 'PLC-001',
        currentValue: 'connected',
        dataType: 'string'
      }
    ];

    // 목업 알람 룰
    const mockRules: AlarmRule[] = [
      {
        id: 'alarm_001',
        name: '온도 상한 경보',
        description: '생산라인 온도가 안전 범위를 초과했을 때 발생',
        sourceType: 'data_point',
        sourceId: 'dp_001',
        sourceName: 'Temperature Sensor 1',
        factory: 'Factory A',
        category: 'Safety',
        conditionType: 'threshold',
        highLimit: 80,
        priority: 'high',
        severity: 4,
        autoAcknowledge: false,
        autoReset: true,
        emailEnabled: true,
        emailRecipients: ['safety@company.com', 'manager@company.com'],
        smsEnabled: false,
        smsRecipients: [],
        soundEnabled: true,
        popupEnabled: true,
        messageTemplate: '온도 센서에서 위험 수준의 온도가 감지되었습니다. 현재 온도: {currentValue}°C',
        isEnabled: true,
        schedule: { type: 'always' },
        triggerCount: 12,
        lastTriggered: new Date(Date.now() - 2 * 60 * 60 * 1000),
        acknowledgeCount: 10,
        createdAt: new Date('2023-11-01'),
        updatedAt: new Date('2023-11-15'),
        createdBy: 'admin'
      },
      {
        id: 'alarm_002',
        name: '압력 이상 감지',
        description: '압력이 정상 범위를 벗어났을 때',
        sourceType: 'data_point',
        sourceId: 'dp_002',
        sourceName: 'Pressure Gauge',
        factory: 'Factory A',
        category: 'Process',
        conditionType: 'range',
        highLimit: 5.0,
        lowLimit: 1.0,
        priority: 'medium',
        severity: 3,
        autoAcknowledge: false,
        autoReset: true,
        emailEnabled: true,
        emailRecipients: ['operator@company.com'],
        smsEnabled: false,
        smsRecipients: [],
        soundEnabled: true,
        popupEnabled: true,
        messageTemplate: '압력이 정상 범위를 벗어났습니다. 현재 압력: {currentValue} bar',
        isEnabled: true,
        schedule: {
          type: 'business_hours',
          startTime: '08:00',
          endTime: '18:00',
          weekdays: [1, 2, 3, 4, 5]
        },
        triggerCount: 5,
        lastTriggered: new Date(Date.now() - 6 * 60 * 60 * 1000),
        acknowledgeCount: 5,
        createdAt: new Date('2023-10-15'),
        updatedAt: new Date('2023-11-20'),
        createdBy: 'engineer1'
      },
      {
        id: 'alarm_003',
        name: '생산 효율 저하',
        description: '생산 효율이 목표치 이하로 떨어졌을 때',
        sourceType: 'virtual_point',
        sourceId: 'vp_001',
        sourceName: 'Production Efficiency',
        factory: 'Factory A',
        category: 'Production',
        conditionType: 'threshold',
        lowLimit: 85,
        priority: 'low',
        severity: 2,
        autoAcknowledge: true,
        autoReset: true,
        emailEnabled: false,
        emailRecipients: [],
        smsEnabled: false,
        smsRecipients: [],
        soundEnabled: false,
        popupEnabled: true,
        messageTemplate: '생산 효율이 목표치를 하회했습니다. 현재 효율: {currentValue}%',
        isEnabled: true,
        schedule: { type: 'always' },
        triggerCount: 8,
        lastTriggered: new Date(Date.now() - 24 * 60 * 60 * 1000),
        acknowledgeCount: 8,
        createdAt: new Date('2023-09-20'),
        updatedAt: new Date('2023-11-10'),
        createdBy: 'manager1'
      },
      {
        id: 'alarm_004',
        name: 'PLC 통신 장애',
        description: 'PLC와의 통신이 끊어졌을 때',
        sourceType: 'device_status',
        sourceId: 'ds_001',
        sourceName: 'PLC Connection Status',
        factory: 'Factory A',
        category: 'System',
        conditionType: 'pattern',
        pattern: 'disconnected',
        priority: 'critical',
        severity: 5,
        autoAcknowledge: false,
        autoReset: false,
        emailEnabled: true,
        emailRecipients: ['it@company.com', 'manager@company.com'],
        smsEnabled: true,
        smsRecipients: ['+82-10-1234-5678'],
        soundEnabled: true,
        popupEnabled: true,
        messageTemplate: 'PLC 통신이 중단되었습니다. 즉시 확인이 필요합니다.',
        isEnabled: false,
        schedule: { type: 'always' },
        triggerCount: 2,
        lastTriggered: new Date(Date.now() - 3 * 24 * 60 * 60 * 1000),
        acknowledgeCount: 2,
        createdAt: new Date('2023-11-05'),
        updatedAt: new Date('2023-11-25'),
        createdBy: 'it_admin'
      }
    ];

    setDataSources(mockDataSources);
    setAlarmRules(mockRules);
  };

  // 필터링된 알람 룰 목록
  const filteredRules = alarmRules.filter(rule => {
    const matchesCategory = filterCategory === 'all' || rule.category === filterCategory;
    const matchesPriority = filterPriority === 'all' || rule.priority === filterPriority;
    const matchesStatus = filterStatus === 'all' || 
      (filterStatus === 'enabled' && rule.isEnabled) || 
      (filterStatus === 'disabled' && !rule.isEnabled);
    const matchesSearch = searchTerm === '' || 
      rule.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.sourceName.toLowerCase().includes(searchTerm.toLowerCase());
    
    return matchesCategory && matchesPriority && matchesStatus && matchesSearch;
  });

  // 새 알람 룰 생성
  const handleCreateRule = () => {
    setFormData({
      name: '',
      description: '',
      sourceType: 'data_point',
      factory: '',
      category: '',
      conditionType: 'threshold',
      priority: 'medium',
      severity: 3,
      autoAcknowledge: false,
      autoReset: true,
      emailEnabled: false,
      smsEnabled: false,
      soundEnabled: true,
      popupEnabled: true,
      emailRecipients: [],
      smsRecipients: [],
      messageTemplate: '{sourceName}에서 알람이 발생했습니다. 현재값: {currentValue}',
      isEnabled: true,
      schedule: { type: 'always' }
    });
    setIsEditing(false);
    setShowCreateModal(true);
  };

  // 알람 룰 편집
  const handleEditRule = (rule: AlarmRule) => {
    setFormData(rule);
    setSelectedRule(rule);
    setIsEditing(true);
    setShowCreateModal(true);
  };

  // 알람 룰 저장
  const handleSaveRule = () => {
    if (!formData.name || !formData.sourceId) {
      alert('필수 정보를 입력해주세요.');
      return;
    }

    const selectedSource = dataSources.find(s => s.id === formData.sourceId);
    if (!selectedSource) {
      alert('유효한 데이터 소스를 선택해주세요.');
      return;
    }

    if (isEditing && selectedRule) {
      // 편집
      setAlarmRules(prev => prev.map(rule =>
        rule.id === selectedRule.id
          ? {
              ...formData,
              id: selectedRule.id,
              sourceName: selectedSource.name,
              triggerCount: rule.triggerCount,
              lastTriggered: rule.lastTriggered,
              acknowledgeCount: rule.acknowledgeCount,
              createdAt: rule.createdAt,
              updatedAt: new Date(),
              createdBy: rule.createdBy
            } as AlarmRule
          : rule
      ));
    } else {
      // 생성
      const newRule: AlarmRule = {
        ...formData,
        id: `alarm_${Date.now()}`,
        sourceName: selectedSource.name,
        triggerCount: 0,
        acknowledgeCount: 0,
        createdAt: new Date(),
        updatedAt: new Date(),
        createdBy: 'current_user'
      } as AlarmRule;
      
      setAlarmRules(prev => [newRule, ...prev]);
    }

    setShowCreateModal(false);
    setSelectedRule(null);
  };

  // 알람 룰 삭제
  const handleDeleteRule = (ruleId: string) => {
    if (confirm('이 알람 룰을 삭제하시겠습니까?')) {
      setAlarmRules(prev => prev.filter(rule => rule.id !== ruleId));
    }
  };

  // 알람 룰 활성화/비활성화
  const handleToggleRule = (ruleId: string) => {
    setAlarmRules(prev => prev.map(rule =>
      rule.id === ruleId
        ? { ...rule, isEnabled: !rule.isEnabled, updatedAt: new Date() }
        : rule
    ));
  };

  // 조건 타입에 따른 입력 필드 렌더링
  const renderConditionFields = () => {
    const conditionType = formData.conditionType;

    switch (conditionType) {
      case 'threshold':
        return (
          <div className="condition-fields">
            <div className="form-row">
              <div className="form-group">
                <label>상한값</label>
                <input
                  type="number"
                  value={formData.highLimit || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, highLimit: Number(e.target.value) }))}
                  className="form-input"
                  placeholder="상한 임계값"
                />
              </div>
              <div className="form-group">
                <label>하한값</label>
                <input
                  type="number"
                  value={formData.lowLimit || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, lowLimit: Number(e.target.value) }))}
                  className="form-input"
                  placeholder="하한 임계값"
                />
              </div>
            </div>
          </div>
        );

      case 'range':
        return (
          <div className="condition-fields">
            <div className="form-row">
              <div className="form-group">
                <label>최대값 *</label>
                <input
                  type="number"
                  value={formData.highLimit || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, highLimit: Number(e.target.value) }))}
                  className="form-input"
                  placeholder="정상 범위 최대값"
                  required
                />
              </div>
              <div className="form-group">
                <label>최소값 *</label>
                <input
                  type="number"
                  value={formData.lowLimit || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, lowLimit: Number(e.target.value) }))}
                  className="form-input"
                  placeholder="정상 범위 최소값"
                  required
                />
              </div>
            </div>
          </div>
        );

      case 'deviation':
        return (
          <div className="condition-fields">
            <div className="form-row">
              <div className="form-group">
                <label>목표값 *</label>
                <input
                  type="number"
                  value={formData.targetValue || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, targetValue: Number(e.target.value) }))}
                  className="form-input"
                  placeholder="목표값"
                  required
                />
              </div>
              <div className="form-group">
                <label>허용 편차 *</label>
                <input
                  type="number"
                  value={formData.tolerance || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, tolerance: Number(e.target.value) }))}
                  className="form-input"
                  placeholder="허용 편차"
                  required
                />
              </div>
            </div>
          </div>
        );

      case 'pattern':
        return (
          <div className="condition-fields">
            <div className="form-group">
              <label>패턴 값 *</label>
              <input
                type="text"
                value={formData.pattern || ''}
                onChange={(e) => setFormData(prev => ({ ...prev, pattern: e.target.value }))}
                className="form-input"
                placeholder="감지할 패턴 또는 값"
                required
              />
            </div>
          </div>
        );

      case 'timeout':
        return (
          <div className="condition-fields">
            <div className="form-group">
              <label>타임아웃 시간 (초) *</label>
              <input
                type="number"
                value={formData.timeWindow || ''}
                onChange={(e) => setFormData(prev => ({ ...prev, timeWindow: Number(e.target.value) }))}
                className="form-input"
                placeholder="타임아웃 시간"
                required
              />
            </div>
          </div>
        );

      default:
        return null;
    }
  };

  // 우선순위별 색상
  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'critical': return 'priority-critical';
      case 'high': return 'priority-high';
      case 'medium': return 'priority-medium';
      case 'low': return 'priority-low';
      default: return 'priority-medium';
    }
  };

  // 고유 값들 추출
  const uniqueCategories = [...new Set(alarmRules.map(r => r.category))];
  const uniqueFactories = [...new Set(alarmRules.map(r => r.factory))];

  return (
    <div className="alarm-settings-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">알람 설정</h1>
        <div className="page-actions">
          <button 
            className="btn btn-primary"
            onClick={handleCreateRule}
          >
            <i className="fas fa-plus"></i>
            새 알람 룰
          </button>
        </div>
      </div>

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-row">
          <div className="filter-group">
            <label>카테고리</label>
            <select
              value={filterCategory}
              onChange={(e) => setFilterCategory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 카테고리</option>
              {uniqueCategories.map(category => (
                <option key={category} value={category}>{category}</option>
              ))}
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
          <div className="stat-value">{alarmRules.filter(r => r.isEnabled).length}</div>
          <div className="stat-label">활성</div>
        </div>
        <div className="stat-card status-disabled">
          <div className="stat-value">{alarmRules.filter(r => !r.isEnabled).length}</div>
          <div className="stat-label">비활성</div>
        </div>
        <div className="stat-card priority-critical">
          <div className="stat-value">{alarmRules.filter(r => r.priority === 'critical').length}</div>
          <div className="stat-label">Critical</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{filteredRules.length}</div>
          <div className="stat-label">필터 결과</div>
        </div>
      </div>

      {/* 알람 룰 목록 */}
      <div className="rules-list">
        <div className="rules-grid">
          {filteredRules.map(rule => (
            <div key={rule.id} className={`rule-card ${rule.isEnabled ? 'enabled' : 'disabled'}`}>
              <div className="rule-header">
                <div className="rule-title">
                  <h3>{rule.name}</h3>
                  <div className="rule-badges">
                    <span className={`priority-badge ${getPriorityColor(rule.priority)}`}>
                      {rule.priority.toUpperCase()}
                    </span>
                    <span className="category-badge">{rule.category}</span>
                    <span className={`status-badge ${rule.isEnabled ? 'enabled' : 'disabled'}`}>
                      {rule.isEnabled ? '활성' : '비활성'}
                    </span>
                  </div>
                </div>
                <div className="rule-actions">
                  <button
                    className={`toggle-btn ${rule.isEnabled ? 'enabled' : 'disabled'}`}
                    onClick={() => handleToggleRule(rule.id)}
                    title={rule.isEnabled ? '비활성화' : '활성화'}
                  >
                    <i className={`fas ${rule.isEnabled ? 'fa-toggle-on' : 'fa-toggle-off'}`}></i>
                  </button>
                  <button
                    className="edit-btn"
                    onClick={() => handleEditRule(rule)}
                    title="편집"
                  >
                    <i className="fas fa-edit"></i>
                  </button>
                  <button
                    className="delete-btn"
                    onClick={() => handleDeleteRule(rule.id)}
                    title="삭제"
                  >
                    <i className="fas fa-trash"></i>
                  </button>
                </div>
              </div>

              <div className="rule-content">
                <div className="rule-description">
                  {rule.description}
                </div>

                <div className="rule-source">
                  <label>데이터 소스:</label>
                  <span>{rule.sourceName}</span>
                </div>

                <div className="rule-condition">
                  <label>조건:</label>
                  <span>
                    {rule.conditionType === 'threshold' && 
                      `임계값 (상한: ${rule.highLimit || 'N/A'}, 하한: ${rule.lowLimit || 'N/A'})`}
                    {rule.conditionType === 'range' && 
                      `범위 (${rule.lowLimit} ~ ${rule.highLimit})`}
                    {rule.conditionType === 'deviation' && 
                      `편차 (목표: ${rule.targetValue}, 허용: ±${rule.tolerance})`}
                    {rule.conditionType === 'pattern' && 
                      `패턴 (${rule.pattern})`}
                    {rule.conditionType === 'timeout' && 
                      `타임아웃 (${rule.timeWindow}초)`}
                  </span>
                </div>

                <div className="rule-notifications">
                  <label>알림:</label>
                  <div className="notification-icons">
                    {rule.emailEnabled && <i className="fas fa-envelope" title="이메일"></i>}
                    {rule.smsEnabled && <i className="fas fa-sms" title="SMS"></i>}
                    {rule.soundEnabled && <i className="fas fa-volume-up" title="소리"></i>}
                    {rule.popupEnabled && <i className="fas fa-window-maximize" title="팝업"></i>}
                  </div>
                </div>

                <div className="rule-stats">
                  <div className="stat-item">
                    <span>발생 횟수:</span>
                    <span>{rule.triggerCount}회</span>
                  </div>
                  <div className="stat-item">
                    <span>최근 발생:</span>
                    <span>{rule.lastTriggered ? rule.lastTriggered.toLocaleString() : 'N/A'}</span>
                  </div>
                </div>
              </div>
            </div>
          ))}
        </div>

        {filteredRules.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-bell-slash empty-icon"></i>
            <div className="empty-title">알람 룰이 없습니다</div>
            <div className="empty-description">
              새 알람 룰을 생성하거나 필터 조건을 변경해보세요.
            </div>
          </div>
        )}
      </div>

      {/* 생성/편집 모달 */}
      {showCreateModal && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>{isEditing ? '알람 룰 편집' : '새 알람 룰 생성'}</h2>
              <button
                className="modal-close-btn"
                onClick={() => setShowCreateModal(false)}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="form-section">
                <h3>기본 정보</h3>
                <div className="form-row">
                  <div className="form-group">
                    <label>알람명 *</label>
                    <input
                      type="text"
                      value={formData.name || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                      className="form-input"
                      placeholder="알람 룰 이름"
                    />
                  </div>
                  <div className="form-group">
                    <label>카테고리 *</label>
                    <select
                      value={formData.category || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, category: e.target.value }))}
                      className="form-select"
                    >
                      <option value="">카테고리 선택</option>
                      <option value="Safety">안전</option>
                      <option value="Process">공정</option>
                      <option value="Production">생산</option>
                      <option value="System">시스템</option>
                      <option value="Quality">품질</option>
                      <option value="Energy">에너지</option>
                    </select>
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label>공장 *</label>
                    <select
                      value={formData.factory || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, factory: e.target.value }))}
                      className="form-select"
                    >
                      <option value="">공장 선택</option>
                      {uniqueFactories.map(factory => (
                        <option key={factory} value={factory}>{factory}</option>
                      ))}
                    </select>
                  </div>
                  <div className="form-group">
                    <label>우선순위 *</label>
                    <select
                      value={formData.priority || 'medium'}
                      onChange={(e) => setFormData(prev => ({ ...prev, priority: e.target.value as any }))}
                      className="form-select"
                    >
                      <option value="low">Low</option>
                      <option value="medium">Medium</option>
                      <option value="high">High</option>
                      <option value="critical">Critical</option>
                    </select>
                  </div>
                </div>

                <div className="form-group">
                  <label>설명</label>
                  <textarea
                    value={formData.description || ''}
                    onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                    className="form-textarea"
                    placeholder="알람 룰에 대한 설명"
                    rows={3}
                  />
                </div>
              </div>

              <div className="form-section">
                <h3>데이터 소스</h3>
                <div className="form-row">
                  <div className="form-group">
                    <label>소스 타입 *</label>
                    <select
                      value={formData.sourceType || 'data_point'}
                      onChange={(e) => setFormData(prev => ({ ...prev, sourceType: e.target.value as any, sourceId: '' }))}
                      className="form-select"
                    >
                      <option value="data_point">데이터 포인트</option>
                      <option value="virtual_point">가상포인트</option>
                      <option value="device_status">디바이스 상태</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label>데이터 소스 *</label>
                    <select
                      value={formData.sourceId || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, sourceId: e.target.value }))}
                      className="form-select"
                    >
                      <option value="">소스 선택</option>
                      {dataSources
                        .filter(source => source.type === formData.sourceType)
                        .map(source => (
                          <option key={source.id} value={source.id}>
                            {source.name} {source.deviceName && `(${source.deviceName})`}
                          </option>
                        ))}
                    </select>
                  </div>
                </div>
              </div>

              <div className="form-section">
                <h3>알람 조건</h3>
                <div className="form-group">
                  <label>조건 타입 *</label>
                  <select
                    value={formData.conditionType || 'threshold'}
                    onChange={(e) => setFormData(prev => ({ 
                      ...prev, 
                      conditionType: e.target.value as any,
                      highLimit: undefined,
                      lowLimit: undefined,
                      targetValue: undefined,
                      tolerance: undefined,
                      pattern: undefined,
                      timeWindow: undefined
                    }))}
                    className="form-select"
                  >
                    <option value="threshold">임계값</option>
                    <option value="range">범위</option>
                    <option value="deviation">편차</option>
                    <option value="pattern">패턴</option>
                    <option value="timeout">타임아웃</option>
                  </select>
                </div>
                
                {renderConditionFields()}
              </div>

              <div className="form-section">
                <h3>알림 설정</h3>
                <div className="notification-options">
                  <div className="checkbox-group">
                    <label className="checkbox-label">
                      <input
                        type="checkbox"
                        checked={formData.emailEnabled || false}
                        onChange={(e) => setFormData(prev => ({ ...prev, emailEnabled: e.target.checked }))}
                      />
                      이메일 알림
                    </label>
                    <label className="checkbox-label">
                      <input
                        type="checkbox"
                        checked={formData.smsEnabled || false}
                        onChange={(e) => setFormData(prev => ({ ...prev, smsEnabled: e.target.checked }))}
                      />
                      SMS 알림
                    </label>
                    <label className="checkbox-label">
                      <input
                        type="checkbox"
                        checked={formData.soundEnabled || false}
                        onChange={(e) => setFormData(prev => ({ ...prev, soundEnabled: e.target.checked }))}
                      />
                      소리 알림
                    </label>
                    <label className="checkbox-label">
                      <input
                        type="checkbox"
                        checked={formData.popupEnabled || false}
                        onChange={(e) => setFormData(prev => ({ ...prev, popupEnabled: e.target.checked }))}
                      />
                      팝업 알림
                    </label>
                  </div>
                </div>

                {formData.emailEnabled && (
                  <div className="form-group">
                    <label>이메일 수신자</label>
                    <input
                      type="text"
                      value={formData.emailRecipients?.join(', ') || ''}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        emailRecipients: e.target.value.split(',').map(email => email.trim()).filter(email => email)
                      }))}
                      className="form-input"
                      placeholder="이메일 주소들을 쉼표로 구분"
                    />
                  </div>
                )}

                <div className="form-group">
                  <label>메시지 템플릿</label>
                  <textarea
                    value={formData.messageTemplate || ''}
                    onChange={(e) => setFormData(prev => ({ ...prev, messageTemplate: e.target.value }))}
                    className="form-textarea"
                    placeholder="알람 메시지 템플릿 ({sourceName}, {currentValue} 등의 변수 사용 가능)"
                    rows={3}
                  />
                </div>
              </div>

              <div className="form-section">
                <h3>기타 설정</h3>
                <div className="form-row">
                  <div className="form-group">
                    <label>심각도 (1-5)</label>
                    <select
                      value={formData.severity || 3}
                      onChange={(e) => setFormData(prev => ({ ...prev, severity: Number(e.target.value) as any }))}
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
                    <label>일정 타입</label>
                    <select
                      value={formData.schedule?.type || 'always'}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        schedule: { ...prev.schedule, type: e.target.value as any }
                      }))}
                      className="form-select"
                    >
                      <option value="always">항상</option>
                      <option value="business_hours">업무시간</option>
                      <option value="custom">사용자 정의</option>
                    </select>
                  </div>
                </div>

                <div className="checkbox-options">
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={formData.autoAcknowledge || false}
                      onChange={(e) => setFormData(prev => ({ ...prev, autoAcknowledge: e.target.checked }))}
                    />
                    자동 승인
                  </label>
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={formData.autoReset || false}
                      onChange={(e) => setFormData(prev => ({ ...prev, autoReset: e.target.checked }))}
                    />
                    자동 리셋
                  </label>
                  <label className="checkbox-label">
                    <input
                      type="checkbox"
                      checked={formData.isEnabled || false}
                      onChange={(e) => setFormData(prev => ({ ...prev, isEnabled: e.target.checked }))}
                    />
                    생성 후 즉시 활성화
                  </label>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-outline"
                onClick={() => setShowCreateModal(false)}
              >
                취소
              </button>
              <button
                className="btn btn-primary"
                onClick={handleSaveRule}
                disabled={!formData.name || !formData.sourceId}
              >
                {isEditing ? '수정' : '생성'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AlarmSettings;
