// ============================================================================
// wizards/steps/Step3Target.tsx
// Step 3: 전송 타겟 선택 또는 생성 (HTTP / MQTT / S3)
// ============================================================================

import React from 'react';
import { Radio, Select, Input, Button, Tag, Checkbox } from 'antd';
import { ExportTarget } from '../../../../api/services/exportGatewayApi';
import { TargetFormData } from '../types';

interface Step3Props {
    tl: (key: string, opts?: any) => string;
    targets: ExportTarget[];
    targetMode: 'existing' | 'new';
    setTargetMode: (mode: 'existing' | 'new') => void;
    selectedProtocols: string[];
    setSelectedProtocols: (protocols: string[]) => void;
    targetData: TargetFormData;
    setTargetData: (data: TargetFormData) => void;
    selectedTargetIds: number[];
    setSelectedTargetIds: (ids: number[]) => void;
    editingTargets: Record<number, any>;
    setEditingTargets: (targets: Record<number, any>) => void;
    updateTargetPriority: (type: 'existing' | 'new', idOrProto: number | string, index: number, newPriority: number) => void;
    totalTargetCount: number;
}

const Step3Target: React.FC<Step3Props> = ({
    tl, targets,
    targetMode, setTargetMode,
    selectedProtocols, setSelectedProtocols,
    targetData, setTargetData,
    selectedTargetIds, setSelectedTargetIds,
    editingTargets, setEditingTargets,
    updateTargetPriority, totalTargetCount
}) => {
    return (
        <div style={{ height: '480px', display: 'flex', flexDirection: 'column' }}>
            <Radio.Group
                value={targetMode}
                onChange={e => setTargetMode(e.target.value)}
                style={{ marginBottom: '16px' }}
            >
                <Radio.Button value="existing">{tl('labels.connectExistingTarget', { ns: 'dataExport' })}</Radio.Button>
                <Radio.Button value="new">{tl('labels.createNewTarget', { ns: 'dataExport' })}</Radio.Button>
            </Radio.Group>

            {targetMode === 'existing' ? (
                /* ── 기존 타겟 연결 ─────────────────────────────────────────────── */
                <div style={{ padding: '24px', textAlign: 'left', background: 'white', borderRadius: '8px', minHeight: '100%', display: 'flex', flexDirection: 'column' }}>
                    <div style={{ marginBottom: '8px', fontWeight: 600 }}>{tl('labels.selectEditTarget', { ns: 'dataExport' })}</div>
                    <Select
                        mode="multiple"
                        style={{ width: '100%' }}
                        placeholder="Select target (multi-select allowed)"
                        value={selectedTargetIds}
                        onChange={setSelectedTargetIds}
                        options={targets.map(t => ({ value: t.id, label: `${t.name} [${t.target_type}]` }))}
                        getPopupContainer={trigger => trigger.parentElement}
                    />

                    {selectedTargetIds.length > 0 && (
                        <div style={{ marginTop: '16px', flex: 1, overflowY: 'auto', border: '1px solid #f0f0f0', borderRadius: '8px', background: '#fafafa', padding: '12px' }}>
                            <div style={{ fontSize: '12px', color: '#888', marginBottom: '8px' }}>
                                Selected Target Settings ({selectedTargetIds.length})
                            </div>
                            {selectedTargetIds
                                .sort((a, b) => {
                                    const orderA = editingTargets[a]?.[0]?.execution_order || 100;
                                    const orderB = editingTargets[b]?.[0]?.execution_order || 100;
                                    return orderA - orderB;
                                })
                                .map(tid => {
                                    const t = targets.find(yt => yt.id === tid);
                                    if (!t) return null;
                                    const configs = editingTargets[tid] || [];
                                    return (
                                        <div key={tid} style={{ background: 'white', border: '1px solid #eee', borderRadius: '6px', padding: '16px', marginBottom: '12px' }}>
                                            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px', borderBottom: '1px solid #f5f5f5', paddingBottom: '8px', alignItems: 'center' }}>
                                                <div>
                                                    <strong style={{ color: '#1890ff', marginRight: '8px' }}>{t.name}</strong>
                                                    <Tag bordered={false} color={t.target_type === 'HTTP' ? 'blue' : t.target_type === 'S3' ? 'orange' : 'default'}>{t.target_type}</Tag>
                                                </div>
                                                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                    <span style={{ fontSize: '12px', color: '#666' }}>{tl('labels.priority', { ns: 'dataExport' })}</span>
                                                    <Select
                                                        size="small"
                                                        style={{ width: '60px' }}
                                                        value={configs[0]?.execution_order || 1}
                                                        getPopupContainer={trigger => trigger.parentElement}
                                                        onChange={(val) => updateTargetPriority('existing', tid, 0, val)}
                                                    >
                                                        {Array.from({ length: totalTargetCount }).map((_, idx) => (
                                                            <Select.Option key={idx + 1} value={idx + 1}>{idx + 1}</Select.Option>
                                                        ))}
                                                    </Select>
                                                </div>
                                            </div>

                                            {configs.map((c: any, cIdx: number) => (
                                                <div key={cIdx} style={{ marginBottom: '12px' }}>
                                                    {t.target_type === 'HTTP' && (
                                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                                            <div style={{ display: 'flex', gap: '8px' }}>
                                                                <Select
                                                                    style={{ width: '100px' }}
                                                                    value={c.method}
                                                                    onChange={val => {
                                                                        const next = { ...editingTargets };
                                                                        next[tid][cIdx] = { ...c, method: val };
                                                                        setEditingTargets(next);
                                                                    }}
                                                                >
                                                                    <Select.Option value="POST">{tl('labels.post', { ns: 'dataExport' })}</Select.Option>
                                                                    <Select.Option value="PUT">{tl('labels.put', { ns: 'dataExport' })}</Select.Option>
                                                                </Select>
                                                                <Input
                                                                    placeholder="Endpoint URL"
                                                                    value={c.url}
                                                                    onChange={e => {
                                                                        const next = { ...editingTargets };
                                                                        next[tid][cIdx] = { ...c, url: e.target.value };
                                                                        setEditingTargets(next);
                                                                    }}
                                                                />
                                                            </div>
                                                            <Input.Password
                                                                placeholder="Authorization Header (Token / Signature)"
                                                                value={c.headers?.Authorization || ''}
                                                                onChange={e => {
                                                                    const next = { ...editingTargets };
                                                                    next[tid][cIdx] = { ...c, headers: { ...c.headers, Authorization: e.target.value } };
                                                                    setEditingTargets(next);
                                                                }}
                                                            />
                                                            <Input.Password
                                                                placeholder="Auth key (x-api-key or Token)"
                                                                value={c.auth?.apiKey || ''}
                                                                onChange={e => {
                                                                    const next = { ...editingTargets };
                                                                    next[tid][cIdx] = { ...c, auth: { ...c.auth, type: 'x-api-key', apiKey: e.target.value } };
                                                                    setEditingTargets(next);
                                                                }}
                                                            />
                                                        </div>
                                                    )}
                                                    {t.target_type === 'S3' && (
                                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                                            <Input
                                                                placeholder="S3 Service URL"
                                                                value={c.S3ServiceUrl}
                                                                onChange={e => {
                                                                    const next = { ...editingTargets };
                                                                    next[tid][cIdx] = { ...c, S3ServiceUrl: e.target.value };
                                                                    setEditingTargets(next);
                                                                }}
                                                            />
                                                            <div style={{ display: 'flex', gap: '8px' }}>
                                                                <Input placeholder="Bucket Name" value={c.BucketName} onChange={e => { const next = { ...editingTargets }; next[tid][cIdx] = { ...c, BucketName: e.target.value }; setEditingTargets(next); }} />
                                                                <Input placeholder="Folder Path" value={c.Folder} onChange={e => { const next = { ...editingTargets }; next[tid][cIdx] = { ...c, Folder: e.target.value }; setEditingTargets(next); }} />
                                                            </div>
                                                            <div style={{ display: 'flex', gap: '8px' }}>
                                                                <Input.Password placeholder="Access Key ID" value={c.AccessKeyID} onChange={e => { const next = { ...editingTargets }; next[tid][cIdx] = { ...c, AccessKeyID: e.target.value }; setEditingTargets(next); }} />
                                                                <Input.Password placeholder="Secret Access Key" value={c.SecretAccessKey} onChange={e => { const next = { ...editingTargets }; next[tid][cIdx] = { ...c, SecretAccessKey: e.target.value }; setEditingTargets(next); }} />
                                                            </div>
                                                        </div>
                                                    )}
                                                    {t.target_type === 'MQTT' && (
                                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                                            <Input placeholder="Broker URL" value={c.url} onChange={e => { const next = { ...editingTargets }; next[tid][cIdx] = { ...c, url: e.target.value }; setEditingTargets(next); }} />
                                                            <Input placeholder="Topic" value={c.topic} onChange={e => { const next = { ...editingTargets }; next[tid][cIdx] = { ...c, topic: e.target.value }; setEditingTargets(next); }} />
                                                        </div>
                                                    )}
                                                </div>
                                            ))}
                                        </div>
                                    );
                                })}
                        </div>
                    )}
                </div>
            ) : (
                /* ── 새 타겟 생성 ───────────────────────────────────────────────── */
                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', height: '100%', overflowY: 'auto' }}>
                    <Input
                        placeholder="Target name (protocol suffix auto-added, e.g. TargetA → TargetA_HTTP)"
                        value={targetData.name}
                        onChange={e => setTargetData({ ...targetData, name: e.target.value })}
                    />
                    <div className="mgmt-modal-form-group">
                        <label>{tl('labels.transmissionProtocolMultiselect', { ns: 'dataExport' })}</label>
                        <Checkbox.Group
                            options={['HTTP', 'MQTT', 'S3']}
                            value={selectedProtocols}
                            onChange={list => setSelectedProtocols(list as string[])}
                        />
                    </div>

                    {/* HTTP */}
                    {selectedProtocols.includes('HTTP') && (
                        <div style={{ padding: '16px', border: '1px solid #ddd', borderRadius: '8px', background: 'white' }}>
                            <div style={{ fontWeight: 600, marginBottom: '12px', borderBottom: '1px solid #eee', paddingBottom: '8px' }}>
                                {tl('labels.httpConnectionSettings', { ns: 'dataExport' })}
                            </div>
                            {targetData.config_http.map((c: any, i: number) => (
                                <div key={i} style={{ marginBottom: '16px', padding: '12px', background: '#f9f9f9', borderRadius: '8px' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', alignItems: 'center' }}>
                                        <span style={{ fontWeight: 600, color: '#666', fontSize: '12px' }}>Mirror #{i + 1}</span>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <span style={{ fontSize: '11px', color: '#999' }}>{tl('labels.priority', { ns: 'dataExport' })}</span>
                                            <Select size="small" style={{ width: '60px' }} value={c.execution_order || i + 1} getPopupContainer={trigger => trigger.parentElement} onChange={(val) => updateTargetPriority('new', 'config_http', i, val)}>
                                                {Array.from({ length: totalTargetCount }).map((_, idx) => (<Select.Option key={idx + 1} value={idx + 1}>{idx + 1}</Select.Option>))}
                                            </Select>
                                        </div>
                                    </div>
                                    <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
                                        <Select style={{ width: '100px' }} value={c.method} getPopupContainer={trigger => trigger.parentElement} onChange={val => { const next = [...targetData.config_http]; next[i].method = val; setTargetData({ ...targetData, config_http: next }); }}>
                                            <Select.Option value="POST">{tl('labels.post', { ns: 'dataExport' })}</Select.Option>
                                            <Select.Option value="PUT">{tl('labels.put', { ns: 'dataExport' })}</Select.Option>
                                        </Select>
                                        <Input placeholder="Endpoint URL (e.g. http://api.server.com/ingest)" value={c.url} onChange={e => { const next = [...targetData.config_http]; next[i].url = e.target.value; setTargetData({ ...targetData, config_http: next }); }} />
                                    </div>
                                    <div style={{ marginBottom: '8px' }}>
                                        <Input.Password placeholder="Authorization Header (Token / Signature)" value={c.headers?.['Authorization'] || ''} onChange={e => { const next = [...targetData.config_http]; if (!next[i].headers) next[i].headers = {}; if (e.target.value) { next[i].headers['Authorization'] = e.target.value; } else { delete next[i].headers['Authorization']; } setTargetData({ ...targetData, config_http: next }); }} />
                                    </div>
                                    <div style={{ marginBottom: '8px' }}>
                                        <Input.Password placeholder="Auth key (x-api-key or Token)" value={c.auth?.apiKey || ''} onChange={e => { const next = [...targetData.config_http]; if (!next[i].auth) next[i].auth = { type: 'x-api-key' }; next[i].auth.apiKey = e.target.value; if (!next[i].headers) next[i].headers = {}; if (e.target.value) { next[i].headers['x-api-key'] = e.target.value; } else { delete next[i].headers['x-api-key']; } setTargetData({ ...targetData, config_http: next }); }} />
                                    </div>
                                    {targetData.config_http.length > 1 && (
                                        <Button danger size="small" onClick={() => { const next = targetData.config_http.filter((_, idx) => idx !== i); setTargetData({ ...targetData, config_http: next }); }}>{tl('delete', { ns: 'common' })}</Button>
                                    )}
                                </div>
                            ))}
                            <Button type="dashed" block onClick={() => setTargetData({ ...targetData, config_http: [...targetData.config_http, { url: '', method: 'POST', auth: { type: 'x-api-key', apiKey: '' }, headers: {}, execution_order: totalTargetCount + 1 }] })}>
                                + Add HTTP Target/Mirror
                            </Button>
                        </div>
                    )}

                    {/* MQTT */}
                    {selectedProtocols.includes('MQTT') && (
                        <div style={{ padding: '16px', border: '1px solid #ddd', borderRadius: '8px', background: 'white', marginTop: '16px' }}>
                            <div style={{ fontWeight: 600, marginBottom: '12px', borderBottom: '1px solid #eee', paddingBottom: '8px' }}>
                                {tl('labels.mqttConnectionSettings', { ns: 'dataExport' })}
                            </div>
                            {targetData.config_mqtt.map((c: any, i: number) => (
                                <div key={i} style={{ marginBottom: '16px', padding: '12px', background: '#f9f9f9', borderRadius: '8px' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', alignItems: 'center' }}>
                                        <span style={{ fontWeight: 600, color: '#666', fontSize: '12px' }}>MQTT Target #{i + 1}</span>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <span style={{ fontSize: '11px', color: '#999' }}>{tl('labels.priority', { ns: 'dataExport' })}</span>
                                            <Select size="small" style={{ width: '60px' }} value={c.execution_order || i + 1} getPopupContainer={trigger => trigger.parentElement} onChange={(val) => updateTargetPriority('new', 'config_mqtt', i, val)}>
                                                {Array.from({ length: totalTargetCount }).map((_, idx) => (<Select.Option key={idx + 1} value={idx + 1}>{idx + 1}</Select.Option>))}
                                            </Select>
                                        </div>
                                    </div>
                                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                        <Input placeholder="Broker URL (e.g. mqtt://broker.hivemq.com:1883)" value={c.url} onChange={e => { const next = [...targetData.config_mqtt]; next[i].url = e.target.value; setTargetData({ ...targetData, config_mqtt: next }); }} />
                                        <Input placeholder="Topic (e.g. pulseone/factory/data)" value={c.topic} onChange={e => { const next = [...targetData.config_mqtt]; next[i].topic = e.target.value; setTargetData({ ...targetData, config_mqtt: next }); }} />
                                    </div>
                                    {targetData.config_mqtt.length > 1 && (
                                        <Button danger size="small" style={{ marginTop: '8px' }} onClick={() => { const next = targetData.config_mqtt.filter((_, idx) => idx !== i); setTargetData({ ...targetData, config_mqtt: next }); }}>{tl('delete', { ns: 'common' })}</Button>
                                    )}
                                </div>
                            ))}
                            <Button type="dashed" block onClick={() => setTargetData({ ...targetData, config_mqtt: [...targetData.config_mqtt, { url: '', topic: 'pulseone/data', execution_order: totalTargetCount + 1 }] })}>
                                + Add MQTT Target
                            </Button>
                        </div>
                    )}

                    {/* S3 */}
                    {selectedProtocols.includes('S3') && (
                        <div style={{ padding: '16px', border: '1px solid #ddd', borderRadius: '8px', background: 'white', marginTop: '16px' }}>
                            <div style={{ fontWeight: 600, marginBottom: '12px', borderBottom: '1px solid #eee', paddingBottom: '8px' }}>
                                S3 / MinIO Connection Settings
                            </div>
                            {targetData.config_s3.map((c: any, i: number) => (
                                <div key={i} style={{ marginBottom: '16px', padding: '12px', background: '#f9f9f9', borderRadius: '8px' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', alignItems: 'center' }}>
                                        <span style={{ fontWeight: 600, color: '#666', fontSize: '12px' }}>S3 Target #{i + 1}</span>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <span style={{ fontSize: '11px', color: '#999' }}>{tl('labels.priority', { ns: 'dataExport' })}</span>
                                            <Select size="small" style={{ width: '60px' }} value={c.execution_order || i + 1} getPopupContainer={trigger => trigger.parentElement} onChange={(val) => updateTargetPriority('new', 'config_s3', i, val)}>
                                                {Array.from({ length: totalTargetCount }).map((_, idx) => (<Select.Option key={idx + 1} value={idx + 1}>{idx + 1}</Select.Option>))}
                                            </Select>
                                        </div>
                                    </div>
                                    <Input style={{ marginBottom: '8px' }} placeholder="S3 Endpoint / Service URL (e.g. https://s3.ap-northeast-2.amazonaws.com)" value={c.endpoint || c.S3ServiceUrl || ''} onChange={e => { const next = [...targetData.config_s3]; next[i].endpoint = e.target.value; setTargetData({ ...targetData, config_s3: next }); }} />
                                    <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
                                        <Input placeholder="Bucket Name" value={c.BucketName} onChange={e => { const next = [...targetData.config_s3]; next[i].BucketName = e.target.value; setTargetData({ ...targetData, config_s3: next }); }} />
                                        <Input placeholder="Folder Path" value={c.Folder} onChange={e => { const next = [...targetData.config_s3]; next[i].Folder = e.target.value; setTargetData({ ...targetData, config_s3: next }); }} />
                                    </div>
                                    <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
                                        <Input.Password placeholder="Access Key ID" value={c.AccessKeyID} onChange={e => { const next = [...targetData.config_s3]; next[i].AccessKeyID = e.target.value; setTargetData({ ...targetData, config_s3: next }); }} />
                                        <Input.Password placeholder="Secret Access Key" value={c.SecretAccessKey} onChange={e => { const next = [...targetData.config_s3]; next[i].SecretAccessKey = e.target.value; setTargetData({ ...targetData, config_s3: next }); }} />
                                    </div>
                                    {targetData.config_s3.length > 1 && (
                                        <Button danger size="small" onClick={() => { const next = targetData.config_s3.filter((_, idx) => idx !== i); setTargetData({ ...targetData, config_s3: next }); }}>{tl('delete', { ns: 'common' })}</Button>
                                    )}
                                </div>
                            ))}
                            <Button type="dashed" block onClick={() => setTargetData({ ...targetData, config_s3: [...targetData.config_s3, { endpoint: '', BucketName: '', Folder: '', AccessKeyID: '', SecretAccessKey: '', execution_order: totalTargetCount + 1 }] })}>
                                + Add S3 Storage
                            </Button>
                        </div>
                    )}
                </div>
            )}
        </div>
    );
};

export default Step3Target;
