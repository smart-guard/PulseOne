import React, { useState, useEffect, useCallback } from 'react';
import { TemplateApiService } from '../api/services/templateApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
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

    const handleDelete = async (template: DeviceTemplate) => {
        if ((template.device_count || 0) > 0) {
            alert('사용 중인 마스터 모델은 삭제할 수 없습니다.');
            return;
        }

        if (window.confirm(`"${template.name}" 모델을 삭제하시겠습니까?`)) {
            try {
                setLoading(true);
                const res = await TemplateApiService.deleteTemplate(template.id);
                if (res.success) {
                    alert('삭제되었습니다.');
                    loadTemplates();
                } else {
                    alert(res.message || '삭제에 실패했습니다.');
                }
            } catch (err) {
                alert('삭제 중 오류가 발생했습니다.');
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
            setError('마스터 모델 상세 정보를 불러오는 중 오류가 발생했습니다.');
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
            setError('마스터 모델을 불러오는 중 오류가 발생했습니다.');
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
                title="디바이스 마스터 모델"
                description="디바이스 모델을 등록하고 관리합니다. 등록된 모델을 기반으로 신규 디바이스를 신속하게 생성할 수 있습니다."
                icon="fas fa-file-invoice"
                actions={
                    <button className="btn-primary" onClick={() => {
                        setEditingTemplate(null);
                        setMasterModalOpen(true);
                    }}>
                        <i className="fas fa-plus-circle"></i> 디바이스 모델 등록
                    </button>
                }
            />

            <div className="mgmt-stats-panel">
                <StatCard label="전체 마스터 모델" value={totalCount} type="primary" />
                <StatCard label="사용 중인 마스터 모델 (검색 결과)" value={usedCount} type="success" />
                <StatCard label="최근 업데이트" value="오늘" type="neutral" />
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
                        label: '제조사',
                        value: selectedManufacturer,
                        options: [
                            { label: '전체 제조사', value: '' },
                            ...manufacturers.map(m => ({ label: m.name, value: m.id.toString() }))
                        ],
                        onChange: setSelectedManufacturer
                    },
                    {
                        label: '프로토콜',
                        value: selectedProtocol,
                        options: [
                            { label: '전체 프로토콜', value: '' },
                            ...protocols.map(p => ({ label: p.display_name, value: p.id.toString() }))
                        ],
                        onChange: setSelectedProtocol
                    },
                    {
                        label: '사용 여부',
                        value: selectedUsage,
                        options: [
                            { label: '전체 보기', value: '' },
                            { label: '사용 중', value: 'used' },
                            { label: '미사용', value: 'unused' }
                        ],
                        onChange: setSelectedUsage
                    }
                ]}
                activeFilterCount={(searchTerm ? 1 : 0) + (selectedManufacturer ? 1 : 0) + (selectedProtocol ? 1 : 0) + (selectedUsage ? 1 : 0)}
                rightActions={
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
                }
            />

            <div className="mgmt-content-area">
                {viewMode === 'card' ? (
                    <div className="mgmt-grid">
                        {templates.map(template => (
                            <div key={template.id} className="mgmt-card">
                                <div className="card-header">
                                    <div className="card-title">
                                        <h4>{template.name}</h4>
                                        <span className="badge">{template.manufacturer_name} / {template.model_name}</span>
                                    </div>
                                </div>
                                <div className="card-body">
                                    <p>{template.description || '설명이 없습니다.'}</p>
                                    <div className="card-meta">
                                        <span><i className="fas fa-microchip"></i> {template.device_type}</span>
                                        <span><i className="fas fa-list-ul"></i> {template.point_count || template.data_points?.length || 0} Points</span>
                                        <span><i className="fas fa-network-wired"></i> {template.protocol_name}</span>
                                        <span className={`badge ${(template.device_count || 0) > 0 ? 'success' : 'neutral'}`}>
                                            {(template.device_count || 0) > 0 ? '사용 중' : '미사용'}
                                        </span>
                                    </div>
                                </div>
                                <div className="card-footer">
                                    <button className="btn-outline" onClick={() => handleViewDetail(template.id)}>상세보기</button>
                                    <div style={{ flex: 1 }}></div>
                                    <div className="action-buttons">
                                        <button className="btn-icon" title="수정" onClick={() => {
                                            setEditingTemplate(template);
                                            setMasterModalOpen(true);
                                        }}><i className="fas fa-edit"></i></button>
                                        <button className="btn-icon danger" title="삭제" onClick={() => handleDelete(template)}><i className="fas fa-trash"></i></button>
                                    </div>
                                    <button className="btn-primary" onClick={() => {
                                        setSelectedTemplateId(template.id);
                                        setWizardOpen(true);
                                    }}>디바이스 생성</button>
                                </div>
                            </div>
                        ))}
                    </div>
                ) : (
                    <div className="mgmt-table-wrapper">
                        <table className="mgmt-table">
                            <thead>
                                <tr>
                                    <th>마스터 모델명</th>
                                    <th>제조사 / 모델</th>
                                    <th>프로토콜</th>
                                    <th>타입</th>
                                    <th>포인트 수</th>
                                    <th>마스터 모델 상태</th>
                                    <th>설명</th>
                                    <th style={{ width: '120px' }}>작업</th>
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
                                        <td><span className="badge">{template.protocol_name}</span></td>
                                        <td>{template.device_type}</td>
                                        <td>{template.point_count || template.data_points?.length || 0}</td>
                                        <td>
                                            <span className={`badge ${template.is_public ? 'success' : 'neutral'}`}>
                                                {template.is_public ? '활성 (준비됨)' : '비활성 (연구용)'}
                                            </span>
                                            {(template.device_count || 0) > 0 && (
                                                <div className="text-xs text-neutral-500 mt-1" style={{ fontSize: '10px' }}>
                                                    <i className="fas fa-link"></i> {template.device_count}개 디바이스 연동됨
                                                </div>
                                            )}
                                        </td>
                                        <td className="text-truncate" style={{ maxWidth: '250px' }}>
                                            {template.description || '-'}
                                        </td>
                                        <td>
                                            <div className="action-buttons">
                                                {!!template.is_public && (
                                                    <button
                                                        className="btn-icon primary"
                                                        title="디바이스 생성"
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
                    setDetailModalOpen(false);
                    handleDelete(t);
                }}
            />
            <MasterModelModal
                isOpen={masterModalOpen}
                onClose={() => setMasterModalOpen(false)}
                onSuccess={() => {
                    loadTemplates();
                    alert(editingTemplate ? '수정이 완료되었습니다.' : '등록이 완료되었습니다.');
                }}
                template={editingTemplate}
                onDelete={(t) => {
                    setMasterModalOpen(false);
                    handleDelete(t);
                }}
                manufacturers={manufacturers}
                protocols={protocols}
            />
        </ManagementLayout>
    );
};

export default DeviceTemplatesPage;
