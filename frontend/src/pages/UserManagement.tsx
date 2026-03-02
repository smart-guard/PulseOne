import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
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
  const { t } = useTranslation('userManagement');
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
      title: t('restoreTitle'),
      message: t('restoreMessage', { name: user.full_name, username: user.username }),
      confirmText: t('restoreBtn'),
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
      'system_admin': t('roleSystem'),
      'company_admin': t('roleCompanyAdmin'),
      'site_admin': t('roleSiteAdmin'),
      'manager': t('roleManager'),
      'engineer': t('roleEngineer'),
      'operator': t('roleOperator'),
      'viewer': t('roleViewer')
    };
    return labels[role] || role;
  };

  return (
    <ManagementLayout>
      <PageHeader
        title={t('pageTitle')}
        description={t('pageDesc')}
        icon="fas fa-users-cog"
        actions={
          <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
            <i className="fas fa-user-plus"></i> {t('newUser')}
          </button>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label={t('totalUsers')} value={stats?.total_users || 0} type="primary" />
        <StatCard label={t('activeUsers')} value={stats?.active_users || 0} type="success" />
        <StatCard label={t('activeToday')} value={stats?.active_today || 0} type="neutral" />
      </div>

      <FilterBar
        searchTerm={searchTerm}
        onSearchChange={setSearchTerm}
        filters={[
          {
            label: t('filterCompany'),
            value: selectedTenantId,
            options: [
              { label: t('filterAll'), value: 'all' },
              ...tenants.map(tn => ({ label: tn.company_name, value: tn.id.toString() }))
            ],
            onChange: setSelectedTenantId
          },
          {
            label: t('filterRole'),
            value: selectedRole,
            options: [
              { label: t('filterAll'), value: 'all' },
              { label: t('filterAdmin'), value: 'company_admin' },
              { label: t('filterEngineer'), value: 'engineer' },
              { label: t('filterOperator'), value: 'operator' },
              { label: t('filterViewer'), value: 'viewer' }
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
              {t('includeDeleted')}
            </label>
          </div>
        }
      />

      <div className="mgmt-content-area">
        <div className="mgmt-table-container">
          <table className="mgmt-table">
            <thead>
              <tr>
                <th style={{ width: '11%' }}>{t('colName')}</th>
                <th style={{ width: '12%' }}>{t('colUsername')}</th>
                <th style={{ width: '13%' }}>{t('colCompany')}</th>
                <th style={{ width: '20%' }}>{t('colEmail')}</th>
                <th style={{ width: '9%' }}>{t('colRole')}</th>
                <th style={{ width: '11%' }}>{t('colDepartment')}</th>
                <th style={{ width: '16%' }}>{t('colLastLogin')}</th>
                <th style={{ width: '8%', textAlign: 'center' }}>{t('colStatus')}</th>
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
                      {tenants.find(tn => tn.id === user.tenant_id)?.company_name || t('unassigned')}
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
                  <td>{user.last_login ? new Date(user.last_login).toLocaleString() : t('noLoginRecord')}</td>
                  <td style={{ textAlign: 'center' }}>
                    <div style={{ display: 'inline-flex', justifyContent: 'center' }}>
                      <StatusBadge
                        status={user.is_deleted ? t('statusDeleted') : (user.is_active ? t('statusActive') : t('statusInactive'))}
                        type={user.is_deleted ? 'error' : (user.is_active ? 'active' : 'inactive')}
                        showDot={true}
                      />
                    </div>
                  </td>
                </tr>
              ))}
              {paginatedUsers.length === 0 && (
                <tr>
                  <td colSpan={8} className="empty-table">{t('noUsers')}</td>
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