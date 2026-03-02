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
    const { t } = useTranslation(['common', 'common']);
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
            confirm({ title: 'Input Error', message: 'Please enter a template name.', confirmButtonType: 'warning', showCancelButton: false });
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
                        {mode === 'create' ? 'Create Alarm Template (Wizard)' : 'Edit Template (Wizard)'}
                    </h3>
                    <button className="modal-close" onClick={onClose} style={{ fontSize: '24px', opacity: 0.5 }}>&times;</button>
                </div>

                {/* --- Wizard Step Indicator --- */}
                <div className="wizard-steps-header">
                    {[
                        { step: 1, label: 'Basic Info' },
                        { step: 2, label: 'Trigger Logic' },
                        { step: 3, label: 'Threshold & Level' },
                        { step: 4, label: 'Final Review' }
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
                                <div className="section-title">Template Identity</div>
                                <div className="modal-form-grid" style={{ padding: 0 }}>
                                    <div className="form-group">
                                        <label className="form-label required">Template Name</label>
                                        <input type="text" className="form-input" placeholder="Enter a name that is easy for engineers to identify"
                                            value={formData.name} onChange={e => setFormData(p => ({ ...p, name: e.target.value }))} />
                                    </div>
                                    <div className="form-group">
                                        <label className="form-label">Default Category</label>
                                        <select className="form-select" value={formData.category} onChange={e => setFormData(p => ({ ...p, category: e.target.value }))}>
                                            <option value="general">General Equipment</option>
                                            <option value="temperature">Temperature/Thermal</option>
                                            <option value="pressure">Pressure System</option>
                                            <option value="electrical">Electrical Signal</option>
                                        </select>
                                    </div>
                                </div>
                            </div>

                            {mode === 'create' && (
                                <div className="form-section">
                                    <div className="section-title">Quick Start Presets (Recommended Logic)</div>
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
                                        💡 Selecting a preset auto-fills optimal defaults and advances to the next step.
                                    </p>
                                </div>
                            )}
                        </div>
                    )}

                    {/* Step 2: Logic Builder */}
                    {step === 2 && (
                        <div className="wizard-content-fade-in">
                            <div className="form-section">
                                <div className="section-title">Trigger Logic Settings</div>
                                <p className="form-desc" style={{ fontSize: '13px', color: '#64748b', marginBottom: '20px' }}>
                                    Define the condition that triggers the alarm. Click the tokens below to build the logic expression.
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
                                            <i className="fas fa-trash-alt"></i> Clear
                                        </button>
                                    </div>
                                    <div className="expression-box" style={{ minHeight: '80px', fontSize: '20px' }}>
                                        {formData.trigger_condition || 'Combine tokens above to complete the logic...'}
                                    </div>
                                    <div className="expression-hint" style={{ marginTop: '16px' }}>
                                        <i className="fas fa-lightbulb" style={{ color: '#f59e0b', marginRight: '6px' }}></i>
                                        Engineer Tip: <code>THRESHOLD</code> will be replaced with the threshold value set in the next step.
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
                                        <div className="section-title">Alarm Severity</div>
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
                                        <div className="section-title">Default Thresholds</div>
                                        <div className="form-group">
                                            <label className="form-label">High Limit</label>
                                            <input type="number" className="form-input" placeholder="80"
                                                value={formData.default_config?.high_limit}
                                                onChange={e => setFormData(p => ({ ...p, default_config: { ...p.default_config, high_limit: e.target.value } as any }))} />
                                        </div>
                                        <div className="form-group">
                                            <label className="form-label">Low Limit</label>
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
                                <div className="section-title">Message Template & Notes</div>
                                <div className="form-group">
                                    <label className="form-label">Trigger Message</label>
                                    <textarea className="form-input" rows={2}
                                        value={formData.message_template} onChange={e => setFormData(p => ({ ...p, message_template: e.target.value }))} />
                                </div>
                                <div className="modal-form-grid" style={{ padding: 0, marginTop: '24px' }}>
                                    <div className="form-group">
                                        <label className="form-label">Template Description</label>
                                        <textarea className="form-input" rows={3} placeholder="Describe the purpose and notes for this template"
                                            value={formData.description} onChange={e => setFormData(p => ({ ...p, description: e.target.value }))} />
                                    </div>
                                    <div className="form-group">
                                        <label className="form-label">Tags</label>
                                        <div className="tag-input-container" style={{ minHeight: '100px', background: '#f8fafc', border: '1px solid #e2e8f0', borderRadius: '12px', padding: '12px' }}>
                                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px', marginBottom: '10px' }}>
                                                {formData.tags?.map(tag => (
                                                    <span key={tag} className="tag-chip" style={{ background: 'var(--blueprint-100)', color: 'var(--blueprint-800)', border: '1px solid var(--blueprint-200)', borderRadius: '6px', padding: '4px 10px', fontSize: '13px', fontWeight: 600 }}>
                                                        #{tag} <button type="button" onClick={() => removeTag(tag)} style={{ border: 'none', background: 'none', color: '#94a3b8', cursor: 'pointer' }}>&times;</button>
                                                    </span>
                                                ))}
                                            </div>
                                            <input ref={tagInputRef} type="text" className="form-input" placeholder="Press Enter to add tag..." onKeyDown={handleAddTag} style={{ background: 'white' }} />
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
