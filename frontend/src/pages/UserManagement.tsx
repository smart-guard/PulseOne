import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/user-management.css';

interface User {
  id: string;
  username: string;
  email: string;
  fullName: string;
  role: 'admin' | 'manager' | 'engineer' | 'operator' | 'viewer';
  department?: string;
  factoryAccess: string[];
  permissions: string[];
  isActive: boolean;
  isOnline: boolean;
  lastLogin?: Date;
  loginCount: number;
  passwordResetRequired: boolean;
  createdAt: Date;
  updatedAt: Date;
  createdBy: string;
}

interface UserStats {
  totalUsers: number;
  activeUsers: number;
  inactiveUsers: number;
  adminUsers: number;
  onlineUsers: number;
  recentLogins: number;
}

const UserManagement: React.FC = () => {
  const [users, setUsers] = useState<User[]>([]);
  const [stats, setStats] = useState<UserStats | null>(null);
  const [selectedUser, setSelectedUser] = useState<User | null>(null);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(20);
  
  // 필터 상태
  const [filterRole, setFilterRole] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [filterDepartment, setFilterDepartment] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  
  // 폼 상태
  const [formData, setFormData] = useState<Partial<User>>({
    username: '',
    email: '',
    fullName: '',
    role: 'viewer',
    department: '',
    factoryAccess: [],
    permissions: [],
    isActive: true,
    passwordResetRequired: true
  });

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 목업 사용자 데이터 생성
    const mockUsers: User[] = [];
    const now = new Date();
    
    const roles: User['role'][] = ['admin', 'manager', 'engineer', 'operator', 'viewer'];
    const departments = ['IT', 'Production', 'Quality', 'Maintenance', 'Safety', 'Engineering'];
    const factories = ['Factory A', 'Factory B', 'Factory C'];
    const permissions = [
      'view_dashboard', 'manage_devices', 'manage_alarms', 'manage_users', 
      'view_reports', 'export_data', 'system_settings', 'backup_restore'
    ];

    for (let i = 0; i < 50; i++) {
      const role = roles[Math.floor(Math.random() * roles.length)];
      const department = departments[Math.floor(Math.random() * departments.length)];
      const isActive = Math.random() > 0.2;
      const isOnline = isActive && Math.random() > 0.7;
      
      const user: User = {
        id: `user_${i + 1000}`,
        username: `user${i + 1}`,
        email: `user${i + 1}@company.com`,
        fullName: `사용자 ${i + 1}`,
        role,
        department,
        factoryAccess: factories.filter(() => Math.random() > 0.5),
        permissions: permissions.filter(() => Math.random() > 0.6),
        isActive,
        isOnline,
        lastLogin: isActive && Math.random() > 0.3 ? 
          new Date(now.getTime() - Math.random() * 30 * 24 * 60 * 60 * 1000) : undefined,
        loginCount: Math.floor(Math.random() * 500) + 10,
        passwordResetRequired: Math.random() > 0.8,
        createdAt: new Date(now.getTime() - Math.random() * 365 * 24 * 60 * 60 * 1000),
        updatedAt: new Date(now.getTime() - Math.random() * 30 * 24 * 60 * 60 * 1000),
        createdBy: 'admin'
      };

      // 역할별 권한 자동 설정
      switch (role) {
        case 'admin':
          user.permissions = [...permissions];
          break;
        case 'manager':
          user.permissions = permissions.filter(p => !p.includes('system_') && !p.includes('backup_'));
          break;
        case 'engineer':
          user.permissions = ['view_dashboard', 'manage_devices', 'manage_alarms', 'view_reports'];
          break;
        case 'operator':
          user.permissions = ['view_dashboard', 'manage_devices'];
          break;
        case 'viewer':
          user.permissions = ['view_dashboard', 'view_reports'];
          break;
      }

      mockUsers.push(user);
    }

    // 통계 계산
    const mockStats: UserStats = {
      totalUsers: mockUsers.length,
      activeUsers: mockUsers.filter(u => u.isActive).length,
      inactiveUsers: mockUsers.filter(u => !u.isActive).length,
      adminUsers: mockUsers.filter(u => u.role === 'admin').length,
      onlineUsers: mockUsers.filter(u => u.isOnline).length,
      recentLogins: mockUsers.filter(u => 
        u.lastLogin && u.lastLogin > new Date(Date.now() - 24 * 60 * 60 * 1000)
      ).length
    };

    setUsers(mockUsers);
    setStats(mockStats);
  };

  // 필터링된 사용자 목록
  const filteredUsers = users.filter(user => {
    const matchesRole = filterRole === 'all' || user.role === filterRole;
    const matchesStatus = filterStatus === 'all' || 
      (filterStatus === 'active' && user.isActive) ||
      (filterStatus === 'inactive' && !user.isActive) ||
      (filterStatus === 'online' && user.isOnline);
    const matchesDepartment = filterDepartment === 'all' || user.department === filterDepartment;
    const matchesSearch = searchTerm === '' || 
      user.username.toLowerCase().includes(searchTerm.toLowerCase()) ||
      user.email.toLowerCase().includes(searchTerm.toLowerCase()) ||
      user.fullName.toLowerCase().includes(searchTerm.toLowerCase());
    
    return matchesRole && matchesStatus && matchesDepartment && matchesSearch;
  });

  // 페이지네이션
  const startIndex = (currentPage - 1) * pageSize;
  const endIndex = startIndex + pageSize;
  const currentUsers = filteredUsers.slice(startIndex, endIndex);

  // 사용자 활성화/비활성화
  const handleToggleUser = (userId: string) => {
    setUsers(prev => prev.map(user =>
      user.id === userId ? { ...user, isActive: !user.isActive } : user
    ));
  };

  // 사용자 삭제
  const handleDeleteUser = (userId: string) => {
    if (confirm('정말로 이 사용자를 삭제하시겠습니까? 이 작업은 되돌릴 수 없습니다.')) {
      setUsers(prev => prev.filter(user => user.id !== userId));
    }
  };

  // 비밀번호 재설정
  const handleResetPassword = (userId: string) => {
    if (confirm('이 사용자의 비밀번호를 재설정하시겠습니까?')) {
      setUsers(prev => prev.map(user =>
        user.id === userId ? { ...user, passwordResetRequired: true } : user
      ));
      alert('비밀번호 재설정 이메일이 발송되었습니다.');
    }
  };

  // 폼 제출
  const handleSubmit = () => {
    if (!formData.username || !formData.email || !formData.fullName) {
      alert('필수 항목을 입력해주세요.');
      return;
    }

    // 이메일 중복 검사
    const emailExists = users.some(user => 
      user.email === formData.email && user.id !== selectedUser?.id
    );
    if (emailExists) {
      alert('이미 사용 중인 이메일입니다.');
      return;
    }

    // 사용자명 중복 검사
    const usernameExists = users.some(user => 
      user.username === formData.username && user.id !== selectedUser?.id
    );
    if (usernameExists) {
      alert('이미 사용 중인 사용자명입니다.');
      return;
    }

    const newUser: User = {
      id: selectedUser?.id || `user_${Date.now()}`,
      username: formData.username!,
      email: formData.email!,
      fullName: formData.fullName!,
      role: formData.role!,
      department: formData.department,
      factoryAccess: formData.factoryAccess!,
      permissions: formData.permissions!,
      isActive: formData.isActive!,
      isOnline: selectedUser?.isOnline || false,
      lastLogin: selectedUser?.lastLogin,
      loginCount: selectedUser?.loginCount || 0,
      passwordResetRequired: formData.passwordResetRequired!,
      createdAt: selectedUser?.createdAt || new Date(),
      updatedAt: new Date(),
      createdBy: selectedUser?.createdBy || 'current_admin'
    };

    if (selectedUser) {
      // 편집
      setUsers(prev => prev.map(user => 
        user.id === selectedUser.id ? newUser : user
      ));
    } else {
      // 생성
      setUsers(prev => [newUser, ...prev]);
    }

    handleCloseModal();
  };

  // 모달 닫기
  const handleCloseModal = () => {
    setShowCreateModal(false);
    setShowEditModal(false);
    setSelectedUser(null);
    setFormData({
      username: '',
      email: '',
      fullName: '',
      role: 'viewer',
      department: '',
      factoryAccess: [],
      permissions: [],
      isActive: true,
      passwordResetRequired: true
    });
  };

  // 편집 모달 열기
  const handleEditUser = (user: User) => {
    setSelectedUser(user);
    setFormData(user);
    setShowEditModal(true);
  };

  // 아바타 색상 가져오기
  const getAvatarInitials = (fullName: string) => {
    return fullName.split(' ').map(n => n[0]).join('').slice(0, 2);
  };

  // 권한 이름 변환
  const getPermissionLabel = (permission: string) => {
    const labels: { [key: string]: string } = {
      'view_dashboard': '대시보드 조회',
      'manage_devices': '디바이스 관리',
      'manage_alarms': '알람 관리',
      'manage_users': '사용자 관리',
      'view_reports': '보고서 조회',
      'export_data': '데이터 내보내기',
      'system_settings': '시스템 설정',
      'backup_restore': '백업/복원'
    };
    return labels[permission] || permission;
  };

  // 고유 값들 추출
  const uniqueDepartments = [...new Set(users.map(u => u.department).filter(Boolean))];

  return (
    <div className="user-management-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">사용자 관리</h1>
        <div className="page-actions">
          <button
            className="btn btn-primary"
            onClick={() => setShowCreateModal(true)}
          >
            <i className="fas fa-user-plus"></i>
            새 사용자
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

      {/* 사용자 통계 */}
      {stats && (
        <div className="user-stats-panel">
          <div className="stat-card users-total">
            <div className="stat-icon">
              <i className="fas fa-users"></i>
            </div>
            <div className="stat-value">{stats.totalUsers}</div>
            <div className="stat-label">전체 사용자</div>
          </div>
          <div className="stat-card users-active">
            <div className="stat-icon">
              <i className="fas fa-user-check"></i>
            </div>
            <div className="stat-value">{stats.activeUsers}</div>
            <div className="stat-label">활성 사용자</div>
          </div>
          <div className="stat-card users-inactive">
            <div className="stat-icon">
              <i className="fas fa-user-times"></i>
            </div>
            <div className="stat-value">{stats.inactiveUsers}</div>
            <div className="stat-label">비활성 사용자</div>
          </div>
          <div className="stat-card users-admin">
            <div className="stat-icon">
              <i className="fas fa-user-shield"></i>
            </div>
            <div className="stat-value">{stats.adminUsers}</div>
            <div className="stat-label">관리자</div>
          </div>
        </div>
      )}

      {/* 사용자 관리 */}
      <div className="users-management">
        <div className="users-header">
          <h2 className="users-title">사용자 목록</h2>
          <div className="users-actions">
            <span className="text-sm text-neutral-600">
              총 {filteredUsers.length}명
            </span>
          </div>
        </div>

        {/* 검색 및 필터 */}
        <div className="users-filters">
          <div className="filter-group flex-1">
            <label>검색</label>
            <div className="search-container">
              <input
                type="text"
                placeholder="사용자명, 이메일, 이름 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="search-input"
              />
              <i className="fas fa-search search-icon"></i>
            </div>
          </div>

          <div className="filter-group">
            <label>역할</label>
            <select
              value={filterRole}
              onChange={(e) => setFilterRole(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체</option>
              <option value="admin">관리자</option>
              <option value="manager">매니저</option>
              <option value="engineer">엔지니어</option>
              <option value="operator">운영자</option>
              <option value="viewer">조회자</option>
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
              <option value="inactive">비활성</option>
              <option value="online">온라인</option>
            </select>
          </div>

          <div className="filter-group">
            <label>부서</label>
            <select
              value={filterDepartment}
              onChange={(e) => setFilterDepartment(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체</option>
              {uniqueDepartments.map(dept => (
                <option key={dept} value={dept}>{dept}</option>
              ))}
            </select>
          </div>
        </div>

        {/* 사용자 목록 테이블 */}
        <div className="users-table">
          <div className="table-header">
            <div className="header-cell"></div>
            <div className="header-cell">사용자 정보</div>
            <div className="header-cell">역할</div>
            <div className="header-cell">상태</div>
            <div className="header-cell">부서</div>
            <div className="header-cell">마지막 로그인</div>
            <div className="header-cell">권한</div>
            <div className="header-cell">액션</div>
          </div>

          {currentUsers.map(user => (
            <div key={user.id} className={`table-row ${!user.isActive ? 'inactive' : ''}`}>
              <div className="table-cell user-avatar-cell">
                <div className={`user-avatar ${user.role}`}>
                  {getAvatarInitials(user.fullName)}
                </div>
              </div>

              <div className="table-cell user-info-cell" data-label="사용자 정보">
                <div className="user-name">{user.fullName}</div>
                <div className="user-email">{user.email}</div>
                <div className="user-id">@{user.username}</div>
              </div>

              <div className="table-cell role-cell" data-label="역할">
                <span className={`role-badge role-${user.role}`}>
                  {user.role === 'admin' ? '관리자' :
                   user.role === 'manager' ? '매니저' :
                   user.role === 'engineer' ? '엔지니어' :
                   user.role === 'operator' ? '운영자' : '조회자'}
                </span>
              </div>

              <div className="table-cell status-cell" data-label="상태">
                <div className="status-indicator">
                  <span className={`status-dot ${user.isOnline ? 'online' : user.isActive ? 'active' : 'inactive'}`}></span>
                  <span className={`status-text ${user.isOnline ? 'online' : user.isActive ? 'active' : 'inactive'}`}>
                    {user.isOnline ? '온라인' : user.isActive ? '활성' : '비활성'}
                  </span>
                </div>
              </div>

              <div className="table-cell department-cell" data-label="부서">
                <div className="department-name">{user.department || '-'}</div>
                {user.factoryAccess.length > 0 && (
                  <div className="factory-access">
                    접근 가능: {user.factoryAccess.join(', ')}
                  </div>
                )}
              </div>

              <div className="table-cell last-login-cell" data-label="마지막 로그인">
                {user.lastLogin ? (
                  <>
                    <div className="last-login-time">
                      {user.lastLogin.toLocaleString()}
                    </div>
                    <div className="login-count">
                      총 {user.loginCount}회 로그인
                    </div>
                  </>
                ) : (
                  <div className="last-login-time">로그인 없음</div>
                )}
              </div>

              <div className="table-cell permissions-cell" data-label="권한">
                <div className="permissions-list">
                  {user.permissions.slice(0, 2).map(permission => (
                    <span key={permission} className="permission-tag">
                      {getPermissionLabel(permission)}
                    </span>
                  ))}
                  {user.permissions.length > 2 && (
                    <span className="permissions-more">
                      +{user.permissions.length - 2}개
                    </span>
                  )}
                </div>
              </div>

              <div className="table-cell action-cell" data-label="액션">
                <div className="action-buttons">
                  <button
                    className="btn btn-sm btn-outline"
                    onClick={() => handleEditUser(user)}
                    title="편집"
                  >
                    <i className="fas fa-edit"></i>
                  </button>
                  <button
                    className="btn btn-sm btn-warning"
                    onClick={() => handleResetPassword(user.id)}
                    title="비밀번호 재설정"
                  >
                    <i className="fas fa-key"></i>
                  </button>
                  <button
                    className={`btn btn-sm ${user.isActive ? 'btn-warning' : 'btn-success'}`}
                    onClick={() => handleToggleUser(user.id)}
                    title={user.isActive ? '비활성화' : '활성화'}
                  >
                    <i className={`fas ${user.isActive ? 'fa-user-slash' : 'fa-user-check'}`}></i>
                  </button>
                  <button
                    className="btn btn-sm btn-error"
                    onClick={() => handleDeleteUser(user.id)}
                    title="삭제"
                  >
                    <i className="fas fa-trash"></i>
                  </button>
                </div>
              </div>
            </div>
          ))}
        </div>

        {currentUsers.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-users empty-icon"></i>
            <div className="empty-title">사용자가 없습니다</div>
            <div className="empty-description">
              선택한 필터 조건에 해당하는 사용자가 없습니다.
            </div>
          </div>
        )}

        {/* 페이지네이션 */}
        {filteredUsers.length > 0 && (
          <div className="pagination-container">
            <div className="pagination-info">
              {startIndex + 1}-{Math.min(endIndex, filteredUsers.length)} / {filteredUsers.length} 항목
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
                <option value={20}>20개씩</option>
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
                {currentPage} / {Math.ceil(filteredUsers.length / pageSize)}
              </span>
              
              <button
                className="btn btn-sm"
                disabled={currentPage === Math.ceil(filteredUsers.length / pageSize)}
                onClick={() => setCurrentPage(prev => prev + 1)}
              >
                <i className="fas fa-angle-right"></i>
              </button>
              <button
                className="btn btn-sm"
                disabled={currentPage === Math.ceil(filteredUsers.length / pageSize)}
                onClick={() => setCurrentPage(Math.ceil(filteredUsers.length / pageSize))}
              >
                <i className="fas fa-angle-double-right"></i>
              </button>
            </div>
          </div>
        )}
      </div>

      {/* 사용자 생성/편집 모달 */}
      {(showCreateModal || showEditModal) && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>{selectedUser ? '사용자 편집' : '새 사용자'}</h2>
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
                <div className="form-grid two-columns">
                  <div className="form-group">
                    <label>사용자명 <span className="required">*</span></label>
                    <input
                      type="text"
                      value={formData.username || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, username: e.target.value }))}
                      className="form-input"
                      placeholder="사용자명을 입력하세요"
                    />
                  </div>

                  <div className="form-group">
                    <label>이메일 <span className="required">*</span></label>
                    <input
                      type="email"
                      value={formData.email || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, email: e.target.value }))}
                      className="form-input"
                      placeholder="이메일을 입력하세요"
                    />
                  </div>

                  <div className="form-group full-width">
                    <label>이름 <span className="required">*</span></label>
                    <input
                      type="text"
                      value={formData.fullName || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, fullName: e.target.value }))}
                      className="form-input"
                      placeholder="이름을 입력하세요"
                    />
                  </div>
                </div>
              </div>

              {/* 역할 및 부서 */}
              <div className="form-section">
                <h3>역할 및 부서</h3>
                <div className="form-grid two-columns">
                  <div className="form-group">
                    <label>역할</label>
                    <select
                      value={formData.role || 'viewer'}
                      onChange={(e) => setFormData(prev => ({ ...prev, role: e.target.value as any }))}
                      className="form-select"
                    >
                      <option value="viewer">조회자</option>
                      <option value="operator">운영자</option>
                      <option value="engineer">엔지니어</option>
                      <option value="manager">매니저</option>
                      <option value="admin">관리자</option>
                    </select>
                  </div>

                  <div className="form-group">
                    <label>부서</label>
                    <select
                      value={formData.department || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, department: e.target.value }))}
                      className="form-select"
                    >
                      <option value="">부서 선택</option>
                      <option value="IT">IT</option>
                      <option value="Production">Production</option>
                      <option value="Quality">Quality</option>
                      <option value="Maintenance">Maintenance</option>
                      <option value="Safety">Safety</option>
                      <option value="Engineering">Engineering</option>
                    </select>
                  </div>
                </div>
              </div>

              {/* 공장 접근 권한 */}
              <div className="form-section">
                <h3>공장 접근 권한</h3>
                <div className="factory-access-grid">
                  {['Factory A', 'Factory B', 'Factory C'].map(factory => (
                    <div key={factory} className="factory-item">
                      <input
                        type="checkbox"
                        id={`factory-${factory}`}
                        checked={formData.factoryAccess?.includes(factory) || false}
                        onChange={(e) => {
                          const factories = formData.factoryAccess || [];
                          if (e.target.checked) {
                            setFormData(prev => ({
                              ...prev,
                              factoryAccess: [...factories, factory]
                            }));
                          } else {
                            setFormData(prev => ({
                              ...prev,
                              factoryAccess: factories.filter(f => f !== factory)
                            }));
                          }
                        }}
                      />
                      <label htmlFor={`factory-${factory}`}>{factory}</label>
                    </div>
                  ))}
                </div>
              </div>

              {/* 시스템 권한 */}
              <div className="form-section">
                <h3>시스템 권한</h3>
                <div className="permissions-grid">
                  {[
                    { id: 'view_dashboard', label: '대시보드 조회', description: '메인 대시보드 및 실시간 데이터 조회' },
                    { id: 'manage_devices', label: '디바이스 관리', description: '디바이스 설정 및 제어' },
                    { id: 'manage_alarms', label: '알람 관리', description: '알람 규칙 설정 및 이벤트 처리' },
                    { id: 'manage_users', label: '사용자 관리', description: '사용자 계정 및 권한 관리' },
                    { id: 'view_reports', label: '보고서 조회', description: '각종 보고서 및 분석 자료 조회' },
                    { id: 'export_data', label: '데이터 내보내기', description: '시스템 데이터 내보내기' },
                    { id: 'system_settings', label: '시스템 설정', description: '시스템 전역 설정 관리' },
                    { id: 'backup_restore', label: '백업/복원', description: '시스템 백업 및 복원' }
                  ].map(permission => (
                    <div key={permission.id} className="permission-item">
                      <input
                        type="checkbox"
                        id={`perm-${permission.id}`}
                        checked={formData.permissions?.includes(permission.id) || false}
                        onChange={(e) => {
                          const permissions = formData.permissions || [];
                          if (e.target.checked) {
                            setFormData(prev => ({
                              ...prev,
                              permissions: [...permissions, permission.id]
                            }));
                          } else {
                            setFormData(prev => ({
                              ...prev,
                              permissions: permissions.filter(p => p !== permission.id)
                            }));
                          }
                        }}
                      />
                      <div>
                        <label htmlFor={`perm-${permission.id}`}>{permission.label}</label>
                        <div className="permission-description">{permission.description}</div>
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* 계정 설정 */}
              <div className="form-section">
                <h3>계정 설정</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <div className="permission-item">
                      <input
                        type="checkbox"
                        id="isActive"
                        checked={formData.isActive || false}
                        onChange={(e) => setFormData(prev => ({ ...prev, isActive: e.target.checked }))}
                      />
                      <label htmlFor="isActive">계정 활성화</label>
                    </div>
                  </div>

                  <div className="form-group">
                    <div className="permission-item">
                      <input
                        type="checkbox"
                        id="passwordReset"
                        checked={formData.passwordResetRequired || false}
                        onChange={(e) => setFormData(prev => ({ ...prev, passwordResetRequired: e.target.checked }))}
                      />
                      <label htmlFor="passwordReset">다음 로그인 시 비밀번호 변경 요구</label>
                    </div>
                  </div>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-primary"
                onClick={handleSubmit}
              >
                <i className="fas fa-save"></i>
                {selectedUser ? '저장' : '생성'}
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

export default UserManagement;