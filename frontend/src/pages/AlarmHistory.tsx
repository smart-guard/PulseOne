import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/alarm-history.css';

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

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 목업 알람 이벤트 생성
    const mockEvents: AlarmEvent[] = [];
    const now = new Date();
    
    // 지난 24시간 동안의 알람 이벤트 생성
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
      
      const event: AlarmEvent = {
        id: `evt_${i + 1000}`,
        alarmRuleId: `rule_${Math.floor(Math.random() * 10) + 1}`,
        alarmRuleName: `${category} 알람 ${Math.floor(Math.random() * 5) + 1}`,
        sourceType: Math.random() > 0.7 ? 'virtual_point' : 'data_point',
        sourceName: `${category} Sensor ${Math.floor(Math.random() * 10) + 1}`,
        factory,
        category,
        priority,
        severity: priority === 'critical' ? 5 : priority === 'high' ? 4 : priority === 'medium' ? 3 : Math.floor(Math.random() * 2) + 1,
        eventType,
        triggeredValue: Math.random() * 100,
        message: `${category} 알람이 발생했습니다. 현재값: ${(Math.random() * 100).toFixed(1)}`,
        description: `${factory}의 ${category} 시스템에서 임계값을 초과했습니다.`,
        triggeredAt: eventTime,
        acknowledgedAt: eventType === 'acknowledged' || Math.random() > 0.6 ? 
          new Date(eventTime.getTime() + Math.random() * 60 * 60 * 1000) : undefined,
        clearedAt: status === 'cleared' ? 
          new Date(eventTime.getTime() + Math.random() * 2 * 60 * 60 * 1000) : undefined,
        duration: status === 'cleared' ? Math.random() * 2 * 60 * 60 * 1000 : undefined,
        acknowledgedBy: eventType === 'acknowledged' ? `user_${Math.floor(Math.random() * 5) + 1}` : undefined,
        acknowledgmentComment: eventType === 'acknowledged' ? '확인 및 조치 완료' : undefined,
        escalated: Math.random() > 0.8,
        escalationLevel: Math.floor(Math.random() * 3),
        status,
        isActive: status === 'active',
        notificationsSent: {
          email: Math.random() > 0.3,
          sms: priority === 'critical' && Math.random() > 0.5,
          sound: Math.random() > 0.2,
          popup: Math.random() > 0.1
        },
        tags: [category.toLowerCase(), priority, factory.toLowerCase().replace(' ', '-')],
        relatedEvents: Math.random() > 0.7 ? [`evt_${i + 999}`, `evt_${i + 1001}`] : undefined
      };

      mockEvents.push(event);
    }

    // 시간순 정렬
    mockEvents.sort((a, b) => b.triggeredAt.getTime() - a.triggeredAt.getTime());

    // 요약 통계 계산
    const mockSummary: AlarmSummary = {
      totalEvents: mockEvents.length,
      activeAlarms: mockEvents.filter(e => e.status === 'active').length,
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
      hourlyTrends: Array.from({ length: 24 }, (_, i) => ({
        hour: i,
        count: Math.floor(Math.random() * 15) + 2
      }))
    };

    setAlarmEvents(mockEvents);
    setSummary(mockSummary);
    setTotalPages(Math.ceil(mockEvents.length / pageSize));
  };

  // 필터링된 이벤트 목록
  const filteredEvents = alarmEvents.filter(event => {
    const inDateRange = event.triggeredAt >= dateRange.start && event.triggeredAt <= dateRange.end;
    const matchesPriority = filterPriority === 'all' || event.priority === filterPriority;
    const matchesStatus = filterStatus === 'all' || event.status === filterStatus;
    const matchesCategory = filterCategory === 'all' || event.category === filterCategory;
    const matchesFactory = filterFactory === 'all' || event.factory === filterFactory;
    const matchesSearch = searchTerm === '' || 
      event.alarmRuleName.toLowerCase().includes(searchTerm.toLowerCase()) ||
      event.sourceName.toLowerCase().includes(searchTerm.toLowerCase()) ||
      event.message.toLowerCase().includes(searchTerm.toLowerCase());
    
    return inDateRange && matchesPriority && matchesStatus && matchesCategory && matchesFactory && matchesSearch;
  });

  // 페이지네이션
  const startIndex = (currentPage - 1) * pageSize;
  const endIndex = startIndex + pageSize;
  const currentEvents = filteredEvents.slice(startIndex, endIndex);

  // 이벤트 상세 보기
  const handleViewDetails = (event: AlarmEvent) => {
    setSelectedEvent(event);
    setShowDetailsModal(true);
  };

  // 알람 승인
  const handleAcknowledge = async (eventId: string, comment?: string) => {
    setAlarmEvents(prev => prev.map(event =>
      event.id === eventId
        ? {
            ...event,
            status: 'acknowledged',
            acknowledgedAt: new Date(),
            acknowledgedBy: 'current_user',
            acknowledgmentComment: comment || '알람 확인 완료'
          }
        : event
    ));
  };

  // 빠른 날짜 설정
  const setQuickDateRange = (hours: number) => {
    const end = new Date();
    const start = new Date(end.getTime() - hours * 60 * 60 * 1000);
    setDateRange({ start, end });
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

  // 상태별 색상
  const getStatusColor = (status: string) => {
    switch (status) {
      case 'active': return 'status-active';
      case 'acknowledged': return 'status-acknowledged';
      case 'cleared': return 'status-cleared';
      case 'suppressed': return 'status-suppressed';
      default: return 'status-active';
    }
  };

  // 기간 포맷
  const formatDuration = (ms: number): string => {
    const minutes = Math.floor(ms / 60000);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) return `${days}일 ${hours % 24}시간`;
    if (hours > 0) return `${hours}시간 ${minutes % 60}분`;
    return `${minutes}분`;
  };

  // 고유 값들 추출
  const uniqueCategories = [...new Set(alarmEvents.map(e => e.category))];
  const uniqueFactories = [...new Set(alarmEvents.map(e => e.factory))];

  return (
    <div className="alarm-history-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">알람 이력</h1>
        <div className="page-actions">
          <div className="view-toggle">
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
              <i className="fas fa-clock"></i>
              타임라인
            </button>
          </div>
        </div>
      </div>

      {/* 요약 통계 */}
      {summary && (
        <div className="summary-panel">
          <div className="summary-stats">
            <div className="stat-card">
              <div className="stat-value">{summary.totalEvents}</div>
              <div className="stat-label">총 이벤트</div>
            </div>
            <div className="stat-card status-active">
              <div className="stat-value">{summary.activeAlarms}</div>
              <div className="stat-label">활성 알람</div>
            </div>
            <div className="stat-card status-acknowledged">
              <div className="stat-value">{summary.acknowledgedAlarms}</div>
              <div className="stat-label">승인됨</div>
            </div>
            <div className="stat-card status-cleared">
              <div className="stat-value">{summary.clearedAlarms}</div>
              <div className="stat-label">해제됨</div>
            </div>
            <div className="stat-card priority-critical">
              <div className="stat-value">{summary.criticalAlarms}</div>
              <div className="stat-label">Critical</div>
            </div>
            <div className="stat-card">
              <div className="stat-value">{summary.averageResponseTime}</div>
              <div className="stat-label">평균 응답시간 (분)</div>
            </div>
          </div>

          <div className="top-sources">
            <h4>주요 알람 소스</h4>
            <div className="source-list">
              {summary.topAlarmSources.map((source, index) => (
                <div key={source.source} className="source-item">
                  <span className="source-rank">#{index + 1}</span>
                  <span className="source-name">{source.source}</span>
                  <span className="source-count">{source.count}회</span>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-section">
          <h3>조회 기간</h3>
          <div className="date-filter">
            <div className="date-inputs">
              <input
                type="datetime-local"
                value={dateRange.start.toISOString().slice(0, 16)}
                onChange={(e) => setDateRange(prev => ({ ...prev, start: new Date(e.target.value) }))}
                className="form-input"
              />
              <span>~</span>
              <input
                type="datetime-local"
                value={dateRange.end.toISOString().slice(0, 16)}
                onChange={(e) => setDateRange(prev => ({ ...prev, end: new Date(e.target.value) }))}
                className="form-input"
              />
            </div>
            <div className="quick-dates">
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(1)}>1시간</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(6)}>6시간</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24)}>24시간</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 7)}>7일</button>
            </div>
          </div>
        </div>

        <div className="filter-section">
          <h3>필터</h3>
          <div className="filter-row">
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
                <option value="cleared">해제됨</option>
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

            <div className="filter-group flex-1">
              <label>검색</label>
              <div className="search-container">
                <input
                  type="text"
                  placeholder="알람명, 소스명, 메시지 검색..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="search-input"
                />
                <i className="fas fa-search search-icon"></i>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* 결과 정보 */}
      <div className="result-info">
        <span className="result-count">
          총 {filteredEvents.length}개 이벤트 (전체 {alarmEvents.length}개 중)
        </span>
        <span className="date-range-display">
          {dateRange.start.toLocaleString()} ~ {dateRange.end.toLocaleString()}
        </span>
      </div>

      {/* 이벤트 목록/타임라인 */}
      {viewMode === 'list' ? (
        <div className="events-list">
          <div className="events-table">
            <div className="table-header">
              <div className="header-cell">시간</div>
              <div className="header-cell">알람 정보</div>
              <div className="header-cell">우선순위</div>
              <div className="header-cell">상태</div>
              <div className="header-cell">소스</div>
              <div className="header-cell">메시지</div>
              <div className="header-cell">처리</div>
              <div className="header-cell">액션</div>
            </div>

            {currentEvents.map(event => (
              <div key={event.id} className={`table-row ${getStatusColor(event.status)}`}>
                <div className="table-cell time-cell">
                  <div className="event-time">
                    {event.triggeredAt.toLocaleString()}
                  </div>
                  {event.duration && (
                    <div className="duration">
                      지속: {formatDuration(event.duration)}
                    </div>
                  )}
                </div>

                <div className="table-cell alarm-info-cell">
                  <div className="alarm-name">{event.alarmRuleName}</div>
                  <div className="alarm-category">{event.category}</div>
                  <div className="alarm-factory">{event.factory}</div>
                </div>

                <div className="table-cell priority-cell">
                  <span className={`priority-badge ${getPriorityColor(event.priority)}`}>
                    {event.priority.toUpperCase()}
                  </span>
                  <div className="severity">심각도: {event.severity}</div>
                </div>

                <div className="table-cell status-cell">
                  <span className={`status-badge ${getStatusColor(event.status)}`}>
                    {event.status === 'active' ? '활성' :
                     event.status === 'acknowledged' ? '승인됨' :
                     event.status === 'cleared' ? '해제됨' : event.status}
                  </span>
                  {event.escalated && (
                    <div className="escalation-badge">에스컬레이션</div>
                  )}
                </div>

                <div className="table-cell source-cell">
                  <div className="source-name">{event.sourceName}</div>
                  <div className="source-type">{event.sourceType}</div>
                  {event.triggeredValue !== undefined && (
                    <div className="triggered-value">
                      값: {typeof event.triggeredValue === 'number' 
                        ? event.triggeredValue.toFixed(2) 
                        : String(event.triggeredValue)}
                    </div>
                  )}
                </div>

                <div className="table-cell message-cell">
                  <div className="message-text">{event.message}</div>
                  <div className="notification-info">
                    {event.notificationsSent.email && <i className="fas fa-envelope" title="이메일 발송"></i>}
                    {event.notificationsSent.sms && <i className="fas fa-sms" title="SMS 발송"></i>}
                    {event.notificationsSent.sound && <i className="fas fa-volume-up" title="소리 알림"></i>}
                    {event.notificationsSent.popup && <i className="fas fa-window-maximize" title="팝업 표시"></i>}
                  </div>
                </div>

                <div className="table-cell processing-cell">
                  {event.acknowledgedBy && (
                    <div className="acknowledged-info">
                      <div className="acknowledged-by">승인: {event.acknowledgedBy}</div>
                      <div className="acknowledged-time">
                        {event.acknowledgedAt?.toLocaleString()}
                      </div>
                      {event.acknowledgmentComment && (
                        <div className="acknowledgment-comment">
                          {event.acknowledgmentComment}
                        </div>
                      )}
                    </div>
                  )}
                </div>

                <div className="table-cell action-cell">
                  <div className="action-buttons">
                    <button
                      className="btn btn-sm btn-primary"
                      onClick={() => handleViewDetails(event)}
                      title="상세 보기"
                    >
                      <i className="fas fa-eye"></i>
                    </button>
                    {event.status === 'active' && (
                      <button
                        className="btn btn-sm btn-success"
                        onClick={() => handleAcknowledge(event.id)}
                        title="승인"
                      >
                        <i className="fas fa-check"></i>
                      </button>
                    )}
                  </div>
                </div>
              </div>
            ))}
          </div>

          {currentEvents.length === 0 && (
            <div className="empty-state">
              <i className="fas fa-bell-slash empty-icon"></i>
              <div className="empty-title">알람 이벤트가 없습니다</div>
              <div className="empty-description">
                선택한 기간과 필터 조건에 해당하는 알람 이벤트가 없습니다.
              </div>
            </div>
          )}

          {/* 페이지네이션 */}
          {filteredEvents.length > 0 && (
            <div className="pagination-container">
              <div className="pagination-info">
                {startIndex + 1}-{Math.min(endIndex, filteredEvents.length)} / {filteredEvents.length} 항목
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
                  <option value={25}>25개씩</option>
                  <option value={50}>50개씩</option>
                  <option value={100}>100개씩</option>
                  <option value={200}>200개씩</option>
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
                  {currentPage} / {Math.ceil(filteredEvents.length / pageSize)}
                </span>
                
                <button
                  className="btn btn-sm"
                  disabled={currentPage === Math.ceil(filteredEvents.length / pageSize)}
                  onClick={() => setCurrentPage(prev => prev + 1)}
                >
                  <i className="fas fa-angle-right"></i>
                </button>
                <button
                  className="btn btn-sm"
                  disabled={currentPage === Math.ceil(filteredEvents.length / pageSize)}
                  onClick={() => setCurrentPage(Math.ceil(filteredEvents.length / pageSize))}
                >
                  <i className="fas fa-angle-double-right"></i>
                </button>
              </div>
            </div>
          )}
        </div>
      ) : (
        // 타임라인 뷰
        <div className="timeline-view">
          <div className="timeline-container">
            {currentEvents.map(event => (
              <div key={event.id} className={`timeline-item ${getPriorityColor(event.priority)}`}>
                <div className="timeline-marker"></div>
                <div className="timeline-content">
                  <div className="timeline-header">
                    <div className="timeline-time">
                      {event.triggeredAt.toLocaleString()}
                    </div>
                    <span className={`priority-badge ${getPriorityColor(event.priority)}`}>
                      {event.priority.toUpperCase()}
                    </span>
                  </div>
                  <div className="timeline-alarm">
                    <h4>{event.alarmRuleName}</h4>
                    <p>{event.message}</p>
                  </div>
                  <div className="timeline-meta">
                    <span>소스: {event.sourceName}</span>
                    <span>카테고리: {event.category}</span>
                    <span>공장: {event.factory}</span>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 상세 정보 모달 */}
      {showDetailsModal && selectedEvent && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>알람 이벤트 상세 정보</h2>
              <button
                className="modal-close-btn"
                onClick={() => setShowDetailsModal(false)}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="detail-section">
                <h3>기본 정보</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>알람 룰:</label>
                    <span>{selectedEvent.alarmRuleName}</span>
                  </div>
                  <div className="detail-item">
                    <label>우선순위:</label>
                    <span className={`priority-badge ${getPriorityColor(selectedEvent.priority)}`}>
                      {selectedEvent.priority.toUpperCase()}
                    </span>
                  </div>
                  <div className="detail-item">
                    <label>심각도:</label>
                    <span>{selectedEvent.severity}/5</span>
                  </div>
                  <div className="detail-item">
                    <label>카테고리:</label>
                    <span>{selectedEvent.category}</span>
                  </div>
                  <div className="detail-item">
                    <label>공장:</label>
                    <span>{selectedEvent.factory}</span>
                  </div>
                  <div className="detail-item">
                    <label>상태:</label>
                    <span className={`status-badge ${getStatusColor(selectedEvent.status)}`}>
                      {selectedEvent.status}
                    </span>
                  </div>
                </div>
              </div>

              <div className="detail-section">
                <h3>이벤트 정보</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>발생 시간:</label>
                    <span>{selectedEvent.triggeredAt.toLocaleString()}</span>
                  </div>
                  <div className="detail-item">
                    <label>이벤트 타입:</label>
                    <span>{selectedEvent.eventType}</span>
                  </div>
                  <div className="detail-item">
                    <label>소스:</label>
                    <span>{selectedEvent.sourceName} ({selectedEvent.sourceType})</span>
                  </div>
                  <div className="detail-item">
                    <label>트리거 값:</label>
                    <span>{selectedEvent.triggeredValue}</span>
                  </div>
                  <div className="detail-item full-width">
                    <label>메시지:</label>
                    <span>{selectedEvent.message}</span>
                  </div>
                  {selectedEvent.description && (
                    <div className="detail-item full-width">
                      <label>설명:</label>
                      <span>{selectedEvent.description}</span>
                    </div>
                  )}
                </div>
              </div>

              {selectedEvent.acknowledgedBy && (
                <div className="detail-section">
                  <h3>승인 정보</h3>
                  <div className="detail-grid">
                    <div className="detail-item">
                      <label>승인자:</label>
                      <span>{selectedEvent.acknowledgedBy}</span>
                    </div>
                    <div className="detail-item">
                      <label>승인 시간:</label>
                      <span>{selectedEvent.acknowledgedAt?.toLocaleString()}</span>
                    </div>
                    {selectedEvent.acknowledgmentComment && (
                      <div className="detail-item full-width">
                        <label>승인 코멘트:</label>
                        <span>{selectedEvent.acknowledgmentComment}</span>
                      </div>
                    )}
                  </div>
                </div>
              )}

              <div className="detail-section">
                <h3>알림 정보</h3>
                <div className="notification-status">
                  <div className={`notification-item ${selectedEvent.notificationsSent.email ? 'sent' : 'not-sent'}`}>
                    <i className="fas fa-envelope"></i>
                    <span>이메일 {selectedEvent.notificationsSent.email ? '발송됨' : '발송 안됨'}</span>
                  </div>
                  <div className={`notification-item ${selectedEvent.notificationsSent.sms ? 'sent' : 'not-sent'}`}>
                    <i className="fas fa-sms"></i>
                    <span>SMS {selectedEvent.notificationsSent.sms ? '발송됨' : '발송 안됨'}</span>
                  </div>
                  <div className={`notification-item ${selectedEvent.notificationsSent.sound ? 'sent' : 'not-sent'}`}>
                    <i className="fas fa-volume-up"></i>
                    <span>소리 알림 {selectedEvent.notificationsSent.sound ? '재생됨' : '재생 안됨'}</span>
                  </div>
                  <div className={`notification-item ${selectedEvent.notificationsSent.popup ? 'sent' : 'not-sent'}`}>
                    <i className="fas fa-window-maximize"></i>
                    <span>팝업 {selectedEvent.notificationsSent.popup ? '표시됨' : '표시 안됨'}</span>
                  </div>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              {selectedEvent.status === 'active' && (
                <button
                  className="btn btn-success"
                  onClick={() => {
                    handleAcknowledge(selectedEvent.id);
                    setShowDetailsModal(false);
                  }}
                >
                  <i className="fas fa-check"></i>
                  승인
                </button>
              )}
              <button
                className="btn btn-outline"
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