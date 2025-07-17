import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/user-management.css';

interface Permission {
  id: string;
  name: string;
  description: string;
  category: string;
  resource: string;
  actions: string[];
  isSystemPermission: boolean;
  createdAt: Date;
}

interface Role {
  id: string;
  name: string;
  description: string;
  permissions: string[];
  userCount: number;
  isSystemRole: boolean;
  createdAt: Date;
}

interface RoleTemplate {
  id: string;
  name: string;
  description: string;
  permissions: string[];
  category: string;
}

const PermissionManagement: React.FC = () => {
  const [permissions, setPermissions] = useState<Permission[]>([]);
  const [roles, setRoles] = useState<Role[]>([]);
  const [roleTemplates, setRoleTemplates] = useState<RoleTemplate[]>([]);
  const [activeTab, setActiveTab] = useState<'permissions' | 'roles' | 'templates'>('roles');
  const [selectedRole, setSelectedRole] = useState<Role | null>(null);
  const [showRoleModal, setShowRoleModal] = useState(false);
  const [showPermissionModal, setShowPermissionModal] = useState(false);
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(20);
  
  // 폼 상태
  const [roleFormData, setRoleFormData] = useState<Partial<Role>>({
    name: '',
    description: '',
    permissions: []
  });

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 권한 목록
    const mockPermissions: Permission[] = [
      { id: 'view_dashboard', name: '대시보드 조회', description: '메인 대시보드 및 실시간 데이터 조회', category: '조회', resource: 'dashboard', actions: ['read'], isSystemPermission: true, createdAt: new Date() },
      { id: 'manage_devices', name: '디바이스 관리', description: '디바이스 설정, 제어, 추가/삭제', category: '관리', resource: 'devices', actions: ['create', 'read', 'update', 'delete'], isSystemPermission: true, createdAt: new Date() },
      { id: 'manage_alarms', name: '알람 관리', description: '알람 규칙 설정 및 이벤트 처리', category: '관리', resource: 'alarms', actions: ['create', 'read', 'update', 'delete'], isSystemPermission: true, createdAt: new Date() },
      { id: 'manage_users', name: '사용자 관리', description: '사용자 계정 및 권한 관리', category: '관리', resource: 'users', actions: ['create', 'read', 'update', 'delete'], isSystemPermission: true, createdAt: new Date() },
      { id: 'view_reports', name: '보고서 조회', description: '각종 보고서 및 분석 자료 조회', category: '조회', resource: 'reports', actions: ['read'], isSystemPermission: true, createdAt: new Date() },
      { id: 'export_data', name: '데이터 내보내기', description: '시스템 데이터 내보내기', category: '데이터', resource: 'data', actions: ['export'], isSystemPermission: true, createdAt: new Date() },
      { id: 'system_settings', name: '시스템 설정', description: '시스템 전역 설정 관리', category: '시스템', resource: 'settings', actions: ['read', 'update'], isSystemPermission: true, createdAt: new Date() },
      { id: 'backup_restore', name: '백업/복원', description: '시스템 백업 및 복원', category: '시스템', resource: 'backup', actions: ['create', 'restore'], isSystemPermission: true, createdAt: new Date() }
    ];

    // 역할 목록
    const mockRoles: Role[] = [
      { id: 'admin', name: '시스템 관리자', description: '모든 시스템 기능에 대한 완전한 권한', permissions: mockPermissions.map(p => p.id), userCount: 3, isSystemRole: true, createdAt: new Date() },
      { id: 'manager', name: '관리자', description: '시스템 설정을 제외한 대부분의 관리 권한', permissions: ['view_dashboard', 'manage_devices', 'manage_alarms', 'view_reports', 'export_data'], userCount: 8, isSystemRole: true, createdAt: new Date() },
      { id: 'engineer', name: '엔지니어', description: '디바이스 및 알람 관리 권한', permissions: ['view_dashboard', 'manage_devices', 'manage_alarms', 'view_reports'], userCount: 15, isSystemRole: true, createdAt: new Date() },
      { id: 'operator', name: '운영자', description: '디바이스 모니터링 및 기본 제어', permissions: ['view_dashboard', 'manage_devices'], userCount: 22, isSystemRole: true, createdAt: new Date() },
      { id: 'viewer', name: '조회자', description: '데이터 조회 및 보고서 확인만 가능', permissions: ['view_dashboard', 'view_reports'], userCount: 12, isSystemRole: true, createdAt: new Date() }
    ];

    // 역할 템플릿
    const mockRoleTemplates: RoleTemplate[] = [
      { id: 'safety_manager', name: '안전 관리자', description: '안전 관련 알람 및 보고서 관리', permissions: ['view_dashboard', 'manage_alarms', 'view_reports'], category: '안전' },
      { id: 'quality_inspector', name: '품질 검사원', description: '품질 관련 데이터 조회 및 보고서 작성', permissions: ['view_dashboard', 'view_reports', 'export_data'], category: '품질' },
      { id: 'maintenance_tech', name: '정비 기술자', description: '설비 상태 모니터링 및 유지보수', permissions: ['view_dashboard', 'manage_devices', 'view_reports'], category: '정비' },
      { id: 'production_supervisor', name: '생산 감독자', description: '생산 라인 모니터링 및 제어', permissions: ['view_dashboard', 'manage_devices', 'manage_alarms'], category: '생산' }
    ];

    setPermissions(mockPermissions);
    setRoles(mockRoles);
    setRoleTemplates(mockRoleTemplates);
  };

  // 역할 저장
  const handleSaveRole = () => {
    if (!roleFormData.name) {
      alert('역할명을 입력해주세요.');
      return;
    }

    const newRole: Role = {
      id: selectedRole?.id || `role_${Date.now()}`,
      name: roleFormData.name!,
      description: roleFormData.description || '',
      permissions: roleFormData.permissions!,
      userCount: selectedRole?.userCount || 0,
      isSystemRole: false,
      createdAt: selectedRole?.createdAt || new Date()
    };

    if (selectedRole) {
      setRoles(prev => prev.map(role => role.id === selectedRole.id ? newRole : role));
    } else {
      setRoles(prev => [newRole, ...prev]);
    }

    handleCloseModal();
  };

  // 역할 삭제
  const handleDeleteRole = (roleId: string) => {
    const role = roles.find(r => r.id === roleId);
    if (role?.isSystemRole) {
      alert('시스템 기본 역할은 삭제할 수 없습니다.');
      return;
    }
    if (role?.userCount && role.userCount > 0) {
      alert('사용자가 할당된 역할은 삭제할 수 없습니다.');
      return;
    }
    if (confirm('정말로 이 역할을 삭제하시겠습니까?')) {
      setRoles(prev => prev.filter(role => role.id !== roleId));
    }
  };

  // 템플릿에서 역할 생성
  const handleCreateFromTemplate = (template: RoleTemplate) => {
    setRoleFormData({
      name: template.name,
      description: template.description,
      permissions: template.permissions
    });
    setShowRoleModal(true);
  };

  // 모달 닫기
  const handleCloseModal = () => {
    setShowRoleModal(false);
    setShowPermissionModal(false);
    setSelectedRole(null);
    setRoleFormData({
      name: '',
      description: '',
      permissions: []
    });
  };

  // 편집 모달 열기
  const handleEditRole = (role: Role) => {
    setSelectedRole(role);
    setRoleFormData(role);
    setShowRoleModal(true);
  };

  // 권한 이름 가져오기
  const getPermissionName = (permissionId: string) => {
    const permission = permissions.find(p => p.id === permissionId);
    return permission?.name || permissionId;
  };

  return (
    <div className="user-management-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">권한 관리</h1>
        <div className="page-actions">
          <button
            className="btn btn-primary"
            onClick={() => setShowRoleModal(true)}
          >
            <i className="fas fa-plus"></i>
            새 역할
          </button>
          <button className="btn btn-outline">
            <i className="fas fa-download"></i>
            내보내기
          </button>
        </div>
      </div>

      {/* 탭 네비게이션 */}
      <div className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'roles' ? 'active' : ''}`}
          onClick={() => setActiveTab('roles')}
        >
          <i className="fas fa-user-tag"></i>
          역할 관리
          <span className="tab-badge">{roles.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'permissions' ? 'active' : ''}`}
          onClick={() => setActiveTab('permissions')}
        >
          <i className="fas fa-key"></i>
          권한 목록
          <span className="tab-badge">{permissions.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'templates' ? 'active' : ''}`}
          onClick={() => setActiveTab('templates')}
        >
          <i className="fas fa-clipboard-list"></i>
          역할 템플릿
          <span className="tab-badge">{roleTemplates.length}</span>
        </button>
      </div>

      {/* 역할 관리 탭 */}
      {activeTab === 'roles' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">역할 목록</h2>
            <div className="users-actions">
              <span className="text-sm text-neutral-600">
                총 {roles.length}개 역할
              </span>
            </div>
          </div>

          <div className="users-table">
            <div className="table-header">
              <div className="header-cell">역할명</div>
              <div className="header-cell">설명</div>
              <div className="header-cell">권한 수</div>
              <div className="header-cell">사용자 수</div>
              <div className="header-cell">타입</div>
              <div className="header-cell">생성일</div>
              <div className="header-cell">액션</div>
            </div>

            {roles.map(role => (
              <div key={role.id} className="table-row">
                <div className="table-cell" data-label="역할명">
                  <div className="user-name">{role.name}</div>
                  <div className="user-id">#{role.id}</div>
                </div>

                <div className="table-cell" data-label="설명">
                  <div className="user-email">{role.description}</div>
                </div>

                <div className="table-cell" data-label="권한 수">
                  <span className="text-primary-600 font-semibold">
                    {role.permissions.length}개
                  </span>
                </div>

                <div className="table-cell" data-label="사용자 수">
                  <span className="text-neutral-700">
                    {role.userCount}명
                  </span>
                </div>

                <div className="table-cell" data-label="타입">
                  <span className={`role-badge ${role.isSystemRole ? 'role-admin' : 'role-engineer'}`}>
                    {role.isSystemRole ? '시스템' : '사용자정의'}
                  </span>
                </div>

                <div className="table-cell" data-label="생성일">
                  <div className="last-login-time">
                    {role.createdAt.toLocaleDateString()}
                  </div>
                </div>

                <div className="table-cell action-cell" data-label="액션">
                  <div className="action-buttons">
                    <button
                      className="btn btn-sm btn-outline"
                      onClick={() => handleEditRole(role)}
                      title="편집"
                    >
                      <i className="fas fa-edit"></i>
                    </button>
                    <button
                      className="btn btn-sm btn-outline"
                      onClick={() => handleCreateFromTemplate({ 
                        id: role.id, 
                        name: `${role.name} (복사본)`, 
                        description: role.description, 
                        permissions: role.permissions, 
                        category: '복사본' 
                      })}
                      title="복제"
                    >
                      <i className="fas fa-copy"></i>
                    </button>
                    {!role.isSystemRole && (
                      <button
                        className="btn btn-sm btn-error"
                        onClick={() => handleDeleteRole(role.id)}
                        title="삭제"
                      >
                        <i className="fas fa-trash"></i>
                      </button>
                    )}
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 권한 목록 탭 */}
      {activeTab === 'permissions' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">시스템 권한</h2>
            <div className="users-actions">
              <span className="text-sm text-neutral-600">
                총 {permissions.length}개 권한
              </span>
            </div>
          </div>

          <div className="users-table">
            <div className="table-header">
              <div className="header-cell">권한명</div>
              <div className="header-cell">설명</div>
              <div className="header-cell">카테고리</div>
              <div className="header-cell">리소스</div>
              <div className="header-cell">액션</div>
              <div className="header-cell">사용 역할</div>
            </div>

            {permissions.map(permission => (
              <div key={permission.id} className="table-row">
                <div className="table-cell" data-label="권한명">
                  <div className="user-name">{permission.name}</div>
                  <div className="user-id">#{permission.id}</div>
                </div>

                <div className="table-cell" data-label="설명">
                  <div className="user-email">{permission.description}</div>
                </div>

                <div className="table-cell" data-label="카테고리">
                  <span className="role-badge role-engineer">
                    {permission.category}
                  </span>
                </div>

                <div className="table-cell" data-label="리소스">
                  <div className="text-neutral-700">{permission.resource}</div>
                </div>

                <div className="table-cell" data-label="액션">
                  <div className="permissions-list">
                    {permission.actions.map(action => (
                      <span key={action} className="permission-tag">
                        {action}
                      </span>
                    ))}
                  </div>
                </div>

                <div className="table-cell" data-label="사용 역할">
                  <div className="text-primary-600">
                    {roles.filter(role => role.permissions.includes(permission.id)).length}개 역할
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 역할 템플릿 탭 */}
      {activeTab === 'templates' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">역할 템플릿</h2>
            <div className="users-actions">
              <span className="text-sm text-neutral-600">
                미리 정의된 역할 템플릿을 사용하여 빠르게 역할을 생성할 수 있습니다.
              </span>
            </div>
          </div>

          <div className="user-stats-panel">
            {roleTemplates.map(template => (
              <div key={template.id} className="stat-card">
                <div className="stat-icon">
                  <i className="fas fa-clipboard-list"></i>
                </div>
                <div className="stat-value">{template.name}</div>
                <div className="stat-label">{template.category}</div>
                <div className="text-xs text-neutral-600 mt-2">
                  {template.description}
                </div>
                <div className="text-xs text-primary-600 mt-1">
                  {template.permissions.length}개 권한
                </div>
                <button
                  className="btn btn-sm btn-primary mt-3"
                  onClick={() => handleCreateFromTemplate(template)}
                >
                  <i className="fas fa-plus"></i>
                  역할 생성
                </button>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 역할 생성/편집 모달 */}
      {showRoleModal && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>{selectedRole ? '역할 편집' : '새 역할'}</h2>
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
                    <label>역할명 <span className="required">*</span></label>
                    <input
                      type="text"
                      value={roleFormData.name || ''}
                      onChange={(e) => setRoleFormData(prev => ({ ...prev, name: e.target.value }))}
                      className="form-input"
                      placeholder="역할명을 입력하세요"
                    />
                  </div>

                  <div className="form-group full-width">
                    <label>설명</label>
                    <textarea
                      value={roleFormData.description || ''}
                      onChange={(e) => setRoleFormData(prev => ({ ...prev, description: e.target.value }))}
                      className="form-input"
                      rows={3}
                      placeholder="역할에 대한 설명을 입력하세요"
                    />
                  </div>
                </div>
              </div>

              {/* 권한 설정 */}
              <div className="form-section">
                <h3>권한 설정</h3>
                <div className="permissions-grid">
                  {permissions.map(permission => (
                    <div key={permission.id} className="permission-item">
                      <input
                        type="checkbox"
                        id={`role-perm-${permission.id}`}
                        checked={roleFormData.permissions?.includes(permission.id) || false}
                        onChange={(e) => {
                          const perms = roleFormData.permissions || [];
                          if (e.target.checked) {
                            setRoleFormData(prev => ({
                              ...prev,
                              permissions: [...perms, permission.id]
                            }));
                          } else {
                            setRoleFormData(prev => ({
                              ...prev,
                              permissions: perms.filter(p => p !== permission.id)
                            }));
                          }
                        }}
                      />
                      <div>
                        <label htmlFor={`role-perm-${permission.id}`}>{permission.name}</label>
                        <div className="permission-description">{permission.description}</div>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-primary"
                onClick={handleSaveRole}
              >
                <i className="fas fa-save"></i>
                {selectedRole ? '저장' : '생성'}
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

export default PermissionManagement;