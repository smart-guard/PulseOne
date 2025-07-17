import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/alarm-history.css';

interface LoginRecord {
  id: string;
  username: string;
  fullName: string;
  email: string;
  role: string;
  loginTime: Date;
  logoutTime?: Date;
  duration?: number; // minutes
  ipAddress: string;
  userAgent: string;
  browser: string;
  os: string;
  device: string;
  location?: string;
  loginMethod: 'password' | 'sso' | 'api_key';
  success: boolean;
  failureReason?: string;
  sessionId: string;
}

interface LoginStats {
  totalLogins: number;
  successfulLogins: number;
  failedLogins: number;
  uniqueUsers: number;
  averageSessionDuration: number;
  peakHour: number;
  topUsers: Array<{ username: string; count: number }>;
  recentFailures: number;
}

const LoginHistory: React.FC = () => {
  const [loginRecords, setLoginRecords] = useState<LoginRecord[]>([]);
  const [stats, setStats] = useState<LoginStats | null>(null);
  const [selectedRecord, setSelectedRecord] = useState<LoginRecord | null>(null);
  const [showDetailsModal, setShowDetailsModal] = useState(false);
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(50);
  
  // 필터 상태
  const [dateRange, setDateRange] = useState({
    start: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000), // 7일 전
    end: new Date()
  });
  const [filterUser, setFilterUser] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [filterMethod, setFilterMethod] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 목업 로그인 기록 생성
    const mockRecords: LoginRecord[] = [];
    const now = new Date();
    
    const users = [
      { username: 'admin', fullName: '시스템 관리자', email: 'admin@company.com', role: 'admin' },
      { username: 'manager1', fullName: '김매니저', email: 'manager1@company.com', role: 'manager' },
      { username: 'engineer1', fullName: '이엔지니어', email: 'engineer1@company.com', role: 'engineer' },
      { username: 'operator1', fullName: '박운영자', email: 'operator1@company.com', role: 'operator' },
      { username: 'viewer1', fullName: '정조회자', email: 'viewer1@company.com', role: 'viewer' }
    ];

    const browsers = ['Chrome', 'Firefox', 'Safari', 'Edge'];
    const os = ['Windows 10', 'Windows 11', 'macOS', 'Linux'];
    const devices = ['Desktop', 'Laptop', 'Tablet', 'Mobile'];
    const locations = ['Seoul, KR', 'Busan, KR', 'Incheon, KR', 'Unknown'];
    const methods: LoginRecord['loginMethod'][] = ['password', 'sso', 'api_key'];

    // 지난 7일간의 로그인 기록 생성
    for (let i = 0; i < 200; i++) {
      const user = users[Math.floor(Math.random() * users.length)];
      const loginTime = new Date(now.getTime() - Math.random() * 7 * 24 * 60 * 60 * 1000);
      const success = Math.random() > 0.1; // 90% 성공률
      const duration = success ? Math.random() * 480 + 10 : undefined; // 10분-8시간
      const logoutTime = success && Math.random() > 0.3 ? 
        new Date(loginTime.getTime() + (duration! * 60 * 1000)) : undefined;

      const record: LoginRecord = {
        id: `login_${i + 1000}`,
        username: user.username,
        fullName: user.fullName,
        email: user.email,
        role: user.role,
        loginTime,
        logoutTime,
        duration: logoutTime ? Math.floor((logoutTime.getTime() - loginTime.getTime()) / 60000) : undefined,
        ipAddress: `192.168.1.${Math.floor(Math.random() * 254) + 1}`,
        userAgent: `Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36`,
        browser: browsers[Math.floor(Math.random() * browsers.length)],
        os: os[Math.floor(Math.random() * os.length)],
        device: devices[Math.floor(Math.random() * devices.length)],
        location: locations[Math.floor(Math.random() * locations.length)],
        loginMethod: methods[Math.floor(Math.random() * methods.length)],
        success,
        failureReason: !success ? 
          ['잘못된 비밀번호', '계정 잠김', '세션 만료', '네트워크 오류'][Math.floor(Math.random() * 4)] : undefined,
        sessionId: `sess_${Date.now()}_${i}`
      };

      mockRecords.push(record);
    }

    // 시간순 정렬 (최신순)
    mockRecords.sort((a, b) => b.loginTime.getTime() - a.loginTime.getTime());

    // 통계 계산
    const mockStats: LoginStats = {
      totalLogins: mockRecords.length,
      successfulLogins: mockRecords.filter(r => r.success).length,
      failedLogins: mockRecords.filter(r => !r.success).length,
      uniqueUsers: new Set(mockRecords.map(r => r.username)).size,
      averageSessionDuration: mockRecords
        .filter(r => r.duration)
        .reduce((sum, r) => sum + r.duration!, 0) / mockRecords.filter(r => r.duration).length,
      peakHour: 14, // 오후 2시
      topUsers: Object.entries(
        mockRecords.reduce((acc, r) => {
          acc[r.username] = (acc[r.username] || 0) + 1;
          return acc;
        }, {} as Record<string, number>)
      )
        .sort(([,a], [,b]) => b - a)
        .slice(0, 5)
        .map(([username, count]) => ({ username, count })),
      recentFailures: mockRecords.filter(r => 
        !r.success && r.loginTime > new Date(Date.now() - 24 * 60 * 60 * 1000)
      ).length
    };

    setLoginRecords(mockRecords);
    setStats(mockStats);
  };

  // 필터링된 기록 목록
  const filteredRecords = loginRecords.filter(record => {
    const inDateRange = record.loginTime >= dateRange.start && record.loginTime <= dateRange.end;
    const matchesUser = filterUser === 'all' || record.username === filterUser;
    const matchesStatus = filterStatus === 'all' || 
      (filterStatus === 'success' && record.success) ||
      (filterStatus === 'failed' && !record.success);
    const matchesMethod = filterMethod === 'all' || record.loginMethod === filterMethod;
    const matchesSearch = searchTerm === '' || 
      record.username.toLowerCase().includes(searchTerm.toLowerCase()) ||
      record.fullName.toLowerCase().includes(searchTerm.toLowerCase()) ||
      record.ipAddress.includes(searchTerm) ||
      record.browser.toLowerCase().includes(searchTerm.toLowerCase());
    
    return inDateRange && matchesUser && matchesStatus && matchesMethod && matchesSearch;
  });

  // 페이지네이션
  const startIndex = (currentPage - 1) * pageSize;
  const endIndex = startIndex + pageSize;
  const currentRecords = filteredRecords.slice(startIndex, endIndex);

  // 상세 보기
  const handleViewDetails = (record: LoginRecord) => {
    setSelectedRecord(record);
    setShowDetailsModal(true);
  };

  // 빠른 날짜 설정
  const setQuickDateRange = (hours: number) => {
    const end = new Date();
    const start = new Date(end.getTime() - hours * 60 * 60 * 1000);
    setDateRange({ start, end });
  };

  // 기간 포맷
  const formatDuration = (minutes: number): string => {
    const hours = Math.floor(minutes / 60);
    const mins = Math.floor(minutes % 60);
    if (hours > 0) return `${hours}시간 ${mins}분`;
    return `${mins}분`;
  };

  // 상태 아이콘 및 색상
  const getStatusIcon = (success: boolean) => {
    return success ? 'fa-check-circle text-success-500' : 'fa-times-circle text-error-500';
  };

  // 로그인 방법 아이콘
  const getMethodIcon = (method: string) => {
    switch (method) {
      case 'password': return 'fa-key';
      case 'sso': return 'fa-id-card';
      case 'api_key': return 'fa-code';
      default: return 'fa-sign-in-alt';
    }
  };

  // 고유 사용자 목록
  const uniqueUsers = [...new Set(loginRecords.map(r => r.username))];

  return (
    <div className="alarm-history-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">로그인 이력</h1>
        <div className="page-actions">
          <button className="btn btn-outline">
            <i className="fas fa-download"></i>
            내보내기
          </button>
          <button className="btn btn-outline">
            <i className="fas fa-shield-alt"></i>
            보안 분석
          </button>
        </div>
      </div>

      {/* 요약 통계 */}
      {stats && (
        <div className="summary-panel">
          <div className="summary-stats">
            <div className="stat-card">
              <div className="stat-value">{stats.totalLogins}</div>
              <div className="stat-label">총 로그인</div>
            </div>
            <div className="stat-card status-cleared">
              <div className="stat-value">{stats.successfulLogins}</div>
              <div className="stat-label">성공</div>
            </div>
            <div className="stat-card status-active">
              <div className="stat-value">{stats.failedLogins}</div>
              <div className="stat-label">실패</div>
            </div>
            <div className="stat-card">
              <div className="stat-value">{stats.uniqueUsers}</div>
              <div className="stat-label">활성 사용자</div>
            </div>
            <div className="stat-card">
              <div className="stat-value">{Math.round(stats.averageSessionDuration)}</div>
              <div className="stat-label">평균 세션(분)</div>
            </div>
            <div className="stat-card status-acknowledged">
              <div className="stat-value">{stats.recentFailures}</div>
              <div className="stat-label">최근 실패</div>
            </div>
          </div>

          <div className="top-sources">
            <h4>활성 사용자 TOP 5</h4>
            <div className="source-list">
              {stats.topUsers.map((user, index) => (
                <div key={user.username} className="source-item">
                  <span className="source-rank">#{index + 1}</span>
                  <span className="source-name">{user.username}</span>
                  <span className="source-count">{user.count}회</span>
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
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24)}>24시간</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 7)}>7일</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 30)}>30일</button>
            </div>
          </div>
        </div>

        <div className="filter-section">
          <h3>필터</h3>
          <div className="filter-row">
            <div className="filter-group">
              <label>사용자</label>
              <select
                value={filterUser}
                onChange={(e) => setFilterUser(e.target.value)}
                className="filter-select"
              >
                <option value="all">전체</option>
                {uniqueUsers.map(user => (
                  <option key={user} value={user}>{user}</option>
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
                <option value="success">성공</option>
                <option value="failed">실패</option>
              </select>
            </div>

            <div className="filter-group">
              <label>로그인 방법</label>
              <select
                value={filterMethod}
                onChange={(e) => setFilterMethod(e.target.value)}
                className="filter-select"
              >
                <option value="all">전체</option>
                <option value="password">비밀번호</option>
                <option value="sso">SSO</option>
                <option value="api_key">API 키</option>
              </select>
            </div>

            <div className="filter-group flex-1">
              <label>검색</label>
              <div className="search-container">
                <input
                  type="text"
                  placeholder="사용자명, IP, 브라우저 검색..."
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
          총 {filteredRecords.length}개 기록 (전체 {loginRecords.length}개 중)
        </span>
        <span className="date-range-display">
          {dateRange.start.toLocaleString()} ~ {dateRange.end.toLocaleString()}
        </span>
      </div>

      {/* 로그인 이력 목록 */}
      <div className="events-list">
        <div className="events-table">
          <div className="table-header">
            <div className="header-cell">시간</div>
            <div className="header-cell">사용자</div>
            <div className="header-cell">상태</div>
            <div className="header-cell">방법</div>
            <div className="header-cell">위치/디바이스</div>
            <div className="header-cell">세션</div>
            <div className="header-cell">IP 주소</div>
            <div className="header-cell">액션</div>
          </div>

          {currentRecords.map(record => (
            <div key={record.id} className={`table-row ${record.success ? 'status-cleared' : 'status-active'}`}>
              <div className="table-cell time-cell">
                <div className="event-time">
                  {record.loginTime.toLocaleString()}
                </div>
                {record.logoutTime && (
                  <div className="duration">
                    로그아웃: {record.logoutTime.toLocaleString()}
                  </div>
                )}
              </div>

              <div className="table-cell alarm-info-cell">
                <div className="alarm-name">{record.fullName}</div>
                <div className="alarm-category">@{record.username}</div>
                <div className="alarm-factory">{record.role}</div>
              </div>

              <div className="table-cell status-cell">
                <span className={`status-badge ${record.success ? 'status-cleared' : 'status-active'}`}>
                  <i className={`fas ${getStatusIcon(record.success)}`}></i>
                  {record.success ? '성공' : '실패'}
                </span>
                {!record.success && record.failureReason && (
                  <div className="severity">{record.failureReason}</div>
                )}
              </div>

              <div className="table-cell priority-cell">
                <span className="priority-badge priority-medium">
                  <i className={`fas ${getMethodIcon(record.loginMethod)}`}></i>
                  {record.loginMethod === 'password' ? '비밀번호' :
                   record.loginMethod === 'sso' ? 'SSO' : 'API 키'}
                </span>
              </div>

              <div className="table-cell source-cell">
                <div className="source-name">{record.location || 'Unknown'}</div>
                <div className="source-type">{record.device}</div>
                <div className="triggered-value">{record.browser} / {record.os}</div>
              </div>

              <div className="table-cell message-cell">
                {record.duration ? (
                  <div className="message-text">
                    세션: {formatDuration(record.duration)}
                  </div>
                ) : record.success ? (
                  <div className="message-text">진행 중</div>
                ) : (
                  <div className="message-text">세션 없음</div>
                )}
                <div className="notification-info">
                  <small className="text-xs text-neutral-500">
                    ID: {record.sessionId.slice(-8)}
                  </small>
                </div>
              </div>

              <div className="table-cell processing-cell">
                <div className="acknowledged-by">{record.ipAddress}</div>
                <div className="acknowledged-time">
                  <small className="text-xs text-neutral-500">
                    {record.userAgent.split(' ')[0]}
                  </small>
                </div>
              </div>

              <div className="table-cell action-cell">
                <div className="action-buttons">
                  <button
                    className="btn btn-sm btn-primary"
                    onClick={() => handleViewDetails(record)}
                    title="상세 보기"
                  >
                    <i className="fas fa-eye"></i>
                  </button>
                </div>
              </div>
            </div>
          ))}
        </div>

        {currentRecords.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-sign-in-alt empty-icon"></i>
            <div className="empty-title">로그인 기록이 없습니다</div>
            <div className="empty-description">
              선택한 기간과 필터 조건에 해당하는 로그인 기록이 없습니다.
            </div>
          </div>
        )}

        {/* 페이지네이션 */}
        {filteredRecords.length > 0 && (
          <div className="pagination-container">
            <div className="pagination-info">
              {startIndex + 1}-{Math.min(endIndex, filteredRecords.length)} / {filteredRecords.length} 항목
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
                {currentPage} / {Math.ceil(filteredRecords.length / pageSize)}
              </span>
              
              <button
                className="btn btn-sm"
                disabled={currentPage === Math.ceil(filteredRecords.length / pageSize)}
                onClick={() => setCurrentPage(prev => prev + 1)}
              >
                <i className="fas fa-angle-right"></i>
              </button>
              <button
                className="btn btn-sm"
                disabled={currentPage === Math.ceil(filteredRecords.length / pageSize)}
                onClick={() => setCurrentPage(Math.ceil(filteredRecords.length / pageSize))}
              >
                <i className="fas fa-angle-double-right"></i>
              </button>
            </div>
          </div>
        )}
      </div>

      {/* 상세 정보 모달 */}
      {showDetailsModal && selectedRecord && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>로그인 기록 상세 정보</h2>
              <button
                className="modal-close-btn"
                onClick={() => setShowDetailsModal(false)}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="detail-section">
                <h3>사용자 정보</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>사용자명:</label>
                    <span>{selectedRecord.username}</span>
                  </div>
                  <div className="detail-item">
                    <label>이름:</label>
                    <span>{selectedRecord.fullName}</span>
                  </div>
                  <div className="detail-item">
                    <label>이메일:</label>
                    <span>{selectedRecord.email}</span>
                  </div>
                  <div className="detail-item">
                    <label>역할:</label>
                    <span>{selectedRecord.role}</span>
                  </div>
                </div>
              </div>

              <div className="detail-section">
                <h3>로그인 정보</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>로그인 시간:</label>
                    <span>{selectedRecord.loginTime.toLocaleString()}</span>
                  </div>
                  <div className="detail-item">
                    <label>로그아웃 시간:</label>
                    <span>{selectedRecord.logoutTime?.toLocaleString() || '진행 중'}</span>
                  </div>
                  <div className="detail-item">
                    <label>세션 기간:</label>
                    <span>{selectedRecord.duration ? formatDuration(selectedRecord.duration) : '-'}</span>
                  </div>
                  <div className="detail-item">
                    <label>로그인 방법:</label>
                    <span>{selectedRecord.loginMethod}</span>
                  </div>
                  <div className="detail-item">
                    <label>상태:</label>
                    <span className={selectedRecord.success ? 'text-success-600' : 'text-error-600'}>
                      {selectedRecord.success ? '성공' : '실패'}
                    </span>
                  </div>
                  {!selectedRecord.success && (
                    <div className="detail-item">
                      <label>실패 사유:</label>
                      <span>{selectedRecord.failureReason}</span>
                    </div>
                  )}
                </div>
              </div>

              <div className="detail-section">
                <h3>기술 정보</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>IP 주소:</label>
                    <span>{selectedRecord.ipAddress}</span>
                  </div>
                  <div className="detail-item">
                    <label>브라우저:</label>
                    <span>{selectedRecord.browser}</span>
                  </div>
                  <div className="detail-item">
                    <label>운영체제:</label>
                    <span>{selectedRecord.os}</span>
                  </div>
                  <div className="detail-item">
                    <label>디바이스:</label>
                    <span>{selectedRecord.device}</span>
                  </div>
                  <div className="detail-item">
                    <label>위치:</label>
                    <span>{selectedRecord.location || 'Unknown'}</span>
                  </div>
                  <div className="detail-item">
                    <label>세션 ID:</label>
                    <span>{selectedRecord.sessionId}</span>
                  </div>
                  <div className="detail-item full-width">
                    <label>User Agent:</label>
                    <span style={{ wordBreak: 'break-all', fontSize: 'var(--text-xs)' }}>
                      {selectedRecord.userAgent}
                    </span>
                  </div>
                </div>
              </div>
            </div>

            <div className="modal-footer">
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

export default LoginHistory;