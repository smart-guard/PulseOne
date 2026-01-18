import React from 'react';
import { AlarmOccurrence, AlarmApiService } from '../../api/services/alarmApi';
import '../../styles/alarm-settings.css';
import '../../styles/notification-grid.css';
import '../../styles/management.css';

interface AlarmHistoryDetailModalProps {
    event: AlarmOccurrence | null;
    onClose: () => void;
    onAcknowledge?: (id: number) => void;
    onClear?: (id: number) => void;
}

const AlarmHistoryDetailModal: React.FC<AlarmHistoryDetailModalProps> = ({ event, onClose, onAcknowledge, onClear }) => {
    if (!event) return null;

    const occurrenceTime = event.occurrence_time || new Date().toISOString();
    const severity = (event.severity || 'LOW').toUpperCase();
    const state = (event.state || 'active').toLowerCase();

    const getSeverityBadgeClass = (s: string) => {
        switch (s?.toLowerCase()) {
            case 'critical': return 'critical';
            case 'high': return 'high';
            case 'medium': return 'medium';
            case 'low': return 'low';
            case 'info': return 'info';
            default: return 'medium';
        }
    };

    const formatValue = (value: any): string => {
        if (value === null || value === undefined) return '-';
        if (typeof value === 'number') {
            return value.toLocaleString(undefined, { maximumFractionDigits: 2 });
        }
        return String(value);
    };

    return (
        <div className="modal-overlay">
            <div className="modal modal-xl">
                <div className="modal-header">
                    <h2 className="modal-title">알람 이력 상세: #{event.id}</h2>
                    <button className="close-button" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>
                <div className="modal-content">
                    <div style={{ padding: '32px' }}>
                        <div className="modal-form-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '32px 48px' }}>

                            {/* --- Section 1: Basic Information --- */}
                            <div className="form-section">
                                <div className="section-title">기본 정보</div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">메시지</label>
                                    <div className="detail-read-value" style={{
                                        fontSize: '15px',
                                        fontWeight: 600,
                                        padding: '12px',
                                        background: 'var(--neutral-50)',
                                        borderRadius: '6px',
                                        border: '1px solid var(--neutral-100)',
                                        color: 'var(--neutral-900)',
                                        lineHeight: '1.5'
                                    }}>
                                        {event.alarm_message || '메시지 없음'}
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">심각도 / 상태</label>
                                    <div style={{ display: 'flex', gap: '8px', padding: '4px 0' }}>
                                        <span className={`badge severity-badge ${getSeverityBadgeClass(severity)}`}>
                                            {severity}
                                        </span>
                                        <span className={`badge ${state === 'active' ? 'danger' : state === 'acknowledged' ? 'warning' : 'success'}`}
                                            style={{ background: state === 'cleared' ? 'var(--success-50)' : 'var(--neutral-100)', color: state === 'cleared' ? 'var(--success-700)' : 'var(--neutral-700)' }}>
                                            {AlarmApiService.getStateDisplayText(state)}
                                        </span>
                                    </div>
                                </div>
                                <div className="detail-item-group">
                                    <label className="form-label">디바이스 / 규칙</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        <div style={{ fontWeight: 600 }}>{event.device_name || 'N/A'}</div>
                                        <div style={{ fontSize: '13px', color: 'var(--neutral-500)', marginTop: '2px' }}>{event.rule_name || 'N/A'}</div>
                                    </div>
                                </div>
                            </div>

                            {/* --- Section 2: Values & Timing --- */}
                            <div className="form-section">
                                <div className="section-title">발생 정보</div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">트리거 값</label>
                                    <div className="detail-read-value" style={{
                                        fontSize: '18px',
                                        fontWeight: 700,
                                        color: 'var(--primary-600)',
                                        padding: '8px 12px',
                                        background: 'var(--primary-50)',
                                        borderRadius: '6px',
                                        border: '1px solid var(--primary-100)',
                                        display: 'inline-block'
                                    }}>
                                        {formatValue(event.trigger_value)}
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">발생 시간</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        {new Date(occurrenceTime).toLocaleString('ko-KR')}
                                    </div>
                                </div>
                                <div className="detail-item-group">
                                    <label className="form-label">데이터포인트</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        {event.data_point_name || '-'}
                                    </div>
                                </div>
                            </div>

                            {/* --- Section 3: Action History (Full Width) --- */}
                            <div className="form-section" style={{ gridColumn: '1 / -1' }}>
                                <div className="section-title">조치 이력</div>
                                <div className="timeline-detail" style={{ marginTop: '16px' }}>
                                    {/* 발생 */}
                                    <div className="timeline-item">
                                        <div className="timeline-marker triggered"></div>
                                        <div className="timeline-content">
                                            <div className="timeline-header">
                                                <span className="action-label" style={{ fontWeight: 600 }}>발생 (Triggered)</span>
                                                <span className="timestamp" style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{new Date(occurrenceTime).toLocaleString('ko-KR')}</span>
                                            </div>
                                            <div style={{ marginTop: '4px', fontSize: '13px', color: 'var(--neutral-700)' }}>
                                                알람이 최초 발생했습니다. (값: {formatValue(event.trigger_value)})
                                            </div>
                                        </div>
                                    </div>

                                    {/* 확인 */}
                                    {event.acknowledged_time && (
                                        <div className="timeline-item">
                                            <div className="timeline-marker acknowledged"></div>
                                            <div className="timeline-content">
                                                <div className="timeline-header">
                                                    <span className="action-label" style={{ fontWeight: 600 }}>확인 (Acknowledged)</span>
                                                    <span className="timestamp" style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{new Date(event.acknowledged_time).toLocaleString('ko-KR')}</span>
                                                </div>
                                                <div style={{ marginTop: '4px', fontSize: '13px' }}>
                                                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                                        <i className="fas fa-user-check" style={{ color: 'var(--neutral-400)' }}></i>
                                                        <span style={{ fontWeight: 500 }}>{event.acknowledged_by || 'Unknown User'}</span>
                                                    </div>
                                                    {event.acknowledge_comment && (
                                                        <div style={{ marginTop: '4px', padding: '8px', background: 'var(--warning-50)', borderRadius: '4px', border: '1px solid var(--warning-100)', color: 'var(--warning-900)' }}>
                                                            "{event.acknowledge_comment}"
                                                        </div>
                                                    )}
                                                </div>
                                            </div>
                                        </div>
                                    )}

                                    {/* 해제 */}
                                    {event.cleared_time && (
                                        <div className="timeline-item">
                                            <div className="timeline-marker cleared"></div>
                                            <div className="timeline-content">
                                                <div className="timeline-header">
                                                    <span className="action-label" style={{ fontWeight: 600 }}>해제 (Cleared)</span>
                                                    <span className="timestamp" style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{new Date(event.cleared_time).toLocaleString('ko-KR')}</span>
                                                </div>
                                                <div style={{ marginTop: '4px', fontSize: '13px' }}>
                                                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                                        <i className="fas fa-check-double" style={{ color: 'var(--neutral-400)' }}></i>
                                                        <span style={{ fontWeight: 500 }}>{event.cleared_by || 'System/Unknown'}</span>
                                                    </div>
                                                    {event.clear_comment && (
                                                        <div style={{ marginTop: '4px', padding: '8px', background: 'var(--success-50)', borderRadius: '4px', border: '1px solid var(--success-100)', color: 'var(--success-900)' }}>
                                                            "{event.clear_comment}"
                                                        </div>
                                                    )}
                                                </div>
                                            </div>
                                        </div>
                                    )}
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
                <div className="modal-footer" style={{ borderTop: '1px solid var(--neutral-100)', padding: '20px 32px' }}>
                    <div className="footer-left"></div>
                    <div className="footer-right" style={{ display: 'flex', gap: '12px' }}>
                        <button type="button" className="btn btn-secondary" style={{ minWidth: '100px' }} onClick={onClose}>닫기</button>
                        {state === 'active' && onAcknowledge && (
                            <button type="button" className="btn btn-warning" style={{ minWidth: '100px', background: 'var(--warning-500)', color: 'white' }} onClick={() => onAcknowledge(event.id)}>
                                <i className="fas fa-check"></i> 확인
                            </button>
                        )}
                        {state === 'acknowledged' && onClear && (
                            <button type="button" className="btn btn-success" style={{ minWidth: '100px', background: 'var(--success-500)', color: 'white' }} onClick={() => onClear(event.id)}>
                                <i className="fas fa-times"></i> 해제
                            </button>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default AlarmHistoryDetailModal;
