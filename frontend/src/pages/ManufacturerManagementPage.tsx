import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
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
    const { t } = useTranslation(['manufacturers', 'common']);
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
            setError(t('manufacturers:loading'));
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
            title: t('manufacturers:confirm.restoreTitle'),
            message: t('manufacturers:confirm.restoreMsg', { name: m.name }),
            confirmText: t('manufacturers:confirm.restoreText'),
            confirmButtonType: 'primary'
        });

        if (confirmed) {
            try {
                const res = await ManufactureApiService.restoreManufacturer(m.id);
                if (res.success) {
                    await confirm({
                        title: t('manufacturers:confirm.restoreSuccessTitle'),
                        message: t('manufacturers:confirm.restoreSuccessMsg'),
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    setIncludeDeleted(false);
                    loadManufacturers();
                    loadStats();
                } else {
                    await confirm({
                        title: t('manufacturers:confirm.restoreFailTitle'),
                        message: res.message || 'Restoration failed.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                }
            } catch (err: any) {
                await confirm({
                    title: t('manufacturers:confirm.errorTitle'),
                    message: err.message || 'Error occurred.',
                    confirmText: 'OK',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        }
    };

    const handleDelete = async (m: Manufacturer) => {
        const confirmed = await confirm({
            title: t('manufacturers:confirm.deleteTitle'),
            message: t('manufacturers:confirm.deleteMsg', { name: m.name }),
            confirmText: t('manufacturers:confirm.deleteText'),
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                const res = await ManufactureApiService.deleteManufacturer(m.id);
                if (res.success) {
                    await confirm({
                        title: t('manufacturers:confirm.deleteSuccessTitle'),
                        message: t('manufacturers:confirm.deleteSuccessMsg'),
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    loadManufacturers();
                    loadStats();
                } else {
                    await confirm({
                        title: t('manufacturers:confirm.deleteFailTitle'),
                        message: res.message || 'Delete failed.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                }
            } catch (err: any) {
                await confirm({
                    title: t('manufacturers:confirm.errorTitle'),
                    message: err.message || 'Error occurred.',
                    confirmText: 'OK',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        }
    };

    return (
        <ManagementLayout>
            <PageHeader
                title={t('manufacturers:title')}
                description={t('manufacturers:description')}
                icon="fas fa-industry"
                actions={
                    <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreate}>
                        <i className="fas fa-plus"></i> {t('manufacturers:register')}
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label={t('manufacturers:stats.totalManufacturers')} value={stats.total_manufacturers} type="primary" />
                <StatCard label={t('manufacturers:stats.totalModels')} value={stats.total_models} type="success" />
                <StatCard label={t('manufacturers:stats.countries')} value={stats.total_countries} type="neutral" />
            </div>

            <FilterBar
                searchTerm={searchTerm}
                onSearchChange={setSearchTerm}
                filters={[
                    {
                        label: t('manufacturers:filter.status'),
                        value: selectedStatus,
                        options: [
                            { label: t('manufacturers:filter.all'), value: 'all' },
                            { label: t('manufacturers:filter.active'), value: 'active' },
                            { label: t('manufacturers:filter.inactive'), value: 'inactive' }
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
                    <>
                        <div style={{ display: 'flex', alignItems: 'center', height: '32px' }}>
                            <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '13px', fontWeight: '500', cursor: 'pointer', whiteSpace: 'nowrap', color: 'var(--neutral-600)', margin: 0 }}>
                                <input
                                    type="checkbox"
                                    checked={includeDeleted}
                                    onChange={(e) => setIncludeDeleted(e.target.checked)}
                                />
                                {t('manufacturers:showDeleted')}
                            </label>
                        </div>
                        <div className="mgmt-view-toggle">
                            <button
                                className={`mgmt-btn-icon ${viewMode === 'card' ? 'active' : ''}`}
                                onClick={() => setViewMode('card')}
                                title={t('manufacturers:cardView')}
                            >
                                <i className="fas fa-th-large"></i>
                            </button>
                            <button
                                className={`mgmt-btn-icon ${viewMode === 'table' ? 'active' : ''}`}
                                onClick={() => setViewMode('table')}
                                title={t('manufacturers:listView')}
                            >
                                <i className="fas fa-list"></i>
                            </button>
                        </div>
                    </>
                }
            />

            <div className="mgmt-content-area">
                {viewMode === 'card' ? (
                    <div className="mgmt-grid">
                        {manufacturers.map(m => (
                            <div key={m.id} className={`mgmt-card ${m.is_deleted ? 'deleted-item' : ''}`} style={m.is_deleted ? { opacity: 0.6 } : undefined}>
                                <div className="mgmt-card-header">
                                    <div className="mgmt-card-title">
                                        <h4 className="mgmt-clickable-name" onClick={() => handleViewDetail(m.id)}>{m.name}</h4>
                                        <div style={{ display: 'flex', gap: '5px' }}>
                                            {!!m.is_deleted && <span className="mgmt-badge danger">{t('manufacturers:badge.deleted')}</span>}
                                            <span className={`mgmt-badge ${m.is_active ? 'success' : 'neutral'}`}>
                                                {m.is_active ? t('manufacturers:badge.active') : t('manufacturers:badge.inactive')}
                                            </span>
                                        </div>
                                    </div>
                                </div>
                                <div className="mgmt-card-body">
                                    <p>{m.description || t('manufacturers:noDescription')}</p>
                                    <div className="mgmt-card-meta">
                                        <span className="mgmt-clickable-name" onClick={() => handleOpenModels(m)}>
                                            <i className="fas fa-microchip"></i> {m.model_count || 0} Models
                                        </span>
                                        {m.country && <span><i className="fas fa-globe"></i> {m.country}</span>}
                                    </div>
                                </div>
                                <div className="mgmt-card-footer" style={{ justifyContent: 'flex-end' }}>
                                    <div className="card-actions">
                                        {m.is_deleted ? (
                                            <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => handleRestore(m)}>
                                                <i className="fas fa-undo"></i> {t('manufacturers:restore')}
                                            </button>
                                        ) : (
                                            <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => handleViewDetail(m.id)}>
                                                {t('manufacturers:detail')}
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
                                    <th>{t('manufacturers:table.name')}</th>
                                    <th>{t('manufacturers:table.description')}</th>
                                    <th>{t('manufacturers:table.modelCount')}</th>
                                    <th>{t('manufacturers:table.country')}</th>
                                    <th>{t('manufacturers:table.status')}</th>
                                </tr>
                            </thead>
                            <tbody>
                                {manufacturers.map(m => (
                                    <tr key={m.id} className={m.is_deleted ? 'deleted-row' : ''} style={m.is_deleted ? { opacity: 0.6, backgroundColor: 'var(--neutral-50)' } : undefined}>
                                        <td>
                                            <div
                                                className="mgmt-table-id-link mgmt-clickable-name"
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
                                                className="mgmt-btn-link"
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
                                                {!!m.is_deleted && <span className="mgmt-badge danger">{t('manufacturers:badge.deleted')}</span>}
                                                <span className={`mgmt-badge ${m.is_active ? 'success' : 'neutral'}`}>
                                                    {m.is_active ? t('manufacturers:badge.active') : t('manufacturers:badge.inactive')}
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
