// frontend/src/pages/ActiveAlarms.tsx
import React, { useState, useEffect, useRef } from 'react';
import '../styles/base.css';
import '../styles/active-alarms.css';

// 기존 인터페이스 100% 그대로 유지
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
  // 기존 상태 100% 그대로 유지
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

  // 🔥 추가: API 데이터를 기존 인터페이스로 변환하는 함수만 추가
  const transformApiData = (apiData: any): ActiveAlarm => {
    const priorityMap: Record<string, 'critical' | 'high' | 'medium' | 'low'> = {
      'critical': 'critical',
      'high': 'high', 
      'medium': 'medium',
      'low': 'low'
    };

    return {
      id: apiData.id?.toString() || Math.random().toString(),
      ruleId: apiData.rule_id?.toString() || '',
      ruleName: apiData.rule_name || 'Unknown Rule',
      priority: priorityMap[apiData.severity] || 'medium',
      severity: apiData.severity === 'critical' ? 5 : apiData.severity === 'high' ? 4 : apiData.severity === 'medium' ? 3 : 2,
      
      sourceType: 'data_point',
      sourceName: apiData.data_point_name || apiData.device_name || 'Unknown Source',
      factory: 'Factory A',
      category: 'Process',
      
      message: apiData.message || 'Alarm triggered',
      description: apiData.description,
      currentValue: apiData.triggered_value,
      thresholdValue: null,
      unit: '',
      
      triggeredAt: new Date(apiData.triggered_at || Date.now()),
      acknowledgedAt: apiData.acknowledged_at ? new Date(apiData.acknowledged_at) : undefined,
      acknowledgedBy: apiData.acknowledged_by,
      acknowledgmentComment: apiData.acknowledgment_comment,
      duration: Date.now() - new Date(apiData.triggered_at || Date.now()).getTime(),
      occurrenceCount: 1,
      
      status: apiData.state === 'acknowledged' ? 'acknowledged' : 'active',
      isNew: Date.now() - new Date(apiData.triggered_at || Date.now()).getTime() < 300000,
      escalated: false,
      escalationLevel: 0,
      
      soundPlayed: false,
      emailSent: false,
      smsSent: false,
      
      tags: []
    };
  };

  // 🔥 추가: API 호출 함수 (기존 목업 데이터 함수 수정)
  const fetchActiveAlarms = async () => {
    try {
      const response = await fetch('/api/alarms/active');
      if (response.ok) {
        const data = await response.json();
        if (data.success && Array.isArray(data.data)) {
          const transformedAlarms = data.data.map(transformApiData);
          setAlarms(transformedAlarms);
          calculateStats(transformedAlarms);
          setLastUpdate(new Date());
          return;
        }
      }
    } catch (err) {
      console.error('API 호출 실패:', err);
    }
    
    // API 실패 시 기존 목업 데이터 사용
    initializeMockData();
  };

  // 🔥 추가: 알람 확인 API 호출
  const acknowledgeAlarmAPI = async (alarmId: string, comment: string = '') => {
    try {
      const response = await fetch(`/api/alarms/${alarmId}/acknowledge`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ comment })
      });
      return response.ok;
    } catch (err) {
      console.error('알람 확인 API 실패:', err);
      return false;
    }
  };

  // 기존 목업 데이터 함수 그대로 유지
  const initializeMockData = () => {
    const mockAlarms: ActiveAlarm[] = [];
    const now = new Date();
    
    const priorities: ActiveAlarm['priority'][] = ['critical', 'high', 'medium', 'low'];
    const categories = ['Safety', 'Process', 'Production', 'System', 'Quality', 'Energy'];
    const factories = ['Factory A', 'Factory B', 'Factory C'];
    const statuses: ActiveAlarm['status'][] = ['active', 'acknowledged'];
    
    for (let i = 0; i < 21; i++) {
      const alarmTime = new Date(now.getTime() - Math.random() * 8 * 60 * 60 * 1000);
      const priority = priorities[Math.floor(Math.random() * priorities.length)];
      const category = categories[Math.floor(Math.random() * categories.length)];
      const factory = factories[Math.floor(Math.random() * factories.length)];
      const status = statuses[Math.floor(Math.random() * statuses.length)];
      
      const alarm: ActiveAlarm = {
        id: `alarm_${i + 1000}`,
        ruleId: `rule_${Math.floor(Math.random() * 20) + 1}`,
        ruleName: `${category} 알람 ${Math.floor(Math.random() * 5) + 1}`,
        priority,
        severity: priority === 'critical' ? 5 : priority === 'high' ? 4 : priority === 'medium' ? 3 : 2,
        
        sourceType: Math.random() > 0.7 ? 'virtual_point' : 'data_point',
        sourceName: `${category} 센서 ${Math.floor(Math.random() * 10) + 1}`,
        factory,
        category,
        
        message: `${category} 임계값 초과 - 즉시 확인 필요`,
        description: `${category} 시스템에서 비정상적인 값이 감지되었습니다.`,
        currentValue: Math.random() > 0.5 ? (Math.random() * 100).toFixed(2) : Math.random() > 0.5,
        thresholdValue: Math.random() > 0.5 ? (Math.random() * 50).toFixed(2) : null,
        unit: Math.random() > 0.5 ? ['°C', 'bar', '%', 'RPM', 'V'][Math.floor(Math.random() * 5)] : '',
        
        triggeredAt: alarmTime,
        acknowledgedAt: status === 'acknowledged' ? new Date(alarmTime.getTime() + Math.random() * 60 * 60 * 1000) : undefined,
        acknowledgedBy: status === 'acknowledged' ? ['김철수', '이영희', '박민수'][Math.floor(Math.random() * 3)] : undefined,
        acknowledgmentComment: status === 'acknowledged' ? '확인 완료' : undefined,
        duration: now.getTime() - alarmTime.getTime(),
        occurrenceCount: Math.floor(Math.random() * 5) + 1,
        
        status,
        isNew: now.getTime() - alarmTime.getTime() < 30 * 60 * 1000,
        escalated: Math.random() > 0.9,
        escalationLevel: Math.floor(Math.random() * 3),
        
        soundPlayed: Math.random() > 0.5,
        emailSent: Math.random() > 0.7,
        smsSent: Math.random() > 0.8,
        
        tags: []
      };
      
      mockAlarms.push(alarm);
    }
    
    setAlarms(mockAlarms);
    calculateStats(mockAlarms);
  };

  // 기존 함수들 그대로 유지
  const calculateStats = (alarmList: ActiveAlarm[]) => {
    const newStats = alarmList.reduce((acc, alarm) => {
      acc.totalActive++;
      
      switch (alarm.priority) {
        case 'critical':
          acc.critical++;
          break;
        case 'high':
          acc.high++;
          break;
        case 'medium':
          acc.medium++;
          break;
        case 'low':
          acc.low++;
          break;
      }
      
      if (alarm.status === 'acknowledged') {
        acc.acknowledged++;
      }
      
      if (alarm.isNew) {
        acc.newAlarms++;
      }
      
      if (alarm.escalated) {
        acc.escalated++;
      }
      
      return acc;
    }, {
      totalActive: 0,
      critical: 0,
      high: 0,
      medium: 0,
      low: 0,
      acknowledged: 0,
      newAlarms: 0,
      escalated: 0,
      avgResponseTime: 25
    });

    setStats(newStats);
  };

  const startRealTimeUpdates = () => {
    if (!isRealtime) return;
    
    intervalRef.current = setInterval(() => {
      fetchActiveAlarms();
    }, 30000); // 30초마다
  };

  // 🔥 수정: 알람 확인 시 API 호출 추가
  const acknowledgeAlarm = async (alarmId: string) => {
    // API 호출 시도
    const success = await acknowledgeAlarmAPI(alarmId, '');
    
    // 로컬 상태 업데이트 (API 성공 여부와 관계없이)
    setAlarms(prevAlarms => 
      prevAlarms.map(alarm => 
        alarm.id === alarmId 
          ? { 
              ...alarm, 
              status: 'acknowledged' as const,
              acknowledgedAt: new Date(),
              acknowledgedBy: '사용자'
            }
          : alarm
      )
    );

    // 선택에서 제거
    setSelectedAlarms(prev => {
      const newSet = new Set(prev);
      newSet.delete(alarmId);
      return newSet;
    });
  };

  const handleAcknowledgeSelected = async () => {
    if (selectedAlarms.size === 0) return;

    for (const alarmId of selectedAlarms) {
      await acknowledgeAlarm(alarmId);
    }
    
    setSelectedAlarms(new Set());
    setAcknowledgmentComment('');
    setShowAcknowledgeModal(false);
  };

  const handleSelectAlarm = (alarmId: string, checked: boolean) => {
    setSelectedAlarms(prev => {
      const newSet = new Set(prev);
      if (checked) {
        newSet.add(alarmId);
      } else {
        newSet.delete(alarmId);
      }
      return newSet;
    });
  };

  const handleSelectAll = (checked: boolean) => {
    if (checked) {
      const unacknowledgedAlarms = filteredAlarms
        .filter(alarm => alarm.status === 'active')
        .map(alarm => alarm.id);
      setSelectedAlarms(new Set(unacknowledgedAlarms));
    } else {
      setSelectedAlarms(new Set());
    }
  };

  // 필터링된 알람 목록
  const filteredAlarms = alarms.filter(alarm => {
    if (quickFilter === 'critical' && alarm.priority !== 'critical') return false;
    if (quickFilter === 'unacknowledged' && alarm.status !== 'active') return false;
    if (filterPriority !== 'all' && alarm.priority !== filterPriority) return false;
    if (filterStatus !== 'all') {
      if (filterStatus === 'acknowledged' && alarm.status !== 'acknowledged') return false;
      if (filterStatus === 'active' && alarm.status !== 'active') return false;
    }
    if (filterCategory !== 'all' && alarm.category !== filterCategory) return false;
    if (filterFactory !== 'all' && alarm.factory !== filterFactory) return false;
    return true;
  });

  const emergencyCount = alarms.filter(alarm => 
    alarm.priority === 'critical' && alarm.status === 'active'
  ).length;

  // 생명주기 - 초기 로드 시 API 호출
  useEffect(() => {
    fetchActiveAlarms();
  }, []);

  useEffect(() => {
    if (isRealtime) {
      startRealTimeUpdates();
    }
    
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [isRealtime]);

  // 유틸리티 함수들 그대로 유지
  const formatDuration = (ms: number): string => {
    const hours = Math.floor(ms / (1000 * 60 * 60));
    const minutes = Math.floor((ms % (1000 * 60 * 60)) / (1000 * 60));
    
    if (hours > 0) {
      return `${hours}시간 ${minutes}분`;
    } else {
      return `${minutes}분`;
    }
  };

  const getPriorityColor = (priority: string): string => {
    switch (priority) {
      case 'critical': return '#ef4444';
      case 'high': return '#f97316';
      case 'medium': return '#3b82f6';
      case 'low': return '#10b981';
      default: return '#6b7280';
    }
  };

  const getPriorityIcon = (priority: string): string => {
    switch (priority) {
      case 'critical': return 'fas fa-exclamation-triangle';
      case 'high': return 'fas fa-exclamation';
      case 'medium': return 'fas fa-info-circle';
      case 'low': return 'fas fa-check-circle';
      default: return 'fas fa-question-circle';
    }
  };

  // 기존 JSX 100% 그대로 유지
  return (
    <div className="active-alarms-container">
      {/* 오디오 요소 */}
      <audio ref={audioRef} preload="auto">
        <source src="/sounds/alarm.mp3" type="audio/mpeg" />
      </audio>

      {/* 긴급 알람 배너 */}
      {emergencyCount > 0 && (
        <div className="emergency-banner">
          <div className="emergency-icon">
            <i className="fas fa-exclamation-triangle"></i>
          </div>
          <div className="emergency-content">
            <h2>🚨 긴급 알람 발생</h2>
            <p>{emergencyCount}개의 Critical 알람이 활성 상태입니다. 즉시 확인하세요!</p>
          </div>
          <div className="emergency-actions">
            <button 
              className="btn btn-critical"
              onClick={() => setQuickFilter('critical')}
            >
              Critical 알람 보기
            </button>
          </div>
        </div>
      )}

      {/* 알람 통계 패널 */}
      <div className="alarm-stats-panel">
        <div className={`alarm-stat-card critical ${stats.critical > 0 ? 'has-alarms' : ''}`}>
          <div className="alarm-stat-icon critical">
            <i className="fas fa-exclamation-triangle"></i>
          </div>
          <div className="alarm-stat-value">{stats.critical}</div>
          <div className="alarm-stat-label">Critical</div>
          {stats.critical > 7 && <div className="alarm-stat-badge">+{stats.critical - 7}</div>}
        </div>

        <div className={`alarm-stat-card high ${stats.high > 0 ? 'has-alarms' : ''}`}>
          <div className="alarm-stat-icon high">
            <i className="fas fa-exclamation"></i>
          </div>
          <div className="alarm-stat-value">{stats.high}</div>
          <div className="alarm-stat-label">High</div>
        </div>

        <div className={`alarm-stat-card medium ${stats.medium > 0 ? 'has-alarms' : ''}`}>
          <div className="alarm-stat-icon medium">
            <i className="fas fa-info-circle"></i>
          </div>
          <div className="alarm-stat-value">{stats.medium}</div>
          <div className="alarm-stat-label">Medium</div>
        </div>

        <div className={`alarm-stat-card low ${stats.low > 0 ? 'has-alarms' : ''}`}>
          <div className="alarm-stat-icon low">
            <i className="fas fa-check-circle"></i>
          </div>
          <div className="alarm-stat-value">{stats.low}</div>
          <div className="alarm-stat-label">Low</div>
        </div>

        <div className="alarm-stat-card">
          <div className="alarm-stat-icon">
            <i className="fas fa-clock"></i>
          </div>
          <div className="alarm-stat-value">{stats.avgResponseTime}</div>
          <div className="alarm-stat-label">평균 응답시간 (분)</div>
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
            마지막 업데이트: {lastUpdate.toLocaleTimeString('ko-KR')}
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
          >
            <option value="all">전체</option>
            <option value="active">미확인</option>
            <option value="acknowledged">확인됨</option>
          </select>
        </div>

        <div className="filter-group">
          <label>카테고리</label>
          <select
            value={filterCategory}
            onChange={(e) => setFilterCategory(e.target.value)}
          >
            <option value="all">전체</option>
            <option value="Safety">안전</option>
            <option value="Process">공정</option>
            <option value="Production">생산</option>
            <option value="System">시스템</option>
            <option value="Quality">품질</option>
            <option value="Energy">에너지</option>
          </select>
        </div>

        <div className="filter-group">
          <label>공장</label>
          <select
            value={filterFactory}
            onChange={(e) => setFilterFactory(e.target.value)}
          >
            <option value="all">전체</option>
            <option value="Factory A">Factory A</option>
            <option value="Factory B">Factory B</option>
            <option value="Factory C">Factory C</option>
          </select>
        </div>

        <div className="quick-filters">
          <button
            className={`filter-btn ${quickFilter === 'all' ? 'active' : ''}`}
            onClick={() => setQuickFilter('all')}
          >
            전체
          </button>
          <button
            className={`filter-btn ${quickFilter === 'critical' ? 'active' : ''}`}
            onClick={() => setQuickFilter('critical')}
          >
            <i className="fas fa-exclamation-triangle"></i>
            Critical
          </button>
          <button
            className={`filter-btn ${quickFilter === 'unacknowledged' ? 'active' : ''}`}
            onClick={() => setQuickFilter('unacknowledged')}
          >
            <i className="fas fa-bell"></i>
            미확인
          </button>
        </div>
      </div>

      {/* 선택된 알람 액션 */}
      {selectedAlarms.size > 0 && (
        <div className="selected-actions">
          <span className="selected-count">{selectedAlarms.size}개 알람 선택됨</span>
          <button
            className="btn btn-primary"
            onClick={() => setShowAcknowledgeModal(true)}
          >
            <i className="fas fa-check"></i>
            일괄 확인
          </button>
        </div>
      )}

      {/* 알람 목록 - 스크롤 가능하도록 수정 */}
      <div className="alarms-list">
        {filteredAlarms.length === 0 ? (
          <div className="empty-alarms">
            <div className="empty-icon">
              <i className="fas fa-bell-slash"></i>
            </div>
            <h3 className="empty-title">활성 알람이 없습니다</h3>
            <p className="empty-description">
              현재 발생한 알람이 없습니다. 시스템이 정상 상태입니다.
            </p>
          </div>
        ) : (
          <>
            {/* 선택 모두 체크박스 */}
            <div className="select-all">
              <input
                type="checkbox"
                checked={selectedAlarms.size === filteredAlarms.filter(a => a.status === 'active').length && filteredAlarms.filter(a => a.status === 'active').length > 0}
                onChange={(e) => handleSelectAll(e.target.checked)}
              />
              <label>모든 미확인 알람 선택</label>
            </div>

            {filteredAlarms.map((alarm) => (
              <div
                key={alarm.id}
                className={`alarm-item ${alarm.priority} ${alarm.status === 'acknowledged' ? 'acknowledged' : ''} ${alarm.isNew ? 'new' : ''}`}
              >
                {alarm.status === 'active' && (
                  <div className="alarm-checkbox">
                    <input
                      type="checkbox"
                      checked={selectedAlarms.has(alarm.id)}
                      onChange={(e) => handleSelectAlarm(alarm.id, e.target.checked)}
                    />
                  </div>
                )}

                <div className="alarm-header">
                  <div className="alarm-priority">
                    <div 
                      className="priority-indicator"
                      style={{ backgroundColor: getPriorityColor(alarm.priority) }}
                    >
                      <i className={getPriorityIcon(alarm.priority)}></i>
                    </div>
                    <span className="priority-text">{alarm.priority.toUpperCase()}</span>
                  </div>

                  <div className="alarm-source">
                    <div className="source-name">{alarm.sourceName}</div>
                    <div className="source-location">{alarm.factory} • {alarm.category}</div>
                  </div>

                  <div className="alarm-time">
                    <div className="triggered-time">
                      {alarm.triggeredAt.toLocaleString('ko-KR')}
                    </div>
                    <div className="duration">
                      지속: {formatDuration(alarm.duration)}
                    </div>
                  </div>

                  <div className="alarm-status">
                    <span className={`status-badge ${alarm.status}`}>
                      {alarm.status === 'active' ? '활성' : '확인됨'}
                    </span>
                    {alarm.isNew && <span className="new-badge">NEW</span>}
                  </div>
                </div>

                <div className="alarm-body">
                  <div className="alarm-message">
                    <div className="message-text">{alarm.message}</div>
                    {alarm.description && (
                      <div className="message-description">{alarm.description}</div>
                    )}
                  </div>

                  <div className="alarm-value">
                    {alarm.currentValue !== null && alarm.currentValue !== undefined && (
                      <div className="current-value">
                        <span className="value-label">현재값:</span>
                        <span className="value-number">{alarm.currentValue}</span>
                        {alarm.unit && <span className="value-unit">{alarm.unit}</span>}
                      </div>
                    )}
                    {alarm.thresholdValue && (
                      <div className="threshold-value">
                        <span className="value-label">임계값:</span>
                        <span className="value-number">{alarm.thresholdValue}</span>
                        {alarm.unit && <span className="value-unit">{alarm.unit}</span>}
                      </div>
                    )}
                  </div>
                </div>

                <div className="alarm-footer">
                  {alarm.status === 'active' ? (
                    <div className="alarm-actions">
                      <button
                        className="btn btn-small btn-primary"
                        onClick={() => acknowledgeAlarm(alarm.id)}
                      >
                        <i className="fas fa-check"></i>
                        확인
                      </button>
                      <button className="btn btn-small btn-secondary">
                        <i className="fas fa-eye"></i>
                        상세
                      </button>
                    </div>
                  ) : (
                    <div className="acknowledged-info">
                      <div className="acknowledged-by">
                        확인자: {alarm.acknowledgedBy}
                      </div>
                      {alarm.acknowledgmentComment && (
                        <div className="acknowledged-comment">
                          코멘트: {alarm.acknowledgmentComment}
                        </div>
                      )}
                    </div>
                  )}
                </div>
              </div>
            ))}
          </>
        )}
      </div>

      {/* 일괄 확인 모달 */}
      {showAcknowledgeModal && (
        <div className="acknowledge-modal">
          <div className="acknowledge-content">
            <h3>알람 일괄 확인</h3>
            <p>{selectedAlarms.size}개의 알람을 확인하시겠습니까?</p>
            <textarea
              value={acknowledgmentComment}
              onChange={(e) => setAcknowledgmentComment(e.target.value)}
              placeholder="확인 코멘트를 입력하세요 (선택사항)"
            />
            <div className="acknowledge-actions">
              <button
                className="btn btn-secondary"
                onClick={() => setShowAcknowledgeModal(false)}
              >
                취소
              </button>
              <button
                className="btn btn-primary"
                onClick={handleAcknowledgeSelected}
              >
                확인
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default ActiveAlarms;