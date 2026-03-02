import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { SiteApiService } from '../api/services/siteApi';
import { CollectorApiService } from '../api/services/collectorApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import { Site } from '../types/common';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import '../styles/management.css';

import { SiteModal } from '../components/modals/SiteModal/SiteModal';
import { SiteDetailModal } from '../components/modals/SiteModal/SiteDetailModal';

const SiteManagementPage: React.FC = () => {
    const { t } = useTranslation(['sites', 'common']);
    const [sites, setSites] = useState<Site[]>([]);
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
        total_sites: 0,
        active_sites: 0,
        main_sites: 0
    });

    const [quota, setQuota] = useState<{ used: number; max: number; available: number; is_exceeded: boolean; online: number; offline: number } | null>(null);

    const loadQuota = useCallback(async () => {
        try {
            const res = await CollectorApiService.getQuotaStatus();
            if (res.success && res.data) setQuota(res.data);
        } catch (e) { /* ignore */ }
    }, []);

    const [editModalOpen, setEditModalOpen] = useState(false);
    const [detailModalOpen, setDetailModalOpen] = useState(false);
    const [editingSite, setEditingSite] = useState<Site | null>(null);
    const [selectedSiteId, setSelectedSiteId] = useState<number | null>(null);

    const loadSites = useCallback(async () => {
        try {
            setLoading(true);
            const response = await SiteApiService.getSites({
                search: searchTerm || undefined,
                isActive: selectedStatus === 'all' ? undefined : (selectedStatus === 'active'),
                page: currentPage,
                limit: pageSize,
                onlyDeleted: includeDeleted
            });
            if (response.success && response.data) {
                setSites(response.data.items || []);
                setTotalCount(response.data.pagination?.total_count || 0);

                // Quick stats calculation from current results or separate call if needed
                // For now, using mock stats or updating based on response
                setStats({
                    total_sites: response.data.pagination?.total_count || 0,
                    active_sites: (response.data.items || []).filter(s => s.is_active).length,
                    main_sites: (response.data.items || []).filter(s => !s.parent_site_id).length
                });
            }
        } catch (err) {
            setError(t('sites:loading'));
        } finally {
            setLoading(false);
        }
    }, [searchTerm, selectedStatus, currentPage, pageSize, includeDeleted]);

    useEffect(() => {
        loadSites();
        loadQuota();
    }, [loadSites, loadQuota, includeDeleted]);

    const handleCreate = () => {
        setEditingSite(null);
        setEditModalOpen(true);
    };

    const handleViewDetail = (id: number) => {
        setSelectedSiteId(id);
        setDetailModalOpen(true);
    };

    const handleRestore = async (s: Site) => {
        const confirmed = await confirm({
            title: t('sites:confirm.restoreTitle'),
            message: t('sites:confirm.restoreMsg', { name: s.name }),
            confirmText: t('sites:confirm.restoreText'),
            confirmButtonType: 'primary'
        });

        if (confirmed) {
            try {
                const res = await SiteApiService.restoreSite(s.id);
                if (res.success) {
                    await confirm({
                        title: t('sites:confirm.restoreSuccessTitle'),
                        message: t('sites:confirm.restoreSuccessMsg'),
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    setIncludeDeleted(false);
                    loadSites();
                }
            } catch (err: any) {
                console.error('Failed to restore site:', err);
            }
        }
    };

    return (
        <ManagementLayout>
            <PageHeader
                title={t('sites:title')}
                description={t('sites:description')}
                icon="fas fa-map-marker-alt"
                actions={
                    <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
                        <i className="fas fa-plus"></i> {t('sites:register')}
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label={t('sites:stats.totalSites')} value={stats.total_sites} type="primary" />
                <StatCard label={t('sites:stats.activeSites')} value={stats.active_sites} type="success" />
                <StatCard label={t('sites:stats.mainSites')} value={stats.main_sites} type="neutral" />
                {quota && (<>
                    <StatCard
                        label={t('sites:stats.edgeServerUsage', { max: quota.max })}
                        value={quota.used}
                        type={quota.is_exceeded ? 'error' : 'primary'}
                    />
                    <StatCard
                        label={t('sites:stats.onlineEdge')}
                        value={quota.online}
                        type="success"
                    />
                    <StatCard
                        label={t('sites:stats.offlineEdge')}
                        value={quota.offline}
                        type={quota.offline > 0 ? 'warning' : 'neutral'}
                    />
                </>)}
            </div>

            <FilterBar
                searchTerm={searchTerm}
                onSearchChange={setSearchTerm}
                filters={[
                    {
                        label: t('sites:filter.status'),
                        value: selectedStatus,
                        options: [
                            { label: t('sites:filter.all'), value: 'all' },
                            { label: t('sites:filter.active'), value: 'active' },
                            { label: t('sites:filter.inactive'), value: 'inactive' }
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
                            {t('sites:showDeleted')}
                        </label>
                    </div>
                }
            />

            <div className="mgmt-content-area">
                <div className="mgmt-table-container">
                    <table className="mgmt-table">
                        <thead>
                            <tr>
                                <th>{t('sites:table.name')}</th>
                                <th>{t('sites:table.code')}</th>
                                <th>{t('sites:table.type')}</th>
                                <th>{t('sites:table.location')}</th>
                                <th style={{ textAlign: 'center' }}>{t('sites:table.edgeServer')}</th>
                                <th style={{ textAlign: 'center' }}>{t('sites:table.status')}</th>
                                <th style={{ textAlign: 'center' }}>{t('sites:table.parentSite')}</th>
                            </tr>
                        </thead>
                        <tbody>
                            {sites.map(s => (
                                <tr key={s.id} className={s.is_active ? '' : 'inactive-row'}>
                                    <td>
                                        <div
                                            className="mgmt-table-id-link mgmt-clickable-name"
                                            onClick={() => handleViewDetail(s.id)}
                                            style={{ fontWeight: '600' }}
                                        >
                                            {s.name}
                                        </div>
                                    </td>
                                    <td><code>{s.code}</code></td>
                                    <td>{s.site_type}</td>
                                    <td>
                                        <div style={{ fontSize: '12px', color: 'var(--neutral-500)', maxWidth: '200px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                            {s.address || s.location || '-'}
                                        </div>
                                    </td>
                                    <td style={{ textAlign: 'center' }}>
                                        <span style={{ fontSize: '12px', color: 'var(--neutral-400)' }}>
                                            <i className="fas fa-server" style={{ marginRight: '4px' }}></i>
                                            {(s as any).collector_count ?? '-'}대
                                        </span>
                                    </td>
                                    <td style={{ textAlign: 'center' }}>
                                        <span className={`mgmt-badge ${s.is_active ? 'success' : 'neutral'}`}>
                                            {s.is_active ? t('sites:badge.active') : t('sites:badge.inactive')}
                                        </span>
                                    </td>
                                    <td style={{ textAlign: 'center' }}>{s.parent_site_id ? `ID: ${s.parent_site_id}` : t('sites:topLevel')}</td>
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

            <SiteModal
                isOpen={editModalOpen}
                onClose={() => setEditModalOpen(false)}
                onSave={loadSites}
                site={editingSite}
            />

            <SiteDetailModal
                isOpen={detailModalOpen}
                onClose={() => setDetailModalOpen(false)}
                onSave={loadSites}
                siteId={selectedSiteId}
            />
        </ManagementLayout>
    );
};

export default SiteManagementPage;
