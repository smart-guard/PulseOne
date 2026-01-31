import React from 'react';
import { Protocol, DeviceTemplate } from '../../../types/manufacturing';

interface ProtocolConfigStepProps {
    protocolConfig: any;
    setProtocolConfig: (config: any) => void;
    formData: Partial<DeviceTemplate>;
    setFormData: (data: Partial<DeviceTemplate>) => void;
    protocols: Protocol[];
}

const ProtocolConfigStep: React.FC<ProtocolConfigStepProps> = ({
    protocolConfig,
    setProtocolConfig,
    formData,
    setFormData,
    protocols
}) => {
    return (
        <div className="protocol-config-section">
            <div className="alert alert-info" style={{ marginBottom: '20px' }}>
                <i className="fas fa-microchip"></i> <strong>{protocols.find(p => p.id === formData.protocol_id)?.display_name}</strong> 프로토콜에 필요한 속성값입니다. 필수 항목(*)을 확인해주세요.
            </div>

            <div className="mgmt-modal-form-grid">
                {(() => {
                    const selectedP = protocols.find(p => p.id === formData.protocol_id);
                    const schema = selectedP?.connection_params;
                    if (!selectedP || !schema) return <div className="mgmt-span-full text-muted">이 프로토콜은 추가 설정이 필요하지 않습니다.</div>;

                    const schemaObj = typeof schema === 'string' ? JSON.parse(schema) : schema;

                    return Object.keys(schemaObj).map(key => {
                        const field = schemaObj[key];
                        return (
                            <div key={key} className="mgmt-modal-form-group">
                                <label>{key.replace(/_/g, ' ').toUpperCase()}{field.required ? ' *' : ''}</label>
                                {field.type === 'integer' || field.type === 'number' ? (
                                    <input
                                        type="number"
                                        className="mgmt-input"
                                        value={protocolConfig[key] ?? field.default ?? ''}
                                        onChange={e => setProtocolConfig({ ...protocolConfig, [key]: Number(e.target.value) })}
                                        placeholder={field.default?.toString()}
                                    />
                                ) : field.options ? (
                                    <select
                                        className="mgmt-select"
                                        value={protocolConfig[key] ?? field.default ?? ''}
                                        onChange={e => setProtocolConfig({ ...protocolConfig, [key]: e.target.value })}
                                    >
                                        {field.options.map((opt: string) => <option key={opt} value={opt}>{opt}</option>)}
                                    </select>
                                ) : (
                                    <input
                                        type="text"
                                        className="mgmt-input"
                                        value={protocolConfig[key] ?? field.default ?? ''}
                                        onChange={e => setProtocolConfig({ ...protocolConfig, [key]: e.target.value })}
                                        placeholder={field.default}
                                    />
                                )}
                            </div>
                        );
                    });
                })()}
            </div>

            <div style={{ marginTop: '32px', paddingTop: '24px', borderTop: '1px dashed #e2e8f0' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                    <h4 style={{ fontSize: '14px', margin: 0, color: '#1e293b' }}>
                        <i className="fas fa-sync-alt"></i> 공통 통신 및 재시도 정책
                    </h4>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <span style={{ fontSize: '12px', color: '#64748b' }}>신뢰성 프로필:</span>
                        <select
                            className="mgmt-select sm"
                            style={{ width: '140px' }}
                            onChange={(e) => {
                                const val = e.target.value;
                                if (val === 'critical') {
                                    setFormData({ ...formData, polling_interval: 100, timeout: 500 });
                                    setProtocolConfig({
                                        ...protocolConfig,
                                        max_retry_count: 5,
                                        retry_interval_ms: 100,
                                        backoff_time_ms: 10000,
                                    });
                                } else if (val === 'standard') {
                                    setFormData({ ...formData, polling_interval: 1000, timeout: 3000 });
                                    setProtocolConfig({
                                        ...protocolConfig,
                                        max_retry_count: 3,
                                        retry_interval_ms: 1000,
                                        backoff_time_ms: 30000,
                                    });
                                } else if (val === 'relaxed') {
                                    setFormData({ ...formData, polling_interval: 5000, timeout: 10000 });
                                    setProtocolConfig({
                                        ...protocolConfig,
                                        max_retry_count: 3,
                                        retry_interval_ms: 5000,
                                        backoff_time_ms: 60000,
                                    });
                                }
                            }}
                        >
                            <option value="">(직접 설정)</option>
                            <option value="critical">Critical (100ms)</option>
                            <option value="standard">Standard (1s)</option>
                            <option value="relaxed">Relaxed (5s)</option>
                        </select>
                    </div>
                </div>

                <div className="mgmt-modal-form-grid">
                    <div className="mgmt-modal-form-group">
                        <label>최대 재시도 횟수</label>
                        <input
                            type="number"
                            className="mgmt-input"
                            value={protocolConfig.max_retry_count ?? 3}
                            onChange={(e) => setProtocolConfig({ ...protocolConfig, max_retry_count: Number(e.target.value) })}
                        />
                    </div>
                    <div className="mgmt-modal-form-group">
                        <label>재시도 간격 (ms)</label>
                        <input
                            type="number"
                            className="mgmt-input"
                            value={protocolConfig.retry_interval_ms ?? 1000}
                            onChange={(e) => setProtocolConfig({ ...protocolConfig, retry_interval_ms: Number(e.target.value) })}
                        />
                    </div>
                    <div className="mgmt-modal-form-group">
                        <label>재시도 백오프 (ms)</label>
                        <input
                            type="number"
                            className="mgmt-input"
                            value={protocolConfig.backoff_time_ms ?? 30000}
                            onChange={(e) => setProtocolConfig({ ...protocolConfig, backoff_time_ms: Number(e.target.value) })}
                        />
                    </div>
                </div>
            </div>
        </div>
    );
};

export default ProtocolConfigStep;
