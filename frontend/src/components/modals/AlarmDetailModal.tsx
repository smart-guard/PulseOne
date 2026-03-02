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
    'temperature': 'temperature', 'pressure': 'pressure', 'flow': 'flow', 'level': 'level', 'vibration': 'vibration', 'electrical': 'electrical', 'safety': 'safety', 'general': 'general'
};

const SCRIPT_PATTERNS = [
    { id: 'threshold_above', labelKey: 'scriptPatterns.upperThreshold', icon: '📈' },
    { id: 'threshold_below', labelKey: 'scriptPatterns.lowerThreshold', icon: '📉' },
    { id: 'moving_avg', labelKey: 'scriptPatterns.movingAvg', icon: '📊' },
    { id: 'hysteresis', labelKey: 'scriptPatterns.hysteresis', icon: '➰' },
    { id: 'rate_of_change', labelKey: 'scriptPatterns.rateOfChange', icon: '⚡' }
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
        pills.push({ text: t('sentence.if', { defaultValue: '만약' }) });
        pills.push({ text: `[${targetName}]`, highlight: true });
        pills.push({ text: t('sentence.of', { defaultValue: '의' }) });
        pills.push({ text: rule.alarm_type === 'analog' ? t('alarmType.analog', { defaultValue: '아날로그' }) : t('alarmType.state', { defaultValue: '상태' }), highlight: true });
        pills.push({ text: t('sentence.value', { defaultValue: '값이' }) });

        if (rule.alarm_type === 'analog') {
            const val = rule.high_limit || rule.trigger_condition || "...";
            pills.push({ text: `[${val}]`, highlight: true });
        } else {
            pills.push({ text: `[${rule.trigger_condition || '...'}]`, highlight: true });
        }

        pills.push({ text: t('sentence.triggers', { defaultValue: '이면 알람 발생.' }) });
        return pills;
    };

    return (
        <div className="modal-overlay">
            <div className="modal modal-xl">
                <div className="modal-header">
                    <h2 className="modal-title">{t('modals.ruleDetailTitle', { defaultValue: '알람 규칙 상세' })}: {rule.name}</h2>
                    <button className="close-button" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>
                <div className="modal-content">
                    {/* Same padding and container as CreateEditModal */}
                    <div style={{ padding: '32px' }}>
                        <div className="modal-form-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '32px 48px' }}>

                            {/* Section 1: Basic Information - Exact mirror of form */}
                            <div className="form-section">
                                <div className="section-title">{t('modals.basicInfo', { ns: 'alarms' })}</div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('settings.ruleName', { ns: 'alarms' })}</label>
                                    <div className="detail-read-value" style={{ fontSize: '15px', fontWeight: 600, padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>{rule.name}</div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('labels.categorySeverity', { ns: 'alarms' })}</label>
                                    <div style={{ display: 'flex', gap: '8px', padding: '4px 0' }}>
                                        <span className="badge neutral" style={{ background: 'var(--neutral-100)', color: 'var(--neutral-700)' }}>
                                            {t(`categories.${CATEGORY_DISPLAY_NAMES[rule.category || ''] || 'general'}`, { defaultValue: CATEGORY_DISPLAY_NAMES[rule.category || ''] || 'General' })}
                                        </span>
                                        <span className={`badge severity-badge ${getSeverityBadgeClass(rule.severity)}`}>
                                            {rule.severity.toUpperCase()}
                                        </span>
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.description', { ns: 'alarms' })}</label>
                                    <div className="detail-read-value" style={{ minHeight: '60px', padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)', color: 'var(--neutral-600)', lineHeight: '1.5' }}>
                                        {rule.description || t('detail.noDescription', { defaultValue: '설명이 없습니다.' })}
                                    </div>
                                </div>
                                <div className="detail-item-group">
                                    <label className="form-label">{t('detail.tags', { ns: 'alarms' })}</label>
                                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', padding: '4px 0' }}>
                                        {rule.tags && rule.tags.length > 0 ? rule.tags.map(t => (
                                            <span key={t} className="tag-item" style={{ background: 'var(--primary-50)', color: 'var(--primary-700)', border: '1px solid var(--primary-100)' }}>{t}</span>
                                        )) : <span style={{ color: 'var(--neutral-400)', fontStyle: 'italic' }}>{t('detail.noTags', { ns: 'alarms' })}</span>}
                                    </div>
                                </div>
                            </div>

                            {/* Section 2: Target Selection - Exact mirror of form */}
                            <div className="form-section">
                                <div className="section-title">{t('labels.targetSettings', { ns: 'alarms' })}</div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.targetType', { ns: 'alarms' })}</label>
                                    <div style={{ display: 'flex', gap: '8px', padding: '4px 0' }}>
                                        {['data_point', 'device', 'virtual_point'].map(tt => (
                                            <span key={tt} className={`logic-pill ${rule.target_type === tt ? 'active' : ''}`} style={{ cursor: 'default', opacity: rule.target_type === tt ? 1 : 0.4 }}>
                                                {tt === 'data_point' ? t('targetType.dataPoint', { defaultValue: '데이터 포인트' }) : tt === 'device' ? t('targetType.device', { defaultValue: '디바이스' }) : t('targetType.virtualPoint', { defaultValue: '가상 포인트' })}
                                            </span>
                                        ))}
                                    </div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.targetGroup', { ns: 'alarms' })}</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)' }}>{rule.target_group || '-'}</div>
                                </div>
                                <div className="detail-item-group" style={{ marginBottom: '20px' }}>
                                    <label className="form-label">{t('detail.selectedTarget', { ns: 'alarms' })}</label>
                                    <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                        <i className={`fas ${rule.target_type === 'device' ? 'fa-server' : 'fa-microchip'}`} style={{ color: 'var(--primary-500)' }}></i>
                                        {getTargetDisplay()}
                                    </div>
                                </div>
                            </div>

                            {/* Section 3: Condition Settings - Exact mirror of form */}
                            <div className="form-section">
                                <div className="section-title">{t('labels.conditionSettings', { ns: 'alarms' })}</div>
                                <div style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
                                    {['analog', 'digital', 'script'].map(at => (
                                        <span key={at} className={`logic-pill ${rule.alarm_type === at ? 'active' : ''}`} style={{ cursor: 'default', opacity: rule.alarm_type === at ? 1 : 0.4 }}>
                                            {at === 'analog' ? t('alarmType.analog', { defaultValue: '아날로그' }) : at === 'digital' ? t('alarmType.digital', { defaultValue: '디지털' }) : t('alarmType.script', { defaultValue: '스크립트' })}
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
                                            <label className="form-label">{t('labels.deadband', { ns: 'alarms' })}</label>
                                            <div className="detail-read-value" style={{ textAlign: 'center', background: '#fff', border: '1px solid var(--neutral-200)', borderRadius: '4px', padding: '4px 0' }}>
                                                {rule.deadband ?? '-'}
                                            </div>
                                        </div>
                                        <div className="form-group">
                                            <label className="form-label">{t('labels.rocLimit', { ns: 'alarms' })}</label>
                                            <div className="detail-read-value" style={{ textAlign: 'center', background: '#fff', border: '1px solid var(--neutral-200)', borderRadius: '4px', padding: '4px 0' }}>
                                                {rule.rate_of_change ?? '-'}
                                            </div>
                                        </div>
                                    </div>
                                )}

                                {rule.alarm_type === 'digital' && (
                                    <div className="detail-item-group">
                                        <label className="form-label">{t('labels.triggerCondition', { ns: 'alarms' })}</label>
                                        <div className="detail-read-value" style={{ padding: '8px 12px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-100)', fontWeight: 600, color: 'var(--primary-600)' }}>
                                            {rule.trigger_condition === 'on_true' ? t('trigger.onTrue', { defaultValue: '값이 True(1)일 때' }) :
                                                rule.trigger_condition === 'on_false' ? t('trigger.onFalse', { defaultValue: '값이 False(0)일 때' }) :
                                                    rule.trigger_condition === 'on_change' ? t('trigger.onChange', { defaultValue: '상태가 변경될 때' }) : t('trigger.connLost', { defaultValue: '연결 끊김' })}
                                        </div>
                                    </div>
                                )}

                                {rule.alarm_type === 'script' && (
                                    <div className="form-group">
                                        <div className="script-patterns-grid" style={{ display: 'flex', flexWrap: 'wrap', gap: '8px', marginBottom: '16px', opacity: 0.7 }}>
                                            {SCRIPT_PATTERNS.map(p => (
                                                <div key={p.id} className={`pattern-chip ${rule.condition_script?.includes(p.id) ? 'active' : ''}`} style={{ cursor: 'default' }}>
                                                    {p.icon} {t(p.labelKey, { defaultValue: p.labelKey })}
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
                                <div className="section-title">{t('detail.notificationActions', { ns: 'alarms' })}</div>
                                <div className="notification-grid" style={{ pointerEvents: 'none' }}>
                                    <div className={`checkbox-group ${rule.auto_acknowledge ? 'active' : ''}`} style={{ height: '52px', background: rule.auto_acknowledge ? 'var(--primary-50)' : 'var(--neutral-50)' }}>
                                        <label className="checkbox-label">
                                            <i className={`fas ${rule.auto_acknowledge ? 'fa-check-circle' : 'fa-circle'}`} style={{ color: rule.auto_acknowledge ? 'var(--primary-500)' : 'var(--neutral-300)' }}></i>
                                            {t('labels.autoAcknowledge', { defaultValue: '자동 인지 (Auto Ack)' })}
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
                        <button type="button" className="btn btn-secondary" style={{ minWidth: '100px' }} onClick={onClose}>{t('close', { ns: 'common' })}</button>
                        <button type="button" className="btn btn-primary" style={{ minWidth: '100px' }} onClick={() => onEdit(rule)}>수정하기</button>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default AlarmDetailModal;
