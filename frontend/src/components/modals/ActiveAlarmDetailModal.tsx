import React from 'react';
import '../../styles/alarm-settings.css';
import '../../styles/notification-grid.css';
import '../../styles/management.css';

// Interface matching the one in ActiveAlarms.tsx
export interface ActiveAlarmDetail {
    id: number;
    rule_name: string;
    device_name?: string;
    severity: 'low' | 'medium' | 'high' | 'critical';
    message: string;
    triggered_at: string;
    state: 'active' | 'acknowledged' | 'cleared';
    is_new?: boolean;
    comment?: string;
    acknowledged_time?: string;
    acknowledged_by?: string | number;
    acknowledged_by_name?: string;
    acknowledged_by_company?: string;
    cleared_time?: string;
    cleared_by?: string | number;
    cleared_by_name?: string;
    cleared_by_company?: string;
    trigger_value?: any;
    rule_id?: number;
}

interface ActiveAlarmDetailModalProps {
    alarm: ActiveAlarmDetail | null;
    onClose: () => void;
    onAcknowledge?: (id: number) => void;
    onClear?: (id: number) => void;
}

const ActiveAlarmDetailModal: React.FC<ActiveAlarmDetailModalProps> = ({ alarm, onClose, onAcknowledge, onClear }) => {
    if (!alarm) return null;

    const severity = (alarm.severity || 'low').toUpperCase();
    const state = (alarm.state || 'active').toLowerCase();

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

    const getStatusText = (s: string) => {
        switch (s) {
            case 'active': return '확인 대기 (Active)';
            case 'acknowledged': return '확인됨 (Acknowledged)';
            case 'cleared': return '해제됨 (Cleared)';
            default: return s;
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
                    <h2 className="modal-title">활성 알람 상세 정보: #{alarm.id}</h2>
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
                                        {alarm.message || '메시지 없음'}
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">심각도 / 상태</label>
                                    <div style={{ display: 'flex', gap: '8px', padding: '4px 0' }}>
                                        <span className={`badge severity-badge ${getSeverityBadgeClass(severity)}`}>
                                            {severity}
                                        </span>
                                        <div className={`status-indicator ${state === 'active' ? 'active-pulse' : ''}`} style={{
                                            display: 'flex', alignItems: 'center', gap: '6px',
                                            color: state === 'active' ? 'var(--danger-600)' : 'var(--warning-600)',
                                            fontWeight: 600, fontSize: '12px'
                                        }}>
                                            <i className={`fas ${state === 'active' ? 'fa-exclamation-circle' : 'fa-check'}`}></i>
                                            {getStatusText(state)}
                                        </div>
                                    </div>
                                </div>
                                <div className="detail-item-group">
                                    <label className="form-label">디바이스 / 규칙</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        <div style={{ fontWeight: 600 }}>{alarm.device_name || 'N/A'}</div>
                                        <div style={{ fontSize: '13px', color: 'var(--neutral-500)', marginTop: '2px' }}>{alarm.rule_name || 'N/A'}</div>
                                    </div>
                                </div>
                            </div>

                            {/* --- Section 2: Real-time Status --- */}
                            <div className="form-section">
                                <div className="section-title">실시간 상태</div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">현재 값 (Trigger Value)</label>
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
                                        {formatValue(alarm.trigger_value)}
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">발생 시간</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        {new Date(alarm.triggered_at).toLocaleString('ko-KR')}
                                    </div>
                                </div>
                                <div className="detail-item-group">
                                    <label className="form-label">담당자 (확인/해제)</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>
                                        {alarm.acknowledged_by ? (
                                            <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                                <i className="fas fa-user-circle"></i>
                                                {alarm.acknowledged_by_name || alarm.acknowledged_by}
                                            </span>
                                        ) : '-'}
                                    </div>
                                </div>
                            </div>

                            {/* --- Section 3: Incident Timeline (Full Width) --- */}
                            <div className="form-section" style={{ gridColumn: '1 / -1' }}>
                                <div className="section-title">사고 타임라인</div>
                                <div className="timeline-detail" style={{ marginTop: '16px' }}>
                                    {/* 발생 */}
                                    <div className="timeline-item">
                                        <div className="timeline-marker triggered"></div>
                                        <div className="timeline-content">
                                            <div className="timeline-header">
                                                <span className="action-label" style={{ fontWeight: 600 }}>발생 (Triggered)</span>
                                                <span className="timestamp" style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{new Date(alarm.triggered_at).toLocaleString('ko-KR')}</span>
                                            </div>
                                            <div style={{ marginTop: '4px', fontSize: '13px', color: 'var(--neutral-700)' }}>
                                                알람이 발생했습니다.
                                            </div>
                                        </div>
                                    </div>

                                    {/* 확인 */}
                                    {alarm.acknowledged_time && (
                                        <div className="timeline-item">
                                            <div className="timeline-marker acknowledged"></div>
                                            <div className="timeline-content">
                                                <div className="timeline-header">
                                                    <span className="action-label" style={{ fontWeight: 600 }}>확인 (Acknowledged)</span>
                                                    <span className="timestamp" style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{new Date(alarm.acknowledged_time).toLocaleString('ko-KR')}</span>
                                                </div>
                                                <div style={{ marginTop: '4px', fontSize: '13px' }}>
                                                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                                        <i className="fas fa-user-check" style={{ color: 'var(--neutral-400)' }}></i>
                                                        <span style={{ fontWeight: 500 }}>{alarm.acknowledged_by_name || alarm.acknowledged_by || 'Unknown'}</span>
                                                    </div>
                                                    {alarm.comment && alarm.state !== 'cleared' && (
                                                        <div style={{ marginTop: '4px', padding: '8px', background: 'var(--warning-50)', borderRadius: '4px', border: '1px solid var(--warning-100)', color: 'var(--warning-900)' }}>
                                                            "{alarm.comment}"
                                                        </div>
                                                    )}
                                                </div>
                                            </div>
                                        </div>
                                    )}

                                    {/* 해제 */}
                                    {alarm.cleared_time && (
                                        <div className="timeline-item">
                                            <div className="timeline-marker cleared"></div>
                                            <div className="timeline-content">
                                                <div className="timeline-header">
                                                    <span className="action-label" style={{ fontWeight: 600 }}>해제 (Cleared)</span>
                                                    <span className="timestamp" style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{new Date(alarm.cleared_time).toLocaleString('ko-KR')}</span>
                                                </div>
                                                <div style={{ marginTop: '4px', fontSize: '13px' }}>
                                                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                                        <i className="fas fa-check-double" style={{ color: 'var(--neutral-400)' }}></i>
                                                        <span style={{ fontWeight: 500 }}>{alarm.cleared_by_name || alarm.cleared_by || 'Unknown'}</span>
                                                    </div>
                                                    {alarm.comment && alarm.state === 'cleared' && (
                                                        <div style={{ marginTop: '4px', padding: '8px', background: 'var(--success-50)', borderRadius: '4px', border: '1px solid var(--success-100)', color: 'var(--success-900)' }}>
                                                            "{alarm.comment}"
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
                            <button type="button" className="btn btn-warning" style={{ minWidth: '100px', background: 'var(--warning-500)', color: 'white' }} onClick={() => onAcknowledge(alarm.id)}>
                                <i className="fas fa-check"></i> 확인
                            </button>
                        )}
                        {state === 'acknowledged' && onClear && (
                            <button type="button" className="btn btn-success" style={{ minWidth: '100px', background: 'var(--success-500)', color: 'white' }} onClick={() => onClear(alarm.id)}>
                                <i className="fas fa-times"></i> 해제
                            </button>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default ActiveAlarmDetailModal;
