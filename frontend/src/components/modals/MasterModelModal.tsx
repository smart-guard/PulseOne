import React, { useState, useEffect } from 'react';
import { createPortal } from 'react-dom';
import { DeviceTemplate, TemplatePoint, Manufacturer, Protocol } from '../../types/manufacturing';
import { TemplateApiService } from '../../api/services/templateApi';
import { ManufactureApiService } from '../../api/services/manufactureApi';
import { ProtocolApiService } from '../../api/services/protocolApi';
import { useConfirmContext } from '../common/ConfirmProvider';
import DeviceDataPointsBulkModal from './DeviceModal/DeviceDataPointsBulkModal';
import '../../styles/management.css';

interface MasterModelModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSuccess: () => void;
    template: DeviceTemplate | null; // null if creating new
    onDelete?: (template: DeviceTemplate) => void;
    manufacturers?: Manufacturer[];
    protocols?: Protocol[];
}

const MasterModelModal: React.FC<MasterModelModalProps> = ({
    isOpen,
    onClose,
    onSuccess,
    template,
    onDelete,
    manufacturers: propsManufacturers = [],
    protocols: propsProtocols = []
}) => {
    const { confirm } = useConfirmContext();
    const isEdit = !!template;
    const [loading, setLoading] = useState(false);
    const [step, setStep] = useState(1);

    // Form Data
    const [formData, setFormData] = useState<Partial<DeviceTemplate>>({
        name: '',
        description: '',
        manufacturer_id: 0,
        device_type: 'PLC',
        protocol_id: 1,
        polling_interval: 1000,
        timeout: 3000,
        version: '1.0.0',
        is_public: true
    });

    const [modelName, setModelName] = useState('');
    const [manualUrl, setManualUrl] = useState(''); // New State
    const [dataPoints, setDataPoints] = useState<Partial<TemplatePoint>[]>([]);
    const [protocolConfig, setProtocolConfig] = useState<any>({});
    const [isBulkModalOpen, setIsBulkModalOpen] = useState(false);
    const [configInput, setConfigInput] = useState('{\n  \n}');

    useEffect(() => {
        if (isOpen) {
            if (template) {
                // Ensure manufacturer_id is retrieved from any possible property (snake_case, camelCase, or nested)
                const mId = template.manufacturer_id
                    || (template as any).manufacturerId
                    || (template as any).manufacturer?.id
                    || 0;

                setFormData({
                    ...template,
                    manufacturer_id: mId
                });
                setModelName(template.model_name || '');
                setManualUrl(template.manual_url || ''); // Init from template
                setDataPoints(template.data_points || []);
                setProtocolConfig(template.config || {});
            } else {
                setFormData({
                    name: '',
                    description: '',
                    manufacturer_id: 0,
                    device_type: 'PLC',
                    protocol_id: 1,
                    polling_interval: 1000,
                    timeout: 3000,
                    version: '1.0.0',
                    is_public: true
                });
                setModelName('');
                setManualUrl(''); // Reset
                setDataPoints([]);
                setProtocolConfig({});
            }
            setStep(1);
        }
    }, [isOpen, template]);

    // Handle Protocol Change to set defaults
    const handleProtocolChange = (pId: number) => {
        const selectedP = propsProtocols.find(p => p.id === pId);
        setFormData({ ...formData, protocol_id: pId });

        if (selectedP && !isEdit) {
            // Pre-populate defaults from protocol schema if not editing
            try {
                const defaults = typeof selectedP.default_config === 'string'
                    ? JSON.parse(selectedP.default_config)
                    : selectedP.default_config || {};
                setProtocolConfig(defaults);
            } catch (e) {
                console.error('Failed to parse protocol defaults:', e);
                setProtocolConfig({});
            }
        }
    };


    const handleSave = async () => {
        try {
            setLoading(true);

            // Validation (keep exiting validation)
            const requiredFields = ['name', 'manufacturer_id', 'device_type', 'protocol_id'];
            const missing = requiredFields.filter(field => !formData[field as keyof DeviceTemplate]);

            if (step === 1) { // Validate Model Name only if in step 1 context, but this is final submit
                if (!modelName) missing.push('model_name');
            }

            if (missing.length > 0 || !modelName) {
                // ... (keep error handling)
                if (!modelName && !missing.includes('model_name')) missing.push('model_name');

                if (missing.length > 0) {
                    confirm({
                        title: '필수 정보 누락',
                        message: `다음 필수 설정 항목이 누락되었습니다: ${missing.join(', ')}`,
                        confirmButtonType: 'warning',
                        showCancelButton: false
                    });
                    setLoading(false);
                    setStep(1); // Go back to step 1
                    return;
                }
            }

            // 0. Validate Protocol Config (Basic mandatory check)
            const selectedP = propsProtocols.find(p => p.id === formData.protocol_id);
            const schema = selectedP?.connection_params;
            if (selectedP && schema) {
                const schemaObj = typeof schema === 'string' ? JSON.parse(schema) : schema;
                const missingConfig = Object.keys(schemaObj).filter(key => schemaObj[key].required && !protocolConfig[key]);
                if (missingConfig.length > 0) {
                    confirm({
                        title: '설명 설정 누락',
                        message: `다음 필수 설정 항목이 누락되었습니다: ${missingConfig.join(', ')}`,
                        confirmButtonType: 'warning',
                        showCancelButton: false
                    });
                    setLoading(false);
                    setStep(3);
                    return;
                }
            }

            // 1. Ensure Device Model exists or create it
            let finalModelId = (template as any)?.model_id;

            // 0-1. Duplicate Point Check
            if (dataPoints.length > 0) {
                const names = dataPoints.map(p => p.name?.toLowerCase() || '');
                const addrs = dataPoints.map(p => String(p.address));

                const duplicateName = names.find((name, i) => name && names.indexOf(name) !== i);
                const duplicateAddr = addrs.find((addr, i) => addr && addrs.indexOf(addr) !== i);

                if (duplicateName || duplicateAddr) {
                    confirm({
                        title: '데이터 포인트 중복',
                        message: duplicateName
                            ? `중복된 이름이 존재합니다: ${duplicateName}`
                            : `중복된 주소가 존재합니다: ${duplicateAddr}`,
                        confirmButtonType: 'warning',
                        showCancelButton: false
                    });
                    setLoading(false);
                    setStep(2);
                    return;
                }
            }

            // 0-1. Duplicate Point Check
            if (dataPoints.length > 0) {
                const names = dataPoints.map(p => p.name?.toLowerCase() || '');
                const addrs = dataPoints.map(p => String(p.address));

                const duplicateName = names.find((name, i) => name && names.indexOf(name) !== i);
                const duplicateAddr = addrs.find((addr, i) => addr && addrs.indexOf(addr) !== i);

                if (duplicateName || duplicateAddr) {
                    confirm({
                        title: '데이터 포인트 중복',
                        message: duplicateName
                            ? `중복된 이름이 존재합니다: ${duplicateName}`
                            : `중복된 주소가 존재합니다: ${duplicateAddr}`,
                        confirmButtonType: 'warning',
                        showCancelButton: false
                    });
                    setLoading(false);
                    setStep(2);
                    return;
                }
            }

            // For now, always create or find model by name for simplicity in "Registration" flow
            // Ideally we check if model exists for this manufacturer
            if (!isEdit || modelName !== template?.model_name) {
                const modelRes = await ManufactureApiService.createModel({
                    manufacturer_id: formData.manufacturer_id,
                    name: modelName,
                    device_type: formData.device_type,
                    protocol_id: formData.protocol_id,
                    manual_url: manualUrl // Save manual URL on creation
                });

                if (modelRes.success && modelRes.data) {
                    finalModelId = modelRes.data.id;
                } else {
                    confirm({
                        title: '생성 실패',
                        message: '디바이스 모델 생성에 실패했습니다.',
                        confirmButtonType: 'danger',
                        showCancelButton: false
                    });
                    setLoading(false);
                    return;
                }
            } else if (finalModelId && manualUrl !== template?.manual_url) {
                // Update existing model if Manual URL changed
                await ManufactureApiService.updateModel(finalModelId, { manual_url: manualUrl });
            }

            // 2. Save Template
            const isNumeric = selectedP?.protocol_type?.includes('MODBUS');

            const preparedPoints = dataPoints.map(p => ({
                ...p,
                address: isNumeric ? Number(p.address) : 0,
                address_string: isNumeric ? String(p.address) : String(p.address),
                scaling_factor: Number(p.scaling_factor) || 1,
                scaling_offset: Number(p.scaling_offset) || 0
            }));

            let res;
            const submitData = {
                ...formData,
                model_id: finalModelId,
                data_points: preparedPoints,
                config: protocolConfig
            };

            if (isEdit && template) {
                res = await TemplateApiService.updateTemplate(template.id, submitData as any);
            } else {
                res = await TemplateApiService.createTemplate(submitData as any);
            }

            if (res.success) {
                onSuccess();
                onClose();
            } else {
                confirm({
                    title: '저장 실패',
                    message: res.message || '저장에 실패했습니다.',
                    confirmButtonType: 'danger',
                    showCancelButton: false
                });
            }
        } catch (err) {
            confirm({
                title: '오류 발생',
                message: '저장 중 오류가 발생했습니다.',
                confirmButtonType: 'danger',
                showCancelButton: false
            });
        } finally {
            setLoading(false);
        }
    };

    const addPoint = () => {
        setDataPoints([...dataPoints, {
            name: '',
            address: '',
            data_type: 'UINT16',
            access_mode: 'read',
            unit: '',
            scaling_factor: 1,
            scaling_offset: 0,
            description: ''
        }]);
    };

    const updatePoint = (index: number, field: string, value: any) => {
        const updated = [...dataPoints];
        updated[index] = { ...updated[index], [field]: value };
        setDataPoints(updated);
    };

    const removePoint = (index: number) => {
        setDataPoints(dataPoints.filter((_, i) => i !== index));
    };

    const getAddressLabel = () => {
        const selectedP = propsProtocols.find(p => p.id === formData.protocol_id);
        const type = selectedP?.protocol_type || '';
        if (type.includes('MODBUS')) return '주소 (Dec) *';
        if (type.includes('MQTT')) return '토픽 (Topic) *';
        if (type.includes('OPC')) return 'Node ID *';
        if (type.includes('BACNET')) return 'Object:Instance *';
        if (type.includes('ROS')) return 'Topic:Field *';
        return '주소 *';
    };

    const handleAddressChange = (index: number, value: string) => {
        const selectedP = propsProtocols.find(p => p.id === formData.protocol_id);
        const type = selectedP?.protocol_type || '';

        // Modbus는 숫자만 허용
        if (type.includes('MODBUS')) {
            const numericValue = value.replace(/[^0-9]/g, '');
            updatePoint(index, 'address', numericValue);
        } else {
            updatePoint(index, 'address', value);
        }
    };

    const getAddressHint = (address: string | number) => {
        const addr = Number(address);
        if (isNaN(addr)) return '';

        const selectedP = propsProtocols.find(p => p.id === formData.protocol_id);
        if (!selectedP?.protocol_type?.includes('MODBUS')) return '';

        if (addr >= 1 && addr <= 9999) return 'Coil (0x)';
        if (addr >= 10001 && addr <= 19999) return 'Discrete Input (1x)';
        if (addr >= 30001 && addr <= 39999) return 'Input Register (3x)';
        if (addr >= 40001 && addr <= 49999) return 'Holding Register (4x)';
        return '';
    };

    const getOverlappingIndices = () => {
        const intervals: { start: number; end: number; index: number }[] = [];
        dataPoints.forEach((p, idx) => {
            const addr = Number(p.address);
            if (!isNaN(addr) && p.address !== '') {
                const size = (p.data_type === 'UINT32' || p.data_type === 'INT32' || p.data_type === 'FLOAT32') ? 2 : 1;
                intervals.push({ start: addr, end: addr + size - 1, index: idx });
            }
        });

        const overlapIndices = new Set<number>();
        for (let i = 0; i < intervals.length; i++) {
            for (let j = i + 1; j < intervals.length; j++) {
                if (intervals[i].start <= intervals[j].end && intervals[j].start <= intervals[i].end) {
                    overlapIndices.add(intervals[i].index);
                    overlapIndices.add(intervals[j].index);
                }
            }
        }
        return overlapIndices;
    };

    const overlapIndices = getOverlappingIndices();

    const validateStep = () => {
        if (step === 1) {
            if (!formData.name || !formData.manufacturer_id || !formData.protocol_id) {
                confirm({
                    title: '입력 확인',
                    message: '모든 필수 정보를 입력해주세요.',
                    confirmButtonType: 'warning',
                    showCancelButton: false
                });
                return false;
            }
        } else if (step === 2) {
            if (dataPoints.length === 0) {
                confirm({
                    title: '데이터 포인트 없음',
                    message: '최소 하나 이상의 데이터 포인트를 등록해주세요.',
                    confirmButtonType: 'warning',
                    showCancelButton: false
                });
                return false;
            }
            if (overlapIndices.size > 0) {
                confirm({
                    title: '주소 중복 감지',
                    message: '주소가 중복된 데이터 포인트가 있습니다. 확인 후 수정해주세요.',
                    confirmButtonType: 'danger',
                    showCancelButton: false
                });
                return false;
            }
            // Add more specific validations here if needed
        }
        return true;
    };

    const handleNext = () => {
        if (validateStep()) {
            setStep(step + 1);
        }
    };

    const renderMemoryMap = () => {
        const selectedP = propsProtocols.find(p => p.id === formData.protocol_id);
        if (!selectedP?.protocol_type?.includes('MODBUS')) return null;

        const sortedPoints = [...dataPoints].filter(p => !isNaN(Number(p.address)) && p.address !== '')
            .sort((a, b) => Number(a.address) - Number(b.address));

        if (sortedPoints.length === 0) return null;

        return (
            <div style={{ padding: '12px', background: '#f1f5f9', borderRadius: '8px', marginTop: '16px' }}>
                <div style={{ fontSize: '12px', fontWeight: 600, color: '#475569', marginBottom: '8px' }}>
                    <i className="fas fa-microchip"></i> 메모리 레지스트리 맵 (Preview)
                </div>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {sortedPoints.map((p, i) => {
                        const size = (p.data_type === 'UINT32' || p.data_type === 'INT32' || p.data_type === 'FLOAT32') ? 2 : 1;
                        return (
                            <div key={i} title={`${p.name} (${p.address}) - ${p.data_type}`} style={{
                                padding: '2px 6px', fontSize: '11px', background: 'white', border: '1px solid #cbd5e1',
                                borderRadius: '4px', cursor: 'help', display: 'flex', alignItems: 'center', gap: '4px'
                            }}>
                                <span style={{ color: '#64748b' }}>#{p.address}</span>
                                <span style={{ fontWeight: 500 }}>{p.name}</span>
                                {size > 1 && <span style={{ fontSize: '9px', background: '#e2e8f0', borderRadius: '2px', padding: '0 2px' }}>{size}W</span>}
                            </div>
                        );
                    })}
                </div>
            </div>
        );
    };

    if (!isOpen) return null;

    return createPortal(
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container" style={{ width: '1250px', maxWidth: '98vw', maxHeight: '90vh', display: 'flex', flexDirection: 'column' }}>
                <div className="mgmt-modal-header">
                    <h3 className="mgmt-modal-title">
                        <i className={`fas ${isEdit ? 'fa-edit' : 'fa-plus-circle'} text-primary`}></i>
                        {isEdit ? '마스터 모델 수정' : '마스터 모델 등록'}
                    </h3>
                    <button className="mgmt-close-btn" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>

                <div className="wizard-steps-container">
                    <div className={`wizard-step ${step === 1 ? 'active' : step > 1 ? 'completed' : ''}`} onClick={() => setStep(1)} style={{ cursor: 'pointer' }}>
                        <div className="step-num">1</div>
                        <div className="step-label">기본 정보</div>
                    </div>
                    <div className="step-line"></div>
                    <div className={`wizard-step ${step === 2 ? 'active' : step > 2 ? 'completed' : ''}`} onClick={() => step > 1 && setStep(2)} style={{ cursor: 'pointer' }}>
                        <div className="step-num">2</div>
                        <div className="step-label">데이터 포인트</div>
                    </div>
                    <div className="step-line"></div>
                    <div className={`wizard-step ${step === 3 ? 'active' : ''}`} onClick={() => step > 2 && setStep(3)} style={{ cursor: 'pointer' }}>
                        <div className="step-num">3</div>
                        <div className="step-label">포트 및 상세 설정</div>
                    </div>
                </div>

                <div className="mgmt-modal-body" style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>
                    {step === 1 ? (
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
                                    {propsManufacturers.map(m => <option key={m.id} value={m.id}>{m.name}</option>)}
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
                                            const mName = formData.manufacturer_id ? propsManufacturers.find(m => m.id === formData.manufacturer_id)?.name : '';
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
                                    {propsProtocols.map(p => <option key={p.id} value={p.id}>{p.display_name}</option>)}
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
                    ) : step === 2 ? (
                        <div className="data-points-section">
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                                <h4 style={{ margin: 0 }}><i className="fas fa-list-ul"></i> 데이터 포인트 ({dataPoints.length})</h4>
                                <div style={{ display: 'flex', gap: '8px' }}>
                                    <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => setIsBulkModalOpen(true)}>
                                        <i className="fas fa-file-excel"></i> 엑셀 일괄 등록
                                    </button>
                                    <button className="mgmt-btn mgmt-btn-primary mgmt-btn-sm" onClick={addPoint}>
                                        <i className="fas fa-plus"></i> 포인트 추가
                                    </button>
                                </div>
                            </div>
                            <div className="table-container">
                                <table className="mgmt-table">
                                    <thead>
                                        <tr>
                                            <th style={{ width: '200px' }}>이름 *</th>
                                            <th style={{ width: '140px', textAlign: 'center' }}>{getAddressLabel()} *</th>
                                            <th style={{ width: '160px', textAlign: 'center' }}>타입 *</th>
                                            <th style={{ width: '180px', textAlign: 'center' }}>권한</th>
                                            <th>설명</th>
                                            <th style={{ width: '110px', textAlign: 'center' }}>단위</th>
                                            <th style={{ width: '100px', textAlign: 'center' }}>스케일</th>
                                            <th style={{ width: '100px', textAlign: 'center' }}>오프셋</th>
                                            <th style={{ width: '50px' }}></th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {dataPoints.map((point, idx) => {
                                            const hasOverlap = overlapIndices.has(idx);
                                            return (
                                                <tr key={idx} className={hasOverlap ? 'row-warning' : ''}>
                                                    <td>
                                                        <input
                                                            className="mgmt-input sm"
                                                            value={point.name || ''}
                                                            onChange={e => updatePoint(idx, 'name', e.target.value)}
                                                            placeholder="Name"
                                                        />
                                                    </td>
                                                    <td>
                                                        <div style={{ position: 'relative' }}>
                                                            <input
                                                                className={`mgmt-input sm ${hasOverlap ? 'error' : ''}`}
                                                                value={point.address || ''}
                                                                onChange={e => updatePoint(idx, 'address', e.target.value)}
                                                                placeholder="0"
                                                                style={{ textAlign: 'center' }}
                                                            />
                                                            {getAddressHint(point.address || '') && (
                                                                <div style={{ fontSize: '10px', color: hasOverlap ? 'var(--danger-500)' : 'var(--primary-500)', marginTop: '2px', position: 'absolute' }}>
                                                                    {getAddressHint(point.address || '')}
                                                                </div>
                                                            )}
                                                            {hasOverlap && (
                                                                <div style={{ fontSize: '10px', color: 'var(--danger-500)', position: 'absolute', top: '-15px', right: 0 }}>
                                                                    <i className="fas fa-exclamation-triangle"></i> 중복
                                                                </div>
                                                            )}
                                                        </div>
                                                    </td>
                                                    <td>
                                                        <select
                                                            className="mgmt-select sm"
                                                            value={point.data_type || 'UINT16'}
                                                            onChange={e => updatePoint(idx, 'data_type', e.target.value)}
                                                            style={{ textAlign: 'center' }}
                                                        >
                                                            <option value="UINT16">UINT16</option>
                                                            <option value="INT16">INT16</option>
                                                            <option value="UINT32">UINT32</option>
                                                            <option value="INT32">INT32</option>
                                                            <option value="FLOAT32">FLOAT32</option>
                                                            <option value="BOOLEAN">BOOLEAN</option>
                                                        </select>
                                                    </td>
                                                    <td>
                                                        <select
                                                            className="mgmt-select sm"
                                                            value={point.access_mode || 'read'}
                                                            onChange={e => updatePoint(idx, 'access_mode', e.target.value)}
                                                            style={{ textAlign: 'center' }}
                                                        >
                                                            <option value="read">Read Only</option>
                                                            <option value="write">Write Only</option>
                                                            <option value="read_write">Read/Write</option>
                                                        </select>
                                                    </td>
                                                    <td>
                                                        <input
                                                            className="mgmt-input sm"
                                                            value={point.description || ''}
                                                            onChange={e => updatePoint(idx, 'description', e.target.value)}
                                                            placeholder="Description"
                                                        />
                                                    </td>
                                                    <td>
                                                        <input
                                                            className="mgmt-input sm"
                                                            value={point.unit || ''}
                                                            onChange={e => updatePoint(idx, 'unit', e.target.value)}
                                                            placeholder="Unit"
                                                            style={{ textAlign: 'center' }}
                                                        />
                                                    </td>
                                                    <td>
                                                        <input
                                                            type="text"
                                                            className="mgmt-input sm"
                                                            value={point.scaling_factor ?? 1}
                                                            onChange={e => updatePoint(idx, 'scaling_factor', e.target.value)}
                                                            placeholder="1.0"
                                                            style={{ textAlign: 'center' }}
                                                        />
                                                    </td>
                                                    <td>
                                                        <input
                                                            type="text"
                                                            className="mgmt-input sm"
                                                            value={point.scaling_offset ?? 0}
                                                            onChange={e => updatePoint(idx, 'scaling_offset', e.target.value)}
                                                            placeholder="0.0"
                                                            style={{ textAlign: 'center' }}
                                                        />
                                                    </td>
                                                    <td>
                                                        <button className="mgmt-btn-icon mgmt-btn-error" onClick={() => removePoint(idx)}>
                                                            <i className="fas fa-trash"></i>
                                                        </button>
                                                    </td>
                                                </tr>
                                            );
                                        })}
                                    </tbody>
                                </table>
                            </div>
                            {renderMemoryMap()}
                        </div>
                    ) : (
                        <div className="protocol-config-section">
                            <div className="alert alert-info" style={{ marginBottom: '20px' }}>
                                <i className="fas fa-microchip"></i> <strong>{propsProtocols.find(p => p.id === formData.protocol_id)?.display_name}</strong> 프로토콜에 필요한 속성값입니다. 필수 항목(*)을 확인해주세요.
                            </div>

                            <div className="mgmt-modal-form-grid">
                                {(() => {
                                    const selectedP = propsProtocols.find(p => p.id === formData.protocol_id);
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
                                                        is_keep_alive_enabled: 1,
                                                        keep_alive_interval_s: 10,
                                                        keep_alive_timeout_s: 5
                                                    });
                                                } else if (val === 'standard') {
                                                    setFormData({ ...formData, polling_interval: 1000, timeout: 3000 });
                                                    setProtocolConfig({
                                                        ...protocolConfig,
                                                        max_retry_count: 3,
                                                        retry_interval_ms: 2000,
                                                        backoff_time_ms: 60000,
                                                        is_keep_alive_enabled: 1,
                                                        keep_alive_interval_s: 30,
                                                        keep_alive_timeout_s: 10
                                                    });
                                                } else if (val === 'eco') {
                                                    setFormData({ ...formData, polling_interval: 5000, timeout: 10000 });
                                                    setProtocolConfig({
                                                        ...protocolConfig,
                                                        max_retry_count: 2,
                                                        retry_interval_ms: 10000,
                                                        backoff_time_ms: 300000,
                                                        is_keep_alive_enabled: 0,
                                                        keep_alive_interval_s: 300,
                                                        keep_alive_timeout_s: 60
                                                    });
                                                }
                                            }}
                                        >
                                            <option value="">프로필 선택</option>
                                            <option value="critical">고신뢰 (제어용)</option>
                                            <option value="standard">표준 (모니터링)</option>
                                            <option value="eco">절전/대역폭 절약</option>
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
                                            onChange={e => setProtocolConfig({ ...protocolConfig, max_retry_count: Number(e.target.value) })}
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>재시도 간격 (ms)</label>
                                        <input
                                            type="number"
                                            className="mgmt-input"
                                            value={protocolConfig.retry_interval_ms ?? 5000}
                                            onChange={e => setProtocolConfig({ ...protocolConfig, retry_interval_ms: Number(e.target.value) })}
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>복구 대기 시간 (ms)</label>
                                        <input
                                            type="number"
                                            className="mgmt-input"
                                            value={protocolConfig.backoff_time_ms ?? 60000}
                                            onChange={e => setProtocolConfig({ ...protocolConfig, backoff_time_ms: Number(e.target.value) })}
                                            placeholder="모든 재시도 실패 후 대기 시간"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>Keep-alive 활성화</label>
                                        <select
                                            className="mgmt-select"
                                            value={protocolConfig.is_keep_alive_enabled ?? 1}
                                            onChange={e => setProtocolConfig({ ...protocolConfig, is_keep_alive_enabled: Number(e.target.value) })}
                                        >
                                            <option value={1}>활성화</option>
                                            <option value={0}>비활성화</option>
                                        </select>
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>K-A 주기 (초)</label>
                                        <input
                                            type="number"
                                            className="mgmt-input"
                                            value={protocolConfig.keep_alive_interval_s ?? 30}
                                            onChange={e => setProtocolConfig({ ...protocolConfig, keep_alive_interval_s: Number(e.target.value) })}
                                            disabled={!protocolConfig.is_keep_alive_enabled}
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>K-A 타임아웃 (초)</label>
                                        <input
                                            type="number"
                                            className="mgmt-input"
                                            value={protocolConfig.keep_alive_timeout_s ?? 10}
                                            onChange={e => setProtocolConfig({ ...protocolConfig, keep_alive_timeout_s: Number(e.target.value) })}
                                            disabled={!protocolConfig.is_keep_alive_enabled}
                                        />
                                    </div>
                                </div>
                            </div>
                        </div>
                    )}
                </div>

                <div className="mgmt-modal-footer">
                    {isEdit && onDelete && (
                        <div style={{ marginRight: 'auto' }}>
                            <button className="mgmt-btn mgmt-btn-outline mgmt-btn-error" onClick={() => onDelete(template!)} title="삭제">
                                <i className="fas fa-trash"></i> 삭제
                            </button>
                        </div>
                    )}
                    <button className="mgmt-btn mgmt-btn-outline" style={{ width: 'auto', minWidth: '100px' }} onClick={onClose}>취소</button>
                    {step < 3 ? (
                        <>
                            {step > 1 && <button className="mgmt-btn mgmt-btn-outline" style={{ width: 'auto', minWidth: '100px' }} onClick={() => setStep(step - 1)}><i className="fas fa-arrow-left"></i> 이전</button>}
                            <button className="mgmt-btn mgmt-btn-primary" style={{ width: 'auto', minWidth: '120px' }} onClick={handleNext}>다음 단계 <i className="fas fa-arrow-right"></i></button>
                        </>
                    ) : (
                        <>
                            <button className="mgmt-btn mgmt-btn-outline" style={{ width: 'auto', minWidth: '100px' }} onClick={() => setStep(2)}><i className="fas fa-arrow-left"></i> 이전</button>
                            <button className="mgmt-btn mgmt-btn-primary" style={{ width: 'auto', minWidth: '120px' }} onClick={handleSave} disabled={loading}>
                                {loading ? '저장 중...' : (isEdit ? '수정 완료' : '등록 완료')}
                            </button>
                        </>
                    )}
                </div>
            </div>

            <style>{`
                .mgmt-modal-overlay {
                  position: fixed; top: 0; left: 0; right: 0; bottom: 0;
                  background: rgba(0,0,0,0.5); display: flex; align-items: center; justify-content: center; z-index: 1000;
                }
                .mgmt-modal-container { background: white; border-radius: 12px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.1); }
                .wizard-steps-container {
                  display: flex; align-items: center; justify-content: center; padding: 24px; background: #f8fafc; border-bottom: 1px solid #e2e8f0;
                }
                .wizard-step { display: flex; flex-direction: column; align-items: center; gap: 8px; position: relative; }
                .step-num { 
                  width: 32px; height: 32px; border-radius: 50%; background: #e2e8f0; color: #64748b; 
                  display: flex; align-items: center; justify-content: center; font-weight: 700; font-size: 14px;
                }
                .wizard-step.active .step-num { background: var(--primary-500); color: white; }
                .wizard-step.completed .step-num { background: var(--success-500); color: white; }
                .step-label { font-size: 12px; font-weight: 600; color: #64748b; }
                .wizard-step.active .step-label { color: var(--primary-600); }
                .step-line { flex: 0.2; height: 2px; background: #e2e8f0; margin: 0 16px; margin-top: -24px; }
                
                .mgmt-input.sm, .mgmt-select.sm {
                    padding: 4px 8px;
                    font-size: 13px;
                    height: 32px;
                }
                .row-warning { background-color: #fffaf0; }
                .mgmt-input.error { border-color: var(--danger-500); background-color: #fff5f5; }
            `}</style>
            <DeviceDataPointsBulkModal
                isOpen={isBulkModalOpen}
                onClose={() => setIsBulkModalOpen(false)}
                deviceId={0} // Not used for templates in terms of DB, used for draft storage prefix
                existingAddresses={dataPoints.map(p => String(p.address))}
                onSave={async (newPoints) => {
                    const mapped: Partial<TemplatePoint>[] = newPoints.map(p => ({
                        name: p.name,
                        address: p.address,
                        data_type: p.data_type?.toUpperCase() || 'UINT16',
                        access_mode: (p.access_mode === 'read_write' ? 'read_write' : p.access_mode === 'write' ? 'write' : 'read') as any,
                        unit: p.unit,
                        scaling_factor: p.scaling_factor || 1,
                        scaling_offset: p.scaling_offset || 0,
                        description: p.description
                    }));

                    // Filter duplicates by name or address
                    const existingNames = new Set(dataPoints.map(p => p.name?.toLowerCase()));
                    const existingAddrs = new Set(dataPoints.map(p => String(p.address)));

                    const filtered = mapped.filter(p => {
                        const nameKey = p.name?.toLowerCase();
                        const addrKey = String(p.address);
                        if (existingNames.has(nameKey) || existingAddrs.has(addrKey)) {
                            return false;
                        }
                        return true;
                    });

                    const duplicateCount = mapped.length - filtered.length;

                    setDataPoints([...dataPoints, ...filtered]);
                    setIsBulkModalOpen(false);

                    if (duplicateCount > 0) {
                        confirm({
                            title: '중복 제외 알림',
                            message: `이미 등록된 이름 또는 주소가 포함된 ${duplicateCount}개의 항목을 제외하고 ${filtered.length}개 항목이 추가되었습니다.`,
                            confirmButtonType: 'primary',
                            showCancelButton: false
                        });
                    }
                }}
            />
        </div>,
        document.body
    );
};

export default MasterModelModal;
