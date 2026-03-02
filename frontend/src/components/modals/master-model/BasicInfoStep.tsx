import React from 'react';
import { useTranslation } from 'react-i18next';
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
    const { t } = useTranslation(['deviceTemplates', 'common']);
    const { confirm } = useConfirmContext();

    return (
        <div className="mgmt-modal-form-grid">
            <div className="mgmt-modal-form-group mgmt-span-full">
                <label>Master Model Name *</label>
                <input
                    className="mgmt-input"
                    value={formData.name || ''}
                    onChange={e => setFormData({ ...formData, name: e.target.value })}
                    placeholder="e.g. ACS880 Standard Template"
                />
            </div>
            <div className="mgmt-modal-form-group">
                <label>Manufacturer *</label>
                <select
                    className="mgmt-select"
                    value={formData.manufacturer_id || ''}
                    onChange={e => setFormData({ ...formData, manufacturer_id: Number(e.target.value) })}
                >
                    <option value="">{t('labels.selectManufacturer', {ns: 'deviceTemplates'})}</option>
                    {manufacturers.map(m => <option key={m.id} value={m.id}>{m.name}</option>)}
                </select>
            </div>
            <div className="mgmt-modal-form-group">
                <label>Model Name *</label>
                <div style={{ display: 'flex', gap: '8px' }}>
                    <input
                        className="mgmt-input"
                        value={modelName}
                        onChange={e => setModelName(e.target.value)}
                        placeholder="e.g. ACS880"
                        style={{ flex: 1 }}
                    />
                    <button
                        className="mgmt-btn mgmt-btn-outline"
                        onClick={() => {
                            if (!modelName) {
                                confirm({
                                    title: 'Model Name Required',
                                    message: 'Please enter a model name before searching.',
                                    confirmButtonType: 'warning',
                                    showCancelButton: false
                                });
                                return;
                            }
                            const mName = formData.manufacturer_id ? manufacturers.find(m => m.id === formData.manufacturer_id)?.name : '';
                            const query = `${mName || ''} ${modelName} manual`.trim();
                            window.open(`https://www.google.com/search?q=${encodeURIComponent(query)}`, '_blank');
                        }}
                        title="Search for manual on Google"
                        style={{ whiteSpace: 'nowrap', display: 'flex', alignItems: 'center', gap: '6px', width: 'auto', flex: 'none' }}
                    >
                        <i className="fab fa-google"></i> Search
                    </button>
                </div>
            </div>
            <div className="mgmt-modal-form-group">
                <label>{t('labels.modelNumber', {ns: 'deviceTemplates'})}</label>
                <input
                    className="mgmt-input"
                    value={formData.model_number || ''}
                    onChange={e => setFormData({ ...formData, model_number: e.target.value })}
                    placeholder="e.g. ACS880-01-045A-4"
                />
            </div>
            <div className="mgmt-modal-form-group mgmt-span-full">
                <label>참조 매뉴얼 URL</label>
                <input
                    className="mgmt-input"
                    value={manualUrl}
                    onChange={e => setManualUrl(e.target.value)}
                    placeholder="Search된 매뉴얼의 URL을 복사하여 입력하세요 (예: https://...)"
                />
            </div>
            <div className="mgmt-modal-form-group">
                <label>디바이스 타입</label>
                <select
                    className="mgmt-select"
                    value={formData.device_type || 'PLC'}
                    onChange={e => setFormData({ ...formData, device_type: e.target.value })}
                >
                    <option value="PLC">{t('labels.plc', {ns: 'deviceTemplates'})}</option>
                    <option value="INVERTER">{t('labels.inverter', {ns: 'deviceTemplates'})}</option>
                    <option value="SENSOR">{t('labels.sensor', {ns: 'deviceTemplates'})}</option>
                    <option value="METER">{t('labels.meter', {ns: 'deviceTemplates'})}</option>
                    <option value="GATEWAY">{t('labels.gateway', {ns: 'deviceTemplates'})}</option>
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
