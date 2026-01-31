import React from 'react';
import { DeviceTemplate, Manufacturer, Protocol } from '../../../types/manufacturing';
import { useConfirmContext } from '../../common/ConfirmProvider';

interface BasicInfoStepProps {
    formData: Partial<DeviceTemplate>;
    setFormData: (data: Partial<DeviceTemplate>) => void;
    modelName: string;
    setModelName: (name: string) => void;
    manualUrl: string;
    setManualUrl: (url: string) => void;
    manufacturers: Manufacturer[];
    protocols: Protocol[];
    handleProtocolChange: (protocolId: number) => void;
}

const BasicInfoStep: React.FC<BasicInfoStepProps> = ({
    formData,
    setFormData,
    modelName,
    setModelName,
    manualUrl,
    setManualUrl,
    manufacturers,
    protocols,
    handleProtocolChange
}) => {
    const { confirm } = useConfirmContext();

    return (
        <div className="mgmt-modal-form-grid">
            <div className="mgmt-modal-form-group mgmt-span-full">
                <label>마스터 모델 이름 *</label>
                <input
                    className="mgmt-input"
                    value={formData.name || ''}
                    onChange={e => setFormData({ ...formData, name: e.target.value })}
                    placeholder="예: ACS880 Standard Template"
                />
            </div>
            <div className="mgmt-modal-form-group">
                <label>제조사 *</label>
                <select
                    className="mgmt-select"
                    value={formData.manufacturer_id || ''}
                    onChange={e => setFormData({ ...formData, manufacturer_id: Number(e.target.value) })}
                >
                    <option value="">제조사 선택</option>
                    {manufacturers.map(m => <option key={m.id} value={m.id}>{m.name}</option>)}
                </select>
            </div>
            <div className="mgmt-modal-form-group">
                <label>모델명 *</label>
                <div style={{ display: 'flex', gap: '8px' }}>
                    <input
                        className="mgmt-input"
                        value={modelName}
                        onChange={e => setModelName(e.target.value)}
                        placeholder="예: ACS880"
                        style={{ flex: 1 }}
                    />
                    <button
                        className="mgmt-btn mgmt-btn-outline"
                        onClick={() => {
                            if (!modelName) {
                                confirm({
                                    title: '모델명 입력 필요',
                                    message: '모델명을 입력 후 검색해주세요.',
                                    confirmButtonType: 'warning',
                                    showCancelButton: false
                                });
                                return;
                            }
                            const mName = formData.manufacturer_id ? manufacturers.find(m => m.id === formData.manufacturer_id)?.name : '';
                            const query = `${mName || ''} ${modelName} manual`.trim();
                            window.open(`https://www.google.com/search?q=${encodeURIComponent(query)}`, '_blank');
                        }}
                        title="구글에서 매뉴얼 검색"
                        style={{ whiteSpace: 'nowrap', display: 'flex', alignItems: 'center', gap: '6px', width: 'auto', flex: 'none' }}
                    >
                        <i className="fab fa-google"></i> 검색
                    </button>
                </div>
            </div>
            <div className="mgmt-modal-form-group mgmt-span-full">
                <label>참조 매뉴얼 URL</label>
                <input
                    className="mgmt-input"
                    value={manualUrl}
                    onChange={e => setManualUrl(e.target.value)}
                    placeholder="검색된 매뉴얼의 URL을 복사하여 입력하세요 (예: https://...)"
                />
            </div>
            <div className="mgmt-modal-form-group">
                <label>디바이스 타입</label>
                <select
                    className="mgmt-select"
                    value={formData.device_type || 'PLC'}
                    onChange={e => setFormData({ ...formData, device_type: e.target.value })}
                >
                    <option value="PLC">PLC</option>
                    <option value="INVERTER">INVERTER</option>
                    <option value="SENSOR">SENSOR</option>
                    <option value="METER">METER</option>
                    <option value="GATEWAY">GATEWAY</option>
                </select>
            </div>
            <div className="mgmt-modal-form-group">
                <label>프로토콜 *</label>
                <select
                    className="mgmt-select"
                    value={formData.protocol_id || ''}
                    onChange={e => handleProtocolChange(Number(e.target.value))}
                >
                    <option value="">프로토콜 선택</option>
                    {protocols.map(p => <option key={p.id} value={p.id}>{p.display_name}</option>)}
                </select>
            </div>
            <div className="mgmt-modal-form-group">
                <label>수집 주기 (ms)</label>
                <input
                    type="number"
                    className="mgmt-input"
                    value={formData.polling_interval || 1000}
                    onChange={e => setFormData({ ...formData, polling_interval: Number(e.target.value) })}
                />
            </div>
            <div className="mgmt-modal-form-group">
                <label>타임아웃 (ms)</label>
                <input
                    type="number"
                    className="mgmt-input"
                    value={formData.timeout || 3000}
                    onChange={e => setFormData({ ...formData, timeout: Number(e.target.value) })}
                />
            </div>
            <div className="mgmt-modal-form-group mgmt-span-full">
                <label>설명</label>
                <textarea
                    className="mgmt-input"
                    rows={3}
                    value={formData.description || ''}
                    onChange={e => setFormData({ ...formData, description: e.target.value })}
                />
            </div>
        </div>
    );
};

export default BasicInfoStep;
