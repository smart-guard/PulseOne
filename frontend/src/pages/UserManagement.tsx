import React, { useState, useEffect, useCallback } from 'react';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import UserApiService, { User, UserStats } from '../api/services/userApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { UserModal } from '../components/modals/UserModal/UserModal';
import { UserDetailModal } from '../components/modals/UserModal/UserDetailModal';
import StatusBadge from '../components/common/StatusBadge';
import { maskEmail } from '../utils/stringUtils';
import { TenantApiService } from '../api/services/tenantApi';
import { Tenant } from '../types/common';
import '../styles/management.css';

const UserManagement: React.FC = () => {
  const [users, setUsers] = useState<User[]>([]);
  const [stats, setStats] = useState<UserStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(10);
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedRole, setSelectedRole] = useState('all');
  const [selectedTenantId, setSelectedTenantId] = useState('all');
  const [tenants, setTenants] = useState<Tenant[]>([]);
  const [includeDeleted, setIncludeDeleted] = useState(false);

  const [editModalOpen, setEditModalOpen] = useState(false);
  const [detailModalOpen, setDetailModalOpen] = useState(false);
  const [editingUser, setEditingUser] = useState<User | null>(null);
  const [selectedUser, setSelectedUser] = useState<User | null>(null);

  const { confirm } = useConfirmContext();

  const fetchTenants = useCallback(async () => {
    try {
      const res = await TenantApiService.getTenants();
      if (res.success) {
        setTenants(res.data.items || []);
      }
    } catch (error) {
      console.error('Failed to fetch tenants:', error);
    }
  }, []);

  const fetchUsers = useCallback(async () => {
    try {
      setLoading(true);
      const filters = {
        includeDeleted,
        tenant_id: selectedTenantId !== 'all' ? selectedTenantId : undefined
      };

      const [usersRes, statsRes] = await Promise.all([
        UserApiService.getAllUsers(filters),
        UserApiService.getStats()
      ]);

      if (usersRes.success) setUsers(usersRes.data || []);
      if (statsRes.success) setStats(statsRes.data || null);
    } catch (error) {
      console.error('Failed to fetch users:', error);
    } finally {
      setLoading(false);
    }
  }, [includeDeleted, selectedTenantId]);

  useEffect(() => {
    fetchTenants();
  }, [fetchTenants]);

  useEffect(() => {
    fetchUsers();
  }, [fetchUsers]);

  const filteredUsers = users.filter(user => {
    const matchesSearch = !searchTerm ||
      user.username.toLowerCase().includes(searchTerm.toLowerCase()) ||
      user.email.toLowerCase().includes(searchTerm.toLowerCase()) ||
      user.full_name.toLowerCase().includes(searchTerm.toLowerCase());
    const matchesRole = selectedRole === 'all' || user.role === selectedRole;
    const matchesTenant = selectedTenantId === 'all' || user.tenant_id?.toString() === selectedTenantId;
    return matchesSearch && matchesRole && matchesTenant;
  });

  const paginatedUsers = filteredUsers.slice((currentPage - 1) * pageSize, currentPage * pageSize);

  const handleCreate = () => {
    setEditingUser(null);
    setEditModalOpen(true);
  };

  const handleRowClick = (user: User) => {
    setSelectedUser(user);
    setDetailModalOpen(true);
  };

  const handleEdit = (user: User) => {
    setEditingUser(user);
    setDetailModalOpen(false);
    setEditModalOpen(true);
  };

  const handleRestore = async (user: User) => {
    const confirmed = await confirm({
      title: '사용자 복구',
      message: `'${user.full_name}(@${user.username})' 사용자를 복구하시겠습니까?`,
      confirmText: '복구',
      confirmButtonType: 'primary'
    });

    if (confirmed) {
      const res = await UserApiService.restoreUser(user.id);
      if (res.success) {
        await fetchUsers();
        if (detailModalOpen && selectedUser?.id === user.id) {
          setSelectedUser(res.data || null);
        }
      }
    }
  };

  const getRoleLabel = (role: string) => {
    const labels: any = {
      'system_admin': '시스템',
      'company_admin': '관리자',
      'site_admin': '사이트',
      'manager': '매니저',
      'engineer': '엔지니어',
      'operator': '운영자',
      'viewer': '조회자'
    };
    return labels[role] || role;
  };

  return (
    <ManagementLayout>
      <PageHeader
        title="사용자 관리"
        description="시스템 계정 및 권한을 관리합니다. 행을 클릭하여 상세 정보를 조회하거나 삭제된 계정을 복구할 수 있습니다."
        icon="fas fa-users-cog"
        actions={
          <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
            <i className="fas fa-user-plus"></i> 새 사용자 등록
          </button>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label="전체 사용자" value={stats?.total_users || 0} type="primary" />
        <StatCard label="활성 사용자" value={stats?.active_users || 0} type="success" />
        <StatCard label="최근 24시간 활성" value={stats?.active_today || 0} type="neutral" />
      </div>

      <FilterBar
        searchTerm={searchTerm}
        onSearchChange={setSearchTerm}
        filters={[
          {
            label: '고객사',
            value: selectedTenantId,
            options: [
              { label: '전체', value: 'all' },
              ...tenants.map(t => ({ label: t.company_name, value: t.id.toString() }))
            ],
            onChange: setSelectedTenantId
          },
          {
            label: '역할',
            value: selectedRole,
            options: [
              { label: '전체', value: 'all' },
              { label: '관리자', value: 'company_admin' },
              { label: '엔지니어', value: 'engineer' },
              { label: '운영자', value: 'operator' },
              { label: '조회자', value: 'viewer' }
            ],
            onChange: setSelectedRole
          }
        ]}
        onReset={() => {
          setSearchTerm('');
          setSelectedRole('all');
          setSelectedTenantId('all');
          setIncludeDeleted(false);
        }}
        activeFilterCount={
          (searchTerm ? 1 : 0) +
          (selectedRole !== 'all' ? 1 : 0) +
          (selectedTenantId !== 'all' ? 1 : 0) +
          (includeDeleted ? 1 : 0)
        }
        rightActions={
          <div className="filter-checkbox-group">
            <label className="mgmt-checkbox-label">
              <input
                type="checkbox"
                checked={includeDeleted}
                onChange={(e) => setIncludeDeleted(e.target.checked)}
              />
              삭제된 사용자 포함
            </label>
          </div>
        }
      />

      <div className="mgmt-content-area">
        <div className="mgmt-table-container">
          <table className="mgmt-table">
            <thead>
              <tr>
                <th>이름</th>
                <th>아이디</th>
                <th>고객사</th>
                <th>이메일</th>
                <th>역할</th>
                <th>부서</th>
                <th>마지막 로그인</th>
                <th style={{ textAlign: 'center' }}>상태</th>
              </tr>
            </thead>
            <tbody>
              {paginatedUsers.map(user => (
                <tr
                  key={user.id}
                  className={`${!user.is_active ? 'inactive-row' : ''} ${user.is_deleted ? 'deleted-row' : ''} clickable`}
                  onClick={() => handleRowClick(user)}
                >
                  <td>
                    <div className="mgmt-table-id-link">
                      {user.full_name}
                    </div>
                  </td>
                  <td>
                    <span className="username">@{user.username}</span>
                  </td>
                  <td>
                    <span className="tenant-name" style={{ fontSize: '12px', fontWeight: 600, color: 'var(--neutral-600)' }}>
                      {tenants.find(t => t.id === user.tenant_id)?.company_name || '미지정'}
                    </span>
                  </td>
                  <td>
                    <span className="email text-neutral-500">{maskEmail(user.email)}</span>
                  </td>
                  <td>
                    <span className={`mgmt-role-badge role-${user.role}`}>
                      {getRoleLabel(user.role)}
                    </span>
                  </td>
                  <td>{user.department || '-'}</td>
                  <td>{user.last_login ? new Date(user.last_login).toLocaleString() : '기록 없음'}</td>
                  <td style={{ textAlign: 'center' }}>
                    <div style={{ display: 'inline-flex', justifyContent: 'center' }}>
                      <StatusBadge
                        status={user.is_deleted ? '삭제됨' : (user.is_active ? '활성' : '비활성')}
                        type={user.is_deleted ? 'error' : (user.is_active ? 'active' : 'inactive')}
                        showDot={true}
                      />
                    </div>
                  </td>
                </tr>
              ))}
              {paginatedUsers.length === 0 && (
                <tr>
                  <td colSpan={8} className="empty-table">등록된 사용자가 없습니다.</td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      <Pagination
        current={currentPage}
        total={filteredUsers.length}
        pageSize={pageSize}
        onChange={setCurrentPage}
      />

      <UserModal
        isOpen={editModalOpen}
        onClose={() => setEditModalOpen(false)}
        onSave={fetchUsers}
        user={editingUser}
      />

      <UserDetailModal
        isOpen={detailModalOpen}
        onClose={() => setDetailModalOpen(false)}
        user={selectedUser}
        onEdit={handleEdit}
        onRestore={handleRestore}
      />
    </ManagementLayout>
  );
};

export default UserManagement;