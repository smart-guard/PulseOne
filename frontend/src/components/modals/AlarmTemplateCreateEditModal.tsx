import React, { useState, useEffect, useRef } from 'react';
import alarmTemplatesApi, {
    AlarmTemplate
} from '../../api/services/alarmTemplatesApi';
import { ALARM_CATEGORIES, ALARM_PRESETS } from '../../api/services/alarmConstants';
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
    onDelete?: (id: number, name: string) => void;
}

const LOGIC_TOKENS = [
    { id: 'val', label: 'value', type: 'var', color: '#3b82f6' },
    { id: 'gt', label: '>', type: 'operator', color: '#64748b' },
    { id: 'lt', label: '<', type: 'operator', color: '#64748b' },
    { id: 'eq', label: '===', type: 'operator', color: '#64748b' },
    { id: 'th', label: 'THRESHOLD', type: 'const', color: '#ef4444' },
    { id: 'and', label: 'AND', type: 'operator', color: '#3b82f6' },
    { id: 'or', label: 'OR', type: 'operator', color: '#3b82f6' },
];

const QUICK_LOGIC_PATTERNS = [
    { id: 'high', title: 'High Threshold (상한 알람)', logic: 'value > THRESHOLD', desc: '값이 설정된 상한값을 넘을 때 발생' },
    { id: 'low', title: 'Low Threshold (하한 알람)', logic: 'value < THRESHOLD', desc: '값이 설정된 하한값보다 낮을 때 발생' },
    { id: 'range_in', title: 'Inside Range (범위 내 진입)', logic: 'value > THRESHOLD AND value < THRESHOLD', desc: '값이 특정 범위 안에 있을 때 발생' },
    { id: 'range_out', title: 'Outside Range (범위 이탈)', logic: 'value < THRESHOLD OR value > THRESHOLD', desc: '값이 특정 범위를 벗어났을 때 발생' },
];

const SEVERITY_CARDS = [
    { id: 'low', label: 'LOW', icon: 'fa-info-circle' },
    { id: 'medium', label: 'MEDIUM', icon: 'fa-exclamation-triangle' },
    { id: 'high', label: 'HIGH', icon: 'fa-fire' },
    { id: 'critical', label: 'CRITICAL', icon: 'fa-radiation' },
];

const AlarmTemplateCreateEditModal: React.FC<Props> = ({
    isOpen,
    onClose,
    onSuccess,
    mode,
    template,
    onDelete
}) => {
    const { confirm } = useConfirmContext();
    const [step, setStep] = useState(1);
    const tagInputRef = useRef<HTMLInputElement>(null);

    const [formData, setFormData] = useState<Partial<AlarmTemplate>>({
        name: '새 알람 템플릿',
        category: 'general',
        template_type: 'simple',
        condition_type: 'threshold',
        trigger_condition: '',
        severity: 'medium',
        message_template: '알람: {device_name}의 값이 임계치를 초과했습니다.',
        description: '',
        tags: [],
        is_active: true,
        default_config: {
            high_limit: '80',
            low_limit: '20',
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
                    name: '새 알람 템플릿',
                    category: 'general',
                    template_type: 'simple',
                    condition_type: 'threshold',
                    trigger_condition: '',
                    severity: 'medium',
                    message_template: '알람: {device_name}의 값이 임계치를 초과했습니다.',
                    description: '',
                    tags: [],
                    is_active: true,
                    default_config: { high_limit: '80', low_limit: '20', deadband: 0, delay_ms: 0 } as any
                });
                setStep(1);
            }
        }
    }, [isOpen, mode, template]);

    const handleNext = () => setStep(p => Math.min(p + 1, 4));
    const handleBack = () => setStep(p => Math.max(p - 1, 1));

    const applyPreset = (preset: typeof ALARM_PRESETS[0]) => {
        setFormData(p => ({
            ...p,
            name: p.name === '새 알람 템플릿' ? preset.title : p.name,
            trigger_condition: preset.logic,
            severity: preset.severity as any,
            condition_type: preset.type as any,
            category: preset.category
        }));
        setStep(3);
    };

    const addToken = (token: typeof LOGIC_TOKENS[0]) => {
        setFormData(p => {
            const currentCondition = p.trigger_condition || '';
            const tokens = currentCondition.trim().split(/\s+/).filter(Boolean);
            const lastToken = tokens[tokens.length - 1];

            // Intelligent Guardrails: Prevent nonsensical sequences
            if (tokens.length === 0 && !['value', 'THRESHOLD'].includes(token.label)) {
                // Initial must be value or THRESHOLD (though value is standard)
            }
            if (lastToken === token.label && token.type === 'operator') return p; // Prevent >> or AND AND

            return {
                ...p,
                trigger_condition: (currentCondition ? currentCondition + ' ' : '') + token.label
            };
        });
    };

    const removeLastToken = () => {
        setFormData(p => {
            const tokens = (p.trigger_condition || '').trim().split(/\s+/);
            tokens.pop();
            return {
                ...p,
                trigger_condition: tokens.join(' ')
            };
        });
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
            confirm({ title: '입력 오류', message: '템플릿 이름을 입력해주세요.', confirmButtonType: 'warning', showCancelButton: false });
            setStep(1);
            return;
        }

        try {
            const payload = {
                ...formData,
                default_config: JSON.stringify(formData.default_config)
            };

            const res: any = mode === 'create'
                ? await alarmTemplatesApi.createTemplate(payload as any)
                : await alarmTemplatesApi.updateTemplate(template!.id, payload as any);

            if (res && (res.success || res.id)) {
                onSuccess();
                onClose();
            }
        } catch (error) {
            console.error('Failed to save template', error);
        }
    };

    if (!isOpen) return null;

    const steps = [
        { id: 1, title: 'Identity', label: '템플릿 기본 정보' },
        { id: 2, title: 'Logic', label: '트리거 조건 구성' },
        { id: 3, title: 'Config', label: '상세 설정 및 레벨' },
        { id: 4, title: 'Finalize', label: '게시 및 최종 확인' }
    ];

    return (
        <div className="modal-overlay">
            <div className="modal-container modal-xl" style={{ width: '1200px', minHeight: '720px', display: 'flex', flexDirection: 'column', transform: 'none', background: 'white', borderRadius: '16px', overflow: 'hidden', boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.1)' }}>
                <div className="modal-header wizard-modal-header">
                    <div style={{ display: 'flex', alignItems: 'center', gap: '20px' }}>
                        <div className="wizard-header-icon">
                            <i className="fas fa-magic"></i>
                        </div>
                        <div>
                            <h3 className="modal-title" style={{ margin: 0, color: 'white', fontSize: '18px', fontWeight: 700 }}>
                                {mode === 'create' ? '새 알람 템플릿 마법사' : '알람 템플릿 수정 마법사'}
                            </h3>
                            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', marginTop: '4px' }}>
                                <p style={{ margin: 0, color: 'rgba(255, 255, 255, 0.95)', fontSize: '13px', lineHeight: '1.4' }}>
                                    데이터 소스의 특성에 맞는 알람 규칙을 정의하고 임계값을 설정하는 4단계 프로세스입니다.
                                </p>
                                <span className="wizard-step-info" style={{ color: 'rgba(255, 255, 255, 0.85)', fontWeight: 600, fontSize: '11px', letterSpacing: '0.8px', marginTop: '2px' }}>
                                    STEP {step} OF 4: {steps[step - 1].label.toUpperCase()}
                                </span>
                            </div>
                        </div>
                    </div>
                    <button className="modal-close" onClick={onClose} style={{ color: 'white' }}>&times;</button>
                </div>

                <div className="wizard-steps-header">
                    {steps.map((s) => (
                        <div key={s.id} className={`wizard-step-item ${step === s.id ? 'active' : ''} ${step > s.id ? 'completed' : ''}`}>
                            <div className="step-number">{step > s.id ? <i className="fas fa-check"></i> : s.id}</div>
                            <div className="step-label">{s.title}</div>
                        </div>
                    ))}
                </div>

                <div className="modal-content" style={{ padding: 0, overflow: 'visible' }}>
                    <div className="modal-content-wrapper" style={{ padding: '48px 40px 12px 40px' }}>
                        {step === 1 && (
                            <div className="wizard-slide-active">
                                <div className="step1-dashboard-container">
                                    <div className="dashboard-left-panel">
                                        <div className="form-section" style={{ marginBottom: '32px' }}>
                                            <div className="section-title">
                                                <i className="fas fa-id-card"></i>
                                                기본 식별 정보
                                            </div>
                                            <div className="form-group" style={{ marginBottom: '16px' }}>
                                                <label className="form-label required">템플릿명 (Name)</label>
                                                <input type="text" className="form-input" placeholder="새 알람 템플릿"
                                                    value={formData.name} onChange={e => setFormData(p => ({ ...p, name: e.target.value }))} />
                                            </div>
                                            <div className="form-group">
                                                <label className="form-label">구분 카테고리 (Category)</label>
                                                <div className="category-selector-wrapper">
                                                    <select className="form-select" value={formData.category} onChange={e => setFormData(p => ({ ...p, category: e.target.value }))}>
                                                        {ALARM_CATEGORIES.map(cat => (
                                                            <option key={cat.value} value={cat.value}>{cat.label}</option>
                                                        ))}
                                                    </select>
                                                </div>
                                            </div>
                                        </div>

                                        <div className="form-section">
                                            <div className="section-title">
                                                <i className="fas fa-sliders-h"></i>
                                                템플릿 모드 선택
                                            </div>
                                            <div className="complexity-selector-vertical">
                                                <div className={`complexity-card-v2 ${formData.template_type === 'simple' ? 'active' : ''}`}
                                                    onClick={() => setFormData(p => ({ ...p, template_type: 'simple' }))}>
                                                    <div className="mode-icon"><i className="fas fa-bolt"></i></div>
                                                    <div className="mode-info">
                                                        <div className="mode-title">Simple (기본형)</div>
                                                        <div className="mode-desc">초보자용. 표준 임계치 위주의 직관적인 설정</div>
                                                    </div>
                                                </div>
                                                <div className={`complexity-card-v2 ${formData.template_type === 'advanced' ? 'active' : ''}`}
                                                    onClick={() => setFormData(p => ({ ...p, template_type: 'advanced' }))}>
                                                    <div className="mode-icon"><i className="fas fa-microchip"></i></div>
                                                    <div className="mode-info">
                                                        <div className="mode-title">Advanced (고급형)</div>
                                                        <div className="mode-desc">엔지니어용. 복합 수식/스크립트 로직 구성</div>
                                                    </div>
                                                </div>
                                            </div>
                                        </div>
                                    </div>

                                    <div className="dashboard-right-panel">
                                        <div className="section-title">
                                            <i className="fas fa-rocket"></i>
                                            산업별 빠른 시작 프리셋
                                        </div>

                                        <div className="preset-group-header">Pumps & Industrial (펌프/공정)</div>
                                        <div className="quick-start-row-v2" style={{ marginBottom: '16px' }}>
                                            {ALARM_PRESETS.filter(p => p.category === 'industrial').map(preset => (
                                                <div key={preset.id} className="preset-card preset-card-v2" onClick={() => applyPreset(preset as any)}>
                                                    <div className="preset-icon">{preset.icon}</div>
                                                    <div className="preset-info">
                                                        <div className="preset-title">{preset.title}</div>
                                                        <div className="preset-desc">{preset.desc}</div>
                                                    </div>
                                                    <i className="fas fa-plus" style={{ color: '#cbd5e1', fontSize: '12px' }}></i>
                                                </div>
                                            ))}
                                        </div>

                                        <div className="preset-group-header">Environmental & HVAC (환경/공조)</div>
                                        <div className="quick-start-row-v2" style={{ marginBottom: '16px' }}>
                                            {ALARM_PRESETS.filter(p => ['env', 'hvac'].includes(p.category)).map(preset => (
                                                <div key={preset.id} className="preset-card preset-card-v2" onClick={() => applyPreset(preset as any)}>
                                                    <div className="preset-icon">{preset.icon}</div>
                                                    <div className="preset-info">
                                                        <div className="preset-title">{preset.title}</div>
                                                        <div className="preset-desc">{preset.desc}</div>
                                                    </div>
                                                    <i className="fas fa-plus" style={{ color: '#cbd5e1', fontSize: '12px' }}></i>
                                                </div>
                                            ))}
                                        </div>

                                        <div className="preset-group-header">Electrical & Power (전기/에너지)</div>
                                        <div className="quick-start-row-v2">
                                            {ALARM_PRESETS.filter(p => p.category === 'electrical').map(preset => (
                                                <div key={preset.id} className="preset-card preset-card-v2" onClick={() => applyPreset(preset as any)}>
                                                    <div className="preset-icon">{preset.icon}</div>
                                                    <div className="preset-info">
                                                        <div className="preset-title">{preset.title}</div>
                                                        <div className="preset-desc">{preset.desc}</div>
                                                    </div>
                                                    <i className="fas fa-plus" style={{ color: '#cbd5e1', fontSize: '12px' }}></i>
                                                </div>
                                            ))}
                                        </div>
                                    </div>
                                </div>
                            </div>
                        )}

                        {step === 2 && (
                            <div className="wizard-slide-active">
                                <div className="form-section">
                                    <div className="section-title">
                                        <i className="fas fa-microchip" style={{ color: 'var(--blueprint-500)' }}></i>
                                        알람 발생 조건 (Trigger Logic)
                                    </div>
                                    <div className="logic-builder-container" style={{ padding: '32px' }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                                            <div className="token-row" style={{ display: 'flex', gap: '8px' }}>
                                                {LOGIC_TOKENS.map(token => {
                                                    const condition = formData.trigger_condition || '';
                                                    const tokens = condition.trim().split(/\s+/).filter(Boolean);
                                                    const lastToken = tokens[tokens.length - 1];

                                                    // Simple validation for disabling tokens
                                                    let disabled = false;
                                                    if (tokens.length === 0) disabled = token.type === 'operator';
                                                    else if (['>', '<', '==='].includes(lastToken)) disabled = token.type === 'operator' || token.type === 'var';
                                                    else if (lastToken === 'value') disabled = token.type === 'var' || token.label === 'THRESHOLD' || token.label === 'AND' || token.label === 'OR';
                                                    else if (lastToken === 'THRESHOLD') disabled = token.type === 'const' || token.type === 'var' || ['>', '<', '==='].includes(token.label);
                                                    else if (['AND', 'OR'].includes(lastToken)) disabled = token.type === 'operator';

                                                    return (
                                                        <button
                                                            key={token.id}
                                                            type="button"
                                                            className={`token-chip ${token.type}`}
                                                            onClick={() => addToken(token)}
                                                            disabled={disabled}
                                                        >
                                                            <span style={{ width: '8px', height: '8px', borderRadius: '50%', background: token.color }}></span>
                                                            {token.label}
                                                        </button>
                                                    );
                                                })}
                                            </div>
                                            <div style={{ display: 'flex', gap: '8px' }}>
                                                <button type="button" className="btn btn-outline" onClick={removeLastToken} style={{ fontSize: '13px', padding: '8px 16px' }}>
                                                    <i className="fas fa-undo-alt"></i> 뒤로
                                                </button>
                                                <button type="button" className="btn btn-outline" onClick={() => setFormData(p => ({ ...p, trigger_condition: '' }))} style={{ fontSize: '13px', padding: '8px 16px', color: '#ef4444' }}>
                                                    <i className="fas fa-trash-alt"></i> 전체 삭제
                                                </button>
                                            </div>
                                        </div>

                                        <div className="expression-box">
                                            {formData.trigger_condition || '빌더를 사용하여 로직을 구성하세요'}
                                        </div>

                                        <div className="quick-logic-patterns">
                                            {QUICK_LOGIC_PATTERNS.map(pattern => (
                                                <div key={pattern.id} className="pattern-btn" onClick={() => setFormData(p => ({ ...p, trigger_condition: pattern.logic }))}>
                                                    <div className="pattern-title">{pattern.title}</div>
                                                    <div className="pattern-logic">{pattern.logic}</div>
                                                    <div style={{ fontSize: '11px', color: '#94a3b8', marginTop: '4px' }}>{pattern.desc}</div>
                                                </div>
                                            ))}
                                        </div>
                                    </div>
                                </div>
                            </div>
                        )}

                        {step === 3 && (
                            <div className="wizard-slide-active">
                                <div className="modal-form-grid" style={{ padding: 0, gap: '24px' }}>
                                    <div className="form-column">
                                        <div className="form-section">
                                            <div className="section-title">
                                                <i className="fas fa-exclamation-circle" style={{ color: '#f59e0b' }}></i>
                                                중요도 설정 (Severity Intensity)
                                            </div>
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
                                            <div className="section-title">
                                                <i className="fas fa-sliders-h" style={{ color: 'var(--blueprint-500)' }}></i>
                                                임계값 및 오차 설정
                                            </div>
                                            <div className="form-row two-col">
                                                <div className="form-group">
                                                    <label className="form-label">High Limit (상한값)</label>
                                                    <input type="number" className="form-input" placeholder="80"
                                                        value={formData.default_config?.high_limit}
                                                        onChange={e => setFormData(p => ({ ...p, default_config: { ...p.default_config, high_limit: e.target.value } as any }))} />
                                                </div>
                                                <div className="form-group">
                                                    <label className="form-label">Low Limit (하한값)</label>
                                                    <input type="number" className="form-input" placeholder="20"
                                                        value={formData.default_config?.low_limit}
                                                        onChange={e => setFormData(p => ({ ...p, default_config: { ...p.default_config, low_limit: e.target.value } as any }))} />
                                                </div>
                                            </div>
                                            <div className="form-group" style={{ marginTop: '16px' }}>
                                                <label className="form-label">데드밴드 (Deadband)</label>
                                                <input type="number" className="form-input" placeholder="0"
                                                    value={formData.default_config?.deadband}
                                                    onChange={e => setFormData(p => ({ ...p, default_config: { ...p.default_config, deadband: Number(e.target.value) } as any }))} />
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        )}

                        {step === 4 && (
                            <div className="wizard-slide-active">
                                <div className="step4-finalize-container">
                                    {/* Left Column: Message Composer */}
                                    <div className="finalize-left-panel">
                                        <div className="form-section">
                                            <div className="section-title">
                                                <i className="fas fa-comment-alt" style={{ color: '#3b82f6' }}></i>
                                                알람 메시지 구성 (Message Composer)
                                            </div>
                                            <div className="composer-container" style={{ background: '#f8fafc', padding: '24px', borderRadius: '16px', border: '1px solid #e2e8f0' }}>
                                                <label className="form-label" style={{ marginBottom: '12px' }}>전송 메시지 형식</label>
                                                <textarea
                                                    className="description-composer"
                                                    rows={4}
                                                    value={formData.message_template}
                                                    onChange={e => setFormData(p => ({ ...p, message_template: e.target.value }))}
                                                    placeholder="알람 발생 시 전송될 메시지 내용을 입력하세요..."
                                                />
                                                <div style={{ marginTop: '20px' }}>
                                                    <label className="form-label" style={{ fontSize: '11px', color: '#64748b' }}>사용 가능한 변수 (클릭하여 추가)</label>
                                                    <div className="variable-chip-group">
                                                        {['{device_name}', '{value}', '{limit}', '{time}'].map(v => (
                                                            <div key={v} className="var-badge" onClick={() => {
                                                                const current = formData.message_template || '';
                                                                setFormData(p => ({ ...p, message_template: current + ' ' + v }));
                                                            }}>
                                                                {v}
                                                            </div>
                                                        ))}
                                                    </div>
                                                </div>
                                            </div>
                                        </div>
                                    </div>

                                    {/* Right Column: Meta & Tags */}
                                    <div className="finalize-right-panel">
                                        <div className="form-section" style={{ marginBottom: '32px' }}>
                                            <div className="section-title">
                                                <i className="fas fa-info-circle" style={{ color: '#64748b' }}></i>
                                                템플릿 설명 및 비고
                                            </div>
                                            <textarea
                                                className="description-composer"
                                                rows={3}
                                                placeholder="엔지니어가 이 템플릿의 용도를 알 수 있게 설명해주세요..."
                                                value={formData.description}
                                                onChange={e => setFormData(p => ({ ...p, description: e.target.value }))}
                                            />
                                        </div>

                                        <div className="form-section">
                                            <div className="section-title">
                                                <i className="fas fa-tags" style={{ color: '#3b82f6' }}></i>
                                                관리 태그 (Tags)
                                            </div>
                                            <div className="modern-tag-container">
                                                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px', marginBottom: '12px' }}>
                                                    {formData.tags && formData.tags.length > 0 ? (
                                                        formData.tags.map(tag => (
                                                            <span key={tag} className="tag-badge">
                                                                #{tag}
                                                                <span className="remove-tag" onClick={() => removeTag(tag)}>&times;</span>
                                                            </span>
                                                        ))
                                                    ) : (
                                                        <span style={{ fontSize: '12px', color: '#94a3b8' }}>설정된 태그가 없습니다.</span>
                                                    )}
                                                </div>
                                                <input
                                                    ref={tagInputRef}
                                                    type="text"
                                                    className="tag-input-v2"
                                                    placeholder="태그를 입력하고 ENTER..."
                                                    onKeyDown={handleAddTag}
                                                />
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        )}
                    </div>

                    <div className="modal-footer" style={{ background: '#f8fafc', borderTop: '1px solid #f1f5f9' }}>
                        {step > 1 ? (
                            <button className="btn btn-outline" onClick={handleBack} style={{ width: '100px' }}>
                                <i className="fas fa-arrow-left"></i> 이전
                            </button>
                        ) : (
                            mode === 'edit' && onDelete && (
                                <button className="btn btn-outline-danger" onClick={() => onDelete(template!.id, template!.name)}>
                                    <i className="fas fa-trash-alt"></i> 템플릿 삭제
                                </button>
                            )
                        )}

                        <div style={{ marginLeft: 'auto', display: 'flex', gap: '12px' }}>
                            <button className="btn btn-ghost" onClick={onClose}>취소</button>
                            {step < 4 ? (
                                <button className="btn btn-primary" onClick={handleNext} disabled={!formData.name && step === 1} style={{ minWidth: '120px' }}>
                                    다음 단계 <i className="fas fa-arrow-right"></i>
                                </button>
                            ) : (
                                <button className="btn btn-primary" onClick={handleSave} style={{ minWidth: '160px' }}>
                                    <i className="fas fa-check"></i> 템플릿 저장 및 게시
                                </button>
                            )}
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default AlarmTemplateCreateEditModal;
