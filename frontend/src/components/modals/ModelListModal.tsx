import React, { useState, useEffect, useCallback } from 'react';
import { createPortal } from 'react-dom';
import { ManufactureApiService } from '../../api/services/manufactureApi';
import { TemplateApiService } from '../../api/services/templateApi';
import { Manufacturer, DeviceModel, DeviceTemplate } from '../../types/manufacturing';
import { Pagination } from '../common/Pagination';
import DeviceTemplateDetailModal from './DeviceTemplateDetailModal';
import '../../styles/management.css';
import '../../styles/pagination.css';

interface ModelListModalProps {
    isOpen: boolean;
    onClose: () => void;
    manufacturer: Manufacturer | null;
}

export const ModelListModal: React.FC<ModelListModalProps> = ({ isOpen, onClose, manufacturer }) => {
    const [models, setModels] = useState<DeviceModel[]>([]);
    const [loading, setLoading] = useState(false);
    const [currentPage, setCurrentPage] = useState(1);
    const [pageSize, setPageSize] = useState(6);
    const [totalCount, setTotalCount] = useState(0);
    const [detailModalOpen, setDetailModalOpen] = useState(false);
    const [selectedTemplate, setSelectedTemplate] = useState<DeviceTemplate | null>(null);

    const loadModels = useCallback(async () => {
        if (!manufacturer) return;
        try {
            setLoading(true);
            const res = await ManufactureApiService.getModelsByManufacturer(manufacturer.id, {
                page: currentPage,
                limit: pageSize
            });
            if (res.success && res.data) {
                setModels(res.data.items || []);
                // backend returned total_count via BaseRepository update
                setTotalCount(res.data.pagination?.total_count || 0);
            }
        } catch (err) {
            console.error('Failed to load models:', err);
        } finally {
            setLoading(false);
        }
    }, [manufacturer, currentPage, pageSize]);

    const handleViewDetail = async (templateId: number) => {
        try {
            setLoading(true);
            const res = await TemplateApiService.getTemplate(templateId);
            if (res.success && res.data) {
                setSelectedTemplate(res.data);
                setDetailModalOpen(true);
            }
        } catch (err) {
            console.error('Failed to load template detail:', err);
            alert('상세 정보를 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        if (isOpen && manufacturer) {
            loadModels();
        } else {
            setModels([]);
            setCurrentPage(1);
        }
    }, [isOpen, manufacturer, loadModels]);

    if (!isOpen || !manufacturer) return null;

    return createPortal(
        <div className="modal-overlay" onClick={onClose}>
            <div
                className="modal-container compact-modal"
                onClick={e => e.stopPropagation()}
            >
                <div className="modal-header">
                    <h3>{manufacturer.name} 모델 목록</h3>
                    <button className="close-btn" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>

                <div className="modal-body" style={{ padding: '16px 20px', minHeight: '320px', display: 'flex', flexDirection: 'column' }}>
                    {loading ? (
                        <div style={{ textAlign: 'center', padding: '60px 0', color: '#64748b', flex: 1 }}>
                            <i className="fas fa-spinner fa-spin fa-2x"></i>
                            <p style={{ marginTop: '12px' }}>모델 정보를 불러오는 중...</p>
                        </div>
                    ) : models.length === 0 ? (
                        <div style={{ textAlign: 'center', padding: '60px 0', color: '#94a3b8', flex: 1 }}>
                            <i className="fas fa-box-open fa-3x" style={{ marginBottom: '16px', opacity: 0.5 }}></i>
                            <p>등록된 모델이 없습니다.</p>
                        </div>
                    ) : (
                        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '16px' }}>
                            <div className="model-list" style={{ flex: 1 }}>
                                {models.map(model => (
                                    <div key={model.id} className="model-item" style={{
                                        padding: '10px 14px',
                                        display: 'flex',
                                        justifyContent: 'space-between',
                                        alignItems: 'center',
                                        backgroundColor: '#f8fafc',
                                        marginBottom: '10px',
                                        borderRadius: '8px',
                                        border: '1px solid #e2e8f0',
                                        boxShadow: '0 1px 2px rgba(0,0,0,0.05)'
                                    }}>
                                        <div style={{ flex: 1, overflow: 'hidden' }}>
                                            <div style={{ fontWeight: '600', fontSize: '13px', color: '#1e293b', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>{model.name}</div>
                                            <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>
                                                <span style={{
                                                    display: 'inline-block',
                                                    padding: '1px 6px',
                                                    background: '#e2e8f0',
                                                    borderRadius: '4px',
                                                    fontSize: '10px',
                                                    fontWeight: '700',
                                                    color: '#475569'
                                                }}>
                                                    {model.device_type}
                                                </span>
                                            </div>
                                        </div>
                                        <button
                                            className="btn-outline btn-sm"
                                            style={{ height: '28px', fontSize: '11px', minWidth: '54px', padding: '0 8px', marginLeft: '12px' }}
                                            onClick={() => {
                                                if ((model as any).template_id) {
                                                    handleViewDetail((model as any).template_id);
                                                } else {
                                                    alert('해당 모델에 등록된 마스터 모델이 없습니다.');
                                                }
                                            }}
                                        >
                                            상세
                                        </button>
                                    </div>
                                ))}
                            </div>

                            <div className="modal-pagination" style={{ marginTop: 'auto', paddingTop: '16px', borderTop: '1px solid #f1f5f9', display: 'flex', justifyContent: 'center' }}>
                                <Pagination
                                    current={currentPage}
                                    total={totalCount}
                                    pageSize={pageSize}
                                    onChange={(page) => setCurrentPage(page)}
                                    showSizeChanger={false}
                                />
                            </div>
                        </div>
                    )}
                </div>

                <div className="modal-footer">
                    <button className="btn-secondary" onClick={onClose}>닫기</button>
                </div>

                <DeviceTemplateDetailModal
                    isOpen={detailModalOpen}
                    onClose={() => setDetailModalOpen(false)}
                    template={selectedTemplate}
                    onUseTemplate={(id) => {
                        setDetailModalOpen(false);
                        // Wizard handling could go here if needed, but for now just showing detail is progress
                        alert('마스터 모델 사용 기능은 디바이스 마스터 모델 페이지에서 이용해 주세요.');
                    }}
                />
            </div>

            <style>{`
                .model-item:last-child { margin-bottom: 0; }
                .model-item:hover { 
                    border-color: var(--primary-400) !important; 
                    background-color: var(--primary-50) !important;
                    transform: translateY(-1px);
                    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
                }
                .btn-secondary {
                    background: #f1f5f9;
                    border: 1px solid #e2e8f0;
                    color: #475569;
                    padding: 8px 16px;
                    border-radius: 6px;
                    font-weight: 600;
                    font-size: 13px;
                    cursor: pointer;
                    transition: all 0.2s;
                }
                .btn-secondary:hover {
                    background: #e2e8f0;
                    color: #1e293b;
                }
                /* 모달 내 페이징 스타일 조정 */
                .modal-pagination .pagination-wrapper {
                    padding: 0;
                    gap: 8px;
                    width: 100%;
                    justify-content: center;
                }
                .modal-pagination .pagination-info {
                    font-size: 11px;
                }
                .modal-pagination .pagination-button {
                    width: 28px;
                    height: 28px;
                    font-size: 11px;
                }
                .modal-pagination .pagination-navigation {
                    gap: 4px;
                }
            `}</style>
        </div>,
        document.body
    );
};
