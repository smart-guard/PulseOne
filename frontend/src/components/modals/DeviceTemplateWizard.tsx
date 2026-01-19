import React, { useState, useEffect } from 'react';
import { createPortal } from 'react-dom';
import { ManufactureApiService } from '../../api/services/manufactureApi';
import { TemplateApiService } from '../../api/services/templateApi';
import { Manufacturer, DeviceModel, DeviceTemplate } from '../../types/manufacturing';
import { SiteApiService } from '../../api/services/siteApi';
import { Site, ApiResponse, PaginatedApiResponse } from '../../types/common';
import { ProtocolApiService, Protocol } from '../../api/services/protocolApi';
import { CollectorApiService, EdgeServer } from '../../api/services/collectorApi';
import { DataPoint } from '../../api/services/dataApi';
import DeviceDataPointsTab from './DeviceModal/DeviceDataPointsTab';
import { apiClient } from '../../api/client';
import { useConfirmContext } from '../common/ConfirmProvider';
import '../../styles/management.css';

interface DeviceTemplateWizardProps {
    isOpen: boolean;
    onClose: () => void;
    onSuccess: (deviceId: number) => void;
    initialTemplateId?: number;
}

const DeviceTemplateWizard: React.FC<DeviceTemplateWizardProps> = ({
    isOpen,
    onClose,
    onSuccess,
    initialTemplateId
}) => {
    const { confirm } = useConfirmContext();
    const [step, setStep] = useState(1);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);

    // Data states
    const [manufacturers, setManufacturers] = useState<Manufacturer[]>([]);
    const [models, setModels] = useState<DeviceModel[]>([]);
    const [templates, setTemplates] = useState<DeviceTemplate[]>([]);
    const [sites, setSites] = useState<Site[]>([]);
    const [availableCollectors, setAvailableCollectors] = useState<EdgeServer[]>([]);

    // Selection states
    const [selectedManufacturer, setSelectedManufacturer] = useState<number | null>(null);
    const [selectedModel, setSelectedModel] = useState<number | null>(null);
    const [selectedTemplate, setSelectedTemplate] = useState<DeviceTemplate | null>(null);

    // Final setup states
    const [deviceName, setDeviceName] = useState('');
    const [selectedSite, setSelectedSite] = useState<number | null>(null);
    const [selectedCollector, setSelectedCollector] = useState<number | null>(0);
    const [endpoint, setEndpoint] = useState('');
    const [ipAddress, setIpAddress] = useState('');
    const [portNumber, setPortNumber] = useState('');

    // Operational settings
    const [pollingInterval, setPollingInterval] = useState(1000);
    const [timeout, setTimeoutVal] = useState(5000);
    const [readTimeout, setReadTimeout] = useState(3000);
    const [writeTimeout, setWriteTimeout] = useState(3000);
    const [retryCount, setRetryCount] = useState(3);
    const [retryInterval, setRetryInterval] = useState(1000);
    const [backoffTime, setBackoffTime] = useState(60000);
    const [backoffMultiplier, setBackoffMultiplier] = useState(1.5);
    const [maxBackoffTime, setMaxBackoffTime] = useState(300000);
    const [isEnabled, setIsEnabled] = useState(true);

    // Advanced operational settings
    const [isKeepAlive, setIsKeepAlive] = useState(true);
    const [kaInterval, setKaInterval] = useState(30);
    const [isDataValidation, setIsDataValidation] = useState(true);
    const [isPerformanceMonitoring, setIsPerformanceMonitoring] = useState(true);
    const [isDetailedLogging, setIsDetailedLogging] = useState(false);
    const [isDiagnosticMode, setIsDiagnosticMode] = useState(false);
    const [isCommLogging, setIsCommLogging] = useState(false);

    // Advanced engineering settings
    const [readBufferSize, setReadBufferSize] = useState(1024);
    const [writeBufferSize, setWriteBufferSize] = useState(1024);
    const [queueSize, setQueueSize] = useState(100);
    const [isOutlierDetection, setIsOutlierDetection] = useState(false);
    const [isDeadband, setIsDeadband] = useState(true);
    const [kaTimeout, setKaTimeout] = useState(10);
    const [deviceTags, setDeviceTags] = useState<string[]>([]);
    const [deviceMetadata, setDeviceMetadata] = useState<any>({});
    const [customFields, setCustomFields] = useState<any>({});

    // Protocol dynamic states
    const [selectedProtocol, setSelectedProtocol] = useState<Protocol | null>(null);
    const [instanceConfig, setInstanceConfig] = useState<Record<string, any>>({});

    // Conflict detection states
    const [portConflicts, setPortConflicts] = useState<any[]>([]);
    const [isCheckingConflicts, setIsCheckingConflicts] = useState(false);
    const [foundMaster, setFoundMaster] = useState<any>(null);

    // Data points for editing
    const [templateDataPoints, setTemplateDataPoints] = useState<DataPoint[]>([]);
    const [isLoadingPoints, setIsLoadingPoints] = useState(false);
    const [suggestedId, setSuggestedId] = useState<number | null>(null);

    useEffect(() => {
        if (isOpen) {
            loadInitialData();
            if (initialTemplateId) {
                jumpToTemplate(initialTemplateId);
            }
        }
    }, [isOpen, initialTemplateId]);

    useEffect(() => {
        if (selectedTemplate) {
            loadProtocolDetails(selectedTemplate.protocol_id);
        }
    }, [selectedTemplate]);

    // Effect to check for port/IP conflicts
    useEffect(() => {
        const checkConflicts = async () => {
            if (!selectedSite || !selectedProtocol) {
                setPortConflicts([]);
                return;
            }

            let currentEndpoint = '';
            const isTcp = !selectedProtocol.uses_serial;

            if (isTcp) {
                currentEndpoint = portNumber ? `${ipAddress}:${portNumber}` : ipAddress;
            } else {
                currentEndpoint = endpoint;
            }

            if (!currentEndpoint) {
                setPortConflicts([]);
                return;
            }

            try {
                setIsCheckingConflicts(true);
                const res = await apiClient.get<any>(`/api/devices`, {
                    site_id: selectedSite,
                    endpoint: currentEndpoint
                });
                if (res.success && res.data) {
                    setPortConflicts(res.data.items || []);
                }
            } catch (err) {
                console.error('Failed to check port conflicts:', err);
            } finally {
                setIsCheckingConflicts(false);
            }
        };

        const timer = setTimeout(checkConflicts, 500);
        return () => clearTimeout(timer);
    }, [selectedSite, endpoint, ipAddress, selectedProtocol]);

    // Auto-increment ID Suggestion & Master Detection
    useEffect(() => {
        if (portConflicts.length > 0) {
            const usedIds = portConflicts.map(d => {
                const cfg = typeof d.config === 'string' ? JSON.parse(d.config) : d.config;
                return Number(cfg?.slave_id || cfg?.unit_id || cfg?.device_id || 0);
            });

            // Detect master among conflicts
            const master = portConflicts.find(d => {
                const cfg = typeof d.config === 'string' ? JSON.parse(d.config) : d.config;
                return cfg?.device_role === 'master' || !cfg?.slave_id;
            });
            setFoundMaster(master || null);

            if (usedIds.length > 0) {
                const maxId = Math.max(...usedIds);
                const nextId = maxId + 1;
                setSuggestedId(nextId);

                setInstanceConfig(prev => {
                    const newConfig = { ...prev };
                    // Auto-increment logic: 
                    // 1. If ID is empty, set it
                    // 2. If ID is the default (usually 1 or the template's default), and it's ALREADY USED, then increment.
                    const mainKey = Object.keys(newConfig).find(k => ['slave_id', 'unit_id', 'device_id'].includes(k));
                    if (mainKey) {
                        const currentId = Number(newConfig[mainKey]);
                        const isIdUsed = usedIds.includes(currentId);

                        if (!currentId || (isIdUsed && (currentId === 1 || currentId === suggestedId - 1))) {
                            newConfig[mainKey] = nextId;
                        }
                    }
                    return newConfig;
                });
            } else {
                setSuggestedId(null);
            }
        } else {
            setSuggestedId(null);
            setFoundMaster(null);
        }
    }, [portConflicts]);

    // Auto-fill master_device_id when role is slave OR when foundMaster changes
    useEffect(() => {
        if (instanceConfig.device_role === 'slave' && foundMaster) {
            setInstanceConfig(prev => {
                if (prev.master_device_id === foundMaster.id) return prev;
                return {
                    ...prev,
                    master_device_id: foundMaster.id
                };
            });
        } else if (instanceConfig.device_role === 'master' && instanceConfig.master_device_id) {
            // Clear master_device_id if role is switched back to master
            setInstanceConfig(prev => ({
                ...prev,
                master_device_id: ''
            }));
        }
    }, [instanceConfig.device_role, foundMaster]);

    const loadProtocolDetails = async (pid: number) => {
        try {
            const res = await ProtocolApiService.getProtocol(pid);
            if (res.success && res.data) {
                const protocol = res.data;
                setSelectedProtocol(protocol);

                // Initialize instanceConfig with defaults from schema
                let schema = protocol.connection_params;
                if (typeof schema === 'string') {
                    try { schema = JSON.parse(schema); } catch (e) { schema = {}; }
                }

                const defaults: Record<string, any> = {};
                if (schema) {
                    Object.keys(schema).forEach(key => {
                        if (schema[key].default !== undefined) {
                            defaults[key] = schema[key].default;
                        }
                    });
                }
                setInstanceConfig(defaults);

                // Set default IP if needed and default port based on protocol
                if (protocol.protocol_type.includes('TCP') || protocol.protocol_type === 'MQTT' || protocol.protocol_type === 'BACNET' || protocol.protocol_type === 'OPC_UA') {
                    if (protocol.protocol_type === 'MODBUS_TCP') setPortNumber('502');
                    else if (protocol.protocol_type === 'MQTT') setPortNumber('1883');
                    else if (protocol.protocol_type === 'BACNET') setPortNumber('47808');
                    else if (protocol.protocol_type === 'OPC_UA') setPortNumber('4840');
                    else if (protocol.protocol_type === 'ROS') setPortNumber('9090');
                }
            }
        } catch (err) {
            console.error('Failed to load protocol details:', err);
        }
    };

    const loadInitialData = async () => {
        try {
            setLoading(true);
            const [mRes, sRes] = await Promise.all([
                ManufactureApiService.getManufacturers(),
                SiteApiService.getSites()
            ]);

            if (mRes.success) setManufacturers(mRes.data?.items || []);
            if (sRes.success) {
                const sItems = sRes.data?.items || [];
                setSites(sItems);
            }
            // Fetch collectors
            const collRes = await CollectorApiService.getAllCollectors();
            if (collRes.success && collRes.data) {
                const collectors = collRes.data;
                setAvailableCollectors(collectors);
                if (collectors.length > 0) {
                    setSelectedCollector(collectors[0].id);
                }
            }
        } catch (err) {
            setError('초기 데이터를 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    };

    const jumpToTemplate = async (templateId: number) => {
        try {
            setLoading(true);
            const tRes = await TemplateApiService.getTemplate(templateId);
            if (tRes.success && tRes.data) {
                const template = tRes.data;
                setSelectedTemplate(template);
                setDeviceName(template.name);
                setPollingInterval(template.polling_interval || 1000);
                setTimeoutVal(template.timeout || 5000);
                setTemplateDataPoints(template.data_points as any || []);
                // If template is already selected, go to Identity & Connection (Step 2)
                setStep(2);
            }
        } catch (err) {
            setError('템플릿 정보를 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    };

    const handleDataPointCreate = async (data: any) => {
        const newPoint = { ...data, id: -(templateDataPoints.length + 1) }; // Temp negative ID
        setTemplateDataPoints([...templateDataPoints, newPoint as any]);
    };

    const handleDataPointUpdate = async (data: any) => {
        setTemplateDataPoints(templateDataPoints.map(p => p.id === data.id ? { ...p, ...data } : p));
    };

    const handleDataPointDelete = async (id: number) => {
        setTemplateDataPoints(templateDataPoints.filter(p => p.id !== id));
    };

    const handleManufacturerSelect = async (id: number) => {
        setSelectedManufacturer(id);
        setSelectedModel(null);
        setModels([]);
        try {
            setLoading(true);
            const res = await ManufactureApiService.getModelsByManufacturer(id);
            if (res.success && res.data) {
                setModels(Array.isArray(res.data) ? res.data : (res.data.items || []));
            }
        } catch (err) {
            setError('모델 정보를 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    };

    const handleModelSelect = async (id: number) => {
        setSelectedModel(id);
        setTemplates([]);
        try {
            setLoading(true);
            const res = await TemplateApiService.getTemplates({ model_id: id });
            if (res.success && res.data) {
                // Backend returns array directly
                setTemplates(Array.isArray(res.data) ? res.data : (res.data.items || []));
            }
        } catch (err) {
            setError('마스터 모델 목록을 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    };

    const handleTemplateSelect = (template: DeviceTemplate) => {
        setSelectedTemplate(template);
        setDeviceName(template.name);
        setPollingInterval(template.polling_interval || 1000);
        setTimeoutVal(template.timeout || 5000);
        setTemplateDataPoints(template.data_points as any || []);
        setStep(2); // Move to Identity & Connection
    };

    const handleCreate = async () => {
        if (!selectedTemplate || !deviceName || !selectedSite) {
            confirm({
                title: '필수 정보 누락',
                message: '모든 필수 정보를 입력해주세요.',
                confirmButtonType: 'warning',
                showCancelButton: false
            });
            return;
        }

        // 접속 정보 검증
        if (selectedProtocol && (selectedProtocol.protocol_type.includes('TCP') || ['MQTT', 'BACNET', 'OPC_UA', 'ROS'].includes(selectedProtocol.protocol_type))) {
            if (!ipAddress || !portNumber) {
                confirm({
                    title: '접속 정보 누락',
                    message: '접속 IP 주소와 포트 번호는 필수입니다.',
                    confirmButtonType: 'warning',
                    showCancelButton: false
                });
                return;
            }
        }

        // 프로토콜 필수 설정 검증
        let schema = selectedProtocol?.connection_params;
        if (typeof schema === 'string') {
            try { schema = JSON.parse(schema); } catch (e) { schema = null; }
        }
        if (schema) {
            for (const [key, field] of Object.entries(schema) as any) {
                if (field.required && (instanceConfig[key] === undefined || instanceConfig[key] === '')) {
                    confirm({
                        title: '설정 누락',
                        message: `${field.label} 필드는 필수입니다.`,
                        confirmButtonType: 'warning',
                        showCancelButton: false
                    });
                    return;
                }
            }
        }

        // ID Conflict Check (Blocking) - Supports Slave ID (Modbus) and Device ID (BACnet)
        const conflictId = instanceConfig['slave_id'] || instanceConfig['device_id'];
        const idConflict = conflictId && portConflicts.some(d => {
            const cfg = typeof d.config === 'string' ? JSON.parse(d.config) : d.config;
            // Check both possible ID fields
            const existingId = cfg?.slave_id || cfg?.device_id;
            return Number(existingId) === Number(conflictId);
        });

        if (idConflict) {
            confirm({
                title: 'ID 충돌 감지',
                message: `ID ${conflictId}번은 해당 경로(Endpoint)에서 이미 사용 중입니다.\n다른 ID를 선택해주세요.`,
                confirmButtonType: 'danger',
                showCancelButton: false
            });
            return;
        }

        // Create a detailed confirmation summary
        const selectedSiteName = sites.find(s => s.id === selectedSite)?.name || '-';
        const selectedCollectorName = availableCollectors.find(c => c.id === selectedCollector)?.name || '-';
        const finalEndpoint = !selectedProtocol?.uses_serial
            ? (portNumber ? `${ipAddress}:${portNumber}` : ipAddress)
            : endpoint;

        let confSchema = selectedProtocol?.connection_params;
        if (typeof confSchema === 'string') {
            try { confSchema = JSON.parse(confSchema); } catch (e) { confSchema = {}; }
        }
        const protocolParams = confSchema ? Object.entries(confSchema).map(([key, field]: [string, any]) => {
            return `${field.label || key}: ${instanceConfig[key] || field.default || '-'}`;
        }).join('\n') : '';

        const confirmationMsg = `
아래 설정으로 디바이스를 생성하시겠습니까?

■ 기본 정보
- 디바이스 명: ${deviceName}
- 설치 사이트: ${selectedSiteName}
- 담당 콜렉터: ${selectedCollectorName}

■ 통신 접속
- 프로토콜: ${selectedProtocol?.display_name || '-'}
- 엔드포인트: ${finalEndpoint || '-'}
${protocolParams ? `\n■ 상세 사양\n${protocolParams}` : ''}

■ 운영 및 안정성
- 폴링 / 타임아웃: ${pollingInterval}ms / ${timeout}ms
- 재시도: ${retryCount}회 (간격: ${retryInterval}ms)
- 백오프: 초기 ${backoffTime}ms (최대 ${maxBackoffTime}ms, x${backoffMultiplier}배율)
- 버퍼 / 큐: R:${readBufferSize} / W:${writeBufferSize} / Q:${queueSize}
- 검증: 이상치 탐지(${isOutlierDetection ? 'ON' : 'OFF'}), 데드밴드(${isDeadband ? 'ON' : 'OFF'})
${deviceTags.length > 0 ? `- 태그: ${deviceTags.join(', ')}\n` : ''}${Object.keys(deviceMetadata).length > 0 ? `- 메타데이터: 포함됨 (JSON)\n` : ''}

■ 데이터 구성
- 데이터포인트: ${templateDataPoints.length} 개
        `.trim();

        // Confirmation before saving
        const confirmed = await confirm({
            title: '최종 등록 확인',
            message: confirmationMsg,
            confirmButtonType: 'primary',
            showCancelButton: true
        });

        if (!confirmed) return;

        try {
            setLoading(true);
            const res = await TemplateApiService.instantiateTemplate(selectedTemplate.id, {
                name: deviceName,
                site_id: selectedSite,
                edge_server_id: selectedCollector,
                endpoint: finalEndpoint || undefined,
                config: instanceConfig,
                polling_interval: pollingInterval,
                timeout: timeout,
                retry_count: retryCount,
                is_enabled: isEnabled ? 1 : 0,
                data_points: templateDataPoints,
                // Advanced settings
                read_timeout_ms: readTimeout,
                write_timeout_ms: writeTimeout,
                retry_interval_ms: retryInterval,
                backoff_time_ms: backoffTime,
                backoff_multiplier: backoffMultiplier,
                max_backoff_time_ms: maxBackoffTime,
                read_buffer_size: readBufferSize,
                write_buffer_size: writeBufferSize,
                queue_size: queueSize,
                is_keep_alive_enabled: isKeepAlive,
                keep_alive_interval_s: kaInterval,
                keep_alive_timeout_s: kaTimeout,
                is_data_validation_enabled: isDataValidation,
                is_performance_monitoring_enabled: isPerformanceMonitoring,
                is_detailed_logging_enabled: isDetailedLogging,
                is_diagnostic_mode_enabled: isDiagnosticMode,
                is_communication_logging_enabled: isCommLogging,
                is_outlier_detection_enabled: isOutlierDetection,
                is_deadband_enabled: isDeadband,
                tags: deviceTags,
                metadata: deviceMetadata,
                custom_fields: customFields
            });

            if (res.success && res.data) {
                await confirm({
                    title: '생성 완료',
                    message: '디바이스가 성공적으로 생성되었습니다.',
                    confirmButtonType: 'success',
                    showCancelButton: false
                });
                onSuccess(res.data.device_id);
                onClose();
            } else {
                confirm({
                    title: '생성 실패',
                    message: res.message || '디바이스 생성에 실패했습니다.',
                    confirmButtonType: 'danger',
                    showCancelButton: false
                });
            }
        } catch (err) {
            confirm({
                title: '오류 발생',
                message: '디바이스 생성 중 오류가 발생했습니다.',
                confirmButtonType: 'danger',
                showCancelButton: false
            });
        } finally {
            setLoading(false);
        }
    };

    if (!isOpen) return null;

    return createPortal(
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container" style={{ width: step >= 3 ? '1200px' : '800px', maxWidth: '95vw', transition: 'width 0.3s ease' }}>

                <div className="mgmt-modal-header">
                    <h3 className="mgmt-modal-title">
                        <i className="fas fa-magic text-primary"></i> 마스터 모델로 디바이스 추가
                    </h3>
                    <button className="mgmt-close-btn" onClick={onClose}><i className="fas fa-times"></i></button>
                </div>

                <div className="wizard-steps-container">
                    <div className={`wizard-step ${step === 1 ? 'active' : step > 1 ? 'completed' : ''}`}>
                        <div className="step-num">{step > 1 ? <i className="fas fa-check"></i> : '1'}</div>
                        <div className="step-label">마스터 모델 선택</div>
                    </div>
                    <div className="step-line"></div>
                    <div className={`wizard-step ${step === 2 ? 'active' : step > 2 ? 'completed' : ''}`}>
                        <div className="step-num">{step > 2 ? <i className="fas fa-check"></i> : '2'}</div>
                        <div className="step-label">접속 설정</div>
                    </div>
                    <div className="step-line"></div>
                    <div className={`wizard-step ${step === 3 ? 'active' : step > 3 ? 'completed' : ''}`}>
                        <div className="step-num">{step > 3 ? <i className="fas fa-check"></i> : '3'}</div>
                        <div className="step-label">데이터 포인트</div>
                    </div>
                    <div className="step-line"></div>
                    <div className={`wizard-step ${step === 4 ? 'active' : ''}`}>
                        <div className="step-num">4</div>
                        <div className="step-label">운영 및 안정성</div>
                    </div>
                </div>


                <div className="mgmt-modal-body" style={{ flex: 1, overflowY: 'auto', padding: '30px' }}>
                    {error && (
                        <div className="alert alert-error" style={{ marginBottom: '16px' }}>
                            <i className="fas fa-exclamation-circle"></i> {error}
                        </div>
                    )}

                    {step === 1 && (
                        <div className="wizard-content">
                            <h4 className="wizard-title">1. 복제하여 사용할 마스터 모델(템플릿)을 선택하십시오.</h4>
                            <div className="mgmt-modal-form-grid" style={{ marginTop: '20px' }}>
                                <div className="mgmt-modal-form-group">
                                    <label>제조사 (Manufacturer)</label>
                                    <select
                                        className="mgmt-select"
                                        value={selectedManufacturer || ''}
                                        onChange={(e) => handleManufacturerSelect(Number(e.target.value))}
                                    >
                                        <option value="">제조사 선택</option>
                                        {manufacturers.map(m => <option key={m.id} value={m.id}>{m.name}</option>)}
                                    </select>
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>하드웨어 모델 (Model)</label>
                                    <select
                                        className="mgmt-select"
                                        value={selectedModel || ''}
                                        disabled={!selectedManufacturer}
                                        onChange={(e) => handleModelSelect(Number(e.target.value))}
                                    >
                                        <option value="">모델 선택</option>
                                        {models.map(m => <option key={m.id} value={m.id}>{m.name}</option>)}
                                    </select>
                                </div>
                            </div>

                            {selectedModel && (
                                <div style={{ marginTop: '24px' }}>
                                    <label style={{ fontSize: '13px', fontWeight: 600, color: '#475569', marginBottom: '12px', display: 'block' }}>
                                        사용 가능한 마스터 모델 (Templates)
                                    </label>
                                    {templates.length > 0 ? (
                                        <div className="mgmt-grid" style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '16px' }}>
                                            {templates.map(t => (
                                                <div
                                                    key={t.id}
                                                    className={`mgmt-card wizard-card clickable ${selectedTemplate?.id === t.id ? 'active' : ''}`}
                                                    onClick={() => handleTemplateSelect(t)}
                                                >
                                                    <div className="card-header">
                                                        <h5 className="card-title">{t.name}</h5>
                                                        <span className="badge badge-primary">v{t.version || '1.0'}</span>
                                                    </div>
                                                    <div className="card-body">
                                                        <p className="card-desc" style={{ fontSize: '12px' }}>{t.description}</p>
                                                        <div className="card-meta">
                                                            <span><i className="fas fa-list"></i> {t.data_points?.length || 0} Points</span>
                                                            <span style={{ marginLeft: '12px' }}><i className="fas fa-bolt"></i> {t.protocol_name}</span>
                                                        </div>
                                                    </div>
                                                </div>
                                            ))}
                                        </div>
                                    ) : (
                                        <div className="info-box" style={{ background: '#f8fafc' }}>
                                            <i className="fas fa-info-circle"></i>
                                            <span>선택한 모델에 등록된 마스터 모델이 없습니다.</span>
                                        </div>
                                    )}
                                </div>
                            )}

                        </div>
                    )}

                    {step === 2 && selectedTemplate && (
                        <div className="wizard-content">
                            <h4 className="wizard-title">2. 디바이스 식별 및 접속 정보를 설정하십시오.</h4>

                            <div className="wizard-section">
                                <h5 className="wizard-section-title"><i className="fas fa-id-card"></i> 기본 식별 정보</h5>
                                <div className="mgmt-modal-form-grid">
                                    <div className="mgmt-modal-form-group mgmt-span-full">
                                        <label>디바이스 이름 *</label>
                                        <input
                                            className="mgmt-input"
                                            value={deviceName}
                                            onChange={e => setDeviceName(e.target.value)}
                                            placeholder="예: 제1공장 인버터 #1"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>설치 사이트 *</label>
                                        <select
                                            className="mgmt-select"
                                            value={selectedSite || ''}
                                            onChange={e => setSelectedSite(Number(e.target.value))}
                                        >
                                            <option value="">사이트 선택</option>
                                            {sites.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}
                                        </select>
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>담당 콜렉터 (Edge Server) *</label>
                                        <select
                                            className="mgmt-select"
                                            value={selectedCollector || 0}
                                            onChange={e => setSelectedCollector(Number(e.target.value))}
                                        >
                                            <option value={0}>콜렉터 선택</option>
                                            {availableCollectors.map(c => (
                                                <option key={c.id} value={c.id}>{c.name} (ID: {c.id})</option>
                                            ))}
                                        </select>
                                    </div>
                                </div>
                            </div>

                            <div className="wizard-section" style={{ marginTop: '20px' }}>
                                <h5 className="wizard-section-title"><i className="fas fa-network-wired"></i> 통신 및 기술 사양 설정</h5>
                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '30px', alignItems: 'start' }}>
                                    {/* Left Column: Connection Path & Main ID */}
                                    <div className="setup-column-left">
                                        <div style={{ display: 'grid', gridTemplateColumns: 'minmax(0, 2fr) minmax(0, 1fr)', gap: '15px', marginBottom: portConflicts.length > 0 ? '15px' : '0' }}>
                                            {!selectedProtocol?.uses_serial ? (
                                                <div className="mgmt-modal-form-group">
                                                    <label>IP 주소 및 포트 *</label>
                                                    <div className="ip-port-input-group">
                                                        <input
                                                            type="text"
                                                            className="mgmt-input ip-field"
                                                            placeholder="127.0.0.1"
                                                            value={ipAddress}
                                                            onChange={e => setIpAddress(e.target.value)}
                                                        />
                                                        <span className="separator">:</span>
                                                        <input
                                                            type="number"
                                                            className="mgmt-input port-field"
                                                            placeholder="502"
                                                            value={portNumber}
                                                            onChange={e => setPortNumber(e.target.value)}
                                                        />
                                                    </div>
                                                </div>
                                            ) : (
                                                <div className="mgmt-modal-form-group">
                                                    <label>시리얼 포트 경로 *</label>
                                                    <input
                                                        className="mgmt-input"
                                                        value={endpoint}
                                                        onChange={e => setEndpoint(e.target.value)}
                                                        placeholder="/dev/ttyUSB0 또는 COM1"
                                                    />
                                                </div>
                                            )}

                                            {/* Extract and show only the Main ID field (Slave/Unit ID) on the left side */}
                                            {(() => {
                                                let schema = selectedProtocol?.connection_params;
                                                if (typeof schema === 'string') {
                                                    try { schema = JSON.parse(schema); } catch (e) { schema = {}; }
                                                }
                                                if (!schema) return null;

                                                const mainIdKeys = ['slave_id', 'unit_id', 'station_id', 'device_id'];
                                                const mainKey = Object.keys(schema).find(k => mainIdKeys.includes(k.toLowerCase()));

                                                if (!mainKey) return null;
                                                const field = schema[mainKey];

                                                return (
                                                    <div className="mgmt-modal-form-group">
                                                        <label>{field.label || mainKey} {field.required && '*'}</label>
                                                        <input
                                                            type="number"
                                                            className="mgmt-input"
                                                            style={{ backgroundColor: (instanceConfig.device_role === 'master' && ['slave_id', 'unit_id'].includes(mainKey.toLowerCase())) ? '#f1f5f9' : 'white' }}
                                                            value={instanceConfig[mainKey] !== undefined ? instanceConfig[mainKey] : (field.default || '')}
                                                            disabled={instanceConfig.device_role === 'master' && ['slave_id', 'unit_id'].includes(mainKey.toLowerCase())}
                                                            onChange={e => setInstanceConfig({ ...instanceConfig, [mainKey]: Number(e.target.value) })}
                                                            placeholder={field.placeholder || '1'}
                                                        />
                                                    </div>
                                                );
                                            })()}
                                        </div>

                                        {/* Port Usage Status (Topology View) */}
                                        <div style={{ marginTop: '20px' }}>
                                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px', borderBottom: '1px solid #f1f5f9', paddingBottom: '8px' }}>
                                                <i className="fas fa-network-wired text-primary"></i>
                                                <span style={{ fontSize: '13px', fontWeight: 700, color: '#1e293b' }}>포트 사용 현황 (Network Map)</span>
                                            </div>

                                            {portConflicts.length > 0 ? (
                                                <div style={{ background: '#f8fafc', borderRadius: '12px', border: '1px solid #e2e8f0', padding: '16px' }}>
                                                    <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                                                        {portConflicts.sort((a, b) => {
                                                            const ac = typeof a.config === 'string' ? JSON.parse(a.config) : a.config;
                                                            const bc = typeof b.config === 'string' ? JSON.parse(b.config) : b.config;
                                                            return (ac?.slave_id || 0) - (bc?.slave_id || 0);
                                                        }).map(d => {
                                                            const cfg = typeof d.config === 'string' ? JSON.parse(d.config) : d.config;
                                                            const sid = cfg?.slave_id || cfg?.unit_id || cfg?.device_id || 'M';
                                                            const isMaster = cfg?.device_role === 'master' || !cfg?.slave_id;

                                                            return (
                                                                <div key={d.id} style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '8px 12px', background: 'white', borderRadius: '8px', border: '1px solid #f1f5f9', boxShadow: '0 1px 2px rgba(0,0,0,0.05)' }}>
                                                                    <div style={{ width: '32px', height: '32px', borderRadius: '50%', background: isMaster ? '#eff6ff' : '#f0fdf4', display: 'flex', alignItems: 'center', justifyContent: 'center', color: isMaster ? '#2563eb' : '#16a34a', fontWeight: 700, fontSize: '12px' }}>
                                                                        {sid}
                                                                    </div>
                                                                    <div style={{ flex: 1 }}>
                                                                        <div style={{ fontSize: '13px', fontWeight: 600, color: '#334155' }}>{d.name}</div>
                                                                        <div style={{ fontSize: '11px', color: '#94a3b8' }}>{isMaster ? 'Master Device' : `Slave #${sid}`}</div>
                                                                    </div>
                                                                    <span className={`badge badge-${isMaster ? 'primary' : 'success'}`} style={{ fontSize: '10px' }}>{isMaster ? 'MASTER' : 'SLAVE'}</span>
                                                                </div>
                                                            );
                                                        })}
                                                    </div>

                                                    {suggestedId && (
                                                        <div className="info-box success" style={{ marginTop: '15px', borderStyle: 'dashed' }}>
                                                            <i className="fas fa-magic"></i>
                                                            <div style={{ fontSize: '12px' }}>
                                                                사용 가능한 다음 슬레이브 ID는 <strong>{suggestedId}</strong>번 입니다. (자동 적용됨)
                                                            </div>
                                                        </div>
                                                    )}
                                                </div>
                                            ) : (
                                                <div style={{ textAlign: 'center', padding: '30px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', color: '#94a3b8' }}>
                                                    <i className="fas fa-plug" style={{ fontSize: '24px', marginBottom: '10px', display: 'block', opacity: 0.3 }}></i>
                                                    <span style={{ fontSize: '12px' }}>현재 해당 포트에 연결된 디바이스가 없습니다.</span>
                                                </div>
                                            )}
                                        </div>
                                    </div>

                                    {/* Right Column: Serial Technical Parameters */}
                                    <div className="setup-column-right" style={{ background: '#f8fafc', padding: '15px', borderRadius: '10px', border: '1px solid #e2e8f0' }}>
                                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '15px' }}>
                                            {(() => {
                                                let schema = selectedProtocol?.connection_params;
                                                if (typeof schema === 'string') {
                                                    try { schema = JSON.parse(schema); } catch (e) { schema = {}; }
                                                }
                                                if (!schema) return null;

                                                const mainIdKeys = ['slave_id', 'unit_id', 'station_id', 'device_id'];

                                                // Filter out the Main ID which is already shown on the left
                                                return Object.entries(schema)
                                                    .filter(([key]) => !mainIdKeys.includes(key.toLowerCase()))
                                                    .map(([key, field]: [string, any]) => (
                                                        <div key={key} className="mgmt-modal-form-group">
                                                            <label style={{ fontSize: '12px', color: '#64748b' }}>{field.label || key}</label>
                                                            {field.type === 'select' ? (
                                                                <select
                                                                    className="mgmt-select"
                                                                    style={{ height: '36px', fontSize: '13px' }}
                                                                    value={instanceConfig[key] || field.default || ''}
                                                                    onChange={e => setInstanceConfig({ ...instanceConfig, [key]: e.target.value })}
                                                                >
                                                                    {field.options?.map((opt: any) => (
                                                                        <option key={String(opt)} value={opt}>{opt}</option>
                                                                    ))}
                                                                </select>
                                                            ) : field.type === 'boolean' ? (
                                                                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '5px' }}>
                                                                    <label className="switch mini">
                                                                        <input
                                                                            type="checkbox"
                                                                            checked={!!instanceConfig[key]}
                                                                            onChange={e => setInstanceConfig({ ...instanceConfig, [key]: e.target.checked })}
                                                                        />
                                                                        <span className="slider"></span>
                                                                    </label>
                                                                    <span style={{ fontSize: '12px' }}>{field.label || key}</span>
                                                                </div>
                                                            ) : (
                                                                <input
                                                                    type={field.type === 'number' ? 'number' : 'text'}
                                                                    className="mgmt-input"
                                                                    style={{ height: '36px', fontSize: '13px', backgroundColor: (key === 'master_device_id' && instanceConfig.device_role === 'master') ? '#f1f5f9' : 'white' }}
                                                                    value={instanceConfig[key] !== undefined ? instanceConfig[key] : (field.default || '')}
                                                                    disabled={key === 'master_device_id' && instanceConfig.device_role === 'master'}
                                                                    onChange={e => setInstanceConfig({ ...instanceConfig, [key]: field.type === 'number' ? Number(e.target.value) : e.target.value })}
                                                                    placeholder={field.placeholder || ''}
                                                                />
                                                            )}
                                                        </div>
                                                    ));
                                            })()}
                                        </div>
                                    </div>
                                </div>
                            </div>

                        </div>
                    )}
                    {step === 3 && selectedTemplate && (
                        <div className="wizard-layout full-width" style={{ display: 'block' }}>
                            <div className="wizard-main-content" style={{ padding: '30px' }}>
                                <div className="data-points-column" style={{ display: 'flex', flexDirection: 'column' }}>
                                    <section className="wizard-section" style={{ flex: 1, display: 'flex', flexDirection: 'column', margin: 0 }}>
                                        <h4 className="wizard-title">3. 데이터포인트 매핑 및 구성 ({templateDataPoints.length} Points)</h4>
                                        <div style={{ flex: 1, minHeight: '500px', border: '1px solid #e2e8f0', borderRadius: '12px', background: 'white' }}>
                                            <DeviceDataPointsTab
                                                deviceId={0}
                                                dataPoints={templateDataPoints as any}
                                                isLoading={isLoadingPoints}
                                                error={null}
                                                mode="create"
                                                onRefresh={async () => { }}
                                                onCreate={handleDataPointCreate}
                                                onUpdate={handleDataPointUpdate}
                                                onDelete={handleDataPointDelete}
                                            />
                                        </div>
                                    </section>
                                </div>
                            </div>

                        </div>
                    )}

                    {step === 4 && selectedTemplate && (
                        <div className="wizard-layout full-width" style={{ display: 'block' }}>
                            <div className="wizard-main-content" style={{ padding: '30px' }}>
                                <h4 className="wizard-title">4. 상세 운영 및 서비스 안정성 설정을 완료하십시오.</h4>
                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '30px' }}>
                                    {/* Column 1: Core Parameters */}
                                    <section className="wizard-section">
                                        <h5 className="wizard-section-title"><i className="fas fa-cog"></i> 기본 운영 파라미터</h5>
                                        <div className="mgmt-modal-form-grid" style={{ gridTemplateColumns: '1fr 1fr' }}>
                                            <div className="mgmt-modal-form-group">
                                                <label>폴링 간격 (ms) *</label>
                                                <input type="number" className="mgmt-input" value={pollingInterval} onChange={e => setPollingInterval(Math.max(10, Number(e.target.value)))} />
                                            </div>
                                            <div className="mgmt-modal-form-group">
                                                <label>응답 타임아웃 (ms) *</label>
                                                <input type="number" className="mgmt-input" value={timeout} onChange={e => setTimeoutVal(Math.max(100, Number(e.target.value)))} />
                                            </div>
                                            <div className="mgmt-modal-form-group">
                                                <label>최대 재시도 (Count) *</label>
                                                <input type="number" className="mgmt-input" value={retryCount} onChange={e => setRetryCount(Math.max(0, Number(e.target.value)))} />
                                            </div>
                                            <div className="mgmt-modal-form-group">
                                                <label>재시도 간격 (ms)</label>
                                                <input type="number" className="mgmt-input" value={retryInterval} onChange={e => setRetryInterval(Number(e.target.value))} />
                                            </div>
                                        </div>

                                        <div className="mgmt-modal-form-group" style={{ marginTop: '20px' }}>
                                            <div className="st-field-toggle">
                                                <div className="toggle-header" style={{ marginBottom: '10px' }}>
                                                    <label>Keep-Alive 세션 유지</label>
                                                    <label className="switch">
                                                        <input type="checkbox" checked={isKeepAlive} onChange={e => setIsKeepAlive(e.target.checked)} />
                                                        <span className="slider"></span>
                                                    </label>
                                                </div>
                                                {isKeepAlive && (
                                                    <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                                                        <span style={{ fontSize: '13px', color: '#64748b' }}>주기/타임아웃(s):</span>
                                                        <input type="number" className="mgmt-input" style={{ width: '70px' }} value={kaInterval} onChange={e => setKaInterval(Number(e.target.value))} />
                                                        <input type="number" className="mgmt-input" style={{ width: '70px' }} value={kaTimeout} onChange={e => setKaTimeout(Number(e.target.value))} />
                                                    </div>
                                                )}
                                            </div>
                                        </div>
                                    </section>

                                    {/* Column 2: Error & Backoff */}
                                    <section className="wizard-section">
                                        <h5 className="wizard-section-title"><i className="fas fa-undo"></i> 에러 제어 및 지루 백오프</h5>
                                        <div className="mgmt-modal-form-grid" style={{ gridTemplateColumns: '1fr' }}>
                                            <div className="mgmt-modal-form-group">
                                                <label>초기 백오프 시간 (ms)</label>
                                                <input type="number" className="mgmt-input" value={backoffTime} onChange={e => setBackoffTime(Number(e.target.value))} />
                                            </div>
                                            <div className="mgmt-modal-form-group">
                                                <label>최대 백오프 시간 (ms)</label>
                                                <input type="number" className="mgmt-input" value={maxBackoffTime} onChange={e => setMaxBackoffTime(Number(e.target.value))} />
                                            </div>
                                            <div className="mgmt-modal-form-group">
                                                <label>지수 증폭 배율 (Multiplier)</label>
                                                <input type="number" step="0.1" className="mgmt-input" value={backoffMultiplier} onChange={e => setBackoffMultiplier(Number(e.target.value))} />
                                            </div>
                                        </div>
                                    </section>

                                    {/* Column 3: Advanced Buffer & Safety */}
                                    <section className="wizard-section">
                                        <h5 className="wizard-section-title"><i className="fas fa-microchip"></i> 리소스 및 데이터 안전</h5>
                                        <div className="mgmt-modal-form-grid" style={{ gridTemplateColumns: '1fr 1fr' }}>
                                            <div className="mgmt-modal-form-group">
                                                <label>읽기 버퍼 크기</label>
                                                <input type="number" className="mgmt-input" value={readBufferSize} onChange={e => setReadBufferSize(Number(e.target.value))} />
                                            </div>
                                            <div className="mgmt-modal-form-group">
                                                <label>쓰기 버퍼 크기</label>
                                                <input type="number" className="mgmt-input" value={writeBufferSize} onChange={e => setWriteBufferSize(Number(e.target.value))} />
                                            </div>
                                            <div className="mgmt-modal-form-group">
                                                <label>큐 사이즈</label>
                                                <input type="number" className="mgmt-input" value={queueSize} onChange={e => setQueueSize(Number(e.target.value))} />
                                            </div>
                                        </div>

                                        <div className="checkbox-list" style={{ marginTop: '20px', display: 'flex', flexDirection: 'column', gap: '12px', background: '#f8fafc', padding: '15px', borderRadius: '8px', border: '1px solid #e2e8f0' }}>
                                            <label className="checkbox-wrap">
                                                <input type="checkbox" checked={isOutlierDetection} onChange={e => setIsOutlierDetection(e.target.checked)} />
                                                이상치 탐지 (Outlier Detection)
                                            </label>
                                            <label className="checkbox-wrap">
                                                <input type="checkbox" checked={isDeadband} onChange={e => setIsDeadband(e.target.checked)} />
                                                데드밴드 필터 사용 (Deadband)
                                            </label>
                                        </div>
                                    </section>

                                    {/* Row 2: Logging & Metadata (Full Width spanned) */}
                                    <div style={{ gridColumn: 'span 3', display: 'grid', gridTemplateColumns: '1fr 2fr', gap: '30px', borderTop: '1px solid #f1f5f9', paddingTop: '30px' }}>
                                        <section className="wizard-section" style={{ margin: 0 }}>
                                            <h5 className="wizard-section-title"><i className="fas fa-shield-alt"></i> 로깅 및 진단</h5>
                                            <div className="mgmt-modal-form-grid" style={{ gridTemplateColumns: '1fr' }}>
                                                <div className="st-field-toggle">
                                                    <div className="toggle-header">
                                                        <label>성능 모니터링 활성화</label>
                                                        <label className="switch">
                                                            <input type="checkbox" checked={isPerformanceMonitoring} onChange={e => setIsPerformanceMonitoring(e.target.checked)} />
                                                            <span className="slider"></span>
                                                        </label>
                                                    </div>
                                                </div>
                                                <div className="st-field-toggle">
                                                    <div className="toggle-header">
                                                        <label>통신 패킷 로깅 활성화</label>
                                                        <label className="switch">
                                                            <input type="checkbox" checked={isCommLogging} onChange={e => setIsCommLogging(e.target.checked)} />
                                                            <span className="slider"></span>
                                                        </label>
                                                    </div>
                                                </div>
                                                <div className="st-field-toggle">
                                                    <div className="toggle-header">
                                                        <label>정밀 디버깅 모드</label>
                                                        <label className="switch">
                                                            <input type="checkbox" checked={isDiagnosticMode} onChange={e => setIsDiagnosticMode(e.target.checked)} />
                                                            <span className="slider"></span>
                                                        </label>
                                                    </div>
                                                </div>
                                            </div>
                                        </section>

                                        <section className="wizard-section" style={{ margin: 0 }}>
                                            <h5 className="wizard-section-title"><i className="fas fa-tags"></i> 분류 태그 및 시스템 메타데이터</h5>
                                            <div className="mgmt-modal-form-grid" style={{ gridTemplateColumns: '1fr' }}>
                                                <div className="mgmt-modal-form-group">
                                                    <label>태그 (쉼표로 구분)</label>
                                                    <input type="text" className="mgmt-input" placeholder="Line1, Inverter, critical" value={deviceTags.join(', ')} onChange={e => setDeviceTags(e.target.value.split(',').map(s => s.trim()).filter(s => s))} />
                                                </div>
                                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '20px' }}>
                                                    <div className="mgmt-modal-form-group">
                                                        <label>메타데이터 (JSON)</label>
                                                        <textarea className="mgmt-input" style={{ height: '80px', fontSize: '12px', fontFamily: 'monospace' }} value={JSON.stringify(deviceMetadata, null, 2)} onChange={e => { try { setDeviceMetadata(JSON.parse(e.target.value)); } catch (err) { } }} />
                                                    </div>
                                                    <div className="mgmt-modal-form-group">
                                                        <label>커스텀 필드 (JSON)</label>
                                                        <textarea className="mgmt-input" style={{ height: '80px', fontSize: '12px', fontFamily: 'monospace' }} value={JSON.stringify(customFields, null, 2)} onChange={e => { try { setCustomFields(JSON.parse(e.target.value)); } catch (err) { } }} />
                                                    </div>
                                                </div>
                                            </div>
                                        </section>
                                    </div>
                                </div>
                            </div>

                        </div>
                    )}

                </div>

                {/* Unified Bottom Footer */}
                <div className="wizard-navigation-footer">
                    <div className="footer-left">
                        {step > 1 && (
                            <button className="mgmt-btn mgmt-btn-outline btn-large" onClick={() => setStep(step - 1)}>
                                <i className="fas fa-arrow-left"></i> 이전 단계로
                            </button>
                        )}
                    </div>
                    <div className="footer-right">
                        {step === 1 ? (
                            selectedTemplate && (
                                <button className="mgmt-btn mgmt-btn-primary btn-large" onClick={() => setStep(2)}>
                                    다음: 접속 설정 진행 <i className="fas fa-arrow-right"></i>
                                </button>
                            )
                        ) : step === 2 ? (
                            <button
                                className="mgmt-btn mgmt-btn-primary btn-large"
                                disabled={!deviceName || !selectedSite || (!endpoint && !ipAddress)}
                                onClick={() => setStep(3)}
                            >
                                다음: 데이터 포인트 매핑 <i className="fas fa-arrow-right"></i>
                            </button>
                        ) : step === 3 ? (
                            <button className="mgmt-btn mgmt-btn-primary btn-large" onClick={() => setStep(4)}>
                                다음: 고급 설정 완료 <i className="fas fa-arrow-right"></i>
                            </button>
                        ) : (
                            <button
                                className="mgmt-btn mgmt-btn-primary btn-large"
                                onClick={handleCreate}
                                disabled={loading || !deviceName}
                            >
                                {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-check-double"></i>}
                                디바이스 최종 생성 승인
                            </button>
                        )}
                    </div>
                </div>
            </div>

            <style>{`
        .mgmt-modal-overlay {
          position: fixed; top: 0; left: 0; right: 0; bottom: 0;
          background: rgba(0,0,0,0.6); display: flex; align-items: center; justify-content: center; z-index: 1000;
          backdrop-filter: blur(2px);
        }
        .mgmt-modal-container { background: white; border-radius: 16px; box-shadow: 0 25px 50px -12px rgba(0,0,0,0.25); overflow: hidden; display: flex; flex-direction: column; height: 85vh; }
        
        .wizard-layout { display: flex; flex: 1; overflow: hidden; background: #f8fafc; }
        .wizard-main-content { flex: 1; padding: 30px; overflow-y: auto; background: white; position: relative; }
        .wizard-summary-area { width: 320px; background: #f8fafc; border-left: 1px solid #e2e8f0; padding: 30px; overflow-y: auto; }

        .wizard-section { margin-bottom: 40px; }
        .wizard-section-title { font-size: 16px; font-weight: 700; color: #1e293b; margin-bottom: 20px; display: flex; align-items: center; gap: 10px; border-bottom: 2px solid #f1f5f9; padding-bottom: 10px; }
        .wizard-section-title i { color: #22c55e; }

        .wizard-steps-container {
          display: flex; align-items: center; justify-content: center; padding: 20px 40px; background: #fdfdfd; border-bottom: 1px solid #f1f5f9;
        }

        .wizard-navigation-footer {
            padding: 20px 40px;
            background: #f8fafc;
            border-top: 1px solid #e2e8f0;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .wizard-navigation-footer .btn-large {
            width: auto;
            min-width: 200px;
            padding: 14px 30px;
        }
        .wizard-step { display: flex; flex-direction: column; align-items: center; gap: 10px; position: relative; z-index: 1; }
        .step-num { 
          width: 36px; height: 36px; border-radius: 50%; background: #f1f5f9; color: #94a3b8; 
          display: flex; align-items: center; justify-content: center; font-weight: 700; font-size: 15px;
          transition: all 0.3s ease; border: 2px solid #e2e8f0;
        }
        .wizard-step.active .step-num { background: #22c55e; color: white; border-color: #22c55e; box-shadow: 0 0 0 4px rgba(34, 197, 94, 0.2); }
        .wizard-step.completed .step-num { background: #22c55e; color: white; border-color: #22c55e; }
        .step-label { font-size: 13px; font-weight: 600; color: #94a3b8; transition: all 0.3s ease; }
        .wizard-step.active .step-label { color: #166534; }
        .wizard-step.completed .step-label { color: #22c55e; }
        
        .step-line { flex: 0.15; height: 2px; background: #e2e8f0; margin: 0 8px; margin-top: -26px; }
        .wizard-step.completed + .step-line { background: #22c55e; }

        .wizard-title { font-size: 18px; font-weight: 700; color: #1e293b; margin-bottom: 24px; border-left: 4px solid #22c55e; padding-left: 12px; }
        
        .ip-port-input-group { display: flex; align-items: center; gap: 8px; }
        .ip-port-input-group .ip-field { flex: 1; }
        .ip-port-input-group .port-field { width: 90px; }
        .ip-port-input-group .separator { font-weight: 700; color: #cbd5e1; }

        .protocol-config-panel { grid-column: 1 / -1; background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 12px; overflow: hidden; margin-top: 10px; }
        .panel-header { background: #f1f5f9; padding: 10px 16px; border-bottom: 1px solid #e2e8f0; display: flex; align-items: center; gap: 8px; font-size: 13px; font-weight: 700; color: #475569; }
        .panel-body { padding: 16px; display: grid; grid-template-columns: repeat(4, 1fr); gap: 16px; }
        
        .mgmt-modal-form-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 20px; }
        .mgmt-modal-form-group.mgmt-span-full { grid-column: 1 / -1; }
        .mgmt-modal-form-group.compact { grid-column: span 1; }
        .mgmt-modal-form-group label { display: block; font-size: 13px; font-weight: 600; color: #475569; margin-bottom: 8px; }
        .mgmt-modal-form-group label .required { color: #ef4444; margin-left: 2px; }

        .wizard-summary-box { background: white; border-radius: 12px; border: 1px solid #e2e8f0; padding: 0; overflow: hidden; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); }
        .summary-title { font-size: 15px; font-weight: 700; color: #1e293b; background: #f1f5f9; padding: 15px 20px; border-bottom: 1px solid #e2e8f0; display: flex; align-items: center; gap: 10px; }
        .summary-title i { color: #64748b; }
        .summary-content { padding: 20px; }
        .summary-item { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
        .summary-item label { font-size: 13px; color: #64748b; font-weight: 500; }
        .summary-item span { font-size: 13px; color: #1e293b; font-weight: 600; }
        .protocol-badge { background: #eff6ff; color: #1d4ed8; padding: 2px 8px; border-radius: 4px; font-size: 11px; }

        .summary-footer { padding: 20px; background: #f8fafc; border-top: 1px solid #e2e8f0; }
        .summary-hint { font-size: 12px; margin-bottom: 15px; display: flex; align-items: flex-start; gap: 8px; line-height: 1.5; }
        .summary-hint.success { color: #166534; }
        .summary-hint i { margin-top: 2px; }

        .summary-actions { display: flex; flex-direction: column; gap: 10px; }
        .btn-large { width: 100%; padding: 12px; font-size: 15px; font-weight: 700; display: flex; align-items: center; justify-content: center; gap: 10px; }

        .wizard-points-area { border: 1px solid #e2e8f0; border-radius: 12px; overflow: hidden; min-height: 300px; }

        .field-error { color: #ef4444; font-size: 11px; font-weight: 600; margin-top: 4px; }

        /* Switch Toggle */
        .switch {
          position: relative; display: inline-block; width: 40px; height: 20px;
        }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider {
          position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
          background-color: #ccc; transition: .4s; border-radius: 20px;
        }
        .slider:before {
          position: absolute; content: ""; height: 16px; width: 16px; left: 2px; bottom: 2px;
          background-color: white; transition: .4s; border-radius: 50%;
        }
        input:checked + .slider { background-color: #22c55e; }
        input:checked + .slider:before { transform: translateX(20px); }

        .toggle-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 4px; }
        .st-field-toggle { padding: 4px 0; }
        .hint-text { font-size: 11px; color: #94a3b8; margin-top: 2px; }

        .info-box { background: #eff6ff; border: 1px solid #dbeafe; border-radius: 8px; padding: 12px; display: flex; gap: 8px; font-size: 12px; color: #1e40af; align-items: flex-start; margin-top: 10px; }
        .info-box i { margin-top: 2px; }
      `}</style>
        </div >,
        document.body
    );
};

export default DeviceTemplateWizard;
