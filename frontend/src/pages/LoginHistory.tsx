import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
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
    const { t } = useTranslation(['loginHistory', 'common']);
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
      { username: 'admin', fullName: 'System Admin', email: 'admin@company.com', role: 'admin' },
      { username: 'manager1', fullName: 'Kim Manager', email: 'manager1@company.com', role: 'manager' },
      { username: 'engineer1', fullName: 'Lee Engineer', email: 'engineer1@company.com', role: 'engineer' },
      { username: 'operator1', fullName: 'Park Operator', email: 'operator1@company.com', role: 'operator' },
      { username: 'viewer1', fullName: 'Chung Viewer', email: 'viewer1@company.com', role: 'viewer' }
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
          ['Wrong password', 'Account locked', 'Session expired', 'Network error'][Math.floor(Math.random() * 4)] : undefined,
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
    if (hours > 0) return `${hours}h ${mins}m`;
    return `${mins}m`;
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
        <h1 className="page-title">{t('labels.loginHistory', {ns: 'auditLog'})}</h1>
        <div className="page-actions">
          <button className="btn btn-outline">
            <i className="fas fa-download"></i>
            Export
          </button>
          <button className="btn btn-outline">
            <i className="fas fa-shield-alt"></i>
            Security Analysis
          </button>
        </div>
      </div>

      {/* 요약 통계 */}
      {stats && (
        <div className="summary-panel">
          <div className="summary-stats">
            <div className="stat-card">
              <div className="stat-value">{stats.totalLogins}</div>
              <div className="stat-label">{t('labels.totalLogins', {ns: 'auditLog'})}</div>
            </div>
            <div className="stat-card status-cleared">
              <div className="stat-value">{stats.successfulLogins}</div>
              <div className="stat-label">{t('labels.successful', {ns: 'auditLog'})}</div>
            </div>
            <div className="stat-card status-active">
              <div className="stat-value">{stats.failedLogins}</div>
              <div className="stat-label">Failed</div>
            </div>
            <div className="stat-card">
              <div className="stat-value">{stats.uniqueUsers}</div>
              <div className="stat-label">{t('labels.activeUsers', {ns: 'auditLog'})}</div>
            </div>
            <div className="stat-card">
              <div className="stat-value">{Math.round(stats.averageSessionDuration)}</div>
              <div className="stat-label">{t('labels.avgSessionMin', {ns: 'auditLog'})}</div>
            </div>
            <div className="stat-card status-acknowledged">
              <div className="stat-value">{stats.recentFailures}</div>
              <div className="stat-label">{t('labels.recentFailures', {ns: 'auditLog'})}</div>
            </div>
          </div>

          <div className="top-sources">
            <h4>Top 5 Active Users</h4>
            <div className="source-list">
              {stats.topUsers.map((user, index) => (
                <div key={user.username} className="source-item">
                  <span className="source-rank">#{index + 1}</span>
                  <span className="source-name">{user.username}</span>
                  <span className="source-count">{user.count}x</span>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-section">
          <h3>{t('labels.timeRange', {ns: 'auditLog'})}</h3>
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
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(1)}>1hr</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24)}>24hrs</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 7)}>7d</button>
              <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 30)}>30d</button>
            </div>
          </div>
        </div>

        <div className="filter-section">
          <h3>{t('labels.filter', {ns: 'auditLog'})}</h3>
          <div className="filter-row">
            <div className="filter-group">
              <label>{t('filter.user', {ns: 'auditLog'})}</label>
              <select
                value={filterUser}
                onChange={(e) => setFilterUser(e.target.value)}
                className="filter-select"
              >
                <option value="all">All</option>
                {uniqueUsers.map(user => (
                  <option key={user} value={user}>{user}</option>
                ))}
              </select>
            </div>

            <div className="filter-group">
              <label>{t('labels.status', {ns: 'auditLog'})}</label>
              <select
                value={filterStatus}
                onChange={(e) => setFilterStatus(e.target.value)}
                className="filter-select"
              >
                <option value="all">All</option>
                <option value="success">{t('labels.successful', {ns: 'auditLog'})}</option>
                <option value="failed">Failed</option>
              </select>
            </div>

            <div className="filter-group">
              <label>{t('labels.loginMethod', {ns: 'auditLog'})}</label>
              <select
                value={filterMethod}
                onChange={(e) => setFilterMethod(e.target.value)}
                className="filter-select"
              >
                <option value="all">All</option>
                <option value="password">{t('labels.password', {ns: 'auditLog'})}</option>
                <option value="sso">{t('labels.sso', {ns: 'auditLog'})}</option>
                <option value="api_key">{t('labels.apiKey', {ns: 'auditLog'})}</option>
              </select>
            </div>

            <div className="filter-group flex-1">
              <label>{t('search', {ns: 'common'})}</label>
              <div className="search-container">
                <input
                  type="text"
                  placeholder="Search username, IP, browser..."
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
          {filteredRecords.length} record(s) (of {loginRecords.length} total)
        </span>
        <span className="date-range-display">
          {dateRange.start.toLocaleString()} ~ {dateRange.end.toLocaleString()}
        </span>
      </div>

      {/* Login History 목록 */}
      <div className="events-list">
        <div className="events-table">
          <div className="table-header">
            <div className="header-cell">{t('table.time', {ns: 'auditLog'})}</div>
            <div className="header-cell">{t('filter.user', {ns: 'auditLog'})}</div>
            <div className="header-cell">{t('labels.status', {ns: 'auditLog'})}</div>
            <div className="header-cell">{t('labels.method', {ns: 'auditLog'})}</div>
            <div className="header-cell">{t('labels.locationdevice', {ns: 'auditLog'})}</div>
            <div className="header-cell">{t('labels.session', {ns: 'auditLog'})}</div>
            <div className="header-cell">{t('labels.ipAddress', {ns: 'auditLog'})}</div>
            <div className="header-cell">{t('table.action', {ns: 'auditLog'})}</div>
          </div>

          {currentRecords.map(record => (
            <div key={record.id} className={`table-row ${record.success ? 'status-cleared' : 'status-active'}`}>
              <div className="table-cell time-cell">
                <div className="event-time">
                  {record.loginTime.toLocaleString()}
                </div>
                {record.logoutTime && (
                  <div className="duration">
                    Logout: {record.logoutTime.toLocaleString()}
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
                  {record.success ? 'Success' : 'Failed'}
                </span>
                {!record.success && record.failureReason && (
                  <div className="severity">{record.failureReason}</div>
                )}
              </div>

              <div className="table-cell priority-cell">
                <span className="priority-badge priority-medium">
                  <i className={`fas ${getMethodIcon(record.loginMethod)}`}></i>
                  {record.loginMethod === 'password' ? 'Password' :
                   record.loginMethod === 'sso' ? 'SSO' : 'API Key'}
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
                    Session: {formatDuration(record.duration)}
                  </div>
                ) : record.success ? (
                  <div className="message-text">{t('labels.inProgress', {ns: 'auditLog'})}</div>
                ) : (
                  <div className="message-text">{t('labels.noSession', {ns: 'auditLog'})}</div>
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
                    title="View Details"
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
            <div className="empty-title">{t('labels.noLoginHistory', {ns: 'auditLog'})}</div>
            <div className="empty-description">
              No login history matching the selected period and filter conditions.
            </div>
          </div>
        )}

        {/* 페이지네이션 */}
        {filteredRecords.length > 0 && (
          <div className="pagination-container">
            <div className="pagination-info">
              {startIndex + 1}-{Math.min(endIndex, filteredRecords.length)} / {filteredRecords.length} record(s)
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
                <option value={25}>25/page</option>
                <option value={50}>50/page</option>
                <option value={100}>100/page</option>
                <option value={200}>200/page</option>
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
              <h2>{t('labels.loginRecordDetails', {ns: 'auditLog'})}</h2>
              <button
                className="modal-close-btn"
                onClick={() => setShowDetailsModal(false)}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="detail-section">
                <h3>{t('labels.userInfo', {ns: 'auditLog'})}</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>{t('labels.username', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.username}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.name', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.fullName}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.email', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.email}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.role', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.role}</span>
                  </div>
                </div>
              </div>

              <div className="detail-section">
                <h3>{t('labels.loginInfo', {ns: 'auditLog'})}</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>{t('labels.loginTime', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.loginTime.toLocaleString()}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.logoutTime', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.logoutTime?.toLocaleString() || 'In Progress'}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.sessionDuration', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.duration ? formatDuration(selectedRecord.duration) : '-'}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.loginMethod1', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.loginMethod}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.status1', {ns: 'auditLog'})}</label>
                    <span className={selectedRecord.success ? 'text-success-600' : 'text-error-600'}>
                      {selectedRecord.success ? '성공' : '실패'}
                    </span>
                  </div>
                  {!selectedRecord.success && (
                    <div className="detail-item">
                      <label>{t('labels.failureReason', {ns: 'auditLog'})}</label>
                      <span>{selectedRecord.failureReason}</span>
                    </div>
                  )}
                </div>
              </div>

              <div className="detail-section">
                <h3>기술 정보</h3>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>{t('labels.ipAddress1', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.ipAddress}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.browser', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.browser}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.os', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.os}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.device', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.device}</span>
                  </div>
                  <div className="detail-item">
                    <label>{t('labels.location', {ns: 'auditLog'})}</label>
                    <span>{selectedRecord.location || 'Unknown'}</span>
                  </div>
                  <div className="detail-item">
                    <label>세션 ID:</label>
                    <span>{selectedRecord.sessionId}</span>
                  </div>
                  <div className="detail-item full-width">
                    <label>{t('labels.userAgent', {ns: 'auditLog'})}</label>
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