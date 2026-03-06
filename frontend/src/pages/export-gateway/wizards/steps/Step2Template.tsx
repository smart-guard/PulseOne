// ============================================================================
// wizards/steps/Step2Template.tsx
// Step 2: Payload 템플릿 선택 또는 생성 + Variable Explorer
// ============================================================================

import React from 'react';
import { Radio, Select, Input } from 'antd';
import { PayloadTemplate } from '../../../../api/services/exportGatewayApi';
import { NewTemplateData, VARIABLE_CATEGORIES, SAMPLE_DATA } from '../types';

interface Step2Props {
    tl: (key: string, opts?: any) => string;
    templates: PayloadTemplate[];
    templateMode: 'existing' | 'new';
    setTemplateMode: (mode: 'existing' | 'new') => void;
    selectedTemplateId: number | null;
    setSelectedTemplateId: (id: number | null) => void;
    newTemplateData: NewTemplateData;
    setNewTemplateData: (data: NewTemplateData) => void;
    templateEditMode: 'simple' | 'advanced';
    setTemplateEditMode: (mode: 'simple' | 'advanced') => void;
}

const Step2Template: React.FC<Step2Props> = ({
    tl, templates,
    templateMode, setTemplateMode,
    selectedTemplateId, setSelectedTemplateId,
    newTemplateData, setNewTemplateData,
    templateEditMode, setTemplateEditMode
}) => {
    return (
        <div>
            <Radio.Group
                value={templateMode}
                onChange={e => setTemplateMode(e.target.value)}
                style={{ marginBottom: '16px', width: '100%' }}
                buttonStyle="solid"
            >
                <Radio.Button value="existing" style={{ width: '50%', textAlign: 'center' }}>
                    {tl('labels.useExistingTemplate', { ns: 'dataExport' })}
                </Radio.Button>
                <Radio.Button value="new" style={{ width: '50%', textAlign: 'center' }}>
                    {tl('labels.defineNewTemplate', { ns: 'dataExport' })}
                </Radio.Button>
            </Radio.Group>

            {templateMode === 'existing' ? (
                /* ── 기존 템플릿 선택 ───────────────────────────────────────────── */
                <div style={{
                    padding: '24px',
                    textAlign: 'left',
                    background: 'white',
                    borderRadius: '12px',
                    height: '100%',
                    display: 'flex',
                    flexDirection: 'column',
                    boxShadow: '0 4px 12px rgba(0,0,0,0.05)',
                    border: '1px solid #f0f0f0'
                }}>
                    <div style={{ marginBottom: '12px', fontWeight: 700, fontSize: '15px', color: '#1a1a1a', display: 'flex', alignItems: 'center' }}>
                        <i className="fas fa-layer-group" style={{ marginRight: '8px', color: '#1890ff' }} />
                        {tl('gwWizard.selectTemplateSectionTitle', { ns: 'dataExport' })}
                    </div>
                    <Select
                        style={{ width: '100%', marginBottom: '20px' }}
                        size="large"
                        placeholder={tl('gwWizard.selectTemplatePh', { ns: 'dataExport' })}
                        value={selectedTemplateId}
                        onChange={setSelectedTemplateId}
                        options={templates.map(t => ({ value: t.id, label: t.name }))}
                        getPopupContainer={trigger => trigger.parentElement}
                    />
                    {selectedTemplateId && (
                        <div style={{ marginTop: '10px', flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                            <div style={{ marginBottom: '8px', fontSize: '12px', fontWeight: 700, color: '#8c8c8c', display: 'flex', alignItems: 'center' }}>
                                <i className="fas fa-code" style={{ marginRight: '6px' }} />
                                {tl('gwWizard.templatePreviewLabel', { ns: 'dataExport' })}
                            </div>
                            <div style={{
                                flex: 1,
                                overflow: 'auto',
                                background: '#282c34',
                                borderRadius: '10px',
                                padding: '20px',
                                border: '1px solid #1a1e26',
                                boxShadow: 'inset 0 4px 8px rgba(0,0,0,0.3)'
                            }}>
                                <pre style={{
                                    fontSize: '13px',
                                    margin: 0,
                                    whiteSpace: 'pre-wrap',
                                    wordBreak: 'break-all',
                                    fontFamily: '"Fira Code", "Source Code Pro", SFMono-Regular, Consolas, monospace',
                                    color: '#abb2bf',
                                    lineHeight: '1.6'
                                }}>
                                    {(() => {
                                        const raw = templates.find(t => t.id === selectedTemplateId)?.template_json;
                                        if (raw === undefined || raw === null) return tl('gwWizard.noTemplateSelected', { ns: 'dataExport' });
                                        try {
                                            const parsed = typeof raw === 'string' ? JSON.parse(raw) : raw;
                                            return JSON.stringify(parsed, null, 2);
                                        } catch {
                                            return typeof raw === 'string' ? raw : JSON.stringify(raw);
                                        }
                                    })()}
                                </pre>
                            </div>
                        </div>
                    )}
                </div>
            ) : (
                /* ── 새 템플릿 생성 ──────────────────────────────────────────────── */
                <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: '15px' }}>
                    {/* 이름 + 시스템 타입 */}
                    <div style={{ display: 'flex', gap: '15px' }}>
                        <div style={{ flex: 1 }}>
                            <div style={{ fontSize: '12px', fontWeight: 600, marginBottom: '4px' }}>{tl('labels.templateName', { ns: 'dataExport' })}</div>
                            <Input
                                size="large"
                                placeholder="e.g. Standard JSON Payload"
                                value={newTemplateData.name}
                                onChange={e => setNewTemplateData({ ...newTemplateData, name: e.target.value })}
                            />
                        </div>
                        <div style={{ flex: 1 }}>
                            <div style={{ fontSize: '12px', fontWeight: 600, marginBottom: '4px' }}>{tl('labels.systemType', { ns: 'dataExport' })}</div>
                            <Input
                                size="large"
                                placeholder="e.g. AWS IoT, MS Azure, etc."
                                value={newTemplateData.system_type}
                                onChange={e => setNewTemplateData({ ...newTemplateData, system_type: e.target.value })}
                            />
                        </div>
                    </div>

                    {/* 엔지니어 가이드 */}
                    <div style={{ padding: '12px', background: '#f0faff', borderRadius: '8px', border: '1px solid #e6f7ff', fontSize: '11px', color: '#0050b3', lineHeight: '1.6' }}>
                        <i className="fas fa-info-circle" style={{ marginRight: '8px' }} />
                        <strong>{tl('labels.engineerGuide', { ns: 'dataExport' })}</strong>{' '}
                        <code>{'{{target_key}}'}</code> to inject the profile&apos;s <b>{tl('labels.targetKey', { ns: 'dataExport' })}</b>.
                        For versatile design, a metadata-centric schema is recommended.
                    </div>

                    {/* 편집 모드 토글 */}
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '15px' }}>
                            <span style={{ fontWeight: 700, fontSize: '13px' }}>{tl('labels.payloadStructure', { ns: 'dataExport' })}</span>
                            <Radio.Group
                                size="small"
                                value={templateEditMode}
                                onChange={e => setTemplateEditMode(e.target.value)}
                            >
                                <Radio.Button value="simple">{tl('labels.builderSimple', { ns: 'dataExport' })}</Radio.Button>
                                <Radio.Button value="advanced">{tl('labels.codeAdvanced', { ns: 'dataExport' })}</Radio.Button>
                            </Radio.Group>
                        </div>
                    </div>

                    {/* Variable Tray */}
                    <div className="variable-tray-v2" style={{ padding: '8px', background: '#f5f5f5', borderRadius: '8px', border: '1px solid #eee' }}>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                            {VARIABLE_CATEGORIES.map(cat => (
                                <div key={cat.name} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <span style={{ fontSize: '10px', fontWeight: 700, color: '#999', minWidth: '80px' }}>{cat.name}</span>
                                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                                        {cat.items.map(v => (
                                            <button
                                                key={v.value}
                                                type="button"
                                                className="mgmt-badge neutral"
                                                style={{ cursor: 'pointer', border: '1px solid #ddd', fontSize: '9px', padding: '1px 6px', borderRadius: '4px', background: '#fff' }}
                                                title={v.desc}
                                                onClick={() => {
                                                    if (templateEditMode === 'advanced') {
                                                        const textarea = document.getElementById('new-template-textarea') as HTMLTextAreaElement;
                                                        if (textarea) {
                                                            const start = textarea.selectionStart;
                                                            const end = textarea.selectionEnd;
                                                            textarea.setRangeText(v.value, start, end, 'end');
                                                            textarea.focus();
                                                            setNewTemplateData({ ...newTemplateData, template_json: textarea.value });
                                                        }
                                                    }
                                                }}
                                            >
                                                {v.label}
                                            </button>
                                        ))}
                                    </div>
                                </div>
                            ))}
                        </div>
                    </div>

                    {/* 에디터 + 미리보기 */}
                    <div style={{ display: 'flex', gap: '15px', flex: 1, minHeight: 0 }}>
                        <div style={{ flex: 1.2, display: 'flex', flexDirection: 'column' }}>
                            {templateEditMode === 'advanced' ? (
                                <Input.TextArea
                                    id="new-template-textarea"
                                    style={{ flex: 1, fontFamily: 'monospace', fontSize: '13px', background: '#fcfcfc' }}
                                    value={typeof newTemplateData.template_json === 'string' ? newTemplateData.template_json : JSON.stringify(newTemplateData.template_json, null, 2)}
                                    onChange={e => setNewTemplateData({ ...newTemplateData, template_json: e.target.value })}
                                />
                            ) : (
                                <div style={{ flex: 1, border: '1px solid #d9d9d9', borderRadius: '6px', background: '#fff', padding: '20px', textAlign: 'center', color: '#999' }}>
                                    The wizard supports &apos;Advanced&apos; mode only.<br />
                                    For the detailed builder, use the [Template Management] tab.
                                </div>
                            )}
                        </div>
                        <div style={{ flex: 1, background: '#1e272e', borderRadius: '8px', padding: '12px', overflowY: 'auto' }}>
                            <div style={{ fontSize: '11px', color: '#55efc4', fontWeight: 700, marginBottom: '8px' }}>{tl('labels.preview', { ns: 'dataExport' })}</div>
                            <pre style={{ margin: 0, fontSize: '11px', color: '#abb2bf', whiteSpace: 'pre-wrap' }}>
                                {(() => {
                                    try {
                                        let preview = typeof newTemplateData.template_json === 'string'
                                            ? newTemplateData.template_json
                                            : JSON.stringify(newTemplateData.template_json, null, 2);
                                        Object.entries(SAMPLE_DATA).forEach(([k, v]) => {
                                            preview = preview.replace(new RegExp(`\\{\\{${k}\\}\\}`, 'g'), String(v));
                                            preview = preview.replace(new RegExp(`\\{${k}\\}`, 'g'), String(v));
                                        });
                                        try { return JSON.stringify(JSON.parse(preview), null, 2); } catch { return preview; }
                                    } catch { return 'JSON Error'; }
                                })()}
                            </pre>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
};

export default Step2Template;
