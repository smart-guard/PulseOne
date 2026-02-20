import React, { useState, useEffect, useMemo } from 'react';
import { createPortal } from 'react-dom';
import { DeviceTemplate, TemplatePoint, Manufacturer, Protocol } from '../../types/manufacturing';
import { TemplateApiService } from '../../api/services/templateApi';
import { ManufactureApiService } from '../../api/services/manufactureApi';
import { useConfirmContext } from '../common/ConfirmProvider';
import DeviceDataPointsBulkModal from './DeviceModal/DeviceDataPointsBulkModal';
import BasicInfoStep from './master-model/BasicInfoStep';
import DataPointsStep from './master-model/DataPointsStep';
import ProtocolConfigStep from './master-model/ProtocolConfigStep';
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
    const [manualUrl, setManualUrl] = useState('');
    const [dataPoints, setDataPoints] = useState<Partial<TemplatePoint>[]>([]);
    const [protocolConfig, setProtocolConfig] = useState<any>({});
    const [isBulkModalOpen, setIsBulkModalOpen] = useState(false);

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

    // Helper: Overlap Calculation for Validation
    const overlapIndices = useMemo(() => {
        const intervals: { start: number; end: number; index: number }[] = [];
        dataPoints.forEach((p, idx) => {
            const addr = Number(p.address);
            if (!isNaN(addr) && p.address !== '' && p.address !== undefined) {
                const size = (p.data_type === 'UINT32' || p.data_type === 'INT32' || p.data_type === 'FLOAT32') ? 2 : 1;
                intervals.push({ start: addr, end: addr + size - 1, index: idx });
            }
        });

        const indices = new Set<number>();
        for (let i = 0; i < intervals.length; i++) {
            for (let j = i + 1; j < intervals.length; j++) {
                if (intervals[i].start <= intervals[j].end && intervals[j].start <= intervals[i].end) {
                    indices.add(intervals[i].index);
                    indices.add(intervals[j].index);
                }
            }
        }
        return indices;
    }, [dataPoints]);

    const handleSave = async () => {
        try {
            setLoading(true);

            // Validation logic...
            const requiredFields = ['name', 'manufacturer_id', 'device_type', 'protocol_id'];
            const missing = requiredFields.filter(field => !formData[field as keyof DeviceTemplate]);

            if (step === 1) {
                if (!modelName) missing.push('model_name');
            }

            if (missing.length > 0 || !modelName) {
                if (!modelName && !missing.includes('model_name')) missing.push('model_name');

                if (missing.length > 0) {
                    confirm({
                        title: '필수 정보 누락',
                        message: `다음 필수 설정 항목이 누락되었습니다: ${missing.join(', ')}`,
                        confirmButtonType: 'warning',
                        showCancelButton: false
                    });
                    setLoading(false);
                    setStep(1);
                    return;
                }
            }

            // Protocol Config Validation
            const selectedP = propsProtocols.find(p => p.id === formData.protocol_id);
            const schema = selectedP?.connection_params;
            if (selectedP && schema) {
                const schemaObj = typeof schema === 'string' ? JSON.parse(schema) : schema;
                const missingConfig = Object.keys(schemaObj).filter(key => schemaObj[key].required && !protocolConfig[key]);
                if (missingConfig.length > 0) {
                    confirm({
                        title: '화면 설정 누락',
                        message: `다음 필수 설정 항목이 누락되었습니다: ${missingConfig.join(', ')}`,
                        confirmButtonType: 'warning',
                        showCancelButton: false
                    });
                    setLoading(false);
                    setStep(3);
                    return;
                }
            }

            // Create Model Logic
            let finalModelId = (template as any)?.model_id;

            // Duplicate Point Check
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

            if (!isEdit || modelName !== template?.model_name) {
                const modelRes = await ManufactureApiService.createModel({
                    manufacturer_id: formData.manufacturer_id,
                    name: modelName,
                    model_number: formData.model_number,
                    device_type: formData.device_type,
                    protocol_id: formData.protocol_id,
                    manual_url: manualUrl
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
            } else if (finalModelId && (manualUrl !== template?.manual_url || formData.model_number !== template?.model_number)) {
                await ManufactureApiService.updateModel(finalModelId, {
                    manual_url: manualUrl,
                    model_number: formData.model_number
                });
            }

            // Save Template
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
        }
        return true;
    };

    const handleNext = () => {
        if (validateStep()) {
            setStep(step + 1);
        }
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

                <style>{`
                    .wizard-steps-container {
                        display: flex;
                        justify-content: center;
                        align-items: center;
                        margin: 0 auto 24px auto;
                        padding: 0 40px;
                        gap: 16px;
                        max-width: 800px;
                        width: 100%;
                    }
                    .wizard-step {
                        display: flex;
                        flex-direction: column;
                        align-items: center;
                        gap: 8px;
                        color: var(--neutral-400);
                        position: relative;
                        z-index: 1;
                        cursor: pointer;
                    }
                    .wizard-step.active {
                        color: var(--primary-600);
                        font-weight: 600;
                    }
                    .wizard-step.completed {
                        color: var(--success-600);
                    }
                    .step-num {
                        width: 32px;
                        height: 32px;
                        border-radius: 50%;
                        background: white;
                        border: 2px solid currentColor;
                        display: flex;
                        align-items: center;
                        justify-content: center;
                        font-size: 14px;
                        font-weight: 700;
                        transition: all 0.2s;
                    }
                    .wizard-step.active .step-num {
                        background: var(--primary-100);
                        border-color: var(--primary-500);
                        color: var(--primary-700);
                    }
                    .wizard-step.completed .step-num {
                        background: var(--success-100);
                        border-color: var(--success-500);
                        color: var(--success-700);
                    }
                    .step-label {
                        font-size: 13px;
                        white-space: nowrap;
                    }
                    .step-line {
                        flex: 1;
                        height: 2px;
                        background: var(--neutral-200);
                        margin-top: -24px;
                        z-index: 0;
                    }
                `}</style>
                <div className="mgmt-modal-body" style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>
                    {step === 1 && (
                        <BasicInfoStep
                            formData={formData}
                            setFormData={setFormData}
                            modelName={modelName}
                            setModelName={setModelName}
                            manualUrl={manualUrl}
                            setManualUrl={setManualUrl}
                            manufacturers={propsManufacturers}
                            protocols={propsProtocols}
                            handleProtocolChange={handleProtocolChange}
                        />
                    )}
                    {step === 2 && (
                        <DataPointsStep
                            dataPoints={dataPoints}
                            setDataPoints={setDataPoints}
                            formData={formData}
                            protocols={propsProtocols}
                            setIsBulkModalOpen={setIsBulkModalOpen}
                            overlapIndices={overlapIndices}
                        />
                    )}
                    {step === 3 && (
                        <ProtocolConfigStep
                            protocolConfig={protocolConfig}
                            setProtocolConfig={setProtocolConfig}
                            formData={formData}
                            setFormData={setFormData}
                            protocols={propsProtocols}
                        />
                    )}
                </div>

                <div className="mgmt-modal-footer">
                    {step > 1 && (
                        <button className="mgmt-btn mgmt-btn-neutral" onClick={() => setStep(step - 1)}>
                            이전
                        </button>
                    )}
                    <div style={{ flex: 1 }}></div>
                    {step < 3 ? (
                        <button className="mgmt-btn mgmt-btn-primary" onClick={handleNext}>
                            다음 <i className="fas fa-arrow-right"></i>
                        </button>
                    ) : (
                        <button className="mgmt-btn mgmt-btn-primary" onClick={handleSave} disabled={loading}>
                            {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-save"></i>}
                            {isEdit ? '수정 완료' : '등록 완료'}
                        </button>
                    )}
                </div>
            </div>

            <DeviceDataPointsBulkModal
                isOpen={isBulkModalOpen}
                onClose={() => setIsBulkModalOpen(false)}
                onImport={(newPoints) => {
                    const formattedPoints = newPoints.map(p => ({
                        ...p,
                        scaling_factor: p.scaling_factor ?? 1,
                        scaling_offset: p.scaling_offset ?? 0
                    }));
                    setDataPoints([...dataPoints, ...formattedPoints]);
                    setIsBulkModalOpen(false);
                }}
            />
        </div>,
        document.body
    );
};

export default MasterModelModal;
