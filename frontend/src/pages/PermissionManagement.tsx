import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import RoleApiService, { Role, Permission } from '../api/services/roleApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { RoleModal } from '../components/modals/RoleModal/RoleModal';
import { RoleDetailModal } from '../components/modals/RoleModal/RoleDetailModal';
import StatusBadge from '../components/common/StatusBadge';
import { Pagination } from '../components/common/Pagination';
import '../styles/management.css';

// Initialize API Service
const roleApi = new RoleApiService();

// Interface extension with UI helpers
interface EnhancedRole extends Role {
  isSystemRole: boolean;
  permissionCount: number;
}

const PermissionManagement: React.FC = () => {
  const [roles, setRoles] = useState<EnhancedRole[]>([]);
  const { t } = useTranslation(['permissions', 'common']);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [filterType, setFilterType] = useState('all'); // all, system, custom
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(10);

  // Modals state
  const [createModalOpen, setCreateModalOpen] = useState(false);
  const [detailModalOpen, setDetailModalOpen] = useState(false);
  const [selectedRole, setSelectedRole] = useState<EnhancedRole | null>(null);
  const [editingRole, setEditingRole] = useState<Role | null>(null);

  const { confirm } = useConfirmContext();

  const fetchData = useCallback(async () => {
    try {
      setLoading(true);
      const fetchedRoles = await roleApi.getRoles();

      // Enhance data for UI
      const enhancedRoles: EnhancedRole[] = fetchedRoles.map(r => ({
        ...r,
        isSystemRole: r.is_system === 1,
        permissionCount: r.permissions ? r.permissions.length : 0
      }));

      setRoles(enhancedRoles);
    } catch (error) {
      console.error('Failed to fetch roles:', error);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  // Filtering
  const filteredRoles = roles.filter(role => {
    const matchesSearch = !searchTerm ||
      role.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      role.description?.toLowerCase().includes(searchTerm.toLowerCase());

    const matchesType = filterType === 'all' ||
      (filterType === 'system' && role.isSystemRole) ||
      (filterType === 'custom' && !role.isSystemRole);

    return matchesSearch && matchesType;
  });

  const paginatedRoles = filteredRoles.slice((currentPage - 1) * pageSize, currentPage * pageSize);

  // Calculate Stats
  const stats = {
    total: roles.length,
    system: roles.filter(r => r.isSystemRole).length,
    custom: roles.filter(r => !r.isSystemRole).length
  };

  const handleRowClick = (role: EnhancedRole) => {
    setSelectedRole(role);
    setDetailModalOpen(true);
  };

  const handleCreate = () => {
    setEditingRole(null);
    setCreateModalOpen(true);
  };

  const handleEdit = (role: Role) => {
    setEditingRole(role);
    setDetailModalOpen(false);
    setCreateModalOpen(true);
  };

  const handleSave = async () => {
    await fetchData();
  };

  return (
    <ManagementLayout>
      <PageHeader
        title={t('page.title', { ns: 'permissions' })}
        description={t('page.desc', { ns: 'permissions' })}
        icon="fas fa-shield-alt"
        actions={
          <button className="btn btn-primary" onClick={handleCreate}>
            <i className="fas fa-plus"></i> {t('page.newRole', { ns: 'permissions' })}
          </button>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label={t('page.statTotal', { ns: 'permissions' })} value={stats.total} type="primary" />
        <StatCard label={t('page.statSystem', { ns: 'permissions' })} value={stats.system} type="blueprint" />
        <StatCard label={t('page.statCustom', { ns: 'permissions' })} value={stats.custom} type="success" />
      </div>

      <FilterBar
        searchTerm={searchTerm}
        onSearchChange={setSearchTerm}
        filters={[
          {
            label: t('page.filterType', { ns: 'permissions' }),
            value: filterType,
            options: [
              { label: t('page.filterAll', { ns: 'permissions' }), value: 'all' },
              { label: t('page.filterSystem', { ns: 'permissions' }), value: 'system' },
              { label: t('page.filterCustom', { ns: 'permissions' }), value: 'custom' }
            ],
            onChange: setFilterType
          }
        ]}
        onReset={() => {
          setSearchTerm('');
          setFilterType('all');
        }}
        activeFilterCount={(searchTerm ? 1 : 0) + (filterType !== 'all' ? 1 : 0)}
      />

      <div className="mgmt-content-area">
        <div className="mgmt-table-container">
          <table className="mgmt-table">
            <thead>
              <tr>
                <th style={{ width: '20%' }}>{t('table.roleName', { ns: 'permissions' })}</th>
                <th style={{ width: '35%' }}>{t('table.description', { ns: 'permissions' })}</th>
                <th style={{ width: '15%' }}>{t('table.type', { ns: 'permissions' })}</th>
                <th style={{ width: '15%' }}>{t('page.colAssignedUsers', { ns: 'permissions' })}</th>
                <th style={{ width: '15%' }}>{t('page.colRegisteredAt', { ns: 'permissions' })}</th>
              </tr>
            </thead>
            <tbody>
              {loading ? (
                <tr>
                  <td colSpan={5} className="loading-cell">
                    <div className="loading-spinner small"></div> {t('page.loadingData', { ns: 'permissions' })}
                  </td>
                </tr>
              ) : paginatedRoles.length > 0 ? (
                paginatedRoles.map(role => (
                  <tr
                    key={role.id}
                    className="clickable"
                    onClick={() => handleRowClick(role)}
                  >
                    <td>
                      <div className="role-name-cell" style={{ display: 'flex', flexDirection: 'column' }}>
                        <span className="role-name" style={{ fontWeight: 600, fontSize: '14px', color: 'var(--neutral-900)' }}>{role.name}</span>
                        <span className="role-id-sub" style={{ fontSize: '12px', color: 'var(--neutral-500)', marginTop: '2px' }}>@{role.id}</span>
                      </div>
                    </td>
                    <td>
                      <span className="description-text">
                        {role.description || '-'}
                      </span>
                    </td>
                    <td>
                      {role.isSystemRole ? (
                        <span className="status-badge status-purple">
                          <i className="fas fa-lock"></i> 시스템
                        </span>
                      ) : (
                        <span className="status-badge status-success">
                          <i className="fas fa-user-edit"></i> {t('page.customRole', { ns: 'permissions' })}
                        </span>
                      )}
                    </td>
                    <td>
                      <span className="count-badge">
                        {t('page.userCount', { ns: 'permissions', count: role.userCount || 0 })}
                      </span>
                    </td>
                    <td>
                      {role.created_at ? new Date(role.created_at).toLocaleDateString() : '-'}
                    </td>
                  </tr>
                ))
              ) : (
                <tr>
                  <td colSpan={5} className="empty-table">
                    {t('page.noRoles', { ns: 'permissions' })}
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      <Pagination
        current={currentPage}
        total={filteredRoles.length}
        pageSize={pageSize}
        onChange={setCurrentPage}
        onShowSizeChange={(_, size) => {
          setPageSize(size);
          setCurrentPage(1);
        }}
      />

      <RoleModal
        isOpen={createModalOpen}
        onClose={() => setCreateModalOpen(false)}
        onSave={handleSave}
        role={editingRole}
      />

      <RoleDetailModal
        isOpen={detailModalOpen}
        onClose={() => setDetailModalOpen(false)}
        role={selectedRole}
        onEdit={handleEdit}
      />
    </ManagementLayout>
  );
};

export default PermissionManagement;