// frontend/src/pages/AlarmHistory.tsx
import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/alarm-history.css';

// 기존 인터페이스 유지하면서 API와 호환
interface AlarmEvent {
  id: string;
  alarmRuleId: string;
  alarmRuleName: string;
  sourceType: 'data_point' | 'virtual_point' | 'device_status';
  sourceName: string;
  factory: string;
  category: string;
  priority: 'low' | 'medium' | 'high' | 'critical';
  severity: 1 | 2 | 3 | 4 | 5;
  
  // 이벤트 정보
  eventType: 'triggered' | 'acknowledged' | 'cleared' | 'escalated' | 'reset';
  triggeredValue?: any;
  message: string;
  description?: string;
  
  // 시간 정보
  triggeredAt: Date;
  acknowledgedAt?: Date;
  clearedAt?: Date;
  duration?: number; // milliseconds
  
  // 처리 정보
  acknowledgedBy?: string;
  acknowledgmentComment?: string;
  escalated: boolean;
  escalationLevel: number;
  
  // 상태
  status: 'active' | 'acknowledged' | 'cleared' | 'suppressed';
  isActive: boolean;
  
  // 알림 정보
  notificationsSent: {
    email: boolean;
    sms: boolean;
    sound: boolean;
    popup: boolean;
  };
  
  // 메타데이터
  tags: string[];
  relatedEvents?: string[];
}

interface AlarmSummary {
  totalEvents: number;
  activeAlarms: number;
  acknowledgedAlarms: number;
  clearedAlarms: number;
  criticalAlarms: number;
  averageResponseTime: number; // minutes
  topAlarmSources: Array<{ source: string; count: number }>;
  hourlyTrends: Array<{ hour: number; count: number }>;
}

const AlarmHistory: React.FC = () => {
  const [alarmEvents, setAlarmEvents] = useState<AlarmEvent[]>([]);
  const [summary, setSummary] = useState<AlarmSummary | null>(null);
  const [selectedEvent, setSelectedEvent] = useState<AlarmEvent | null>(null);
  const [showDetailsModal, setShowDetailsModal] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 페이징 상태
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(50);
  const [totalPages, setTotalPages] = useState(1);
  
  // 필터 상태
  const [dateRange, setDateRange] = useState({
    start: new Date(Date.now() - 24 * 60 * 60 * 1000), // 24시간 전
    end: new Date()
  });
  const [filterPriority, setFilterPriority] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [filterCategory, setFilterCategory] = useState('all');
  const [filterFactory, setFilterFactory] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  const [viewMode, setViewMode] = useState<'list' | 'timeline'>('list');

  // API 데이터 변환 함수
  const transformApiDataToAlarmEvent = (apiData: any): AlarmEvent => {
    const priorityMap: Record<string, 'low' | 'medium' | 'high' | 'critical'> = {
      'low': 'low',
      'medium': 'medium',
      'high': 'high',
      'critical': 'critical'
    };

    const severityMap: Record<string, 1 | 2 | 3 | 4 | 5> = {
      'low': 1,
      'medium': 2,
      'high': 3,
      'critical': 4
    };

    const statusMap: Record<string, 'active' | 'acknowledged' | 'cleared' | 'suppressed'> = {
      'active': 'active',
      'acknowledged': 'acknowledged',
      'cleared': 'cleared',
      'suppressed': 'suppressed'
    };

    const triggeredAt = new Date(apiData.triggered_at || Date.now());
    const acknowledgedAt = apiData.acknowledged_at ? new Date(apiData.acknowledged_at) : undefined;
    const clearedAt = apiData.cleared_at ? new Date(apiData.cleared_at) : undefined;
    
    return {
      id: apiData.id?.toString() || Math.random().toString(),
      alarmRuleId: apiData.rule_id?.toString() || '',
      alarmRuleName: apiData.rule_name || 'Unknown Rule',
      sourceType: apiData.target_type || 'data_point',
      sourceName: apiData.data_point_name || apiData.device_name || 'Unknown Source',
      factory: 'Factory A', // 기본값
      category: 'Process', // 기본값
      priority: priorityMap[apiData.severity] || 'medium',
      severity: severityMap[apiData.severity] || 3,
      
      eventType: clearedAt ? 'cleared' : acknowledgedAt ? 'acknowledged' : 'triggered',
      triggeredValue: apiData.triggered_value,
      message: apiData.message || 'Alarm event',
      description: apiData.description,
      
      triggeredAt,
      acknowledgedAt,
      clearedAt,
      duration: acknowledgedAt ? acknowledgedAt.getTime() - triggeredAt.getTime() : 
                clearedAt ? clearedAt.getTime() - triggeredAt.getTime() : 
                Date.now() - triggeredAt.getTime(),
      
      acknowledgedBy: apiData.acknowledged_by,
      acknowledgmentComment: apiData.acknowledgment_comment,
      escalated: false,
      escalationLevel: 0,
      
      status: statusMap[apiData.state] || 'active',
      isActive: apiData.state === 'active',
      
      notificationsSent: {
        email: false,
        sms: false,
        sound: false,
        popup: false
      },
      
      tags: [],
      relatedEvents: []
    };
  };

  // API 호출 함수들
  const fetchAlarmHistory = async () => {
    try {
      setIsLoading(true);
      setError(null);
      
      const params = new URLSearchParams({
        page: currentPage.toString(),
        limit: pageSize.toString(),
        ...(filterPriority !== 'all' && { severity: filterPriority }),
        ...(filterStatus !== 'all' && { state: filterStatus }),
        ...(searchTerm && { search: searchTerm })
      });

      const response = await fetch(`/api/alarms/history?${params}`);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      
      if (data.success) {
        const transformedEvents = (data.data || []).map(transformApiDataToAlarmEvent);
        setAlarmEvents(transformedEvents);
        
        if (data.pagination) {
          setTotalPages(data.pagination.totalPages || 1);
        }
      } else {
        throw new Error(data.error || 'Failed to fetch alarm history');
      }
    } catch (err) {
      console.error('Error fetching alarm history:', err);
      // API 실패 시 목업 데이터로 대체
      initializeMockData();
      setError('API 연결 실패 - 목업 데이터를 표시합니다');
    } finally {
      setIsLoading(false);
    }
  };

  const fetchAlarmSummary = async () => {
    try {
      const response = await fetch('/api/alarms/statistics');
      
      if (response.ok) {
        const data = await response.json();
        if (data.success) {
          setSummary({
            totalEvents: data.data.totalActive || 0,
            activeAlarms: data.data.totalActive || 0,
            acknowledgedAlarms: data.data.bySeverity?.acknowledged || 0,
            clearedAlarms: data.data.bySeverity?.cleared || 0,
            criticalAlarms: data.data.bySeverity?.critical || 0,
            averageResponseTime: data.data.avgResponseTime || 0,
            topAlarmSources: data.data.byDevice || [],
            hourlyTrends: []
          });
        }
      }
    } catch (err) {
      console.error('Error fetching alarm summary:', err);
      // 기본 요약 설정
      setSummary({
        totalEvents: alarmEvents.length,
        activeAlarms: alarmEvents.filter(e => e.isActive).length,
        acknowledgedAlarms: alarmEvents.filter(e => e.status === 'acknowledged').length,
        clearedAlarms: alarmEvents.filter(e => e.status === 'cleared').length,
        criticalAlarms: alarmEvents.filter(e => e.priority === 'critical').length,
        averageResponseTime: 25,
        topAlarmSources: [],
        hourlyTrends: []
      });
    }
  };

  // 기존 목업 데이터 (API 실패 시 백업용)
  const initializeMockData = () => {
    const mockEvents: AlarmEvent[] = [];
    const now = new Date();
    
    for (let i = 0; i < 150; i++) {
      const eventTime = new Date(now.getTime() - Math.random() * 24 * 60 * 60 * 1000);
      const priorities: AlarmEvent['priority'][] = ['low', 'medium', 'high', 'critical'];
      const categories = ['Safety', 'Process', 'Production', 'System', 'Quality', 'Energy'];
      const factories = ['Factory A', 'Factory B', 'Factory C'];
      const eventTypes: AlarmEvent['eventType'][] = ['triggered', 'acknowledged', 'cleared', 'escalated'];
      const statuses: AlarmEvent['status'][] = ['active', 'acknowledged', 'cleared'];
      
      const priority = priorities[Math.floor(Math.random() * priorities.length)];
      const category = categories[Math.floor(Math.random() * categories.length)];
      const factory = factories[Math.floor(Math.random() * factories.length)];
      const eventType = eventTypes[Math.floor(Math.random() * eventTypes.length)];
      const status = statuses[Math.floor(Math.random() * statuses.length)];
      
      const acknowledgedAt = status === 'acknowledged' ? 
        new Date(eventTime.getTime() + Math.random() * 60 * 60 * 1000) : undefined;
      const clearedAt = status === 'cleared' ? 
        new Date(eventTime.getTime() + Math.random() * 2 * 60 * 60 * 1000) : undefined;
      
      const event: AlarmEvent = {
        id: `evt_${i + 1000}`,
        alarmRuleId: `rule_${Math.floor(Math.random() * 10) + 1}`,
        alarmRuleName: `${category} 알람 ${Math.floor(Math.random() * 5) + 1}`,
        sourceType: Math.random() > 0.7 ? 'virtual_point' : 'data_point',
        sourceName: `${category} 센서 ${Math.floor(Math.random() * 10) + 1}`,
        factory,
        category,
        priority,
        severity: priority === 'critical' ? 5 : priority === 'high' ? 4 : priority === 'medium' ? 3 : 2,
        
        eventType,
        triggeredValue: Math.random() > 0.5 ? (Math.random() * 100).toFixed(2) : undefined,
        message: `${category} 임계값 ${eventType === 'triggered' ? '초과' : '정상화'}`,
        description: `${category} 시스템에서 ${eventType} 이벤트가 발생했습니다.`,
        
        triggeredAt: eventTime,
        acknowledgedAt,
        clearedAt,
        duration: acknowledgedAt ? acknowledgedAt.getTime() - eventTime.getTime() : 
                 clearedAt ? clearedAt.getTime() - eventTime.getTime() : 
                 now.getTime() - eventTime.getTime(),
        
        acknowledgedBy: acknowledgedAt ? ['김철수', '이영희', '박민수'][Math.floor(Math.random() * 3)] : undefined,
        acknowledgmentComment: acknowledgedAt ? '확인 완료' : undefined,
        escalated: Math.random() > 0.9,
        escalationLevel: Math.floor(Math.random() * 3),
        
        status,
        isActive: status === 'active',
        
        notificationsSent: {
          email: Math.random() > 0.7,
          sms: Math.random() > 0.8,
          sound: Math.random() > 0.5,
          popup: true
        },
        
        tags: [],
        relatedEvents: []
      };
      
      mockEvents.push(event);
    }
    
    setAlarmEvents(mockEvents);
    
    // 요약 데이터 계산
    setSummary({
      totalEvents: mockEvents.length,
      activeAlarms: mockEvents.filter(e => e.isActive).length,
      acknowledgedAlarms: mockEvents.filter(e => e.status === 'acknowledged').length,
      clearedAlarms: mockEvents.filter(e => e.status === 'cleared').length,
      criticalAlarms: mockEvents.filter(e => e.priority === 'critical').length,
      averageResponseTime: 25,
      topAlarmSources: [
        { source: 'Temperature Sensor', count: 45 },
        { source: 'Pressure Gauge', count: 32 },
        { source: 'Flow Meter', count: 28 },
        { source: 'Production Counter', count: 22 },
        { source: 'Vibration Monitor', count: 18 }
      ],
      hourlyTrends: []
    });
  };

  // 이벤트 핸들러들
  const handleSearch = () => {
    setCurrentPage(1);
    fetchAlarmHistory();
  };

  const handlePageChange = (newPage: number) => {
    if (newPage >= 1 && newPage <= totalPages) {
      setCurrentPage(newPage);
    }
  };

  const handleViewDetails = (event: AlarmEvent) => {
    setSelectedEvent(event);
    setShowDetailsModal(true);
  };

  const handleExportToCSV = () => {
    if (alarmEvents.length === 0) return;

    const headers = [
      'ID', '규칙명', '소스', '메시지', '우선순위', '상태', 
      '발생시간', '확인시간', '해제시간', '지속시간(분)', '확인자', '코멘트'
    ];
    
    const rows = alarmEvents.map(event => [
      event.id,
      event.alarmRuleName,
      event.sourceName,
      event.message,
      event.priority,
      event.status,
      event.triggeredAt.toLocaleString('ko-KR'),
      event.acknowledgedAt ? event.acknowledgedAt.toLocaleString('ko-KR') : '',
      event.clearedAt ? event.clearedAt.toLocaleString('ko-KR') : '',
      event.duration ? Math.round(event.duration / 60000) : '',
      event.acknowledgedBy || '',
      event.acknowledgmentComment || ''
    ]);

    const csvContent = [headers, ...rows]
      .map(row => row.map(cell => `"${cell}"`).join(','))
      .join('\n');

    const blob = new Blob(['\uFEFF' + csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = `alarm_history_${new Date().toISOString().split('T')[0]}.csv`;
    link.click();
  };

  // 필터링된 이벤트 목록
  const filteredEvents = alarmEvents.filter(event => {
    const matchesSearch = searchTerm === '' || 
      event.alarmRuleName.toLowerCase().includes(searchTerm.toLowerCase()) ||
      event.sourceName.toLowerCase().includes(searchTerm.toLowerCase()) ||
      event.message.toLowerCase().includes(searchTerm.toLowerCase());
    
    const matchesPriority = filterPriority === 'all' || event.priority === filterPriority;
    const matchesStatus = filterStatus === 'all' || event.status === filterStatus;
    const matchesCategory = filterCategory === 'all' || event.category === filterCategory;
    const matchesFactory = filterFactory === 'all' || event.factory === filterFactory;
    
    const matchesDateRange = event.triggeredAt >= dateRange.start && event.triggeredAt <= dateRange.end;
    
    return matchesSearch && matchesPriority && matchesStatus && 
           matchesCategory && matchesFactory && matchesDateRange;
  });

  // 유틸리티 함수들
  const formatDuration = (ms?: number): string => {
    if (!ms) return '-';
    
    const hours = Math.floor(ms / (1000 * 60 * 60));
    const minutes = Math.floor((ms % (1000 * 60 * 60)) / (1000 * 60));
    const seconds = Math.floor((ms % (1000 * 60)) / 1000);
    
    if (hours > 0) {
      return `${hours}시간 ${minutes}분`;
    } else if (minutes > 0) {
      return `${minutes}분 ${seconds}초`;
    } else {
      return `${seconds}초`;
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

  const getStatusColor = (status: string): string => {
    switch (status) {
      case 'active': return '#ef4444';
      case 'acknowledged': return '#f97316';
      case 'cleared': return '#10b981';
      case 'suppressed': return '#6b7280';
      default: return '#6b7280';
    }
  };

  const getStatusIcon = (status: string): string => {
    switch (status) {
      case 'active': return 'fas fa-exclamation-circle';
      case 'acknowledged': return 'fas fa-check-circle';
      case 'cleared': return 'fas fa-times-circle';
      case 'suppressed': return 'fas fa-pause-circle';
      default: return 'fas fa-question-circle';
    }
  };

  // 생명주기
  useEffect(() => {
    fetchAlarmHistory();
    fetchAlarmSummary();
  }, [currentPage, pageSize]);

  useEffect(() => {
    setCurrentPage(1);
    fetchAlarmHistory();
  }, [filterPriority, filterStatus, filterCategory, filterFactory, dateRange]);

  return (
    <div className="alarm-history-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-content">
          <h1>📜 알람 이력</h1>
          <p>시스템에서 발생한 모든 알람의 이력을 확인하고 분석할 수 있습니다.</p>
        </div>
        <div className="header-actions">
          <button
            className="btn btn-secondary"
            onClick={() => {
              fetchAlarmHistory();
              fetchAlarmSummary();
            }}
            disabled={isLoading}
          >
            <i className={`fas fa-refresh ${isLoading ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
          <button
            className="btn btn-primary"
            onClick={handleExportToCSV}
            disabled={alarmEvents.length === 0}
          >
            <i className="fas fa-download"></i>
            CSV 내보내기
          </button>
        </div>
      </div>

      {/* 요약 통계 */}
      {summary && (
        <div className="summary-panel">
          <div className="summary-card">
            <div className="summary-icon">
              <i className="fas fa-list-alt"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{summary.totalEvents}</div>
              <div className="summary-label">총 이벤트</div>
            </div>
          </div>

          <div className="summary-card critical">
            <div className="summary-icon">
              <i className="fas fa-exclamation-triangle"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{summary.criticalAlarms}</div>
              <div className="summary-label">Critical 알람</div>
            </div>
          </div>

          <div className="summary-card warning">
            <div className="summary-icon">
              <i className="fas fa-check-circle"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{summary.acknowledgedAlarms}</div>
              <div className="summary-label">확인된 알람</div>
            </div>
          </div>

          <div className="summary-card success">
            <div className="summary-icon">
              <i className="fas fa-times-circle"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{summary.clearedAlarms}</div>
              <div className="summary-label">해제된 알람</div>
            </div>
          </div>

          <div className="summary-card">
            <div className="summary-icon">
              <i className="fas fa-clock"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{Math.round(summary.averageResponseTime)}</div>
              <div className="summary-label">평균 응답시간 (분)</div>
            </div>
          </div>
        </div>
      )}

      {/* 에러 메시지 */}
      {error && (
        <div className="error-alert">
          <i className="fas fa-exclamation-circle"></i>
          <span>{error}</span>
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-row">
          <div className="filter-group">
            <label>기간</label>
            <div className="date-range">
              <input
                type="date"
                value={dateRange.start.toISOString().split('T')[0]}
                onChange={(e) => setDateRange(prev => ({ ...prev, start: new Date(e.target.value) }))}
              />
              <span>~</span>
              <input
                type="date"
                value={dateRange.end.toISOString().split('T')[0]}
                onChange={(e) => setDateRange(prev => ({ ...prev, end: new Date(e.target.value) }))}
              />
            </div>
          </div>

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
              <option value="active">활성</option>
              <option value="acknowledged">확인됨</option>
              <option value="cleared">해제됨</option>
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
        </div>

        <div className="filter-row">
          <div className="search-group">
            <input
              type="text"
              placeholder="알람명, 소스명, 메시지로 검색..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
            />
            <button onClick={handleSearch}>
              <i className="fas fa-search"></i>
              검색
            </button>
          </div>

          <div className="view-controls">
            <button
              className={`view-btn ${viewMode === 'list' ? 'active' : ''}`}
              onClick={() => setViewMode('list')}
            >
              <i className="fas fa-list"></i>
              목록
            </button>
            <button
              className={`view-btn ${viewMode === 'timeline' ? 'active' : ''}`}
              onClick={() => setViewMode('timeline')}
            >
              <i className="fas fa-chart-line"></i>
              타임라인
            </button>
          </div>
        </div>
      </div>

      {/* 이력 목록 */}
      <div className="history-content">
        {isLoading ? (
          <div className="loading-state">
            <div className="loading-spinner"></div>
            <p>알람 이력을 불러오는 중...</p>
          </div>
        ) : filteredEvents.length === 0 ? (
          <div className="empty-state">
            <div className="empty-icon">
              <i className="fas fa-history"></i>
            </div>
            <h3>알람 이력이 없습니다</h3>
            <p>검색 조건에 맞는 알람 이력을 찾을 수 없습니다. 필터를 조정해보세요.</p>
          </div>
        ) : viewMode === 'list' ? (
          <div className="history-table-container">
            <table className="history-table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>우선순위</th>
                  <th>소스</th>
                  <th>메시지</th>
                  <th>상태</th>
                  <th>발생시간</th>
                  <th>지속시간</th>
                  <th>액션</th>
                </tr>
              </thead>
              <tbody>
                {filteredEvents.map((event, index) => (
                  <tr
                    key={event.id}
                    className={`history-row ${event.priority}`}
                    style={{ animationDelay: `${index * 0.05}s` }}
                  >
                    <td>
                      <span className="event-id">#{event.id}</span>
                    </td>
                    
                    <td>
                      <div className="priority-cell">
                        <div 
                          className="priority-indicator"
                          style={{ backgroundColor: getPriorityColor(event.priority) }}
                        ></div>
                        <span>{event.priority.toUpperCase()}</span>
                      </div>
                    </td>
                    
                    <td>
                      <div className="source-cell">
                        <div className="source-name">{event.sourceName}</div>
                        <div className="source-location">{event.factory} • {event.category}</div>
                      </div>
                    </td>
                    
                    <td>
                      <div className="message-cell">
                        <div className="message-text">{event.message}</div>
                        {event.triggeredValue !== null && event.triggeredValue !== undefined && (
                          <div className="triggered-value">값: {event.triggeredValue}</div>
                        )}
                      </div>
                    </td>
                    
                    <td>
                      <div className="status-cell">
                        <div 
                          className="status-indicator"
                          style={{ color: getStatusColor(event.status) }}
                        >
                          <i className={getStatusIcon(event.status)}></i>
                        </div>
                        <span>{
                          event.status === 'active' ? '활성' :
                          event.status === 'acknowledged' ? '확인됨' :
                          event.status === 'cleared' ? '해제됨' : 
                          event.status === 'suppressed' ? '억제됨' : event.status
                        }</span>
                      </div>
                    </td>
                    
                    <td>
                      <div className="time-cell">
                        <div className="time-main">
                          {event.triggeredAt.toLocaleDateString('ko-KR')}
                        </div>
                        <div className="time-detail">
                          {event.triggeredAt.toLocaleTimeString('ko-KR')}
                        </div>
                      </div>
                    </td>
                    
                    <td>
                      <div className="duration-cell">
                        {formatDuration(event.duration)}
                      </div>
                    </td>
                    
                    <td>
                      <div className="action-cell">
                        <button
                          className="btn btn-small btn-secondary"
                          onClick={() => handleViewDetails(event)}
                        >
                          <i className="fas fa-eye"></i>
                          상세
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        ) : (
          /* 타임라인 뷰 */
          <div className="timeline-view">
            <div className="timeline-container">
              {filteredEvents.map((event, index) => (
                <div key={event.id} className={`timeline-item ${event.priority}`}>
                  <div className="timeline-marker">
                    <div 
                      className="timeline-dot"
                      style={{ backgroundColor: getPriorityColor(event.priority) }}
                    ></div>
                  </div>
                  <div className="timeline-content">
                    <div className="timeline-header">
                      <div className="timeline-title">
                        <span 
                          className="priority-badge"
                          style={{ backgroundColor: getPriorityColor(event.priority) }}
                        >
                          {event.priority.toUpperCase()}
                        </span>
                        <span>{event.alarmRuleName}</span>
                      </div>
                      <div className="timeline-time">
                        {event.triggeredAt.toLocaleString('ko-KR')}
                      </div>
                    </div>
                    <div className="timeline-body">
                      <div className="timeline-message">{event.message}</div>
                      <div className="timeline-source">
                        {event.sourceName} ({event.factory} • {event.category})
                      </div>
                      <div className="timeline-footer">
                        <div className="timeline-status">
                          <i 
                            className={getStatusIcon(event.status)} 
                            style={{ color: getStatusColor(event.status) }}
                          ></i>
                          {event.status === 'active' ? '활성' :
                           event.status === 'acknowledged' ? '확인됨' :
                           event.status === 'cleared' ? '해제됨' : 
                           event.status === 'suppressed' ? '억제됨' : event.status}
                        </div>
                        {event.duration && (
                          <div className="timeline-duration">
                            지속시간: {formatDuration(event.duration)}
                          </div>
                        )}
                      </div>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* 페이지네이션 */}
      {totalPages > 1 && (
        <div className="pagination-container">
          <div className="pagination-info">
            <span>
              페이지 {currentPage} / {totalPages}
            </span>
            <select
              value={pageSize}
              onChange={(e) => setPageSize(Number(e.target.value))}
            >
              <option value={25}>25개씩</option>
              <option value={50}>50개씩</option>
              <option value={100}>100개씩</option>
            </select>
          </div>

          <div className="pagination-controls">
            <button
              onClick={() => handlePageChange(1)}
              disabled={currentPage === 1}
            >
              <i className="fas fa-angle-double-left"></i>
            </button>
            <button
              onClick={() => handlePageChange(currentPage - 1)}
              disabled={currentPage === 1}
            >
              <i className="fas fa-angle-left"></i>
            </button>

            {Array.from({ length: Math.min(5, totalPages) }, (_, i) => {
              const page = Math.max(1, Math.min(totalPages - 4, currentPage - 2)) + i;
              return (
                <button
                  key={page}
                  className={page === currentPage ? 'active' : ''}
                  onClick={() => handlePageChange(page)}
                >
                  {page}
                </button>
              );
            })}

            <button
              onClick={() => handlePageChange(currentPage + 1)}
              disabled={currentPage === totalPages}
            >
              <i className="fas fa-angle-right"></i>
            </button>
            <button
              onClick={() => handlePageChange(totalPages)}
              disabled={currentPage === totalPages}
            >
              <i className="fas fa-angle-double-right"></i>
            </button>
          </div>
        </div>
      )}

      {/* 상세 보기 모달 */}
      {showDetailsModal && selectedEvent && (
        <div className="modal-overlay">
          <div className="modal-content">
            <div className="modal-header">
              <h3>알람 상세 정보</h3>
              <button onClick={() => setShowDetailsModal(false)}>
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div className="modal-body">
              <div className="detail-section">
                <h4>기본 정보</h4>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>이벤트 ID</label>
                    <span>#{selectedEvent.id}</span>
                  </div>
                  <div className="detail-item">
                    <label>규칙명</label>
                    <span>{selectedEvent.alarmRuleName}</span>
                  </div>
                  <div className="detail-item">
                    <label>우선순위</label>
                    <span 
                      className="priority-badge"
                      style={{ backgroundColor: getPriorityColor(selectedEvent.priority) }}
                    >
                      {selectedEvent.priority.toUpperCase()}
                    </span>
                  </div>
                  <div className="detail-item">
                    <label>상태</label>
                    <span 
                      className="status-badge"
                      style={{ color: getStatusColor(selectedEvent.status) }}
                    >
                      <i className={getStatusIcon(selectedEvent.status)}></i>
                      {selectedEvent.status === 'active' ? '활성' :
                       selectedEvent.status === 'acknowledged' ? '확인됨' :
                       selectedEvent.status === 'cleared' ? '해제됨' : 
                       selectedEvent.status === 'suppressed' ? '억제됨' : selectedEvent.status}
                    </span>
                  </div>
                </div>
              </div>

              <div className="detail-section">
                <h4>시간 정보</h4>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>발생시간</label>
                    <span>{selectedEvent.triggeredAt.toLocaleString('ko-KR')}</span>
                  </div>
                  {selectedEvent.acknowledgedAt && (
                    <div className="detail-item">
                      <label>확인시간</label>
                      <span>{selectedEvent.acknowledgedAt.toLocaleString('ko-KR')}</span>
                    </div>
                  )}
                  {selectedEvent.clearedAt && (
                    <div className="detail-item">
                      <label>해제시간</label>
                      <span>{selectedEvent.clearedAt.toLocaleString('ko-KR')}</span>
                    </div>
                  )}
                  <div className="detail-item">
                    <label>지속시간</label>
                    <span>{formatDuration(selectedEvent.duration)}</span>
                  </div>
                </div>
              </div>

              <div className="detail-section">
                <h4>메시지</h4>
                <div className="message-content">
                  <p><strong>메시지:</strong> {selectedEvent.message}</p>
                  {selectedEvent.description && (
                    <p><strong>설명:</strong> {selectedEvent.description}</p>
                  )}
                  {selectedEvent.triggeredValue !== null && selectedEvent.triggeredValue !== undefined && (
                    <p><strong>발생값:</strong> {selectedEvent.triggeredValue}</p>
                  )}
                </div>
              </div>

              {(selectedEvent.acknowledgedBy || selectedEvent.acknowledgmentComment) && (
                <div className="detail-section">
                  <h4>확인 정보</h4>
                  <div className="detail-grid">
                    {selectedEvent.acknowledgedBy && (
                      <div className="detail-item">
                        <label>확인자</label>
                        <span>{selectedEvent.acknowledgedBy}</span>
                      </div>
                    )}
                    {selectedEvent.acknowledgmentComment && (
                      <div className="detail-item full-width">
                        <label>확인 코멘트</label>
                        <span>{selectedEvent.acknowledgmentComment}</span>
                      </div>
                    )}
                  </div>
                </div>
              )}
            </div>
            <div className="modal-footer">
              <button 
                className="btn btn-secondary"
                onClick={() => setShowDetailsModal(false)}
              >
                닫기
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AlarmHistory;