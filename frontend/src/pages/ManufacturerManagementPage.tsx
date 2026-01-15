import React, { useState, useEffect, useCallback } from 'react';
import { ManufactureApiService } from '../api/services/manufactureApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import { ModelListModal } from '../components/modals/ModelListModal';
import { ManufacturerModal } from '../components/modals/ManufacturerModal/ManufacturerModal';
import { ManufacturerDetailModal } from '../components/modals/ManufacturerModal/ManufacturerDetailModal';
import { Manufacturer, DeviceModel } from '../types/manufacturing';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import '../styles/management.css';

const ManufacturerManagementPage: React.FC = () => {
    const [manufacturers, setManufacturers] = useState<Manufacturer[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [searchTerm, setSearchTerm] = useState('');
    const [currentPage, setCurrentPage] = useState(1);
    const [pageSize, setPageSize] = useState(12);
    const [totalCount, setTotalCount] = useState(0);
    const [selectedStatus, setSelectedStatus] = useState<string>('all');
    const [includeDeleted, setIncludeDeleted] = useState(false);
    const { confirm } = useConfirmContext();

    const [viewMode, setViewMode] = useState<'card' | 'table'>('table');
    const [stats, setStats] = useState({
        total_manufacturers: 0,
        total_models: 0,
        total_countries: 0
    });
    const [modalOpen, setModalOpen] = useState(false);
    const [editModalOpen, setEditModalOpen] = useState(false);
    const [detailModalOpen, setDetailModalOpen] = useState(false);
    const [selectedManufacturer, setSelectedManufacturer] = useState<Manufacturer | null>(null);
    const [editingManufacturer, setEditingManufacturer] = useState<Manufacturer | null>(null);
    const [selectedManufacturerId, setSelectedManufacturerId] = useState<number | null>(null);

    const loadStats = useCallback(async () => {
        try {
            const res = await ManufactureApiService.getStatistics();
            if (res.success && res.data) {
                setStats(res.data);
            }
        } catch (err) {
            console.error('Failed to load stats:', err);
        }
    }, []);

    const loadManufacturers = useCallback(async () => {
        try {
            setLoading(true);
            const response = await ManufactureApiService.getManufacturers({
                search: searchTerm || undefined,
                isActive: selectedStatus === 'all' ? undefined : (selectedStatus === 'active'),
                page: currentPage,
                limit: pageSize,
                onlyDeleted: includeDeleted
            });
            if (response.success && response.data) {
                setManufacturers(response.data.items || []);
                setTotalCount(response.data.pagination?.total_count || 0);
            }
        } catch (err) {
            setError('제조사 정보를 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    }, [searchTerm, selectedStatus, currentPage, pageSize, includeDeleted]);

    useEffect(() => {
        loadManufacturers();
        loadStats();
    }, [loadManufacturers, loadStats, includeDeleted]);

    const handleOpenModels = (m: Manufacturer) => {
        setSelectedManufacturer(m);
        setModalOpen(true);
    };

    const handleCreate = () => {
        setEditingManufacturer(null);
        setEditModalOpen(true);
    };

    const handleEdit = (m: Manufacturer) => {
        setEditingManufacturer(m);
        setEditModalOpen(true);
    };

    const handleViewDetail = (id: number) => {
        setSelectedManufacturerId(id);
        setDetailModalOpen(true);
    };

    const handleRestore = async (m: Manufacturer) => {
        const confirmed = await confirm({
            title: '제조사 복구 확인',
            message: `'${m.name}' 제조사를 복구하시겠습니까?`,
            confirmText: '복구',
            confirmButtonType: 'primary'
        });

        if (confirmed) {
            try {
                const res = await ManufactureApiService.restoreManufacturer(m.id);
                if (res.success) {
                    await confirm({
                        title: '복구 완료',
                        message: '제조사가 성공적으로 복구되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    setIncludeDeleted(false);
                    loadManufacturers();
                    loadStats();
                } else {
                    await confirm({
                        title: '복구 실패',
                        message: res.message || '복구에 실패했습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                }
            } catch (err: any) {
                await confirm({
                    title: '오류 발생',
                    message: err.message || '복구 중 오류가 발생했습니다.',
                    confirmText: '확인',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        }
    };

    const handleDelete = async (m: Manufacturer) => {
        const confirmed = await confirm({
            title: '제조사 삭제 확인',
            message: `'${m.name}' 제조사를 삭제하시겠습니까? 연결된 모델이 있는 경우 삭제할 수 없습니다.`,
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                const res = await ManufactureApiService.deleteManufacturer(m.id);
                if (res.success) {
                    await confirm({
                        title: '삭제 완료',
                        message: '제조사가 성공적으로 삭제되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    loadManufacturers();
                    loadStats();
                } else {
                    await confirm({
                        title: '삭제 실패',
                        message: res.message || '삭제에 실패했습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                }
            } catch (err: any) {
                await confirm({
                    title: '오류 발생',
                    message: err.message || '삭제 중 오류가 발생했습니다.',
                    confirmText: '확인',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        }
    };

    return (
        <ManagementLayout>
            <PageHeader
                title="제조사 관리"
                description="하드웨어 제조사 정보를 관리합니다. 모델 상세 관리는 '디바이마스터 모델' 페이지에서 가능합니다."
                icon="fas fa-industry"
                actions={
                    <button className="btn btn-primary" onClick={handleCreate}>
                        <i className="fas fa-plus"></i> 새 제조사 등록
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label="전체 제조사" value={stats.total_manufacturers} type="primary" />
                <StatCard label="전체 모델" value={stats.total_models} type="success" />
                <StatCard label="제조사 국가" value={stats.total_countries} type="neutral" />
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
                        <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '14px', fontWeight: '500', cursor: 'pointer', whiteSpace: 'nowrap', color: 'var(--neutral-600)' }}>
                            <input
                                type="checkbox"
                                checked={includeDeleted}
                                onChange={(e) => setIncludeDeleted(e.target.checked)}
                            />
                            삭제된 제조사 보기
                        </label>
                        <div className="view-toggle">
                            <button
                                className={`btn-icon ${viewMode === 'card' ? 'active' : ''}`}
                                onClick={() => setViewMode('card')}
                                title="카드 보기"
                            >
                                <i className="fas fa-th-large"></i>
                            </button>
                            <button
                                className={`btn-icon ${viewMode === 'table' ? 'active' : ''}`}
                                onClick={() => setViewMode('table')}
                                title="리스트 보기"
                            >
                                <i className="fas fa-list"></i>
                            </button>
                        </div>
                    </div>
                }
            />

            <div className="mgmt-content-area">
                {viewMode === 'card' ? (
                    <div className="mgmt-grid">
                        {manufacturers.map(m => (
                            <div key={m.id} className={`mgmt-card ${m.is_deleted ? 'deleted-item' : ''}`} style={m.is_deleted ? { opacity: 0.6 } : undefined}>
                                <div className="card-header">
                                    <div className="card-title">
                                        <h4 className="clickable-name" onClick={() => handleViewDetail(m.id)}>{m.name}</h4>
                                        <div style={{ display: 'flex', gap: '5px' }}>
                                            {!!m.is_deleted && <span className="badge danger">삭제됨</span>}
                                            <span className={`badge ${m.is_active ? 'success' : 'neutral'}`}>
                                                {m.is_active ? '활성' : '비활성'}
                                            </span>
                                        </div>
                                    </div>
                                </div>
                                <div className="card-body">
                                    <p>{m.description || '제조사 설명이 없습니다.'}</p>
                                    <div className="card-meta">
                                        <span className="clickable-name" onClick={() => handleOpenModels(m)}>
                                            <i className="fas fa-microchip"></i> {m.model_count || 0} Models
                                        </span>
                                        {m.country && <span><i className="fas fa-globe"></i> {m.country}</span>}
                                    </div>
                                </div>
                                <div className="card-footer" style={{ justifyContent: 'flex-end' }}>
                                    <div className="card-actions">
                                        {m.is_deleted ? (
                                            <button className="btn btn-outline btn-sm" onClick={() => handleRestore(m)}>
                                                <i className="fas fa-undo"></i> 복구하기
                                            </button>
                                        ) : (
                                            <button className="btn btn-outline btn-sm" onClick={() => handleViewDetail(m.id)}>
                                                상세보기
                                            </button>
                                        )}
                                    </div>
                                </div>
                            </div>
                        ))}
                    </div>
                ) : (
                    <div className="mgmt-table-container">
                        <table className="mgmt-table">
                            <thead>
                                <tr>
                                    <th>제조사명</th>
                                    <th>설명</th>
                                    <th>모델 수</th>
                                    <th>국가</th>
                                    <th>상태</th>
                                </tr>
                            </thead>
                            <tbody>
                                {manufacturers.map(m => (
                                    <tr key={m.id} className={m.is_deleted ? 'deleted-row' : ''} style={m.is_deleted ? { opacity: 0.6, backgroundColor: 'var(--neutral-50)' } : undefined}>
                                        <td>
                                            <div
                                                className="mgmt-table-id-link clickable-name"
                                                onClick={() => handleViewDetail(m.id)}
                                                style={{ fontWeight: '600', fontSize: '14px', cursor: 'pointer' }}
                                            >
                                                {m.name}
                                            </div>
                                        </td>
                                        <td>
                                            <div style={{
                                                maxWidth: '350px',
                                                overflow: 'hidden',
                                                textOverflow: 'ellipsis',
                                                whiteSpace: 'nowrap',
                                                color: '#64748b',
                                                fontSize: '13px'
                                            }}>
                                                {m.description || '-'}
                                            </div>
                                        </td>
                                        <td>
                                            <button
                                                className="btn-link"
                                                onClick={() => handleOpenModels(m)}
                                            >
                                                <i className="fas fa-microchip"></i> {m.model_count || 0}
                                            </button>
                                        </td>
                                        <td>
                                            <div style={{ fontSize: '13px', color: '#64748b' }}>
                                                {m.country ? (
                                                    <span>
                                                        <i className="fas fa-globe"></i> {m.country}
                                                    </span>
                                                ) : '-'}
                                            </div>
                                        </td>
                                        <td>
                                            <div style={{ display: 'flex', gap: '5px' }}>
                                                {!!m.is_deleted && <span className="badge danger">삭제됨</span>}
                                                <span className={`badge ${m.is_active ? 'success' : 'neutral'}`}>
                                                    {m.is_active ? '활성' : '비활성'}
                                                </span>
                                            </div>
                                        </td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    </div>
                )}
            </div>

            <Pagination
                current={currentPage}
                total={totalCount}
                pageSize={pageSize}
                onChange={(page) => setCurrentPage(page)}
            />

            <ModelListModal
                isOpen={modalOpen}
                onClose={() => {
                    setModalOpen(false);
                    setSelectedManufacturer(null);
                }}
                manufacturer={selectedManufacturer}
            />
            <ManufacturerModal
                isOpen={editModalOpen}
                onClose={() => {
                    setEditModalOpen(false);
                    setEditingManufacturer(null);
                }}
                onSave={() => {
                    loadManufacturers();
                    loadStats();
                }}
                manufacturer={editingManufacturer}
            />
            <ManufacturerDetailModal
                isOpen={detailModalOpen}
                onClose={() => {
                    setDetailModalOpen(false);
                    setSelectedManufacturerId(null);
                }}
                onSave={() => {
                    loadManufacturers();
                    loadStats();
                }}
                manufacturerId={selectedManufacturerId}
            />
        </ManagementLayout>
    );
};

export default ManufacturerManagementPage;
