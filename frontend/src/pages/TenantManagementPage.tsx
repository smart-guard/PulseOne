import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
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
    const { t } = useTranslation('tenantManagement');
    const [tenants, setTenants] = useState<Tenant[]>([]);;
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
            setError(t('errLoad'));
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

    const handleRestore = async (tnt: Tenant) => {
        const confirmed = await confirm({
            title: t('restoreTitle'),
            message: t('restoreMessage', { name: tnt.company_name }),
            confirmText: t('restoreBtn'),
            confirmButtonType: 'primary'
        });

        if (confirmed) {
            try {
                const res = await TenantApiService.restoreTenant(tnt.id);
                if (res.success) {
                    await confirm({
                        title: t('restoreCompleteTitle'),
                        message: t('restoreCompleteMsg'),
                        confirmText: t('restoreCompleteBtn'),
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
                title={t('pageTitle')}
                description={t('pageDesc')}
                icon="fas fa-building"
                actions={
                    <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
                        <i className="fas fa-plus"></i> {t('newTenant')}
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label={t('totalTenants')} value={stats.total_tenants} type="primary" />
                <StatCard label={t('activeTenants')} value={stats.active_tenants} type="success" />
                <StatCard label={t('trialTenants')} value={stats.trial_tenants} type="warning" />
            </div>

            <FilterBar
                searchTerm={searchTerm}
                onSearchChange={setSearchTerm}
                filters={[
                    {
                        label: t('filterStatus'),
                        value: selectedStatus,
                        options: [
                            { label: t('filterAll'), value: 'all' },
                            { label: t('filterActive'), value: 'active' },
                            { label: t('filterInactive'), value: 'inactive' }
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
                                <th>{t('colCompanyName')}</th>
                                <th>{t('colCompanyCode')}</th>
                                <th>{t('colDomain')}</th>
                                <th>{t('colPlan')}</th>
                                <th>{t('colStatus')}</th>
                                <th>{t('colRegistered')}</th>
                            </tr>
                        </thead>
                        <tbody>
                            {tenants.map(tnt => (
                                <tr key={tnt.id} className={tnt.is_active ? '' : 'inactive-row'}>
                                    <td>
                                        <div
                                            className="mgmt-table-id-link mgmt-clickable-name"
                                            onClick={() => handleViewDetail(tnt.id)}
                                            style={{ fontWeight: '600' }}
                                        >
                                            {tnt.company_name}
                                        </div>
                                    </td>
                                    <td><div style={{ fontWeight: '500', color: 'var(--neutral-700)' }}>{tnt.company_code}</div></td>
                                    <td>{tnt.domain || '-'}</td>
                                    <td>
                                        <span className={`mgmt-badge ${tnt.subscription_plan === 'enterprise' ? 'primary' : 'neutral'}`}>
                                            {tnt.subscription_plan.toUpperCase()}
                                        </span>
                                    </td>
                                    <td>
                                        <span className={`mgmt-badge ${tnt.subscription_status === 'active' ? 'success' : 'warning'}`}>
                                            {tnt.subscription_status.toUpperCase()}
                                        </span>
                                    </td>
                                    <td>{new Date(tnt.created_at).toLocaleDateString()}</td>
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
