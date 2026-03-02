import React from 'react';
import { useTranslation } from 'react-i18next';
import { AlarmRule } from '../../api/services/alarmApi';
import '../../styles/alarm-settings.css';
import '../../styles/notification-grid.css';
import '../../styles/script-patterns.css';
import '../../styles/limits-grid.css';

interface AlarmDetailModalProps {
    rule: AlarmRule | null;
    onClose: () => void;
    onEdit: (rule: AlarmRule) => void;
}

const CATEGORY_DISPLAY_NAMES: Record<string, string> = {
    'temperature': 'Temperature', 'pressure': 'Pressure', 'flow': 'Flow Rate', 'level': 'Level', 'vibration': 'Vibration', 'electrical': 'Electrical', 'safety': 'Safety', 'general': 'General'
};

const SCRIPT_PATTERNS = [
    { id: 'threshold_above', label: 'Upper Threshold', icon: '📈' },
    { id: 'threshold_below', label: 'Lower Threshold', icon: '📉' },
    { id: 'moving_avg', label: 'Moving Average', icon: '📊' },
    { id: 'hysteresis', label: 'Hysteresis', icon: '➰' },
    { id: 'rate_of_change', label: 'Rate of Change', icon: '⚡' }
];

const AlarmDetailModal: React.FC<AlarmDetailModalProps> = ({ rule, onClose, onEdit }) => {
    const { t } = useTranslation(['alarms', 'common']);
    if (!rule) return null;

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

    const getTargetDisplay = () => {
        if (rule.target_type === 'device') return rule.device_name || `Device #${rule.target_id}`;
        // Prefer data_point_name provided by backend join
        return (rule as any).data_point_name || rule.target_display || `DataPoint #${rule.target_id}`;
    };

    const renderSentencePills = (pills: { text: string; highlight?: boolean }[]) => (
        <div className="sentence-content" style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
            {pills.map((pill, idx) => (
                <span key={idx} className={`sentence-pill ${pill.highlight ? 'highlight' : ''}`} style={{
                    background: pill.highlight ? 'var(--primary-600)' : 'transparent',
                    padding: pill.highlight ? '2px 6px' : '0',
                    borderRadius: '4px',
                    color: '#fff'
                }}>{pill.text}</span>
            ))}
        </div>
    );

    const generateSentence = () => {
        const targetName = getTargetDisplay();
        const pills: { text: string; highlight?: boolean }[] = [];
        pills.push({ text: "If" });
        pills.push({ text: `[${targetName}]`, highlight: true });
        pills.push({ text: "of" });
        pills.push({ text: rule.alarm_type === 'analog' ? 'Analog' : 'State', highlight: true });
        pills.push({ text: "value" });

        if (rule.alarm_type === 'analog') {
            const val = rule.high_limit || rule.trigger_condition || "...";
            pills.push({ text: `[${val}]`, highlight: true });
        } else {
            pills.push({ text: `[${rule.trigger_condition || '...'}]`, highlight: true });
        }

        pills.push({ text: "triggers an alarm." });
        return pills;
    };

    return (
        <div className="modal-overlay">
            <div className="modal modal-xl">
                <div className="modal-header">
                    <h2 className="modal-title">Alarm Rule Details: {rule.name}</h2>
                    <button className="close-button" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>
                <div className="modal-content">
                    {/* Same padding and container as CreateEditModal */}
                    <div style={{ padding: '32px' }}>
                        <div className="modal-form-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '32px 48px' }}>

                            {/* Section 1: Basic Information - Exact mirror of form */}
                            <div className="form-section">
                                <div className="section-title">{t('modals.basicInfo', {ns: 'alarms'})}</div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('settings.ruleName', {ns: 'alarms'})}</label>
                                    <div className="detail-read-value" style={{ fontSize: '15px', fontWeight: 600, padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>{rule.name}</div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('labels.categorySeverity', {ns: 'alarms'})}</label>
                                    <div style={{ display: 'flex', gap: '8px', padding: '4px 0' }}>
                                        <span className="badge neutral" style={{ background: 'var(--neutral-100)', color: 'var(--neutral-700)' }}>
                                            {CATEGORY_DISPLAY_NAMES[rule.category || ''] || 'General'}
                                        </span>
                                        <span className={`badge severity-badge ${getSeverityBadgeClass(rule.severity)}`}>
                                            {rule.severity.toUpperCase()}
                                        </span>
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.description', {ns: 'alarms'})}</label>
                                    <div className="detail-read-value" style={{ minHeight: '60px', padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)', color: 'var(--neutral-600)', lineHeight: '1.5' }}>
                                        {rule.description || 'No description available.'}
                                    </div>
                                </div>
                                <div className="detail-item-group">
                                    <label className="form-label">{t('detail.tags', {ns: 'alarms'})}</label>
                                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', padding: '4px 0' }}>
                                        {rule.tags && rule.tags.length > 0 ? rule.tags.map(t => (
                                            <span key={t} className="tag-item" style={{ background: 'var(--primary-50)', color: 'var(--primary-700)', border: '1px solid var(--primary-100)' }}>{t}</span>
                                        )) : <span style={{ color: 'var(--neutral-400)', fontStyle: 'italic' }}>{t('detail.noTags', {ns: 'alarms'})}</span>}
                                    </div>
                                </div>
                            </div>

                            {/* Section 2: Target Selection - Exact mirror of form */}
                            <div className="form-section">
                                <div className="section-title">{t('labels.targetSettings', {ns: 'alarms'})}</div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.targetType', {ns: 'alarms'})}</label>
                                    <div style={{ display: 'flex', gap: '8px', padding: '4px 0' }}>
                                        {['data_point', 'device', 'virtual_point'].map(t => (
                                            <span key={t} className={`logic-pill ${rule.target_type === t ? 'active' : ''}`} style={{ cursor: 'default', opacity: rule.target_type === t ? 1 : 0.4 }}>
                                                {t === 'data_point' ? 'Data Point' : t === 'device' ? 'Device' : 'Virtual Point'}
                                            </span>
                                        ))}
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.targetGroup', {ns: 'alarms'})}</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>{rule.target_group || '-'}</div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.selectedTarget', {ns: 'alarms'})}</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                        <i className={`fas ${rule.target_type === 'device' ? 'fa-server' : 'fa-microchip'}`} style={{ color: 'var(--primary-500)' }}></i>
                                        {getTargetDisplay()}
                                    </div>
                                </div>
                            </div>

                            {/* Section 3: Condition Settings - Exact mirror of form */}
                            <div className="form-section">
                                <div className="section-title">{t('labels.conditionSettings', {ns: 'alarms'})}</div>
                                <div style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
                                    {['analog', 'digital', 'script'].map(t => (
                                        <span key={t} className={`logic-pill ${rule.alarm_type === t ? 'active' : ''}`} style={{ cursor: 'default', opacity: rule.alarm_type === t ? 1 : 0.4 }}>
                                            {t === 'analog' ? 'Analog' : t === 'digital' ? 'Digital' : 'Script'}
                                        </span>
                                    ))}
                                </div>

                                {rule.alarm_type === 'analog' && (
                                    <div className="limits-grid" style={{ pointerEvents: 'none', marginTop: '0' }}>
                                        {['HH', 'H', 'L', 'LL'].map(l => (
                                            <div key={l} className="form-group">
                                                <label className="form-label">{l} LIMIT</label>
                                                <div className="detail-read-value" style={{ textAlign: 'center', background: '#fff', border: '1px solid var(--neutral-200)', borderRadius: '4px', padding: '4px 0', fontWeight: 700 }}>
                                                    {(rule as any)[l === 'HH' ? 'high_high_limit' : l === 'H' ? 'high_limit' : l === 'L' ? 'low_limit' : 'low_low_limit'] ?? '-'}
                                                </div>
                                            </div>
                                        ))}
                                        <div className="form-group">
                                            <label className="form-label">{t('labels.deadband', {ns: 'alarms'})}</label>
                                            <div className="detail-read-value" style={{ textAlign: 'center', background: '#fff', border: '1px solid var(--neutral-200)', borderRadius: '4px', padding: '4px 0' }}>
                                                {rule.deadband ?? '-'}
                                            </div>
                                        </div>
                                        <div className="form-group">
                                            <label className="form-label">{t('labels.rocLimit', {ns: 'alarms'})}</label>
                                            <div className="detail-read-value" style={{ textAlign: 'center', background: '#fff', border: '1px solid var(--neutral-200)', borderRadius: '4px', padding: '4px 0' }}>
                                                {rule.rate_of_change ?? '-'}
                                            </div>
                                        </div>
                                    </div>
                                )}

                                {rule.alarm_type === 'digital' && (
                                    <div className="detail-item-group">
                                        <label className="form-label">{t('labels.triggerCondition', {ns: 'alarms'})}</label>
                                        <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)', fontWeight: 600, color: 'var(--primary-600)' }}>
                                            {rule.trigger_condition === 'on_true' ? 'When value is True (1)' :
                                                rule.trigger_condition === 'on_false' ? 'When value is False (0)' :
                                                    rule.trigger_condition === 'on_change' ? 'When state changes' : 'Connection lost'}
                                        </div>
                                    </div>
                                )}

                                {rule.alarm_type === 'script' && (
                                    <div className="form-group">
                                        <div className="script-patterns-grid" style={{ display: 'flex', flexWrap: 'wrap', gap: '8px', marginBottom: '16px', opacity: 0.7 }}>
                                            {SCRIPT_PATTERNS.map(p => (
                                                <div key={p.id} className={`pattern-chip ${rule.condition_script?.includes(p.id) ? 'active' : ''}`} style={{ cursor: 'default' }}>
                                                    {p.icon} {p.label}
                                                </div>
                                            ))}
                                        </div>
                                        <pre className="script-editor" style={{ maxHeight: '120px', overflowY: 'auto', background: 'var(--neutral-900)', color: '#a9b7c6', padding: '12px', borderRadius: '6px', fontSize: '12px' }}>
                                            {rule.condition_script}
                                        </pre>
                                    </div>
                                )}
                            </div>

                            {/* Section 4: Notifications & Actions - Exact mirror of form */}
                            <div className="form-section">
                                <div className="section-title">{t('detail.notificationActions', {ns: 'alarms'})}</div>
                                <div className="notification-grid" style={{ pointerEvents: 'none' }}>
                                    <div className={`checkbox-group ${rule.auto_acknowledge ? 'active' : ''}`} style={{ height: '52px', background: rule.auto_acknowledge ? 'var(--primary-50)' : 'var(--neutral-50)' }}>
                                        <label className="checkbox-label">
                                            <i className={`fas ${rule.auto_acknowledge ? 'fa-check-circle' : 'fa-circle'}`} style={{ color: rule.auto_acknowledge ? 'var(--primary-500)' : 'var(--neutral-300)' }}></i>
                                            Auto Acknowledge (Auto Ack)
                                        </label>
                                    </div>
                                    <div className={`checkbox-group ${rule.auto_clear ? 'active' : ''}`} style={{ height: '52px', background: rule.auto_clear ? 'var(--primary-50)' : 'var(--neutral-50)' }}>
                                        <label className="checkbox-label">
                                            <i className={`fas ${rule.auto_clear ? 'fa-check-circle' : 'fa-circle'}`} style={{ color: rule.auto_clear ? 'var(--primary-500)' : 'var(--neutral-300)' }}></i>
                                            자동 해제 (Auto Clear)
                                        </label>
                                    </div>
                                    <div className={`checkbox-group ${rule.is_enabled ? 'active' : ''}`} style={{ height: '52px', background: rule.is_enabled ? 'var(--success-50)' : 'var(--neutral-50)' }}>
                                        <label className="checkbox-label">
                                            <i className={`fas ${rule.is_enabled ? 'fa-check-circle' : 'fa-circle'}`} style={{ color: rule.is_enabled ? 'var(--success-500)' : 'var(--neutral-300)' }}></i>
                                            규칙 활성화
                                        </label>
                                    </div>
                                    <div className="priority-group" style={{ height: '52px', background: 'var(--neutral-50)' }}>
                                        <label className="form-label">우선순위</label>
                                        <div className="detail-read-value" style={{ fontWeight: 700, color: 'var(--primary-700)', fontSize: '18px' }}>{rule.priority}</div>
                                    </div>
                                </div>
                            </div>
                        </div>

                        <div className="sentence-builder-bar" style={{ marginTop: '32px', borderRadius: '8px' }}>
                            <i className="fas fa-robot"></i>
                            <div className="sentence-label" style={{ marginLeft: '12px' }}>조건 요약:</div>
                            {renderSentencePills(generateSentence())}
                        </div>
                    </div>
                </div>
                <div className="modal-footer" style={{ borderTop: '1px solid var(--neutral-100)', padding: '20px 32px' }}>
                    <div className="footer-left"></div>
                    <div className="footer-right" style={{ display: 'flex', gap: '12px' }}>
                        <button type="button" className="btn btn-secondary" style={{ minWidth: '100px' }} onClick={onClose}>{t('close', {ns: 'common'})}</button>
                        <button type="button" className="btn btn-primary" style={{ minWidth: '100px' }} onClick={() => onEdit(rule)}>수정하기</button>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default AlarmDetailModal;
