import React, { useState, useEffect, useCallback } from 'react';
import { TenantApiService } from '../api/services/tenantApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import { Tenant } from '../types/common';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import '../styles/management.css';

// TODO: Create TenantModal and TenantDetailModal
import { TenantModal } from '../components/modals/TenantModal/TenantModal';
import { TenantDetailModal } from '../components/modals/TenantModal/TenantDetailModal';

const TenantManagementPage: React.FC = () => {
    const [tenants, setTenants] = useState<Tenant[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [searchTerm, setSearchTerm] = useState('');
    const [currentPage, setCurrentPage] = useState(1);
    const [pageSize, setPageSize] = useState(10);
    const [totalCount, setTotalCount] = useState(0);
    const [selectedStatus, setSelectedStatus] = useState<string>('all');
    const [includeDeleted, setIncludeDeleted] = useState(false);
    const { confirm } = useConfirmContext();

    const [stats, setStats] = useState({
        total_tenants: 0,
        active_tenants: 0,
        trial_tenants: 0
    });

    const [editModalOpen, setEditModalOpen] = useState(false);
    const [detailModalOpen, setDetailModalOpen] = useState(false);
    const [editingTenant, setEditingTenant] = useState<Tenant | null>(null);
    const [selectedTenantId, setSelectedTenantId] = useState<number | null>(null);

    const loadStats = useCallback(async () => {
        try {
            const res = await TenantApiService.getTenantStats();
            if (res.success && res.data) {
                setStats(res.data);
            }
        } catch (err) {
            console.error('Failed to load stats:', err);
        }
    }, []);

    const loadTenants = useCallback(async () => {
        try {
            setLoading(true);
            const response = await TenantApiService.getTenants({
                search: searchTerm || undefined,
                isActive: selectedStatus === 'all' ? undefined : (selectedStatus === 'active'),
                page: currentPage,
                limit: pageSize,
                onlyDeleted: includeDeleted
            });
            if (response.success && response.data) {
                setTenants(response.data.items || []);
                setTotalCount(response.data.pagination?.total_count || 0);
            }
        } catch (err) {
            setError('Error loading tenant information.');
        } finally {
            setLoading(false);
        }
    }, [searchTerm, selectedStatus, currentPage, pageSize, includeDeleted]);

    useEffect(() => {
        loadTenants();
        loadStats();
    }, [loadTenants, loadStats, includeDeleted]);

    const handleCreate = () => {
        setEditingTenant(null);
        setEditModalOpen(true);
    };

    const handleViewDetail = (id: number) => {
        console.log('[TenantPage] handleViewDetail clicked, id:', id);
        setSelectedTenantId(id);
        setDetailModalOpen(true);
        console.log('[TenantPage] setDetailModalOpen(true) called');
    };

    const handleRestore = async (t: Tenant) => {
        const confirmed = await confirm({
            title: 'Confirm Tenant Restore',
            message: `Are you sure you want to restore tenant '${t.company_name}'?`,
            confirmText: 'Restore',
            confirmButtonType: 'primary'
        });

        if (confirmed) {
            try {
                const res = await TenantApiService.restoreTenant(t.id);
                if (res.success) {
                    await confirm({
                        title: 'Restore Complete',
                        message: 'Tenant restored successfully.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    setIncludeDeleted(false);
                    loadTenants();
                    loadStats();
                }
            } catch (err: any) {
                console.error('Failed to restore tenant:', err);
            }
        }
    };

    return (
        <ManagementLayout>
            <PageHeader
                title="Tenant Management"
                description="Manage tenant information in a multi-tenant environment. Configure subscription plans and usage limits."
                icon="fas fa-building"
                actions={
                    <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
                        <i className="fas fa-plus"></i> New Tenant Registration
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label="Total Tenants" value={stats.total_tenants} type="primary" />
                <StatCard label="Active Tenants" value={stats.active_tenants} type="success" />
                <StatCard label="Trial" value={stats.trial_tenants} type="warning" />
            </div>

            <FilterBar
                searchTerm={searchTerm}
                onSearchChange={setSearchTerm}
                filters={[
                    {
                        label: 'Status',
                        value: selectedStatus,
                        options: [
                            { label: 'All', value: 'all' },
                            { label: 'Active', value: 'active' },
                            { label: 'Inactive', value: 'inactive' }
                        ],
                        onChange: setSelectedStatus
                    }
                ]}
                onReset={() => {
                    setSearchTerm('');
                    setSelectedStatus('all');
                    setIncludeDeleted(false);
                }}
                activeFilterCount={(searchTerm ? 1 : 0) + (selectedStatus !== 'all' ? 1 : 0) + (includeDeleted ? 1 : 0)}
                rightActions={
                    <div style={{ display: 'flex', alignItems: 'center', gap: '20px' }}>
                        <label className="mgmt-checkbox-label">
                            <input
                                type="checkbox"
                                checked={includeDeleted}
                                onChange={(e) => setIncludeDeleted(e.target.checked)}
                            />
                            View Deleted Tenants
                        </label>
                    </div>
                }
            />

            <div className="mgmt-content-area">
                <div className="mgmt-table-container">
                    <table className="mgmt-table">
                        <thead>
                            <tr>
                                <th>Company Name</th>
                                <th>Company Code</th>
                                <th>Domain</th>
                                <th>Subscription Plan</th>
                                <th>Status</th>
                                <th>Registered</th>
                            </tr>
                        </thead>
                        <tbody>
                            {tenants.map(t => (
                                <tr key={t.id} className={t.is_active ? '' : 'inactive-row'}>
                                    <td>
                                        <div
                                            className="mgmt-table-id-link mgmt-clickable-name"
                                            onClick={() => handleViewDetail(t.id)}
                                            style={{ fontWeight: '600' }}
                                        >
                                            {t.company_name}
                                        </div>
                                    </td>
                                    <td><div style={{ fontWeight: '500', color: 'var(--neutral-700)' }}>{t.company_code}</div></td>
                                    <td>{t.domain || '-'}</td>
                                    <td>
                                        <span className={`mgmt-badge ${t.subscription_plan === 'enterprise' ? 'primary' : 'neutral'}`}>
                                            {t.subscription_plan.toUpperCase()}
                                        </span>
                                    </td>
                                    <td>
                                        <span className={`mgmt-badge ${t.subscription_status === 'active' ? 'success' : 'warning'}`}>
                                            {t.subscription_status.toUpperCase()}
                                        </span>
                                    </td>
                                    <td>{new Date(t.created_at).toLocaleDateString()}</td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
            </div>

            <Pagination
                current={currentPage}
                total={totalCount}
                pageSize={pageSize}
                onChange={(page) => setCurrentPage(page)}
            />

            <TenantModal
                isOpen={editModalOpen}
                onClose={() => setEditModalOpen(false)}
                onSave={() => {
                    loadTenants();
                    loadStats();
                }}
                tenant={editingTenant}
            />

            <TenantDetailModal
                isOpen={detailModalOpen}
                onClose={() => setDetailModalOpen(false)}
                onSave={() => {
                    loadTenants();
                    loadStats();
                }}
                tenantId={selectedTenantId}
            />
        </ManagementLayout>
    );
};

export default TenantManagementPage;
