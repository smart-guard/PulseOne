import React, { useState, useEffect, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import alarmTemplatesApi, {
    AlarmTemplate
} from '../../api/services/alarmTemplatesApi';
import { useConfirmContext } from '../common/ConfirmProvider';
import '../../styles/management.css';
import '../../styles/alarm-rule-templates.css';
import '../../styles/modal-form-grid.css';

interface Props {
    isOpen: boolean;
    onClose: () => void;
    onSuccess: () => void;
    mode: 'create' | 'edit';
    template?: AlarmTemplate | null;
}

const PRESETS = [
    { id: 'high_temp', title: 'High Temperature Warning', desc: 'Alarm triggers when value exceeds 80°C', icon: '🌡️', logic: 'value > 80', severity: 'high' as const },
    { id: 'range_out', title: 'Out of Range', desc: 'Triggers when value is outside 10~90 range', icon: '↔️', logic: 'value < 10 OR value > 90', severity: 'medium' as const },
    { id: 'pump_stop', title: 'Pump Stopped', desc: 'Triggers when status value is 0', icon: '🚨', logic: 'value === 0', severity: 'critical' as const },
];

const LOGIC_TOKENS = [
    { id: 'val', label: 'value', type: 'var', color: '#3b82f6' },
    { id: 'gt', label: '>', type: 'operator', color: '#64748b' },
    { id: 'lt', label: '<', type: 'operator', color: '#64748b' },
    { id: 'eq', label: '===', type: 'operator', color: '#64748b' },
    { id: 'th', label: 'THRESHOLD', type: 'const', color: '#ef4444' },
    { id: 'and', label: 'AND', type: 'operator', color: '#64748b' },
    { id: 'or', label: 'OR', type: 'operator', color: '#64748b' },
];

const SEVERITY_CARDS = [
    { id: 'low', label: 'LOW', icon: 'fa-info-circle', color: '#22c55e' },
    { id: 'medium', label: 'MEDIUM', icon: 'fa-exclamation-triangle', color: '#f59e0b' },
    { id: 'high', label: 'HIGH', icon: 'fa-fire', color: '#f97316' },
    { id: 'critical', label: 'CRITICAL', icon: 'fa-radiation', color: '#ef4444' },
];

const AlarmTemplateCreateEditModal: React.FC<Props> = ({
    isOpen,
    onClose,
    onSuccess,
    mode,
    template
}) => {
    const { confirm } = useConfirmContext();
    const [step, setStep] = useState(1);
    const { t } = useTranslation(['alarms', 'common']);
    const [activePreset, setActivePreset] = useState<string | null>(null);
    const tagInputRef = useRef<HTMLInputElement>(null);

    const [formData, setFormData] = useState<Partial<AlarmTemplate>>({
        name: '',
        category: 'general',
        template_type: 'simple',
        trigger_condition: '',
        severity: 'medium',
        message_template: 'Alarm: {device_name} value exceeded threshold.',
        description: '',
        tags: [],
        is_active: true,
        default_config: {
            high_limit: '',
            low_limit: '',
            deadband: 0,
            delay_ms: 0
        } as any
    });

    useEffect(() => {
        if (isOpen) {
            if (mode === 'edit' && template) {
                setFormData({
                    ...template,
                    default_config: typeof template.default_config === 'string'
                        ? JSON.parse(template.default_config)
                        : (template.default_config || {})
                });
                setStep(1);
            } else {
                setFormData({
                    name: '',
                    category: 'general',
                    template_type: 'simple',
                    trigger_condition: '',
                    severity: 'medium',
                    message_template: 'Alarm: {device_name} value exceeded threshold.',
                    description: '',
                    tags: [],
                    is_active: true,
                    default_config: { high_limit: '', low_limit: '', deadband: 0, delay_ms: 0 } as any
                });
                setStep(1);
                setActivePreset(null);
            }
        }
    }, [isOpen, mode, template]);

    const handleNext = () => setStep(p => Math.min(p + 1, 4));
    const handleBack = () => setStep(p => Math.max(p - 1, 1));

    const applyPreset = (preset: typeof PRESETS[0]) => {
        setActivePreset(preset.id);
        setFormData(p => ({
            ...p,
            name: p.name || `New ${preset.title} Template`,
            trigger_condition: preset.logic,
            severity: preset.severity
        }));
        // Auto-advance if creating
        if (mode === 'create') handleNext();
    };

    const addToken = (token: typeof LOGIC_TOKENS[0]) => {
        setFormData(p => ({
            ...p,
            trigger_condition: (p.trigger_condition ? p.trigger_condition + ' ' : '') + token.label
        }));
    };

    const handleAddTag = (e: React.KeyboardEvent) => {
        if (e.key === 'Enter' && tagInputRef.current?.value) {
            const val = tagInputRef.current.value.trim();
            if (val && !formData.tags?.includes(val)) {
                setFormData(p => ({ ...p, tags: [...(p.tags || []), val] }));
            }
            tagInputRef.current.value = '';
        }
    };

    const removeTag = (tag: string) => {
        setFormData(p => ({ ...p, tags: p.tags?.filter(t => t !== tag) }));
    };

    const handleSave = async () => {
        if (!formData.name) {
            confirm({ title: t('templates.inputError', { defaultValue: '입력 오류' }), message: t('templates.enterTemplateName', { defaultValue: '템플릿 이름을 입력해주세요.' }), confirmButtonType: 'warning', showCancelButton: false });
            setStep(1);
            return;
        }

        try {
            const payload = {
                ...formData,
                default_config: JSON.stringify(formData.default_config)
            };

            const res = mode === 'create'
                ? await alarmTemplatesApi.createTemplate(payload as any)
                : await alarmTemplatesApi.updateTemplate(template!.id, payload as any);

            if ((res as any).success) {
                onSuccess();
                onClose();
            }
        } catch (error) {
            console.error('Failed to save template', error);
        }
    };

    if (!isOpen) return null;

    return (
        <div className="modal-overlay">
            <div className="modal-container modal-xl" style={{ width: '1000px' }}>
                <div className="modal-header" style={{ borderBottom: 'none', paddingBottom: '10px' }}>
                    <h3 className="modal-title">
                        <i className="fas fa-magic" style={{ color: 'var(--blueprint-500)', marginRight: '10px' }}></i>
                        {mode === 'create' ? t('templates.wizardCreateTitle', { defaultValue: '알람 템플릿 생성 (위저드)' }) : t('templates.wizardEditTitle', { defaultValue: '템플릿 편집 (위저드)' })}
                    </h3>
                    <button className="modal-close" onClick={onClose} style={{ fontSize: '24px', opacity: 0.5 }}>&times;</button>
                </div>

                {/* --- Wizard Step Indicator --- */}
                <div className="wizard-steps-header">
                    {[
                        { step: 1, label: t('templates.wizardStep1', { defaultValue: '기본 정보' }) },
                        { step: 2, label: t('templates.wizardStep2', { defaultValue: '트리거 로직' }) },
                        { step: 3, label: t('templates.wizardStep3', { defaultValue: '임계값 및 레벨' }) },
                        { step: 4, label: t('templates.wizardStep4', { defaultValue: '최종 확인' }) }
                    ].map((s) => (
                        <div key={s.step} className={`wizard-step-item ${step === s.step ? 'active' : ''} ${step > s.step ? 'completed' : ''}`}>
                            <div className="step-number">{step > s.step ? <i className="fas fa-check"></i> : s.step}</div>
                            <div className="step-label">{s.label}</div>
                        </div>
                    ))}
                </div>

                <div className="modal-content" style={{ padding: '0 40px 40px 40px', minHeight: '450px' }}>

                    {/* Step 1: Identity & Presets */}
                    {step === 1 && (
                        <div className="wizard-content-fade-in">
                            <div className="form-section" style={{ marginBottom: '32px' }}>
                                <div className="section-title">{t('templates.templateIdentity', { defaultValue: '템플릿 식별 정보' })}</div>
                                <div className="modal-form-grid" style={{ padding: 0 }}>
                                    <div className="form-group">
                                        <label className="form-label required">{t('templates.templateName', { defaultValue: '템플릿 이름' })}</label>
                                        <input type="text" className="form-input" placeholder={t('templates.templateNamePlaceholder', { defaultValue: '엔지니어가 식별하기 쉬운 이름을 입력하세요' })}
                                            value={formData.name} onChange={e => setFormData(p => ({ ...p, name: e.target.value }))} />
                                    </div>
                                    <div className="form-group">
                                        <label className="form-label">{t('templates.defaultCategory', { defaultValue: '다어트 카테고리' })}</label>
                                        <select className="form-select" value={formData.category} onChange={e => setFormData(p => ({ ...p, category: e.target.value }))}>
                                            <option value="general">{t('categories.general', { defaultValue: '일반 설비' })}</option>
                                            <option value="temperature">{t('categories.temperature', { defaultValue: '온도/열' })}</option>
                                            <option value="pressure">{t('categories.pressure', { defaultValue: '압력 시스템' })}</option>
                                            <option value="electrical">{t('categories.electrical', { defaultValue: '전기 신호' })}</option>
                                        </select>
                                    </div>
                                </div>
                            </div>

                            {mode === 'create' && (
                                <div className="form-section">
                                    <div className="section-title">{t('templates.quickStartPresets', { defaultValue: '빠른 시작 프리셋 (권장 로직)' })}</div>
                                    <div className="quick-start-row">
                                        {PRESETS.map(preset => (
                                            <div key={preset.id}
                                                className={`preset-card ${activePreset === preset.id ? 'active' : ''}`}
                                                onClick={() => applyPreset(preset)}>
                                                <div className="preset-icon">{preset.icon}</div>
                                                <div className="preset-info">
                                                    <div className="preset-title">{preset.title}</div>
                                                    <div className="preset-desc">{preset.desc}</div>
                                                </div>
                                            </div>
                                        ))}
                                    </div>
                                    <p style={{ fontSize: '12px', color: '#94a3b8', textAlign: 'center' }}>
                                        💡 {t('templates.presetHint', { defaultValue: '프리셋을 선택하면 최적의 기본값이 자동으로 설정되어 다음 단계로 진행됩니다.' })}
                                    </p>
                                </div>
                            )}
                        </div>
                    )}

                    {/* Step 2: Logic Builder */}
                    {step === 2 && (
                        <div className="wizard-content-fade-in">
                            <div className="form-section">
                                <div className="section-title">{t('templates.triggerLogicSettings', { defaultValue: '트리거 로직 설정' })}</div>
                                <p className="form-desc" style={{ fontSize: '13px', color: '#64748b', marginBottom: '20px' }}>
                                    {t('templates.triggerLogicDesc', { defaultValue: '알람을 발생시키는 조건을 정의하세요. 아래 토큐을 클릭해 로직 표현식을 만드세요.' })}
                                </p>

                                <div className="logic-builder-container" style={{ background: '#f8fafc', border: '1px solid #e2e8f0', padding: '32px', borderRadius: '16px' }}>
                                    <div className="token-row" style={{ marginBottom: '24px' }}>
                                        {LOGIC_TOKENS.map(token => (
                                            <button key={token.id} type="button" className={`token-chip ${token.type}`} onClick={() => addToken(token)}>
                                                <span style={{ width: '8px', height: '8px', borderRadius: '50%', background: token.color }}></span>
                                                {token.label}
                                            </button>
                                        ))}
                                        <button type="button" className="token-chip" onClick={() => setFormData(p => ({ ...p, trigger_condition: '' }))} style={{ color: '#ef4444', marginLeft: 'auto', border: '1px dashed #fecaca' }}>
                                            <i className="fas fa-trash-alt"></i> {t('templates.clear', { defaultValue: '지우기' })}
                                        </button>
                                    </div>
                                    <div className="expression-box" style={{ minHeight: '80px', fontSize: '20px' }}>
                                        {formData.trigger_condition || t('templates.combineTokensHint', { defaultValue: '위의 토큐을 조합하여 로직을 완성하세요...' })}
                                    </div>
                                    <div className="expression-hint" style={{ marginTop: '16px' }}>
                                        <i className="fas fa-lightbulb" style={{ color: '#f59e0b', marginRight: '6px' }}></i>
                                        <i className="fas fa-lightbulb" style={{ color: '#f59e0b', marginRight: '6px' }}></i>
                                        {t('templates.engineerTip', { defaultValue: '엔지니어 팁' })}: <code>THRESHOLD</code> {t('templates.engineerTipDesc', { defaultValue: '는 다음 단계에서 설정한 임계값으로 대체됩니다.' })}
                                    </div>
                                </div>
                            </div>
                        </div>
                    )}

                    {/* Step 3: Intensity & Limits */}
                    {step === 3 && (
                        <div className="wizard-content-fade-in">
                            <div className="modal-form-grid" style={{ padding: 0, gap: '40px' }}>
                                <div className="form-column">
                                    <div className="form-section">
                                        <div className="section-title">{t('templates.alarmSeverity', { defaultValue: '알람 심각도' })}</div>
                                        <div className="severity-grid">
                                            {SEVERITY_CARDS.map(s => (
                                                <div key={s.id}
                                                    className={`intensity-card ${s.id} ${formData.severity === s.id ? 'active' : ''}`}
                                                    onClick={() => setFormData(p => ({ ...p, severity: s.id as any }))}>
                                                    <i className={`fas ${s.icon}`}></i>
                                                    <span>{s.label}</span>
                                                </div>
                                            ))}
                                        </div>
                                    </div>
                                </div>
                                <div className="form-column">
                                    <div className="form-section">
                                        <div className="section-title">{t('templates.defaultThresholds', { defaultValue: '기본 임계값' })}</div>
                                        <div className="form-group">
                                            <label className="form-label">{t('templates.highLimit', { defaultValue: '상한값' })}</label>
                                            <input type="number" className="form-input" placeholder="80"
                                                value={formData.default_config?.high_limit}
                                                onChange={e => setFormData(p => ({ ...p, default_config: { ...p.default_config, high_limit: e.target.value } as any }))} />
                                        </div>
                                        <div className="form-group">
                                            <label className="form-label">{t('templates.lowLimit', { defaultValue: '하한값' })}</label>
                                            <input type="number" className="form-input" placeholder="20"
                                                value={formData.default_config?.low_limit}
                                                onChange={e => setFormData(p => ({ ...p, default_config: { ...p.default_config, low_limit: e.target.value } as any }))} />
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    )}

                    {/* Step 4: Finalize & Tags */}
                    {step === 4 && (
                        <div className="wizard-content-fade-in">
                            <div className="form-section" style={{ marginBottom: '32px' }}>
                                <div className="section-title">{t('templates.messageSectionTitle', { defaultValue: '메시지 템플릿 및 설명' })}</div>
                                <div className="form-group">
                                    <label className="form-label">{t('templates.triggerMessage', { defaultValue: '트리거 메시지' })}</label>
                                    <textarea className="form-input" rows={2}
                                        value={formData.message_template} onChange={e => setFormData(p => ({ ...p, message_template: e.target.value }))} />
                                </div>
                                <div className="modal-form-grid" style={{ padding: 0, marginTop: '24px' }}>
                                    <div className="form-group">
                                        <label className="form-label">{t('templates.templateDescription', { defaultValue: '템플릿 설명' })}</label>
                                        <textarea className="form-input" rows={3} placeholder={t('templates.templateDescPlaceholder', { defaultValue: '이 템플릿의 목적과 주의 사항을 설명하세요' })}
                                            value={formData.description} onChange={e => setFormData(p => ({ ...p, description: e.target.value }))} />
                                    </div>
                                    <div className="form-group">
                                        <label className="form-label">{t('templates.tags', { defaultValue: '태그' })}</label>
                                        <div className="tag-input-container" style={{ minHeight: '100px', background: '#f8fafc', border: '1px solid #e2e8f0', borderRadius: '12px', padding: '12px' }}>
                                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px', marginBottom: '10px' }}>
                                                {formData.tags?.map(tag => (
                                                    <span key={tag} className="tag-chip" style={{ background: 'var(--blueprint-100)', color: 'var(--blueprint-800)', border: '1px solid var(--blueprint-200)', borderRadius: '6px', padding: '4px 10px', fontSize: '13px', fontWeight: 600 }}>
                                                        #{tag} <button type="button" onClick={() => removeTag(tag)} style={{ border: 'none', background: 'none', color: '#94a3b8', cursor: 'pointer' }}>&times;</button>
                                                    </span>
                                                ))}
                                            </div>
                                            <input ref={tagInputRef} type="text" className="form-input" placeholder={t('templates.tagsPlaceholder', { defaultValue: 'Enter 를 눌러 태그 추가...' })} onKeyDown={handleAddTag} style={{ background: 'white' }} />
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    )}

                </div>

                <div className="modal-footer" style={{ padding: '24px 40px', background: '#f8fafc', borderTop: '1px solid #f1f5f9', borderBottomLeftRadius: '16px', borderBottomRightRadius: '16px' }}>
                    {step > 1 && (
                        <button className="btn btn-outline" onClick={handleBack}>
                            <i className="fas fa-arrow-left"></i> 이전
                        </button>
                    )}

                    <div style={{ marginLeft: 'auto', display: 'flex', gap: '12px' }}>
                        <button className="btn btn-ghost" onClick={onClose}>취소</button>
                        {step < 4 ? (
                            <button className="btn btn-primary" onClick={handleNext} disabled={!formData.name && step === 1}>
                                다음 단계 <i className="fas fa-arrow-right"></i>
                            </button>
                        ) : (
                            <button className="btn btn-primary" onClick={handleSave}>
                                <i className="fas fa-check-circle"></i> 템플릿 저장 및 완료
                            </button>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default AlarmTemplateCreateEditModal;
