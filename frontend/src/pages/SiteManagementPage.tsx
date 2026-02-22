import React, { useState, useEffect, useCallback } from 'react';
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
            setError('사이트 정보를 불러오는 중 오류가 발생했습니다.');
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
            title: '사이트 복구 확인',
            message: `'${s.name}' 사이트를 복구하시겠습니까?`,
            confirmText: '복구',
            confirmButtonType: 'primary'
        });

        if (confirmed) {
            try {
                const res = await SiteApiService.restoreSite(s.id);
                if (res.success) {
                    await confirm({
                        title: '복구 완료',
                        message: '사이트가 성공적으로 복구되었습니다.',
                        confirmText: '확인',
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
                title="사이트(Site) 관리"
                description="물리적 위치 및 설치 장소 정보를 관리합니다. 계층 구조(본사-지사-동/호 등)를 지원합니다."
                icon="fas fa-map-marker-alt"
                actions={
                    <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
                        <i className="fas fa-plus"></i> 새 사이트 등록
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label="전체 사이트" value={stats.total_sites} type="primary" />
                <StatCard label="활성 사이트" value={stats.active_sites} type="success" />
                <StatCard label="본사/메인" value={stats.main_sites} type="neutral" />
                {quota && (<>
                    <StatCard
                        label={`Collector 사용 (/${quota.max})`}
                        value={quota.used}
                        type={quota.is_exceeded ? 'error' : 'primary'}
                    />
                    <StatCard
                        label="온라인 Collector"
                        value={quota.online}
                        type="success"
                    />
                    <StatCard
                        label="오프라인 Collector"
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
                        label: '상태',
                        value: selectedStatus,
                        options: [
                            { label: '전체', value: 'all' },
                            { label: '활성', value: 'active' },
                            { label: '비활성', value: 'inactive' }
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
                            삭제된 사이트 보기
                        </label>
                    </div>
                }
            />

            <div className="mgmt-content-area">
                <div className="mgmt-table-container">
                    <table className="mgmt-table">
                        <thead>
                            <tr>
                                <th>사이트명</th>
                                <th>코드</th>
                                <th>유형</th>
                                <th>위치/주소</th>
                                <th>Collector</th>
                                <th>상태</th>
                                <th>상위 사이트</th>
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
                                    <td>
                                        <span style={{ fontSize: '12px', color: 'var(--neutral-400)' }}>
                                            <i className="fas fa-server" style={{ marginRight: '4px' }}></i>
                                            {(s as any).collector_count ?? '-'}개
                                        </span>
                                    </td>
                                    <td>
                                        <span className={`mgmt-badge ${s.is_active ? 'success' : 'neutral'}`}>
                                            {s.is_active ? '활성' : '비활성'}
                                        </span>
                                    </td>
                                    <td>{s.parent_site_id ? `ID: ${s.parent_site_id}` : '최상위'}</td>
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
