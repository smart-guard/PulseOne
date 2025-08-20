import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/alarm-rules.css';

interface AlarmRule {
  id: string;
  name: string;
  description?: string;
  
  // 소스 정보
  sourceType: 'data_point' | 'virtual_point' | 'device_status';
  sourceId: string;
  sourceName: string;
  factory: string;
  category: string;
  
  // 알람 타입 및 설정
  alarmType: 'high' | 'low' | 'deviation' | 'discrete';
  priority: 'low' | 'medium' | 'high' | 'critical';
  severity: 1 | 2 | 3 | 4 | 5;
  
  // 임계값 설정
  highLimit?: number;
  lowLimit?: number;
  deadband?: number;
  triggerValue?: any; // for discrete alarms
  
  // 지연 및 타이밍
  triggerDelay?: number; // seconds
  clearDelay?: number; // seconds
  
  // 상태 및 제어
  enabled: boolean;
  autoAcknowledge: boolean;
  suppressDuplicates: boolean;
  
  // 알림 설정
  notifications: {
    email: {
      enabled: boolean;
      recipients: string[];
    };
    sms: {
      enabled: boolean;
      recipients: string[];
    };
    sound: {
      enabled: boolean;
      soundFile?: string;
    };
    popup: {
      enabled: boolean;
    };
  };
  
  // 메시지 템플릿
  messageTemplate: string;
  clearMessageTemplate?: string;
  
  // 스케줄링 (선택적)
  schedule?: {
    enabled: boolean;
    timeRanges: Array<{
      start: string; // HH:MM
      end: string;   // HH:MM
      days: number[]; // 0=Sunday, 1=Monday, etc.
    }>;
  };
  
  // 통계 정보
  triggeredCount: number;
  lastTriggered?: Date;
  averageResponseTime?: number; // seconds
  
  // 메타데이터
  tags: string[];
  createdBy: string;
  createdAt: Date;
  updatedAt: Date;
}

interface AlarmRuleStats {
  totalRules: number;
  enabledRules: number;
  disabledRules: number;
  criticalRules: number;
  recentlyTriggered: number;
  averageResponseTime: number;
}

const AlarmRules: React.FC = () => {
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [stats, setStats] = useState<AlarmRuleStats | null>(null);
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [activeTab, setActiveTab] = useState('all');
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(25);
  
  // 필터 상태
  const [filterPriority, setFilterPriority] = useState('all');
  const [filterCategory, setFilterCategory] = useState('all');
  const [filterFactory, setFilterFactory] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  
  // 폼 상태
  const [formData, setFormData] = useState<Partial<AlarmRule>>({
    name: '',
    description: '',
    sourceType: 'data_point',
    sourceId: '',
    sourceName: '',
    factory: '',
    category: '',
    alarmType: 'high',
    priority: 'medium',
    severity: 3,
    enabled: true,
    autoAcknowledge: false,
    suppressDuplicates: true,
    messageTemplate: '{{source}} 알람이 발생했습니다. 현재값: {{value}}',
    notifications: {
      email: { enabled: false, recipients: [] },
      sms: { enabled: false, recipients: [] },
      sound: { enabled: true },
      popup: { enabled: true }
    },
    tags: []
  });
  
  const [testResult, setTestResult] = useState<{ success: boolean; message: string } | null>(null);

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 목업 알람 규칙 생성
    const mockRules: AlarmRule[] = [];
    const now = new Date();
    
    // 다양한 알람 규칙 생성
    for (let i = 0; i < 50; i++) {
      const priorities: AlarmRule['priority'][] = ['low', 'medium', 'high', 'critical'];
      const categories = ['Safety', 'Process', 'Production', 'System', 'Quality', 'Energy'];
      const factories = ['Factory A', 'Factory B', 'Factory C'];
      const alarmTypes: AlarmRule['alarmType'][] = ['high', 'low', 'deviation', 'discrete'];
      
      const priority = priorities[Math.floor(Math.random() * priorities.length)];
      const category = categories[Math.floor(Math.random() * categories.length)];
      const factory = factories[Math.floor(Math.random() * factories.length)];
      const alarmType = alarmTypes[Math.floor(Math.random() * alarmTypes.length)];
      
      const rule: AlarmRule = {
        id: `rule_${i + 1000}`,
        name: `${category} 알람 ${i + 1}`,
        description: `${factory}의 ${category} 시스템 알람`,
        sourceType: Math.random() > 0.7 ? 'virtual_point' : 'data_point',
        sourceId: `source_${i + 1}`,
        sourceName: `${category} Sensor ${i + 1}`,
        factory,
        category,
        alarmType,
        priority,
        severity: priority === 'critical' ? 5 : priority === 'high' ? 4 : priority === 'medium' ? 3 : Math.floor(Math.random() * 2) + 1,
        highLimit: alarmType === 'high' ? Math.random() * 100 + 50 : undefined,
        lowLimit: alarmType === 'low' ? Math.random() * 50 : undefined,
        deadband: Math.random() * 5,
        triggerValue: alarmType === 'discrete' ? Math.random() > 0.5 ? 1 : 0 : undefined,
        triggerDelay: Math.floor(Math.random() * 30),
        clearDelay: Math.floor(Math.random() * 15),
        enabled: Math.random() > 0.2,
        autoAcknowledge: Math.random() > 0.7,
        suppressDuplicates: Math.random() > 0.3,
        notifications: {
          email: {
            enabled: Math.random() > 0.5,
            recipients: ['engineer@company.com', 'manager@company.com']
          },
          sms: {
            enabled: priority === 'critical' && Math.random() > 0.5,
            recipients: ['+82-10-1234-5678']
          },
          sound: {
            enabled: Math.random() > 0.3,
            soundFile: 'alarm_' + priority + '.wav'
          },
          popup: {
            enabled: Math.random() > 0.2
          }
        },
        messageTemplate: `${category} 알람이 발생했습니다. 현재값: {{value}} (임계값: {{limit}})`,
        clearMessageTemplate: `${category} 알람이 해제되었습니다.`,
        schedule: Math.random() > 0.8 ? {
          enabled: true,
          timeRanges: [{
            start: '08:00',
            end: '18:00',
            days: [1, 2, 3, 4, 5] // 월-금
          }]
        } : undefined,
        triggeredCount: Math.floor(Math.random() * 100),
        lastTriggered: Math.random() > 0.5 ? 
          new Date(now.getTime() - Math.random() * 7 * 24 * 60 * 60 * 1000) : undefined,
        averageResponseTime: Math.random() * 300 + 30,
        tags: [category.toLowerCase(), priority, factory.toLowerCase().replace(' ', '-')],
        createdBy: `user_${Math.floor(Math.random() * 5) + 1}`,
        createdAt: new Date(now.getTime() - Math.random() * 30 * 24 * 60 * 60 * 1000),
        updatedAt: new Date(now.getTime() - Math.random() * 7 * 24 * 60 * 60 * 1000)
      };

      mockRules.push(rule);
    }

    // 통계 계산
    const mockStats: AlarmRuleStats = {
      totalRules: mockRules.length,
      enabledRules: mockRules.filter(r => r.enabled).length,
      disabledRules: mockRules.filter(r => !r.enabled).length,
      criticalRules: mockRules.filter(r => r.priority === 'critical').length,
      recentlyTriggered: mockRules.filter(r => 
        r.lastTriggered && r.lastTriggered > new Date(Date.now() - 24 * 60 * 60 * 1000)
      ).length,
      averageResponseTime: mockRules.reduce((sum, r) => sum + (r.averageResponseTime || 0), 0) / mockRules.length
    };

    setAlarmRules(mockRules);
    setStats(mockStats);
  };

  // 필터링된 규칙 목록
  const filteredRules = alarmRules.filter(rule => {
    // 탭 필터
    if (activeTab !== 'all') {
      switch (activeTab) {
        case 'enabled': if (!rule.enabled) return false; break;
        case 'disabled': if (rule.enabled) return false; break;
        case 'critical': if (rule.priority !== 'critical') return false; break;
        case 'recent': 
          if (!rule.lastTriggered || rule.lastTriggered < new Date(Date.now() - 24 * 60 * 60 * 1000)) return false; 
          break;
      }
    }
    
    // 기타 필터
    const matchesPriority = filterPriority === 'all' || rule.priority === filterPriority;
    const matchesCategory = filterCategory === 'all' || rule.category === filterCategory;
    const matchesFactory = filterFactory === 'all' || rule.factory === filterFactory;
    const matchesStatus = filterStatus === 'all' || 
      (filterStatus === 'enabled' && rule.enabled) ||
      (filterStatus === 'disabled' && !rule.enabled);
    const matchesSearch = searchTerm === '' || 
      rule.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.sourceName.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.description?.toLowerCase().includes(searchTerm.toLowerCase());
    
    return matchesPriority && matchesCategory && matchesFactory && matchesStatus && matchesSearch;
  });

  // 페이지네이션
  const startIndex = (currentPage - 1) * pageSize;
  const endIndex = startIndex + pageSize;
  const currentRules = filteredRules.slice(startIndex, endIndex);

  // 규칙 활성화/비활성화
  const handleToggleRule = (ruleId: string) => {
    setAlarmRules(prev => prev.map(rule =>
      rule.id === ruleId ? { ...rule, enabled: !rule.enabled } : rule
    ));
  };

  // 규칙 삭제
  const handleDeleteRule = (ruleId: string) => {
    if (confirm('정말로 이 알람 규칙을 삭제하시겠습니까?')) {
      setAlarmRules(prev => prev.filter(rule => rule.id !== ruleId));
    }
  };

  // 규칙 복제
  const handleDuplicateRule = (rule: AlarmRule) => {
    const newRule: AlarmRule = {
      ...rule,
      id: `rule_${Date.now()}`,
      name: `${rule.name} (복사본)`,
      createdAt: new Date(),
      updatedAt: new Date(),
      triggeredCount: 0,
      lastTriggered: undefined
    };
    setAlarmRules(prev => [newRule, ...prev]);
  };

  // 폼 제출
  const handleSubmit = () => {
    if (!formData.name || !formData.sourceName) {
      alert('필수 항목을 입력해주세요.');
      return;
    }

    const newRule: AlarmRule = {
      id: selectedRule?.id || `rule_${Date.now()}`,
      name: formData.name!,
      description: formData.description,
      sourceType: formData.sourceType!,
      sourceId: formData.sourceId!,
      sourceName: formData.sourceName!,
      factory: formData.factory!,
      category: formData.category!,
      alarmType: formData.alarmType!,
      priority: formData.priority!,
      severity: formData.severity!,
      highLimit: formData.highLimit,
      lowLimit: formData.lowLimit,
      deadband: formData.deadband,
      triggerValue: formData.triggerValue,
      triggerDelay: formData.triggerDelay,
      clearDelay: formData.clearDelay,
      enabled: formData.enabled!,
      autoAcknowledge: formData.autoAcknowledge!,
      suppressDuplicates: formData.suppressDuplicates!,
      notifications: formData.notifications!,
      messageTemplate: formData.messageTemplate!,
      clearMessageTemplate: formData.clearMessageTemplate,
      schedule: formData.schedule,
      triggeredCount: selectedRule?.triggeredCount || 0,
      lastTriggered: selectedRule?.lastTriggered,
      averageResponseTime: selectedRule?.averageResponseTime,
      tags: formData.tags!,
      createdBy: selectedRule?.createdBy || 'current_user',
      createdAt: selectedRule?.createdAt || new Date(),
      updatedAt: new Date()
    };

    if (selectedRule) {
      // 편집
      setAlarmRules(prev => prev.map(rule => 
        rule.id === selectedRule.id ? newRule : rule
      ));
    } else {
      // 생성
      setAlarmRules(prev => [newRule, ...prev]);
    }

    handleCloseModal();
  };

  // 모달 닫기
  const handleCloseModal = () => {
    setShowCreateModal(false);
    setShowEditModal(false);
    setSelectedRule(null);
    setFormData({
      name: '',
      description: '',
      sourceType: 'data_point',
      sourceId: '',
      sourceName: '',
      factory: '',
      category: '',
      alarmType: 'high',
      priority: 'medium',
      severity: 3,
      enabled: true,
      autoAcknowledge: false,
      suppressDuplicates: true,
      messageTemplate: '{{source}} 알람이 발생했습니다. 현재값: {{value}}',
      notifications: {
        email: { enabled: false, recipients: [] },
        sms: { enabled: false, recipients: [] },
        sound: { enabled: true },
        popup: { enabled: true }
      },
      tags: []
    });
    setTestResult(null);
  };

  // 편집 모달 열기
  const handleEditRule = (rule: AlarmRule) => {
    setSelectedRule(rule);
    setFormData(rule);
    setShowEditModal(true);
  };

  // 알람 테스트
  const handleTestAlarm = () => {
    // 모의 테스트 결과
    const isSuccess = Math.random() > 0.3;
    setTestResult({
      success: isSuccess,
      message: isSuccess 
        ? '알람 테스트가 성공적으로 완료되었습니다. 모든 알림이 정상적으로 발송되었습니다.'
        : '알람 테스트 실패: 이메일 서버 연결 오류 (SMTP 503 Error)'
    });
  };

  // 고유 값들 추출
  const uniqueCategories = [...new Set(alarmRules.map(r => r.category))];
  const uniqueFactories = [...new Set(alarmRules.map(r => r.factory))];

  // 탭별 개수 계산
  const getTabCount = (tab: string) => {
    switch (tab) {
      case 'all': return alarmRules.length;
      case 'enabled': return alarmRules.filter(r => r.enabled).length;
      case 'disabled': return alarmRules.filter(r => !r.enabled).length;
      case 'critical': return alarmRules.filter(r => r.priority === 'critical').length;
      case 'recent': return alarmRules.filter(r => 
        r.lastTriggered && r.lastTriggered > new Date(Date.now() - 24 * 60 * 60 * 1000)
      ).length;
      default: return 0;
    }
  };

  return (
    <div className="alarm-rules-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">알람 규칙 관리</h1>
        <div className="page-actions">
          <button
            className="btn btn-primary"
            onClick={() => setShowCreateModal(true)}
          >
            <i className="fas fa-plus"></i>
            새 알람 규칙
          </button>
          <button className="btn btn-outline">
            <i className="fas fa-download"></i>
            내보내기
          </button>
          <button className="btn btn-outline">
            <i className="fas fa-upload"></i>
            가져오기
          </button>
        </div>
      </div>

      {/* 탭 네비게이션 */}
      <div className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'all' ? 'active' : ''}`}
          onClick={() => setActiveTab('all')}
        >
          <i className="fas fa-list"></i>
          전체
          <span className="tab-badge">{getTabCount('all')}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'enabled' ? 'active' : ''}`}
          onClick={() => setActiveTab('enabled')}
        >
          <i className="fas fa-toggle-on"></i>
          활성화
          <span className="tab-badge">{getTabCount('enabled')}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'disabled' ? 'active' : ''}`}
          onClick={() => setActiveTab('disabled')}
        >
          <i className="fas fa-toggle-off"></i>
          비활성화
          <span className="tab-badge">{getTabCount('disabled')}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'critical' ? 'active' : ''}`}
          onClick={() => setActiveTab('critical')}
        >
          <i className="fas fa-exclamation-triangle"></i>
          Critical
          <span className="tab-badge">{getTabCount('critical')}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'recent' ? 'active' : ''}`}
          onClick={() => setActiveTab('recent')}
        >
          <i className="fas fa-clock"></i>
          최근 발생
          <span className="tab-badge">{getTabCount('recent')}</span>
        </button>
      </div>

      {/* 알람 규칙 관리 */}
      <div className="rules-management">
        <div className="rules-header">
          <h2 className="rules-title">알람 규칙</h2>
          <div className="rules-actions">
            <span className="text-sm text-neutral-600">
              총 {filteredRules.length}개 규칙
            </span>
          </div>
        </div>

        {/* 검색 및 필터 */}
        <div className="rules-filters">
          <div className="filter-group flex-1">
            <label>검색</label>
            <div className="search-container">
              <input
                type="text"
                placeholder="규칙명, 소스명, 설명 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="search-input"
              />
              <i className="fas fa-search search-icon"></i>
            </div>
          </div>

          <div className="filter-group">
            <label>우선순위</label>
            <select
              value={filterPriority}
              onChange={(e) => setFilterPriority(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체</option>
              <option value="critical">Critical</option>
              <option value="high">High</option>
              <option value="medium">Medium</option>
              <option value="low">Low</option>
            </select>
          </div>

          <div className="filter-group">
            <label>카테고리</label>
            <select
              value={filterCategory}
              onChange={(e) => setFilterCategory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체</option>
              {uniqueCategories.map(category => (
                <option key={category} value={category}>{category}</option>
              ))}
            </select>
          </div>

          <div className="filter-group">
            <label>공장</label>
            <select
              value={filterFactory}
              onChange={(e) => setFilterFactory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체</option>
              {uniqueFactories.map(factory => (
                <option key={factory} value={factory}>{factory}</option>
              ))}
            </select>
          </div>

          <div className="filter-group">
            <label>상태</label>
            <select
              value={filterStatus}
              onChange={(e) => setFilterStatus(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체</option>
              <option value="enabled">활성화</option>
              <option value="disabled">비활성화</option>
            </select>
          </div>
        </div>

        {/* 규칙 목록 테이블 */}
        <div className="rules-table">
          <div className="table-header">
            <div className="header-cell">규칙 정보</div>
            <div className="header-cell">알람 타입</div>
            <div className="header-cell">우선순위</div>
            <div className="header-cell">임계값</div>
            <div className="header-cell">상태</div>
            <div className="header-cell">알림 설정</div>
            <div className="header-cell">최근 발생</div>
            <div className="header-cell">액션</div>
          </div>

          {currentRules.map(rule => (
            <div key={rule.id} className={`table-row ${!rule.enabled ? 'disabled' : ''}`}>
              <div className="table-cell rule-info-cell" data-label="규칙 정보">
                <div className="rule-name">{rule.name}</div>
                <div className="rule-description">{rule.description}</div>
                <div className="rule-source">
                  <i className="fas fa-database"></i>
                  {rule.sourceName} ({rule.factory})
                </div>
              </div>

              <div className="table-cell alarm-type-cell" data-label="알람 타입">
                <span className={`alarm-type-badge type-${rule.alarmType}`}>
                  {rule.alarmType === 'high' ? 'HIGH' :
                   rule.alarmType === 'low' ? 'LOW' :
                   rule.alarmType === 'deviation' ? 'DEV' :
                   'DISC'}
                </span>
              </div>

              <div className="table-cell priority-cell" data-label="우선순위">
                <span className={`priority-badge priority-${rule.priority}`}>
                  {rule.priority.toUpperCase()}
                </span>
              </div>

              <div className="table-cell threshold-cell" data-label="임계값">
                {rule.highLimit !== undefined && (
                  <div className="threshold-value">
                    H: {rule.highLimit.toFixed(1)}
                    <span className="threshold-unit">°C</span>
                  </div>
                )}
                {rule.lowLimit !== undefined && (
                  <div className="threshold-value">
                    L: {rule.lowLimit.toFixed(1)}
                    <span className="threshold-unit">°C</span>
                  </div>
                )}
                {rule.triggerValue !== undefined && (
                  <div className="threshold-value">
                    = {String(rule.triggerValue)}
                  </div>
                )}
              </div>

              <div className="table-cell status-cell" data-label="상태">
                <label className="status-toggle">
                  <input
                    type="checkbox"
                    checked={rule.enabled}
                    onChange={() => handleToggleRule(rule.id)}
                  />
                  <span className="toggle-slider"></span>
                </label>
                <div className="status-label">
                  {rule.enabled ? '활성화' : '비활성화'}
                </div>
              </div>

              <div className="table-cell notification-cell" data-label="알림 설정">
                <div className="notification-icon-group">
                  <span 
                    className={`notification-icon ${rule.notifications.email.enabled ? 'enabled' : 'disabled'}`}
                    title="이메일"
                  >
                    <i className="fas fa-envelope"></i>
                  </span>
                  <span 
                    className={`notification-icon ${rule.notifications.sms.enabled ? 'enabled' : 'disabled'}`}
                    title="SMS"
                  >
                    <i className="fas fa-sms"></i>
                  </span>
                  <span 
                    className={`notification-icon ${rule.notifications.sound.enabled ? 'enabled' : 'disabled'}`}
                    title="소리"
                  >
                    <i className="fas fa-volume-up"></i>
                  </span>
                  <span 
                    className={`notification-icon ${rule.notifications.popup.enabled ? 'enabled' : 'disabled'}`}
                    title="팝업"
                  >
                    <i className="fas fa-window-maximize"></i>
                  </span>
                </div>
              </div>

              <div className="table-cell last-triggered-cell" data-label="최근 발생">
                {rule.lastTriggered ? (
                  <>
                    <div className="last-triggered-time">
                      {rule.lastTriggered.toLocaleString()}
                    </div>
                    <div className="last-triggered-count">
                      총 {rule.triggeredCount}회 발생
                    </div>
                  </>
                ) : (
                  <div className="last-triggered-time">발생 없음</div>
                )}
              </div>

              <div className="table-cell action-cell" data-label="액션">
                <div className="action-buttons">
                  <button
                    className="btn btn-sm btn-outline"
                    onClick={() => handleEditRule(rule)}
                    title="편집"
                  >
                    <i className="fas fa-edit"></i>
                  </button>
                  <button
                    className="btn btn-sm btn-outline"
                    onClick={() => handleDuplicateRule(rule)}
                    title="복제"
                  >
                    <i className="fas fa-copy"></i>
                  </button>
                  <button
                    className="btn btn-sm btn-error"
                    onClick={() => handleDeleteRule(rule.id)}
                    title="삭제"
                  >
                    <i className="fas fa-trash"></i>
                  </button>
                </div>
              </div>
            </div>
          ))}
        </div>

        {currentRules.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-exclamation-circle empty-icon"></i>
            <div className="empty-title">알람 규칙이 없습니다</div>
            <div className="empty-description">
              선택한 필터 조건에 해당하는 알람 규칙이 없습니다.
            </div>
          </div>
        )}

        {/* 페이지네이션 */}
        {filteredRules.length > 0 && (
          <div className="pagination-container">
            <div className="pagination-info">
              {startIndex + 1}-{Math.min(endIndex, filteredRules.length)} / {filteredRules.length} 항목
            </div>
            
            <div className="pagination-controls">
              <select
                value={pageSize}
                onChange={(e) => {
                  setPageSize(Number(e.target.value));
                  setCurrentPage(1);
                }}
                className="page-size-select"
              >
                <option value={10}>10개씩</option>
                <option value={25}>25개씩</option>
                <option value={50}>50개씩</option>
                <option value={100}>100개씩</option>
              </select>

              <button
                className="btn btn-sm"
                disabled={currentPage === 1}
                onClick={() => setCurrentPage(1)}
              >
                <i className="fas fa-angle-double-left"></i>
              </button>
              <button
                className="btn btn-sm"
                disabled={currentPage === 1}
                onClick={() => setCurrentPage(prev => prev - 1)}
              >
                <i className="fas fa-angle-left"></i>
              </button>
              
              <span className="page-info">
                {currentPage} / {Math.ceil(filteredRules.length / pageSize)}
              </span>
              
              <button
                className="btn btn-sm"
                disabled={currentPage === Math.ceil(filteredRules.length / pageSize)}
                onClick={() => setCurrentPage(prev => prev + 1)}
              >
                <i className="fas fa-angle-right"></i>
              </button>
              <button
                className="btn btn-sm"
                disabled={currentPage === Math.ceil(filteredRules.length / pageSize)}
                onClick={() => setCurrentPage(Math.ceil(filteredRules.length / pageSize))}
              >
                <i className="fas fa-angle-double-right"></i>
              </button>
            </div>
          </div>
        )}
      </div>

      {/* 규칙 생성/편집 모달 */}
      {(showCreateModal || showEditModal) && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>{selectedRule ? '알람 규칙 편집' : '새 알람 규칙'}</h2>
              <button
                className="modal-close-btn"
                onClick={handleCloseModal}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              {/* 기본 정보 */}
              <div className="form-section">
                <h3>기본 정보</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>규칙명 <span className="required">*</span></label>
                    <input
                      type="text"
                      value={formData.name || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                      className="form-input"
                      placeholder="알람 규칙명을 입력하세요"
                    />
                  </div>

                  <div className="form-group">
                    <label>카테고리</label>
                    <select
                      value={formData.category || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, category: e.target.value }))}
                      className="form-select"
                    >
                      <option value="">카테고리 선택</option>
                      <option value="Safety">Safety</option>
                      <option value="Process">Process</option>
                      <option value="Production">Production</option>
                      <option value="System">System</option>
                      <option value="Quality">Quality</option>
                      <option value="Energy">Energy</option>
                    </select>
                  </div>

                  <div className="form-group full-width">
                    <label>설명</label>
                    <textarea
                      value={formData.description || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                      className="form-input form-textarea"
                      placeholder="알람 규칙에 대한 설명을 입력하세요"
                    />
                  </div>
                </div>
              </div>

              {/* 소스 설정 */}
              <div className="form-section">
                <h3>소스 설정</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>소스 타입</label>
                    <select
                      value={formData.sourceType || 'data_point'}
                      onChange={(e) => setFormData(prev => ({ ...prev, sourceType: e.target.value as any }))}
                      className="form-select"
                    >
                      <option value="data_point">데이터 포인트</option>
                      <option value="virtual_point">가상 포인트</option>
                      <option value="device_status">디바이스 상태</option>
                    </select>
                  </div>

                  <div className="form-group">
                    <label>소스명 <span className="required">*</span></label>
                    <input
                      type="text"
                      value={formData.sourceName || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, sourceName: e.target.value }))}
                      className="form-input"
                      placeholder="모니터링할 소스명을 입력하세요"
                    />
                  </div>

                  <div className="form-group">
                    <label>공장</label>
                    <select
                      value={formData.factory || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, factory: e.target.value }))}
                      className="form-select"
                    >
                      <option value="">공장 선택</option>
                      <option value="Factory A">Factory A</option>
                      <option value="Factory B">Factory B</option>
                      <option value="Factory C">Factory C</option>
                    </select>
                  </div>
                </div>
              </div>

              {/* 알람 설정 */}
              <div className="form-section">
                <h3>알람 설정</h3>
                <div className="form-grid two-columns">
                  <div className="form-group">
                    <label>알람 타입</label>
                    <select
                      value={formData.alarmType || 'high'}
                      onChange={(e) => setFormData(prev => ({ ...prev, alarmType: e.target.value as any }))}
                      className="form-select"
                    >
                      <option value="high">상한 알람</option>
                      <option value="low">하한 알람</option>
                      <option value="deviation">편차 알람</option>
                      <option value="discrete">디지털 알람</option>
                    </select>
                  </div>

                  <div className="form-group">
                    <label>우선순위</label>
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

                  <div className="form-group">
                    <label>심각도 (1-5)</label>
                    <input
                      type="number"
                      min="1"
                      max="5"
                      value={formData.severity || 3}
                      onChange={(e) => setFormData(prev => ({ ...prev, severity: Number(e.target.value) as any }))}
                      className="form-input"
                    />
                  </div>
                </div>

                {/* 임계값 설정 */}
                <div className="threshold-config">
                  {(formData.alarmType === 'high' || formData.alarmType === 'deviation') && (
                    <div className="form-group">
                      <label>상한 임계값</label>
                      <input
                        type="number"
                        step="0.1"
                        value={formData.highLimit || ''}
                        onChange={(e) => setFormData(prev => ({ ...prev, highLimit: Number(e.target.value) }))}
                        className="form-input threshold-input"
                        placeholder="상한값"
                      />
                    </div>
                  )}

                  {(formData.alarmType === 'low' || formData.alarmType === 'deviation') && (
                    <div className="form-group">
                      <label>하한 임계값</label>
                      <input
                        type="number"
                        step="0.1"
                        value={formData.lowLimit || ''}
                        onChange={(e) => setFormData(prev => ({ ...prev, lowLimit: Number(e.target.value) }))}
                        className="form-input threshold-input"
                        placeholder="하한값"
                      />
                    </div>
                  )}

                  {formData.alarmType === 'discrete' && (
                    <div className="form-group">
                      <label>트리거 값</label>
                      <input
                        type="text"
                        value={formData.triggerValue || ''}
                        onChange={(e) => setFormData(prev => ({ ...prev, triggerValue: e.target.value }))}
                        className="form-input threshold-input"
                        placeholder="트리거값 (예: 1, true, ON)"
                      />
                    </div>
                  )}

                  <div className="form-group">
                    <label>데드밴드</label>
                    <input
                      type="number"
                      step="0.1"
                      value={formData.deadband || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, deadband: Number(e.target.value) }))}
                      className="form-input threshold-input"
                      placeholder="0.0"
                    />
                  </div>

                  <select className="form-select threshold-unit-select">
                    <option value="°C">°C</option>
                    <option value="°F">°F</option>
                    <option value="%">%</option>
                    <option value="bar">bar</option>
                    <option value="psi">psi</option>
                    <option value="V">V</option>
                    <option value="A">A</option>
                  </select>
                </div>
              </div>

              {/* 타이밍 설정 */}
              <div className="form-section">
                <h3>타이밍 설정</h3>
                <div className="form-grid two-columns">
                  <div className="form-group">
                    <label>트리거 지연 (초)</label>
                    <input
                      type="number"
                      min="0"
                      value={formData.triggerDelay || 0}
                      onChange={(e) => setFormData(prev => ({ ...prev, triggerDelay: Number(e.target.value) }))}
                      className="form-input"
                      placeholder="0"
                    />
                  </div>

                  <div className="form-group">
                    <label>해제 지연 (초)</label>
                    <input
                      type="number"
                      min="0"
                      value={formData.clearDelay || 0}
                      onChange={(e) => setFormData(prev => ({ ...prev, clearDelay: Number(e.target.value) }))}
                      className="form-input"
                      placeholder="0"
                    />
                  </div>
                </div>

                <div className="checkbox-group">
                  <div className="checkbox-item">
                    <input
                      type="checkbox"
                      id="autoAck"
                      checked={formData.autoAcknowledge || false}
                      onChange={(e) => setFormData(prev => ({ ...prev, autoAcknowledge: e.target.checked }))}
                    />
                    <label htmlFor="autoAck">자동 승인</label>
                  </div>
                  <div className="checkbox-item">
                    <input
                      type="checkbox"
                      id="suppressDup"
                      checked={formData.suppressDuplicates || false}
                      onChange={(e) => setFormData(prev => ({ ...prev, suppressDuplicates: e.target.checked }))}
                    />
                    <label htmlFor="suppressDup">중복 알람 억제</label>
                  </div>
                </div>
              </div>

              {/* 알림 설정 */}
              <div className="form-section">
                <h3>알림 설정</h3>
                
                {/* 이메일 알림 */}
                <div className="notification-config">
                  <div className="notification-toggle">
                    <label>이메일 알림</label>
                    <label className="status-toggle">
                      <input
                        type="checkbox"
                        checked={formData.notifications?.email.enabled || false}
                        onChange={(e) => setFormData(prev => ({
                          ...prev,
                          notifications: {
                            ...prev.notifications!,
                            email: { ...prev.notifications!.email, enabled: e.target.checked }
                          }
                        }))}
                      />
                      <span className="toggle-slider"></span>
                    </label>
                  </div>
                  {formData.notifications?.email.enabled && (
                    <div className="notification-details">
                      <div className="form-group">
                        <label>수신자 (쉼표로 구분)</label>
                        <input
                          type="text"
                          value={formData.notifications.email.recipients.join(', ')}
                          onChange={(e) => setFormData(prev => ({
                            ...prev,
                            notifications: {
                              ...prev.notifications!,
                              email: {
                                ...prev.notifications!.email,
                                recipients: e.target.value.split(',').map(s => s.trim()).filter(s => s)
                              }
                            }
                          }))}
                          className="form-input"
                          placeholder="user1@company.com, user2@company.com"
                        />
                      </div>
                    </div>
                  )}
                </div>

                {/* SMS 알림 */}
                <div className="notification-config">
                  <div className="notification-toggle">
                    <label>SMS 알림</label>
                    <label className="status-toggle">
                      <input
                        type="checkbox"
                        checked={formData.notifications?.sms.enabled || false}
                        onChange={(e) => setFormData(prev => ({
                          ...prev,
                          notifications: {
                            ...prev.notifications!,
                            sms: { ...prev.notifications!.sms, enabled: e.target.checked }
                          }
                        }))}
                      />
                      <span className="toggle-slider"></span>
                    </label>
                  </div>
                  {formData.notifications?.sms.enabled && (
                    <div className="notification-details">
                      <div className="form-group">
                        <label>수신자 번호 (쉼표로 구분)</label>
                        <input
                          type="text"
                          value={formData.notifications.sms.recipients.join(', ')}
                          onChange={(e) => setFormData(prev => ({
                            ...prev,
                            notifications: {
                              ...prev.notifications!,
                              sms: {
                                ...prev.notifications!.sms,
                                recipients: e.target.value.split(',').map(s => s.trim()).filter(s => s)
                              }
                            }
                          }))}
                          className="form-input"
                          placeholder="+82-10-1234-5678, +82-10-9876-5432"
                        />
                      </div>
                    </div>
                  )}
                </div>

                {/* 소리 알림 */}
                <div className="notification-config">
                  <div className="notification-toggle">
                    <label>소리 알림</label>
                    <label className="status-toggle">
                      <input
                        type="checkbox"
                        checked={formData.notifications?.sound.enabled || false}
                        onChange={(e) => setFormData(prev => ({
                          ...prev,
                          notifications: {
                            ...prev.notifications!,
                            sound: { ...prev.notifications!.sound, enabled: e.target.checked }
                          }
                        }))}
                      />
                      <span className="toggle-slider"></span>
                    </label>
                  </div>
                </div>

                {/* 팝업 알림 */}
                <div className="notification-config">
                  <div className="notification-toggle">
                    <label>팝업 알림</label>
                    <label className="status-toggle">
                      <input
                        type="checkbox"
                        checked={formData.notifications?.popup.enabled || false}
                        onChange={(e) => setFormData(prev => ({
                          ...prev,
                          notifications: {
                            ...prev.notifications!,
                            popup: { ...prev.notifications!.popup, enabled: e.target.checked }
                          }
                        }))}
                      />
                      <span className="toggle-slider"></span>
                    </label>
                  </div>
                </div>
              </div>

              {/* 메시지 템플릿 */}
              <div className="form-section">
                <h3>메시지 템플릿</h3>
                <div className="form-grid">
                  <div className="form-group full-width">
                    <label>알람 메시지</label>
                    <textarea
                      value={formData.messageTemplate || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, messageTemplate: e.target.value }))}
                      className="form-input form-textarea"
                      placeholder="{{source}} 알람이 발생했습니다. 현재값: {{value}} (임계값: {{limit}})"
                    />
                    <small className="text-xs text-neutral-500">
                      사용 가능한 변수: {'{{source}}, {{value}}, {{limit}}, {{time}}, {{factory}}'}
                    </small>
                  </div>

                  <div className="form-group full-width">
                    <label>해제 메시지</label>
                    <textarea
                      value={formData.clearMessageTemplate || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, clearMessageTemplate: e.target.value }))}
                      className="form-input form-textarea"
                      placeholder="{{source}} 알람이 해제되었습니다."
                    />
                  </div>
                </div>
              </div>

              {/* 태그 */}
              <div className="form-section">
                <h3>태그</h3>
                <div className="form-group">
                  <label>태그 (쉼표로 구분)</label>
                  <input
                    type="text"
                    value={formData.tags?.join(', ') || ''}
                    onChange={(e) => setFormData(prev => ({
                      ...prev,
                      tags: e.target.value.split(',').map(s => s.trim()).filter(s => s)
                    }))}
                    className="form-input"
                    placeholder="safety, critical, production"
                  />
                </div>
              </div>

              {/* 테스트 섹션 */}
              <div className="form-section">
                <h3>알람 테스트</h3>
                <div className="test-section">
                  <p className="text-sm text-neutral-600 mb-3">
                    설정된 알람 규칙을 테스트하여 알림이 정상적으로 작동하는지 확인할 수 있습니다.
                  </p>
                  <button
                    type="button"
                    className="btn btn-outline"
                    onClick={handleTestAlarm}
                  >
                    <i className="fas fa-play"></i>
                    알람 테스트 실행
                  </button>
                  
                  {testResult && (
                    <div className={`test-result ${testResult.success ? 'success' : 'error'}`}>
                      <i className={`fas ${testResult.success ? 'fa-check-circle' : 'fa-exclamation-circle'}`}></i>
                      {testResult.message}
                    </div>
                  )}
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-primary"
                onClick={handleSubmit}
              >
                <i className="fas fa-save"></i>
                {selectedRule ? '저장' : '생성'}
              </button>
              <button
                className="btn btn-outline"
                onClick={handleCloseModal}
              >
                취소
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AlarmRules;

