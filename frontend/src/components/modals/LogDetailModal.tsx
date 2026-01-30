import React, { useMemo } from 'react';
import { ExportLog } from '../../api/services/exportGatewayApi';
import '../../styles/alarm-settings.css'; // Reusing for modal structure
import '../../styles/management.css';

interface LogDetailModalProps {
    log: ExportLog | null;
    onClose: () => void;
}

const LogDetailModal: React.FC<LogDetailModalProps> = ({ log, onClose }) => {
    if (!log) return null;

    const getStatusBadgeClass = (status: string) => {
        const s = (status || '').toLowerCase();
        if (s === 'success') return 'status-pill success';
        if (s === 'failure') return 'status-pill error';
        return 'status-pill neutral';
    };

    const formatPayload = (value: string | undefined | null) => {
        if (!value) return '-';
        try {
            // Try to parse as JSON and pretty print
            const json = JSON.parse(value);
            return JSON.stringify(json, null, 2);
        } catch (e) {
            // Return as is if not valid JSON
            return value;
        }
    };

    const formattedSource = useMemo(() => formatPayload(log.source_value), [log.source_value]);
    const formattedResponse = useMemo(() => formatPayload(log.response_data), [log.response_data]);

    return (
        <div className="modal-overlay">
            <div className="modal modal-xl">
                <div className="modal-header">
                    <h2 className="modal-title">
                        <i className="fas fa-history mr-2" style={{ color: 'var(--primary-600)' }}></i>
                        전송 이력 상세: #{log.id}
                    </h2>
                    <button className="close-button" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>
                <div className="modal-content">
                    <div style={{ padding: '32px' }}>
                        <div className="modal-form-grid" style={{ display: 'grid', gridTemplateColumns: 'minmax(300px, 1fr) 2fr', gap: '32px' }}>

                            {/* Left Column: Meta Info */}
                            <div className="form-section">
                                <div className="section-title">기본 정보</div>

                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">상태 / 발생 시간</label>
                                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        <span className={getStatusBadgeClass(log.status)}>
                                            {(log.status || 'UNKNOWN').toUpperCase()}
                                        </span>
                                        <span className="text-neutral-600 font-medium">
                                            {new Date(log.timestamp).toLocaleString('ko-KR')}
                                        </span>
                                    </div>
                                </div>

                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">타겟 시스템</label>
                                    <div className="detail-read-value" style={{ padding: '12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        <div className="font-bold text-neutral-800" style={{ fontSize: '15px' }}>{log.target_name || '-'}</div>
                                        {log.target_id && <div className="text-xs text-neutral-500 mt-1">ID: {log.target_id}</div>}
                                    </div>
                                </div>

                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">프로필 / 게이트웨이</label>
                                    <div className="detail-read-value" style={{ padding: '12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        <div className="flex items-center mb-2">
                                            <i className="fas fa-file-alt text-primary-500 mr-2 w-5 text-center"></i>
                                            <span className="font-medium text-neutral-800">{log.profile_name || '-'}</span>
                                        </div>
                                        <div className="flex items-center">
                                            <i className="fas fa-server text-neutral-500 mr-2 w-5 text-center"></i>
                                            <span className="text-sm text-neutral-600">{log.gateway_name || '-'}</span>
                                        </div>
                                    </div>
                                </div>

                                {log.status === 'failure' && (log.error_message || log.error_code) && (
                                    <div className="detail-item-group">
                                        <label className="form-label text-error-600">오류 정보</label>
                                        <div className="detail-read-value" style={{ padding: '12px', background: '#fef2f2', borderRadius: '6px', border: '1px solid #fecaca' }}>
                                            {log.error_code && (
                                                <div className="font-mono text-xs text-error-800 mb-1 font-bold">Code: {log.error_code}</div>
                                            )}
                                            <div className="text-sm text-error-700">{log.error_message || '오류 메시지 없음'}</div>
                                            {log.http_status_code > 0 && (
                                                <div className="mt-2 text-xs font-mono text-error-600">HTTP Status: {log.http_status_code}</div>
                                            )}
                                        </div>
                                    </div>
                                )}
                            </div>

                            {/* Right Column: Payloads */}
                            <div className="form-section">
                                <div className="section-title">데이터 상세</div>

                                <div className="detail-item-group" style={{ marginBottom: '24px' }}>
                                    <label className="form-label">전송 데이터 (Source)</label>
                                    <div className="code-block-wrapper" style={{ position: 'relative' }}>
                                        <pre style={{
                                            maxHeight: '300px',
                                            overflow: 'auto',
                                            background: '#1e293b',
                                            color: '#e2e8f0',
                                            padding: '16px',
                                            borderRadius: '8px',
                                            fontSize: '13px',
                                            fontFamily: 'Menlo, Monaco, Consolas, "Courier New", monospace',
                                            lineHeight: '1.5',
                                            whiteSpace: 'pre-wrap',
                                            wordBreak: 'break-all'
                                        }}>
                                            {formattedSource}
                                        </pre>
                                    </div>
                                </div>

                                <div className="detail-item-group">
                                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                                        <label className="form-label" style={{ marginBottom: 0 }}>응답 데이터 (Response)</label>
                                        <span style={{
                                            fontSize: '12px',
                                            fontFamily: 'monospace',
                                            padding: '2px 8px',
                                            borderRadius: '4px',
                                            background: (log.http_status_code && log.http_status_code >= 200 && log.http_status_code < 300) ? '#dcfce7' : '#f1f5f9',
                                            color: (log.http_status_code && log.http_status_code >= 200 && log.http_status_code < 300) ? '#166534' : '#64748b',
                                            border: `1px solid ${(log.http_status_code && log.http_status_code >= 200 && log.http_status_code < 300) ? '#bbf7d0' : '#cbd5e1'}`,
                                            fontWeight: 600,
                                            display: 'inline-block',
                                            minWidth: '60px',
                                            textAlign: 'center'
                                        }}>
                                            {/* Explicitly show '0' if the value is 0, or '-' if undefined/null */}
                                            STATUS {(log.http_status_code !== undefined && log.http_status_code !== null) ? log.http_status_code : '-'}
                                        </span>
                                    </div>
                                    <div className="code-block-wrapper">
                                        <pre style={{
                                            maxHeight: '200px',
                                            overflow: 'auto',
                                            background: '#f8fafc',
                                            color: '#334155',
                                            padding: '16px',
                                            borderRadius: '8px',
                                            fontSize: '13px',
                                            fontFamily: 'Menlo, Monaco, Consolas, "Courier New", monospace',
                                            border: '1px solid #e2e8f0',
                                            lineHeight: '1.5',
                                            whiteSpace: 'pre-wrap',
                                            wordBreak: 'break-all'
                                        }}>
                                            {formattedResponse}
                                        </pre>
                                    </div>
                                </div>
                            </div>

                        </div>
                    </div>
                </div>
                <div className="modal-footer" style={{ borderTop: '1px solid var(--neutral-100)', padding: '20px 32px', display: 'flex', justifyContent: 'flex-end' }}>
                    <div className="footer-right">
                        <button type="button" className="btn btn-secondary" style={{ minWidth: '100px' }} onClick={onClose}>닫기</button>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default LogDetailModal;
