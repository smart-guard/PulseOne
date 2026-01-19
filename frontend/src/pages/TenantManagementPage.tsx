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
            setError('고객사 정보를 불러오는 중 오류가 발생했습니다.');
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
            title: '고객사 복구 확인',
            message: `'${t.company_name}' 고객사를 복구하시겠습니까?`,
            confirmText: '복구',
            confirmButtonType: 'primary'
        });

        if (confirmed) {
            try {
                const res = await TenantApiService.restoreTenant(t.id);
                if (res.success) {
                    await confirm({
                        title: '복구 완료',
                        message: '고객사가 성공적으로 복구되었습니다.',
                        confirmText: '확인',
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
                title="고객사(Tenant) 관리"
                description="멀티테넌트 환경의 고객사 정보를 관리합니다. 구독 플랜 및 사용량 제한을 설정할 수 있습니다."
                icon="fas fa-building"
                actions={
                    <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
                        <i className="fas fa-plus"></i> 새 고객사 등록
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label="전체 고객사" value={stats.total_tenants} type="primary" />
                <StatCard label="활성 고객사" value={stats.active_tenants} type="success" />
                <StatCard label="트라이얼" value={stats.trial_tenants} type="warning" />
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
                            삭제된 고객사 보기
                        </label>
                    </div>
                }
            />

            <div className="mgmt-content-area">
                <div className="mgmt-table-container">
                    <table className="mgmt-table">
                        <thead>
                            <tr>
                                <th>회사명</th>
                                <th>회사 코드</th>
                                <th>도메인</th>
                                <th>구독 플랜</th>
                                <th>상태</th>
                                <th>등록일</th>
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
