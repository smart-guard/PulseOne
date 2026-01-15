import React, { useState } from 'react';
import { createPortal } from 'react-dom';
import { DeviceTemplate } from '../../types/manufacturing';
import Pagination from '../common/Pagination';
import '../../styles/management.css';
import '../../styles/pagination.css';

interface DeviceTemplateDetailModalProps {
    isOpen: boolean;
    onClose: () => void;
    template: DeviceTemplate | null;
    onUseTemplate: (templateId: number) => void;
    onEdit?: (template: DeviceTemplate) => void;
    onDelete?: (template: DeviceTemplate) => void;
}

const DeviceTemplateDetailModal: React.FC<DeviceTemplateDetailModalProps> = ({
    isOpen,
    onClose,
    template,
    onUseTemplate,
    onEdit,
    onDelete
}) => {
    const [currentPage, setCurrentPage] = useState<number>(1);
    const [pageSize, setPageSize] = useState<number>(10);

    if (!isOpen || !template) return null;

    // Client-side pagination logic
    const dataPoints = template.data_points || [];
    const totalPoints = dataPoints.length;

    // Calculate current page data
    const offset = (currentPage - 1) * pageSize;
    const currentPoints = dataPoints.slice(offset, offset + pageSize);

    return createPortal(
        <div className="modal-overlay">
            <div className="modal-container" style={{ width: '1100px', maxWidth: '95vw', maxHeight: '90vh', display: 'flex', flexDirection: 'column' }}>
                <div className="modal-header">
                    <h3 className="modal-title">
                        <i className="fas fa-file-invoice text-primary"></i> 마스터 모델 상세 정보
                    </h3>
                    <button className="close-btn" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>

                <div className="modal-body" style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>

                    <div className="modal-form-section">
                        <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                        <div className="modal-form-grid">
                            <div className="modal-form-group">
                                <label>마스터 모델 이름</label>
                                <div className="detail-value">{template.name}</div>
                            </div>
                            <div className="modal-form-group">
                                <label>타입</label>
                                <div className="detail-value"><span className="badge">{template.device_type}</span></div>
                            </div>
                            <div className="modal-form-group">
                                <label>제조사</label>
                                <div className="detail-value">{template.manufacturer_name}</div>
                            </div>
                            <div className="modal-form-group">
                                <label>모델</label>
                                <div className="detail-value">{template.model_name}</div>
                            </div>
                            <div className="modal-form-group span-full">
                                <label>설명</label>
                                <div className="detail-value">{template.description || '-'}</div>
                            </div>
                            {template.manual_url && (
                                <div className="modal-form-group span-full">
                                    <label>참조 매뉴얼</label>
                                    <div className="detail-value">
                                        <a
                                            href={template.manual_url}
                                            target="_blank"
                                            rel="noopener noreferrer"
                                            style={{ color: '#2563eb', textDecoration: 'none', display: 'flex', alignItems: 'center', gap: '6px' }}
                                        >
                                            <i className="fas fa-book-open"></i> 제조사 매뉴얼 열기 ({template.manual_url})
                                        </a>
                                    </div>
                                </div>
                            )}
                        </div>
                    </div>

                    <div className="modal-form-section">
                        <h3><i className="fas fa-network-wired"></i> 통신 설정</h3>
                        <div className="modal-form-grid">
                            <div className="modal-form-group">
                                <label>프로토콜</label>
                                <div className="detail-value">{template.protocol_name || (template.protocol_id === 1 ? 'Modbus TCP' : template.protocol_id === 2 ? 'Modbus RTU' : 'Other')}</div>
                            </div>
                            <div className="modal-form-group">
                                <label>수집 주기</label>
                                <div className="detail-value">{template.polling_interval} ms</div>
                            </div>
                            <div className="modal-form-group">
                                <label>타임아웃</label>
                                <div className="detail-value">{template.timeout} ms</div>
                            </div>
                            <div className="modal-form-group span-full">
                                <label>기본 설정 (Config)</label>
                                <pre className="code-block">{typeof template.config === 'string' ? template.config : JSON.stringify(template.config, null, 2)}</pre>
                            </div>
                        </div>
                    </div>

                    <div className="modal-form-section">
                        <h3><i className="fas fa-list-ul"></i> 데이터 포인트 ({template.data_points?.length || 0})</h3>
                        <div className="table-container">
                            <table className="mgmt-table">
                                <thead>
                                    <tr>
                                        <th style={{ width: '200px' }}>이름</th>
                                        <th style={{ width: '140px', textAlign: 'center' }}>주소</th>
                                        <th style={{ width: '160px', textAlign: 'center' }}>타입</th>
                                        <th style={{ width: '180px', textAlign: 'center' }}>권한</th>
                                        <th>설명</th>
                                        <th style={{ width: '110px', textAlign: 'center' }}>단위</th>
                                        <th style={{ width: '100px', textAlign: 'center' }}>스케일</th>
                                        <th style={{ width: '100px', textAlign: 'center' }}>오프셋</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {currentPoints.length > 0 ? (
                                        currentPoints.map((point: any, idx: number) => (
                                            <tr key={idx}>
                                                <td className="fw-bold">{point.name}</td>
                                                <td style={{ textAlign: 'center' }}>{point.address}</td>
                                                <td style={{ textAlign: 'center' }}><span className="badge">{point.data_type}</span></td>
                                                <td style={{ textAlign: 'center' }}>{point.access_mode}</td>
                                                <td className="text-sm text-neutral-500">{point.description || '-'}</td>
                                                <td style={{ textAlign: 'center' }}>{point.unit || '-'}</td>
                                                <td style={{ textAlign: 'center' }}>{point.scaling_factor ?? 1}</td>
                                                <td style={{ textAlign: 'center' }}>{point.scaling_offset ?? 0}</td>
                                            </tr>
                                        ))
                                    ) : (
                                        <tr>
                                            <td colSpan={8} style={{ textAlign: 'center', color: '#999' }}>데이터 포인트가 없습니다.</td>
                                        </tr>
                                    )}
                                </tbody>
                            </table>
                        </div>
                        {/* Pagination */}
                        {/* Pagination */}
                        {totalPoints > 0 && (
                            <Pagination
                                current={currentPage}
                                total={totalPoints}
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
                        )}
                    </div>

                </div>

                <div className="modal-footer" style={{ padding: '16px 24px', borderTop: '1px solid #e2e8f0', display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                    {onDelete && (
                        <button className="btn-outline danger" onClick={() => onDelete(template)} title="삭제">
                            <i className="fas fa-trash"></i> 삭제
                        </button>
                    )}
                    <div style={{ flex: 1 }}></div>
                    <button className="btn-outline" onClick={onClose}>닫기</button>
                    {onEdit && (
                        <button className="btn-outline" onClick={() => onEdit(template)}>
                            <i className="fas fa-edit"></i> 수정
                        </button>
                    )}
                    <button className="btn-primary" onClick={() => onUseTemplate(template.id)}>
                        <i className="fas fa-magic"></i> 이 마스터 모델 사용
                    </button>
                </div>
            </div>

            <style>{`
                .detail-value {
                    padding: 8px 12px;
                    background: #f8fafc;
                    border-radius: 6px;
                    border: 1px solid #e2e8f0;
                    color: #334155;
                    font-size: 14px;
                    min-height: 20px;
                }
                .code-block {
                    background: #1e293b;
                    color: #e2e8f0;
                    padding: 12px;
                    border-radius: 6px;
                    font-family: monospace;
                    font-size: 12px;
                    overflow-x: auto;
                    margin: 0;
                }
            `}</style>
        </div>,
        document.body
    );
};

export default DeviceTemplateDetailModal;
