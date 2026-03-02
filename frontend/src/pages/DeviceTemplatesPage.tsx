import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { TemplateApiService } from '../api/services/templateApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { notification } from 'antd';
import { StatCard } from '../components/common/StatCard';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import { DeviceTemplate, Manufacturer, Protocol } from '../types/manufacturing';
import { ManufactureApiService } from '../api/services/manufactureApi';
import { ProtocolApiService } from '../api/services/protocolApi';
import DeviceTemplateWizard from '../components/modals/DeviceTemplateWizard';
import DeviceTemplateDetailModal from '../components/modals/DeviceTemplateDetailModal';
import MasterModelModal from '../components/modals/MasterModelModal';
import '../styles/management.css';
import '../styles/pagination.css';

const DeviceTemplatesPage: React.FC = () => {
    const { t } = useTranslation(['deviceTemplates', 'common']);
    const [templates, setTemplates] = useState<DeviceTemplate[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [searchTerm, setSearchTerm] = useState('');
    const [selectedManufacturer, setSelectedManufacturer] = useState<string>('');
    const [selectedProtocol, setSelectedProtocol] = useState<string>('');
    const [selectedUsage, setSelectedUsage] = useState<string>('');
    const [manufacturers, setManufacturers] = useState<Manufacturer[]>([]);
    const [protocols, setProtocols] = useState<Protocol[]>([]);
    const [currentPage, setCurrentPage] = useState(1);
    const [pageSize, setPageSize] = useState(12);
    const [totalCount, setTotalCount] = useState(0);
    const [usedCount, setUsedCount] = useState(0);
    const [wizardOpen, setWizardOpen] = useState(false);
    const [selectedTemplateId, setSelectedTemplateId] = useState<number | undefined>(undefined);
    const [detailModalOpen, setDetailModalOpen] = useState(false);
    const [selectedDetailTemplate, setSelectedDetailTemplate] = useState<DeviceTemplate | null>(null);
    const [masterModalOpen, setMasterModalOpen] = useState(false);
    const [editingTemplate, setEditingTemplate] = useState<DeviceTemplate | null>(null);

    const onEditTemplate = (template: DeviceTemplate) => {
        setDetailModalOpen(false);
        setEditingTemplate(template);
        setMasterModalOpen(true);
    };

    const { confirm } = useConfirmContext();

    const handleDelete = async (template: DeviceTemplate, onSuccess?: () => void) => {
        // Frontend check for immediate feedback (optional but good UX)
        if ((template.device_count || 0) > 0) {
            await confirm({
                title: t('deviceTemplates:confirm.inUseTitle'),
                message: t('deviceTemplates:confirm.inUseMsg', { count: template.device_count }),
                confirmButtonType: 'warning',
                showCancelButton: false
            });
            return;
        }

        const confirmed = await confirm({
            title: t('deviceTemplates:confirm.deleteTitle'),
            message: t('deviceTemplates:confirm.deleteMsg', { name: template.name }),
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setLoading(true);
                const res = await TemplateApiService.deleteTemplate(template.id);
                if (res.success) {
                    await confirm({
                        title: t('deviceTemplates:confirm.deleteSuccessTitle'),
                        message: t('deviceTemplates:confirm.deleteSuccessMsg'),
                        confirmButtonType: 'primary',
                        showCancelButton: false
                    });
                    loadTemplates();
                    if (onSuccess) onSuccess();
                } else {
                    await confirm({
                        title: t('deviceTemplates:confirm.deleteFailTitle'),
                        message: res.message || 'Delete failed.',
                        confirmButtonType: 'danger',
                        showCancelButton: false
                    });
                }
            } catch (err: any) {
                await confirm({
                    title: t('deviceTemplates:confirm.errorTitle'),
                    message: err.response?.data?.message || 'Error occurred.',
                    confirmButtonType: 'danger',
                    showCancelButton: false
                });
            } finally {
                setLoading(false);
            }
        }
    };

    const handleViewDetail = async (id: number) => {
        try {
            setLoading(true);
            const res = await TemplateApiService.getTemplate(id);
            if (res.success && res.data) {
                setSelectedDetailTemplate(res.data);
                setDetailModalOpen(true);
            }
        } catch (err) {
            setError(t('deviceTemplates:confirm.loadErrorMsg'));
        } finally {
            setLoading(false);
        }
    };

    const loadTemplates = useCallback(async () => {
        try {
            setLoading(true);
            const response = await TemplateApiService.getTemplates({
                search: searchTerm || undefined,
                manufacturerId: selectedManufacturer || undefined,
                protocolId: selectedProtocol || undefined,
                usageStatus: selectedUsage || undefined,
                page: currentPage,
                limit: pageSize
            });
            if (response.success && response.data) {
                const items = response.data.items || [];
                setTemplates(items);
                setTotalCount(response.data.pagination?.total_count || items.length);

                // 간단한 사용 중인 마스터 모델 수 계산 (현재 페이지 기준이 아닌 전체 데이터를 원할 경우 별도 API 필요)
                // 하지만 일단은 가져온 리스트에서 사용 중인 것이 있다면 그것으로 가이드
                setUsedCount(items.filter(t => (t.device_count || 0) > 0).length);
            }
        } catch (err) {
            setError(t('deviceTemplates:loading'));
        } finally {
            setLoading(false);
        }
    }, [searchTerm, selectedManufacturer, selectedProtocol, selectedUsage, currentPage, pageSize]);

    const loadFilterData = useCallback(async () => {
        try {
            const [mRes, pRes] = await Promise.all([
                ManufactureApiService.getManufacturers({ limit: 100 }),
                ProtocolApiService.getProtocols({ limit: 100 })
            ]);
            if (mRes.success) setManufacturers(mRes.data.items || []);
            if (pRes.success) setProtocols(pRes.data.items as any || []);
        } catch (err) {
            console.error('필터 데이터를 불러오는 중 오류 발생:', err);
        }
    }, []);

    useEffect(() => {
        loadFilterData();
    }, [loadFilterData]);

    useEffect(() => {
        loadTemplates();
    }, [loadTemplates]);

    const [viewMode, setViewMode] = useState<'card' | 'table'>('table');

    return (
        <ManagementLayout>
            <PageHeader
                title={t('deviceTemplates:title')}
                description={t('deviceTemplates:description')}
                icon="fas fa-file-invoice"
                actions={
                    <button className="mgmt-btn mgmt-btn-primary" onClick={() => {
                        setEditingTemplate(null);
                        setMasterModalOpen(true);
                    }}>
                        <i className="fas fa-plus-circle"></i> {t('deviceTemplates:register')}
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label={t('deviceTemplates:stats.totalModels')} value={totalCount} type="primary" />
                <StatCard label={t('deviceTemplates:stats.usedModels')} value={usedCount} type="success" />
                <StatCard label={t('deviceTemplates:stats.lastUpdated')} value={t('deviceTemplates:stats.today')} type="neutral" />
            </div>

            <FilterBar
                searchTerm={searchTerm}
                onSearchChange={setSearchTerm}
                onReset={() => {
                    setSearchTerm('');
                    setSelectedManufacturer('');
                    setSelectedProtocol('');
                    setSelectedUsage('');
                }}
                filters={[
                    {
                        label: t('deviceTemplates:filter.manufacturer'),
                        value: selectedManufacturer,
                        options: [
                            { label: t('deviceTemplates:filter.allManufacturers'), value: '' },
                            ...manufacturers.map(m => ({ label: m.name, value: m.id.toString() }))
                        ],
                        onChange: setSelectedManufacturer
                    },
                    {
                        label: t('deviceTemplates:filter.protocol'),
                        value: selectedProtocol,
                        options: [
                            { label: t('deviceTemplates:filter.allProtocols'), value: '' },
                            ...protocols.map(p => ({ label: p.display_name, value: p.id.toString() }))
                        ],
                        onChange: setSelectedProtocol
                    },
                    {
                        label: t('deviceTemplates:filter.usage'),
                        value: selectedUsage,
                        options: [
                            { label: t('deviceTemplates:filter.all'), value: '' },
                            { label: t('deviceTemplates:filter.used'), value: 'used' },
                            { label: t('deviceTemplates:filter.unused'), value: 'unused' }
                        ],
                        onChange: setSelectedUsage
                    }
                ]}
                activeFilterCount={(searchTerm ? 1 : 0) + (selectedManufacturer ? 1 : 0) + (selectedProtocol ? 1 : 0) + (selectedUsage ? 1 : 0)}
                rightActions={
                    <div className="mgmt-view-toggle">
                        <button
                            className={`mgmt-btn-icon ${viewMode === 'card' ? 'active' : ''}`}
                            onClick={() => setViewMode('card')}
                            title={t('deviceTemplates:cardView')}
                        >
                            <i className="fas fa-th-large"></i>
                        </button>
                        <button
                            className={`mgmt-btn-icon ${viewMode === 'table' ? 'active' : ''}`}
                            onClick={() => setViewMode('table')}
                            title={t('deviceTemplates:listView')}
                        >
                            <i className="fas fa-list"></i>
                        </button>
                    </div>
                }
            />

            <div className="mgmt-content-area">
                {viewMode === 'card' ? (
                    <div className="mgmt-grid">
                        {templates.map(template => (
                            <div key={template.id} className="mgmt-card">
                                <div className="mgmt-card-header">
                                    <div className="mgmt-card-title">
                                        <h4>{template.name}</h4>
                                        <span className="mgmt-badge">{template.manufacturer_name} / {template.model_name}</span>
                                    </div>
                                </div>
                                <div className="mgmt-card-body">
                                    <p>{template.description || t('deviceTemplates:noDescription')}</p>
                                    <div className="mgmt-card-meta">
                                        <span><i className="fas fa-microchip"></i> {template.device_type}</span>
                                        <span><i className="fas fa-list-ul"></i> {template.point_count || template.data_points?.length || 0} Points</span>
                                        <span><i className="fas fa-network-wired"></i> {template.protocol_name}</span>
                                        <span className={`mgmt-badge ${(template.device_count || 0) > 0 ? 'success' : 'neutral'}`}>
                                            {(template.device_count || 0) > 0 ? t('deviceTemplates:table.inUse') : t('deviceTemplates:table.notInUse')}
                                        </span>
                                    </div>
                                </div>
                                <div className="mgmt-card-footer">
                                    <button className="mgmt-btn mgmt-btn-outline" onClick={() => handleViewDetail(template.id)}>{t('deviceTemplates:detail')}</button>
                                    <div style={{ flex: 1 }}></div>
                                    <div className="mgmt-card-actions">
                                        <button className="mgmt-btn-icon" title={t('deviceTemplates:edit')} onClick={() => {
                                            setEditingTemplate(template);
                                            setMasterModalOpen(true);
                                        }}><i className="fas fa-edit"></i></button>
                                        <button className="mgmt-btn-icon mgmt-btn-error" title={t('deviceTemplates:delete')} onClick={() => handleDelete(template)}><i className="fas fa-trash"></i></button>
                                    </div>
                                    <button className="mgmt-btn mgmt-btn-primary" onClick={() => {
                                        setSelectedTemplateId(template.id);
                                        setWizardOpen(true);
                                    }}>{t('deviceTemplates:createDevice')}</button>
                                </div>
                            </div>
                        ))}
                    </div>
                ) : (
                    <div className="mgmt-table-container">
                        <table className="mgmt-table">
                            <thead>
                                <tr>
                                    <th>{t('deviceTemplates:table.modelName')}</th>
                                    <th>{t('deviceTemplates:table.manufacturerModel')}</th>
                                    <th>{t('deviceTemplates:table.protocol')}</th>
                                    <th>{t('deviceTemplates:table.type')}</th>
                                    <th>{t('deviceTemplates:table.pointCount')}</th>
                                    <th>{t('deviceTemplates:table.status')}</th>
                                    <th>{t('deviceTemplates:table.description')}</th>
                                    <th style={{ width: '120px' }}>{t('deviceTemplates:table.actions')}</th>
                                </tr>
                            </thead>
                            <tbody>
                                {templates.map(template => (
                                    <tr key={template.id}>
                                        <td className="fw-bold">
                                            <button
                                                className="mgmt-table-id-link"
                                                onClick={() => handleViewDetail(template.id)}
                                                style={{
                                                    padding: 0,
                                                    border: 'none',
                                                    background: 'none',
                                                    color: 'var(--neutral-900)',
                                                    cursor: 'pointer',
                                                    fontWeight: 600,
                                                    textAlign: 'left',
                                                    fontSize: 'inherit'
                                                }}
                                            >
                                                {template.name}
                                            </button>
                                        </td>
                                        <td>
                                            <div className="text-sm text-neutral-600">{template.manufacturer_name}</div>
                                            <div className="text-xs text-neutral-400">{template.model_name}</div>
                                        </td>
                                        <td><span className="mgmt-badge">{template.protocol_name}</span></td>
                                        <td>{template.device_type}</td>
                                        <td>{template.point_count || template.data_points?.length || 0}</td>
                                        <td>
                                            <span className={`mgmt-badge ${template.is_public ? 'success' : 'neutral'}`}>
                                                {template.is_public ? t('deviceTemplates:table.active') : t('deviceTemplates:table.inactive')}
                                            </span>
                                            {(template.device_count || 0) > 0 && (
                                                <div className="text-xs text-neutral-500 mt-1" style={{ fontSize: '10px' }}>
                                                    <i className="fas fa-link"></i> {t('deviceTemplates:table.devicesLinked', { count: template.device_count })}
                                                </div>
                                            )}
                                        </td>
                                        <td className="text-truncate" style={{ maxWidth: '250px' }}>
                                            {template.description || '-'}
                                        </td>
                                        <td>
                                            <div className="mgmt-card-actions">
                                                {!!template.is_public && (
                                                    <button
                                                        className="mgmt-btn-icon primary"
                                                        title={t('deviceTemplates:createDevice')}
                                                        onClick={() => {
                                                            setSelectedTemplateId(template.id);
                                                            setWizardOpen(true);
                                                        }}
                                                    >
                                                        <i className="fas fa-plus"></i>
                                                    </button>
                                                )}
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
                onChange={(page, size) => {
                    setCurrentPage(page);
                    if (size) setPageSize(size);
                }}
                onShowSizeChange={(current, size) => {
                    setCurrentPage(1);
                    setPageSize(size);
                }}
            />

            <DeviceTemplateWizard
                isOpen={wizardOpen}
                onClose={() => setWizardOpen(false)}
                onSuccess={(deviceId) => {
                    // Wizard manages success dialog internally
                    // alert(`디바이스가 성공적으로 생성되었습니다. (ID: ${deviceId})`);
                }}
                initialTemplateId={selectedTemplateId}
            />

            <DeviceTemplateDetailModal
                isOpen={detailModalOpen}
                onClose={() => setDetailModalOpen(false)}
                template={selectedDetailTemplate}
                onUseTemplate={(id) => {
                    setDetailModalOpen(false);
                    setSelectedTemplateId(id);
                    setWizardOpen(true);
                }}
                onEdit={onEditTemplate}
                onDelete={(t) => {
                    handleDelete(t, () => setDetailModalOpen(false));
                }}
            />
            <MasterModelModal
                isOpen={masterModalOpen}
                onClose={() => setMasterModalOpen(false)}
                onSuccess={() => {
                    loadTemplates();
                    notification.success({
                        message: editingTemplate ? t('deviceTemplates:notification.editSuccess') : t('deviceTemplates:notification.createSuccess'),
                        description: editingTemplate ? t('deviceTemplates:notification.editSuccessDesc') : t('deviceTemplates:notification.createSuccessDesc'),
                        placement: 'topRight'
                    });
                }}
                template={editingTemplate}
                onDelete={(t) => {
                    handleDelete(t, () => setMasterModalOpen(false));
                }}
                manufacturers={manufacturers}
                protocols={protocols}
            />
        </ManagementLayout>
    );
};

export default DeviceTemplatesPage;
