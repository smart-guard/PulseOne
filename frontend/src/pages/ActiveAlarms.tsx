import React, { useState, useEffect, useRef } from 'react';
import '../styles/base.css';
import '../styles/active-alarms.css';

interface ActiveAlarm {
  id: string;
  ruleId: string;
  ruleName: string;
  priority: 'critical' | 'high' | 'medium' | 'low';
  severity: 1 | 2 | 3 | 4 | 5;
  
  // 소스 정보
  sourceType: 'data_point' | 'virtual_point' | 'device_status';
  sourceName: string;
  factory: string;
  category: string;
  
  // 알람 정보
  message: string;
  description?: string;
  currentValue: any;
  thresholdValue: any;
  unit: string;
  
  // 시간 정보
  triggeredAt: Date;
  acknowledgedAt?: Date;
  acknowledgedBy?: string;
  acknowledgmentComment?: string;
  duration: number; // milliseconds
  occurrenceCount: number;
  
  // 상태
  status: 'active' | 'acknowledged' | 'auto_acknowledged';
  isNew: boolean;
  escalated: boolean;
  escalationLevel: number;
  
  // 알림 정보
  soundPlayed: boolean;
  emailSent: boolean;
  smsSent: boolean;
  
  // 메타데이터
  tags: string[];
}

interface AlarmStats {
  totalActive: number;
  critical: number;
  high: number;
  medium: number;
  low: number;
  acknowledged: number;
  newAlarms: number;
  escalated: number;
  avgResponseTime: number; // minutes
}

const ActiveAlarms: React.FC = () => {
  const [alarms, setAlarms] = useState<ActiveAlarm[]>([]);
  const [stats, setStats] = useState<AlarmStats>({
    totalActive: 0,
    critical: 0,
    high: 0,
    medium: 0,
    low: 0,
    acknowledged: 0,
    newAlarms: 0,
    escalated: 0,
    avgResponseTime: 0
  });
  
  const [isRealtime, setIsRealtime] = useState(true);
  const [soundEnabled, setSoundEnabled] = useState(true);
  const [lastUpdate, setLastUpdate] = useState(new Date());
  const [selectedAlarms, setSelectedAlarms] = useState<Set<string>>(new Set());
  const [showAcknowledgeModal, setShowAcknowledgeModal] = useState(false);
  const [acknowledgmentComment, setAcknowledgmentComment] = useState('');
  
  // 필터 상태
  const [filterPriority, setFilterPriority] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [filterCategory, setFilterCategory] = useState('all');
  const [filterFactory, setFilterFactory] = useState('all');
  const [quickFilter, setQuickFilter] = useState('all');
  
  const audioRef = useRef<HTMLAudioElement>(null);
  const intervalRef = useRef<NodeJS.Timeout>();

  useEffect(() => {
    initializeMockData();
    
    if (isRealtime) {
      startRealTimeUpdates();
    }
    
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [isRealtime]);

  const initializeMockData = () => {
    const mockAlarms: ActiveAlarm[] = [];
    const now = new Date();
    
    const priorities: ActiveAlarm['priority'][] = ['critical', 'high', 'medium', 'low'];
    const categories = ['Safety', 'Process', 'Production', 'System', 'Quality', 'Energy'];
    const factories = ['Factory A', 'Factory B', 'Factory C'];
    const units = ['°C', '%', 'bar', 'psi', 'V', 'A', 'Hz', 'RPM'];

    // 활성 알람 생성
    for (let i = 0; i < 25; i++) {
      const priority = priorities[Math.floor(Math.random() * priorities.length)];
      const category = categories[Math.floor(Math.random() * categories.length)];
      const factory = factories[Math.floor(Math.random() * factories.length)];
      const unit = units[Math.floor(Math.random() * units.length)];
      const triggeredAt = new Date(now.getTime() - Math.random() * 24 * 60 * 60 * 1000);
      const currentValue = (Math.random() * 100).toFixed(1);
      const thresholdValue = (parseFloat(currentValue) - Math.random() * 20).toFixed(1);
      const isAcknowledged = Math.random() > 0.7;

      const alarm: ActiveAlarm = {
        id: `alarm_${i + 1000}`,
        ruleId: `rule_${Math.floor(Math.random() * 50) + 1}`,
        ruleName: `${category} ${Math.random() > 0.5 ? '상한' : '하한'} 알람`,
        priority,
        severity: priority === 'critical' ? 5 : priority === 'high' ? 4 : priority === 'medium' ? 3 : Math.floor(Math.random() * 2) + 1,
        sourceType: Math.random() > 0.7 ? 'virtual_point' : 'data_point',
        sourceName: `${category} Sensor ${Math.floor(Math.random() * 10) + 1}`,
        factory,
        category,
        message: `${category} 센서에서 임계값을 초과했습니다. 즉시 확인이 필요합니다.`,
        description: `${factory}의 ${category} 시스템에서 비정상 값이 감지되었습니다.`,
        currentValue: parseFloat(currentValue),
        thresholdValue: parseFloat(thresholdValue),
        unit,
        triggeredAt,
        acknowledgedAt: isAcknowledged ? new Date(triggeredAt.getTime() + Math.random() * 60 * 60 * 1000) : undefined,
        acknowledgedBy: isAcknowledged ? `user_${Math.floor(Math.random() * 5) + 1}` : undefined,
        acknowledgmentComment: isAcknowledged ? '확인 완료' : undefined,
        duration: now.getTime() - triggeredAt.getTime(),
        occurrenceCount: Math.floor(Math.random() * 10) + 1,
        status: isAcknowledged ? 'acknowledged' : 'active',
        isNew: Math.random() > 0.8,
        escalated: priority === 'critical' && Math.random() > 0.6,
        escalationLevel: Math.floor(Math.random() * 3),
        soundPlayed: Math.random() > 0.3,
        emailSent: Math.random() > 0.4,
        smsSent: priority === 'critical' && Math.random() > 0.5,
        tags: [category.toLowerCase(), priority, factory.toLowerCase().replace(' ', '-')]
      };

      mockAlarms.push(alarm);
    }

    // 우선순위와 시간별 정렬
    mockAlarms.sort((a, b) => {
      const priorityOrder = { critical: 4, high: 3, medium: 2, low: 1 };
      if (priorityOrder[a.priority] !== priorityOrder[b.priority]) {
        return priorityOrder[b.priority] - priorityOrder[a.priority];
      }
      return b.triggeredAt.getTime() - a.triggeredAt.getTime();
    });

    setAlarms(mockAlarms);
    updateStats(mockAlarms);
  };

  const updateStats = (alarmsList: ActiveAlarm[]) => {
    const newStats: AlarmStats = {
      totalActive: alarmsList.length,
      critical: alarmsList.filter(a => a.priority === 'critical' && a.status === 'active').length,
      high: alarmsList.filter(a => a.priority === 'high' && a.status === 'active').length,
      medium: alarmsList.filter(a => a.priority === 'medium' && a.status === 'active').length,
      low: alarmsList.filter(a => a.priority === 'low' && a.status === 'active').length,
      acknowledged: alarmsList.filter(a => a.status === 'acknowledged').length,
      newAlarms: alarmsList.filter(a => a.isNew).length,
      escalated: alarmsList.filter(a => a.escalated).length,
      avgResponseTime: calculateAvgResponseTime(alarmsList)
    };
    setStats(newStats);
  };

  const calculateAvgResponseTime = (alarmsList: ActiveAlarm[]): number => {
    const acknowledgedAlarms = alarmsList.filter(a => a.acknowledgedAt);
    if (acknowledgedAlarms.length === 0) return 0;
    
    const totalResponseTime = acknowledgedAlarms.reduce((sum, alarm) => {
      if (alarm.acknowledgedAt) {
        return sum + (alarm.acknowledgedAt.getTime() - alarm.triggeredAt.getTime());
      }
      return sum;
    }, 0);
    
    return Math.round(totalResponseTime / acknowledgedAlarms.length / 60000); // minutes
  };

  const startRealTimeUpdates = () => {
    intervalRef.current = setInterval(() => {
      // 실제 환경에서는 WebSocket이나 SSE를 사용
      setLastUpdate(new Date());
      
      // 새로운 알람 시뮬레이션 (5% 확률)
      if (Math.random() < 0.05) {
        simulateNewAlarm();
      }
      
      // 알람 상태 업데이트 (duration 등)
      setAlarms(prev => prev.map(alarm => ({
        ...alarm,
        duration: Date.now() - alarm.triggeredAt.getTime(),
        isNew: alarm.isNew && (Date.now() - alarm.triggeredAt.getTime()) < 30000 // 30초 후 new 제거
      })));
    }, 2000); // 2초마다 업데이트
  };

  const simulateNewAlarm = () => {
    const priorities: ActiveAlarm['priority'][] = ['critical', 'high', 'medium', 'low'];
    const categories = ['Safety', 'Process', 'Production', 'System'];
    const factories = ['Factory A', 'Factory B', 'Factory C'];
    
    const priority = priorities[Math.floor(Math.random() * priorities.length)];
    const category = categories[Math.floor(Math.random() * categories.length)];
    const factory = factories[Math.floor(Math.random() * factories.length)];
    const currentValue = (Math.random() * 100).toFixed(1);
    
    const newAlarm: ActiveAlarm = {
      id: `alarm_${Date.now()}`,
      ruleId: `rule_${Math.floor(Math.random() * 50) + 1}`,
      ruleName: `${category} 긴급 알람`,
      priority,
      severity: priority === 'critical' ? 5 : 4,
      sourceType: 'data_point',
      sourceName: `${category} Sensor ${Math.floor(Math.random() * 10) + 1}`,
      factory,
      category,
      message: `${category} 센서에서 긴급 상황이 발생했습니다!`,
      description: `${factory}의 ${category} 시스템에서 위험 수준에 도달했습니다.`,
      currentValue: parseFloat(currentValue),
      thresholdValue: parseFloat(currentValue) - 10,
      unit: '°C',
      triggeredAt: new Date(),
      duration: 0,
      occurrenceCount: 1,
      status: 'active',
      isNew: true,
      escalated: priority === 'critical',
      escalationLevel: 0,
      soundPlayed: false,
      emailSent: false,
      smsSent: false,
      tags: [category.toLowerCase(), priority, factory.toLowerCase().replace(' ', '-')]
    };

    setAlarms(prev => {
      const updated = [newAlarm, ...prev];
      updateStats(updated);
      return updated;
    });

    // 사운드 재생
    if (soundEnabled && priority === 'critical') {
      playAlarmSound();
    }
  };

  const playAlarmSound = () => {
    if (audioRef.current) {
      audioRef.current.play().catch(e => console.log('Audio play failed:', e));
    }
  };

  // 필터링된 알람 목록
  const filteredAlarms = alarms.filter(alarm => {
    // Quick Filter
    if (quickFilter === 'critical' && alarm.priority !== 'critical') return false;
    if (quickFilter === 'new' && !alarm.isNew) return false;
    if (quickFilter === 'unacknowledged' && alarm.status !== 'active') return false;
    
    // Regular Filters
    const matchesPriority = filterPriority === 'all' || alarm.priority === filterPriority;
    const matchesStatus = filterStatus === 'all' || 
      (filterStatus === 'active' && alarm.status === 'active') ||
      (filterStatus === 'acknowledged' && alarm.status === 'acknowledged');
    const matchesCategory = filterCategory === 'all' || alarm.category === filterCategory;
    const matchesFactory = filterFactory === 'all' || alarm.factory === filterFactory;
    
    return matchesPriority && matchesStatus && matchesCategory && matchesFactory;
  });

  // 알람 승인
  const handleAcknowledgeAlarm = (alarmId: string, comment?: string) => {
    setAlarms(prev => prev.map(alarm =>
      alarm.id === alarmId
        ? {
            ...alarm,
            status: 'acknowledged',
            acknowledgedAt: new Date(),
            acknowledgedBy: 'current_user',
            acknowledgmentComment: comment || '알람 확인 완료'
          }
        : alarm
    ));
    setSelectedAlarms(prev => {
      const updated = new Set(prev);
      updated.delete(alarmId);
      return updated;
    });
  };

  // 대량 승인
  const handleBulkAcknowledge = () => {
    if (selectedAlarms.size === 0) return;
    
    if (!acknowledgmentComment.trim()) {
      alert('승인 코멘트를 입력해주세요.');
      return;
    }
    
    setAlarms(prev => prev.map(alarm =>
      selectedAlarms.has(alarm.id)
        ? {
            ...alarm,
            status: 'acknowledged',
            acknowledgedAt: new Date(),
            acknowledgedBy: 'current_user',
            acknowledgmentComment
          }
        : alarm
    ));
    
    setSelectedAlarms(new Set());
    setShowAcknowledgeModal(false);
    setAcknowledgmentComment('');
  };

  // 알람 선택
  const handleSelectAlarm = (alarmId: string, checked: boolean) => {
    setSelectedAlarms(prev => {
      const updated = new Set(prev);
      if (checked) {
        updated.add(alarmId);
      } else {
        updated.delete(alarmId);
      }
      return updated;
    });
  };

  // 전체 선택
  const handleSelectAll = (checked: boolean) => {
    if (checked) {
      setSelectedAlarms(new Set(filteredAlarms.filter(a => a.status === 'active').map(a => a.id)));
    } else {
      setSelectedAlarms(new Set());
    }
  };

  // 기간 포맷
  const formatDuration = (ms: number): string => {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) return `${days}일 ${hours % 24}시간`;
    if (hours > 0) return `${hours}시간 ${minutes % 60}분`;
    if (minutes > 0) return `${minutes}분`;
    return `${seconds}초`;
  };

  // 우선순위별 아이콘
  const getPriorityIcon = (priority: string) => {
    switch (priority) {
      case 'critical': return 'fa-exclamation-triangle';
      case 'high': return 'fa-exclamation-circle';
      case 'medium': return 'fa-info-circle';
      case 'low': return 'fa-check-circle';
      default: return 'fa-circle';
    }
  };

  // 고유 값들 추출
  const uniqueCategories = [...new Set(alarms.map(a => a.category))];
  const uniqueFactories = [...new Set(alarms.map(a => a.factory))];

  // Critical 알람이 있는지 확인
  const hasCriticalAlarms = stats.critical > 0;

  return (
    <div className="active-alarms-container">
      {/* 히든 오디오 엘리먼트 */}
      <audio ref={audioRef} preload="auto">
        <source src="/alarm-sound.mp3" type="audio/mpeg" />
        <source src="/alarm-sound.wav" type="audio/wav" />
      </audio>

      {/* 긴급 알람 배너 */}
      {hasCriticalAlarms && (
        <div className="emergency-banner">
          <div className="emergency-icon">
            <i className="fas fa-exclamation-triangle"></i>
          </div>
          <div className="emergency-content">
            <h2>🚨 긴급 알람 발생</h2>
            <p>{stats.critical}개의 Critical 알람이 활성 상태입니다. 즉시 확인하세요!</p>
          </div>
          <div className="emergency-actions">
            <button 
              className="btn btn-sm"
              style={{ background: 'rgba(255,255,255,0.2)', color: 'white', border: '1px solid rgba(255,255,255,0.3)' }}
              onClick={() => setQuickFilter('critical')}
            >
              Critical 알람 보기
            </button>
          </div>
        </div>
      )}

      {/* 알람 통계 */}
      <div className="alarm-stats-panel">
        <div className="alarm-stat-card critical">
          <div className="alarm-stat-icon">
            <i className="fas fa-exclamation-triangle"></i>
          </div>
          <div className="alarm-stat-value">{stats.critical}</div>
          <div className="alarm-stat-label">Critical</div>
          {stats.critical > 0 && <div className="alarm-trend up">+{stats.critical}</div>}
        </div>
        <div className="alarm-stat-card high">
          <div className="alarm-stat-icon">
            <i className="fas fa-exclamation-circle"></i>
          </div>
          <div className="alarm-stat-value">{stats.high}</div>
          <div className="alarm-stat-label">High</div>
        </div>
        <div className="alarm-stat-card medium">
          <div className="alarm-stat-icon">
            <i className="fas fa-info-circle"></i>
          </div>
          <div className="alarm-stat-value">{stats.medium}</div>
          <div className="alarm-stat-label">Medium</div>
        </div>
        <div className="alarm-stat-card low">
          <div className="alarm-stat-icon">
            <i className="fas fa-check-circle"></i>
          </div>
          <div className="alarm-stat-value">{stats.low}</div>
          <div className="alarm-stat-label">Low</div>
        </div>
        <div className="alarm-stat-card">
          <div className="alarm-stat-icon">
            <i className="fas fa-clock text-neutral-600"></i>
          </div>
          <div className="alarm-stat-value">{stats.avgResponseTime}</div>
          <div className="alarm-stat-label">평균 응답시간 (분)</div>
        </div>
        <div className="alarm-stat-card">
          <div className="alarm-stat-icon">
            <i className="fas fa-bell text-warning-600"></i>
          </div>
          <div className="alarm-stat-value">{stats.newAlarms}</div>
          <div className="alarm-stat-label">새 알람</div>
        </div>
      </div>

      {/* 실시간 제어 패널 */}
      <div className="realtime-controls">
        <div className="realtime-status">
          <div className="live-indicator">
            <div className="live-dot"></div>
            실시간 모니터링
          </div>
          <div className="last-update">
            마지막 업데이트: {lastUpdate.toLocaleTimeString()}
          </div>
        </div>

        <div className="sound-controls">
          <label htmlFor="soundToggle" className="text-sm font-medium text-neutral-700 mr-2">
            알람 사운드
          </label>
          <label className="sound-toggle">
            <input
              id="soundToggle"
              type="checkbox"
              checked={soundEnabled}
              onChange={(e) => setSoundEnabled(e.target.checked)}
            />
            <span className="sound-slider"></span>
          </label>
        </div>
      </div>

      {/* 필터 패널 */}
      <div className="alarm-filters">
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
          <label>상태</label>
          <select
            value={filterStatus}
            onChange={(e) => setFilterStatus(e.target.value)}
            className="filter-select"
          >
            <option value="all">전체</option>
            <option value="active">활성</option>
            <option value="acknowledged">승인됨</option>
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

        <div className="quick-filters">
          <button
            className={`quick-filter-btn ${quickFilter === 'all' ? 'active' : ''}`}
            onClick={() => setQuickFilter('all')}
          >
            <i className="fas fa-list"></i>
            전체
          </button>
          <button
            className={`quick-filter-btn ${quickFilter === 'critical' ? 'critical' : ''} ${quickFilter === 'critical' ? 'active' : ''}`}
            onClick={() => setQuickFilter('critical')}
          >
            <i className="fas fa-exclamation-triangle"></i>
            Critical
          </button>
          <button
            className={`quick-filter-btn ${quickFilter === 'new' ? 'active' : ''}`}
            onClick={() => setQuickFilter('new')}
          >
            <i className="fas fa-bell"></i>
            새 알람
          </button>
          <button
            className={`quick-filter-btn ${quickFilter === 'unacknowledged' ? 'active' : ''}`}
            onClick={() => setQuickFilter('unacknowledged')}
          >
            <i className="fas fa-clock"></i>
            미승인
          </button>
        </div>
      </div>

      {/* 알람 목록 */}
      <div className="alarms-list">
        <div className="alarms-header">
          <h2 className="alarms-title">
            <i className="fas fa-bell"></i>
            활성 알람
            <span className="alarm-count">{filteredAlarms.length}</span>
          </h2>
          
          {selectedAlarms.size > 0 && (
            <div className="bulk-actions">
              <span className="text-sm text-neutral-600 mr-3">
                {selectedAlarms.size}개 선택됨
              </span>
              <button
                className="btn btn-sm btn-success"
                onClick={() => setShowAcknowledgeModal(true)}
              >
                <i className="fas fa-check"></i>
                일괄 승인
              </button>
              <button
                className="btn btn-sm btn-outline"
                onClick={() => setSelectedAlarms(new Set())}
              >
                <i className="fas fa-times"></i>
                선택 해제
              </button>
            </div>
          )}
        </div>

        {filteredAlarms.length > 0 ? (
          <>
            {/* 전체 선택 체크박스 */}
            <div className="alarm-card" style={{ background: 'var(--neutral-50)', borderLeft: 'none' }}>
              <div className="alarm-header">
                <div className="alarm-main-info">
                  <label className="flex items-center gap-2 text-sm font-medium text-neutral-700">
                    <input
                      type="checkbox"
                      checked={selectedAlarms.size === filteredAlarms.filter(a => a.status === 'active').length && filteredAlarms.filter(a => a.status === 'active').length > 0}
                      onChange={(e) => handleSelectAll(e.target.checked)}
                    />
                    전체 선택 (활성 알람만)
                  </label>
                </div>
              </div>
            </div>

            {filteredAlarms.map(alarm => (
              <div
                key={alarm.id}
                className={`alarm-card ${alarm.priority} ${alarm.isNew ? 'new' : ''} ${alarm.status === 'acknowledged' ? 'acknowledged' : ''}`}
              >
                <div className="alarm-header">
                  <div className="alarm-main-info">
                    <div className="flex items-start gap-3">
                      {alarm.status === 'active' && (
                        <input
                          type="checkbox"
                          checked={selectedAlarms.has(alarm.id)}
                          onChange={(e) => handleSelectAlarm(alarm.id, e.target.checked)}
                          className="mt-1"
                        />
                      )}
                      <div className="flex-1">
                        <div className={`alarm-priority ${alarm.priority}`}>
                          <i className={`fas ${getPriorityIcon(alarm.priority)}`}></i>
                          {alarm.priority.toUpperCase()}
                          {alarm.isNew && <span className="new-alarm-badge">NEW</span>}
                        </div>
                        <h3 className="alarm-title">{alarm.ruleName}</h3>
                        <div className="alarm-source">
                          <i className="fas fa-database"></i>
                          {alarm.sourceName} ({alarm.factory})
                          <span className="text-neutral-400">•</span>
                          {alarm.category}
                        </div>
                      </div>
                    </div>
                  </div>
                  <div className="alarm-time">
                    <div className="text-neutral-700">
                      {alarm.triggeredAt.toLocaleString()}
                    </div>
                    <div className="text-neutral-500">
                      {formatDuration(alarm.duration)} 전
                    </div>
                  </div>
                </div>

                <div className="alarm-body">
                  <div className="alarm-details">
                    <p className="alarm-message">{alarm.message}</p>
                    
                    <div className="alarm-value">
                      <div className="current-value">
                        현재값: {alarm.currentValue} {alarm.unit}
                      </div>
                      <div className="threshold-info">
                        임계값: {alarm.thresholdValue} {alarm.unit}
                      </div>
                    </div>

                    <div className="alarm-meta">
                      <div className="alarm-duration">
                        <i className="fas fa-clock"></i>
                        지속시간: {formatDuration(alarm.duration)}
                      </div>
                      <div className="alarm-count-info">
                        <i className="fas fa-repeat"></i>
                        발생횟수: {alarm.occurrenceCount}회
                      </div>
                      {alarm.escalated && (
                        <div className="alarm-count-info">
                          <i className="fas fa-arrow-up text-error-500"></i>
                          에스컬레이션됨
                        </div>
                      )}
                    </div>

                    {alarm.status === 'acknowledged' && alarm.acknowledgedBy && (
                      <div className="mt-3 p-2 bg-success-50 rounded text-sm">
                        <strong>승인:</strong> {alarm.acknowledgedBy} ({alarm.acknowledgedAt?.toLocaleString()})
                        {alarm.acknowledgmentComment && (
                          <div className="text-success-700 mt-1">"{alarm.acknowledgmentComment}"</div>
                        )}
                      </div>
                    )}
                  </div>

                  <div className="alarm-actions">
                    {alarm.status === 'active' ? (
                      <>
                        <button
                          className="btn btn-sm btn-success"
                          onClick={() => handleAcknowledgeAlarm(alarm.id)}
                        >
                          <i className="fas fa-check"></i>
                          승인
                        </button>
                        <button className="btn btn-sm btn-outline">
                          <i className="fas fa-eye"></i>
                          상세
                        </button>
                      </>
                    ) : (
                      <button className="btn btn-sm btn-outline">
                        <i className="fas fa-eye"></i>
                        상세 보기
                      </button>
                    )}
                  </div>
                </div>

                {/* 알림 상태 표시 */}
                <div className="absolute top-4 right-6 flex gap-1">
                  {alarm.emailSent && (
                    <i className="fas fa-envelope text-primary-500" title="이메일 발송됨"></i>
                  )}
                  {alarm.smsSent && (
                    <i className="fas fa-sms text-primary-500" title="SMS 발송됨"></i>
                  )}
                  {alarm.soundPlayed && (
                    <i className="fas fa-volume-up text-primary-500" title="알람음 재생됨"></i>
                  )}
                </div>
              </div>
            ))}
          </>
        ) : (
          <div className="empty-alarms">
            <div className="empty-icon">
              <i className="fas fa-check-circle"></i>
            </div>
            <h3 className="empty-title">모든 시스템이 정상입니다</h3>
            <p className="empty-description">
              현재 활성 알람이 없습니다. 모든 시스템이 정상적으로 작동하고 있습니다.
            </p>
          </div>
        )}
      </div>

      {/* 일괄 승인 모달 */}
      {showAcknowledgeModal && (
        <div className="acknowledge-modal">
          <div className="acknowledge-content">
            <h3>알람 일괄 승인</h3>
            <p className="text-sm text-neutral-600 mb-4">
              선택된 {selectedAlarms.size}개의 알람을 승인합니다. 승인 사유를 입력해주세요.
            </p>
            <textarea
              value={acknowledgmentComment}
              onChange={(e) => setAcknowledgmentComment(e.target.value)}
              placeholder="승인 사유를 입력하세요..."
              className="w-full"
            />
            <div className="acknowledge-actions">
              <button
                className="btn btn-success"
                onClick={handleBulkAcknowledge}
              >
                <i className="fas fa-check"></i>
                승인
              </button>
              <button
                className="btn btn-outline"
                onClick={() => {
                  setShowAcknowledgeModal(false);
                  setAcknowledgmentComment('');
                }}
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

export default ActiveAlarms;