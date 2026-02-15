import React, { useState, useEffect, useRef } from 'react';
import { Modal, Steps, Form, Input, Row, Col, Radio, Select, Checkbox, Button, Divider, Space, InputNumber, Tag } from 'antd';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';
import exportGatewayApi, { Gateway, Assignment, ExportTarget, ExportProfile, PayloadTemplate, DataPoint, ExportTargetMapping, ExportSchedule } from '../../../api/services/exportGatewayApi';
import DataPointSelector from '../components/DataPointSelector';

// Helper to safely extract array from various API response formats
const extractItems = (data: any): any[] => {
    if (!data) return [];
    if (Array.isArray(data)) return data;
    if (Array.isArray(data.items)) return data.items;
    if (Array.isArray(data.rows)) return data.rows;
    return [];
};

const VARIABLE_CATEGORIES = [
    {
        name: '매핑 (Target Mapping)',
        items: [
            { label: '매핑 명칭 (TARGET KEY)', value: '{{target_key}}', desc: '프로파일 탭에서 설정한 데이터의 최종 이름' },
            { label: '데이터 값 (VALUE)', value: '{{measured_value}}', desc: 'Scale/Offset이 반영된 실제 측정값' },
            { label: '타켓 설명 (DESC)', value: '{{target_description}}', desc: '프로파일에서 설정한 데이터 설명' }
        ]
    },
    {
        name: '수집기 원본 (Collector)',
        items: [
            { label: '건물 ID', value: '{{site_id}}', desc: '원본 건물(Building) ID' },
            { label: '포인트 ID', value: '{{point_id}}', desc: '원본 포인트(Point) ID' },
            { label: '원본 이름', value: '{{original_name}}', desc: '수집기 내부 원본 포인트명' },
            { label: '데이터 타입', value: '{{type}}', desc: 'num:숫자, bit:디지털, str:문자열' }
        ]
    },
    {
        name: '상태 및 알람 (Status)',
        items: [
            { label: '통신 상태', value: '{{status_code}}', desc: '0:정상, 1:통신끊김' },
            { label: '알람 등급', value: '{{alarm_level}}', desc: '0:정상, 1:주의, 2:경고' },
            { label: '알람 상태명', value: '{{alarm_status}}', desc: 'NORMAL, WARNING, CRITICAL 등' }
        ]
    },
    {
        name: '범위 및 한계 (Ranges)',
        items: [
            { label: '계측 범위(Min)', value: '{{mi}}', desc: '미터링 최소 한계값' },
            { label: '계측 범위(Max)', value: '{{mx}}', desc: '미터링 최대 한계값' },
            { label: '정보 한계', value: '{{il}}', desc: '임계치 정보(Information Limit)' },
            { label: '위험 한계', value: '{{xl}}', desc: '임계치 위험(Extra Limit)' }
        ]
    },
    {
        name: '시간 (Timestamp)',
        items: [
            { label: '표준 시간', value: '{{timestamp}}', desc: 'YYYY-MM-DD HH:mm:ss.fff' },
            { label: 'ISO8601', value: '{{timestamp_iso8601}}', desc: '표준 시각 포맷' },
            { label: 'Unix (ms)', value: '{{timestamp_unix_ms}}', desc: '1970년 기준 밀리초' }
        ]
    }
];

const SAMPLE_DATA: Record<string, any> = {
    // New Aliases
    target_key: "PRO_FILE_TARGET_KEY",
    mapping_name: "PRO_FILE_TARGET_KEY",
    measured_value: "45.2 (MEASURED_VALUE)",
    point_value: "45.2 (MEASURED_VALUE)",
    target_description: "프로파일에 정의된 데이터 설명",
    site_id: "280 (SITE_ID)",
    status_code: "0 (NORMAL)",
    alarm_level: "1 (WARNING)",
    timestamp: new Date().toISOString().replace('T', ' ').split('.')[0],
    type: "num",

    // Backward Compatibility
    nm: "PRO_FILE_TARGET_KEY",
    vl: "45.2 (MEASURED_VALUE)",
    tm: new Date().toISOString().replace('T', ' ').split('.')[0],
    des: "프로파일에 정의된 데이터 설명",
    bd: "280 (SITE_ID)",
    st: "0 (NORMAL)",
    al: "1 (WARNING)",
    ty: "num",

    // Others
    point_id: "1024",
    original_name: "PUMP_01_STS",
    original_nm: "PUMP_01_STS",
    timestamp_iso8601: new Date().toISOString(),
    timestamp_unix_ms: Date.now(),
    alarm_status: "WARNING",
    il: "-",
    xl: "1",
    mi: "[0]",
    mx: "[100]"
};

interface ExportGatewayWizardProps {
    visible: boolean;
    onClose: () => void;
    onSuccess: () => void;
    profiles: ExportProfile[];
    targets: ExportTarget[];
    templates: PayloadTemplate[];
    allPoints: DataPoint[];
    editingGateway?: Gateway | null;
    assignments?: Assignment[];
    schedules?: ExportSchedule[];
    onDelete?: (gateway: Gateway) => Promise<void>; // [NEW] Delete handler for edit mode
}

const ExportGatewayWizard: React.FC<ExportGatewayWizardProps> = ({ visible, onClose, onSuccess, profiles, targets, templates, allPoints, editingGateway, assignments = [], schedules = [], onDelete }) => {
    const { confirm } = useConfirmContext();
    const [currentStep, setCurrentStep] = useState(0);
    const [loading, setLoading] = useState(false);

    // Track previous states to prevent redundant resets during polling
    const prevVisibleRef = React.useRef(visible);
    const prevEditingIdRef = React.useRef<number | undefined>(editingGateway?.id);
    const hydratedTargetIdRef = React.useRef<number | null>(null);
    const lastMappedProfileIdRef = React.useRef<number | null>(null);

    // Step Data
    const [gatewayData, setGatewayData] = useState({
        name: '',
        ip_address: '127.0.0.1',
        description: '',
        subscription_mode: 'all' as 'all' | 'selective',
        config: {} as any
    });

    // Profile State
    const [profileMode, setProfileMode] = useState<'existing' | 'new'>('existing');
    const [selectedProfileId, setSelectedProfileId] = useState<number | null>(null);
    const [newProfileData, setNewProfileData] = useState({ name: '', description: '', data_points: [] as any[] });

    // Template State
    const [templateMode, setTemplateMode] = useState<'existing' | 'new'>('existing');
    const [selectedTemplateId, setSelectedTemplateId] = useState<number | null>(templates[0]?.id || null);
    const [newTemplateData, setNewTemplateData] = useState({
        name: '',
        system_type: 'custom',
        template_json: '{\n  "site_id": "{{site_id}}",\n  "type": "{{type}}",\n  "target_key": "{{target_key}}",\n  "measured_value": "{{measured_value}}",\n  "timestamp": "{{timestamp}}"\n}'
    });
    const [templateEditMode, setTemplateEditMode] = useState<'simple' | 'advanced'>('advanced');
    const [templateSimpleMappings, setTemplateSimpleMappings] = useState<{ key: string; value: string }[]>([]);
    const [templateIsWrappedInArray, setTemplateIsWrappedInArray] = useState(false);
    const [lastFocusedTemplateElement, setLastFocusedTemplateElement] = useState<{ id: string, index?: number, field?: 'key' | 'value' } | null>(null);

    // Target & Mode State
    const [transmissionMode, setTransmissionMode] = useState<'INTERVAL' | 'EVENT'>('INTERVAL');
    const [selectedProtocols, setSelectedProtocols] = useState<string[]>(['HTTP']);
    const [targetMode, setTargetMode] = useState<'existing' | 'new'>('new');
    const [selectedTargetIds, setSelectedTargetIds] = useState<number[]>([]);
    const [targetData, setTargetData] = useState({
        name: '',
        config_http: [{
            url: '',
            method: 'POST',
            auth_type: 'NONE',
            headers: { Authorization: '' },
            auth: { type: 'x-api-key', apiKey: '' },
            execution_order: 0
        }] as any[],
        config_mqtt: [{ url: '', topic: 'pulseone/data', execution_order: 0 }] as any[],
        config_s3: [{ endpoint: '', BucketName: '', Folder: '', ObjectKeyTemplate: '', AccessKeyID: '', SecretAccessKey: '', execution_order: 0 }] as any[]
    });

    const [editingTargets, setEditingTargets] = useState<Record<number, any>>({});

    // [NEW] Robust sync: Ensure editingTargets is always populated for selected IDs
    // This handles both manual selection and hydration during Edit mode.
    useEffect(() => {
        if (selectedTargetIds.length === 0) return;

        setEditingTargets(prev => {
            let changed = false;
            const next = { ...prev };

            // Step 1: Populate missing configurations
            selectedTargetIds.forEach(id => {
                if (!next[id]) {
                    const t = targets.find(x => x.id === id);
                    if (t) {
                        try {
                            let parsed = typeof t.config === 'string' ? JSON.parse(t.config) : t.config;
                            if (!parsed) parsed = {};
                            if (!Array.isArray(parsed)) parsed = [parsed];

                            const priorityMap = editingGateway?.config?.target_priorities || {};
                            const overrideOrder = priorityMap[id];

                            // Default to 100 if no override (will be re-sequenced below if needed)
                            parsed[0].execution_order = overrideOrder !== undefined ? overrideOrder : 100;

                            next[id] = parsed;
                            changed = true;
                        } catch (err) {
                            console.error(`Failed to parse config for target ${id}`, err);
                            next[id] = [{}];
                            changed = true;
                        }
                    }
                }
            });

            // Step 2: Auto-sequence priorities to ensure they are always 1, 2, 3...
            // This handles additions, removals, and initial hydration consistently.
            selectedTargetIds.forEach((id, idx) => {
                const priority = idx + 1;
                if (next[id] && next[id][0]) {
                    if (next[id][0].execution_order !== priority) {
                        next[id][0] = { ...next[id][0], execution_order: priority };
                        changed = true;
                    }
                }
            });

            return changed ? next : prev;
        });
    }, [selectedTargetIds, targets, editingGateway]);


    const [mappings, setMappings] = useState<Partial<ExportTargetMapping>[]>([]);
    const [bulkSiteId, setBulkSiteId] = useState<string>('');
    const [scheduleData, setScheduleData] = useState({
        schedule_name: '',
        cron_expression: '*/1 * * * *',
        data_range: 'minute' as const,
        lookback_periods: 1
    });

    const updateTargetPriority = (type: 'existing' | 'new', idOrProto: number | string, index: number, newPriority: number) => {
        // Collect all targets that need a priority
        const items: any[] = [];

        // Existing targets
        selectedTargetIds.forEach(tid => {
            const configs = editingTargets[tid] || [];
            configs.forEach((c: any, cIdx: number) => {
                items.push({ type: 'existing', id: tid, index: cIdx, priority: c.execution_order || 0, name: targets.find(t => t.id === tid)?.name });
            });
        });

        // New targets
        selectedProtocols.forEach(proto => {
            const protoKey = `config_${proto.toLowerCase()}` as 'config_http' | 'config_mqtt' | 'config_s3';
            const configs = targetData[protoKey] || [];
            configs.forEach((c: any, cIdx: number) => {
                items.push({ type: 'new', protocol: protoKey, index: cIdx, priority: c.execution_order || 0, name: `${targetData.name}_${proto}` });
            });
        });

        // Identify the target being changed
        const target = items.find(it =>
            (type === 'existing' ? (it.type === 'existing' && it.id === idOrProto && it.index === index) : (it.type === 'new' && it.protocol === idOrProto && it.index === index))
        );

        if (!target) return;

        // Shift logic: If we set a priority, others shift up
        // Simple and effective reordering: 
        // 1. Remove the target being moved
        // 2. Insert it at the desired position
        // 3. Re-assign priorities sequentially

        const otherItems = items.filter(it => it !== target).sort((a, b) => (a.priority || 0) - (b.priority || 0));

        // Insert at index (newPriority - 1)
        const insertionIndex = Math.max(0, Math.min(newPriority - 1, otherItems.length));
        otherItems.splice(insertionIndex, 0, target);

        // Update all items with new sequential priorities
        const nextTargetData = { ...targetData };
        const nextEditingTargets = { ...editingTargets };

        otherItems.forEach((it, idx) => {
            const priority = idx + 1;
            if (it.type === 'existing') {
                const configs = [...(nextEditingTargets[it.id] || [])];
                if (configs[it.index]) {
                    configs[it.index] = { ...configs[it.index], execution_order: priority };
                    nextEditingTargets[it.id] = configs;
                }
            } else {
                const protoKey = it.protocol as 'config_http' | 'config_mqtt' | 'config_s3';
                const configs = [...(nextTargetData[protoKey] || [])];
                if (configs[it.index]) {
                    configs[it.index] = { ...configs[it.index], execution_order: priority };
                    nextTargetData[protoKey] = configs;
                }
            }
        });

        setTargetData(nextTargetData);
        setEditingTargets(nextEditingTargets);
    };

    const totalTargetCount = selectedTargetIds.length +
        (selectedProtocols.includes('HTTP') ? targetData.config_http.length : 0) +
        (selectedProtocols.includes('MQTT') ? targetData.config_mqtt.length : 0) +
        (selectedProtocols.includes('S3') ? targetData.config_s3.length : 0);

    const isHydratingRef = React.useRef(false);

    // [REFINED] Robust Hydration Logic: Runs whenever required data changes until fully hydrated for current ID
    useEffect(() => {
        if (!visible) {
            // Reset state when closing
            if (prevVisibleRef.current) {
                prevVisibleRef.current = false;
                prevEditingIdRef.current = undefined;
                hydratedTargetIdRef.current = null;
                setGatewayData({ name: '', ip_address: '127.0.0.1', description: '', subscription_mode: 'all', config: {} });
                setProfileMode('existing');
                setSelectedProfileId(null);
                setMappings([]);
                setCurrentStep(0);
            }
            return;
        }

        prevVisibleRef.current = visible;

        // If not in Edit mode, just handle initial open reset if needed
        if (!editingGateway) {
            if (hydratedTargetIdRef.current !== -1) { // -1 marks "New registration mode"
                setGatewayData({ name: '', ip_address: '127.0.0.1', description: '', subscription_mode: 'all', config: {} });
                setProfileMode('existing');
                setSelectedProfileId(null);
                setMappings([]);
                setTargetMode('new');
                setSelectedTargetIds([]);
                setTargetData({
                    name: '',
                    config_http: [{ url: '', method: 'POST', auth_type: 'NONE', headers: { Authorization: '' }, auth: { type: 'x-api-key', apiKey: '' }, execution_order: 0 }],
                    config_mqtt: [{ url: '', topic: 'pulseone/data', execution_order: 0 }],
                    config_s3: [{ endpoint: '', BucketName: '', Folder: '', ObjectKeyTemplate: '', AccessKeyID: '', SecretAccessKey: '', execution_order: 0 }]
                });
                setScheduleData({ schedule_name: '', cron_expression: '*/1 * * * *', data_range: 'minute', lookback_periods: 1 });
                setTransmissionMode('INTERVAL');
                setCurrentStep(0);
                hydratedTargetIdRef.current = -1;
            }
            return;
        }

        const currentId = editingGateway.id;

        // Skip if already hydrated for this ID
        if (hydratedTargetIdRef.current === currentId) {
            // Potentially handle live updates to 'assignments' even after hydration? 
            // Usually, we only want initial hydration to let user edit.
            return;
        }

        // Check if data is ready for hydration
        const myAssignments = assignments || [];
        if (myAssignments.length === 0) {
            console.log(`[Wizard] Waiting for assignments for Gateway ${currentId}...`);
            return;
        }

        console.log(`[Wizard] Hydrating Gateway ${currentId}...`);
        prevEditingIdRef.current = currentId;

        // Base Data
        setGatewayData({
            name: editingGateway.name,
            ip_address: editingGateway.ip_address,
            description: editingGateway.description || '',
            subscription_mode: editingGateway.subscription_mode || 'all',
            config: editingGateway.config || {}
        });

        // Profile & Targets
        // Profile & Targets - Pick the LATEST assignment if multiple exist
        const sortedAssignments = [...myAssignments].sort((a, b) => (b.id || 0) - (a.id || 0));
        const assign = sortedAssignments[0];

        setSelectedProfileId(assign.profile_id);
        lastMappedProfileIdRef.current = assign.profile_id;
        setProfileMode('existing');

        // Mappings Population - Robust Fallback
        const p = profiles.find(x => x.id === assign.profile_id);
        if (p && p.data_points) {
            const initialMappings = (p.data_points || []).map((dp: any) => ({
                point_id: dp.id,
                site_id: dp.site_id || dp.building_id || 0,
                target_field_name: dp.name,
                is_enabled: true,
                conversion_config: { scale: 1, offset: 0 }
            }));
            setMappings(initialMappings);
        }

        const linkedTargets = targets.filter(t => t.profile_id === assign.profile_id);
        if (linkedTargets.length > 0) {
            setTargetMode('existing');
            setSelectedTargetIds(linkedTargets.map(t => t.id));

            const primaryTarget = linkedTargets[0];

            // Template
            if (primaryTarget.template_id) {
                setSelectedTemplateId(primaryTarget.template_id);
                setTemplateMode('existing');
            }

            // Mappings (Fetch from server if available, overwrite defaults)
            (async () => {
                try {
                    const mRes = await exportGatewayApi.getTargetMappings(primaryTarget.id);
                    if (mRes.success && mRes.data && mRes.data.length > 0) {
                        setMappings(mRes.data);
                    }
                } catch (err) {
                    console.error("Failed to fetch existing mappings", err);
                }
            })();

            // Schedule
            const linkedSchedule = schedules.find(s => s.target_id === primaryTarget.id);
            if (linkedSchedule) {
                setTransmissionMode('INTERVAL');
                setScheduleData({
                    schedule_name: linkedSchedule.schedule_name,
                    cron_expression: linkedSchedule.cron_expression,
                    data_range: (linkedSchedule.data_range as any) || 'minute',
                    lookback_periods: linkedSchedule.lookback_periods || 1
                });
            } else {
                setTransmissionMode('EVENT');
            }
        } else {
            // No targets yet, but keep it in existing mode if we have a profile
            setTargetMode('new');
        }

        hydratedTargetIdRef.current = currentId; // Mark as done for this ID
    }, [visible, editingGateway, assignments, targets, templates, schedules]);


    // [NEW] Auto-generate mappings when target configs change or profile is selected
    useEffect(() => {
        // [SCENARIO A] New Profile
        if (profileMode === 'new' && newProfileData.data_points.length > 0) {
            if (mappings.length === 0) {
                const initialMappings = newProfileData.data_points.map(p => ({
                    point_id: p.id,
                    site_id: p.site_id || p.building_id || 0,
                    target_field_name: p.name,
                    is_enabled: true,
                    conversion_config: { scale: 1, offset: 0 }
                }));
                setMappings(initialMappings);
            }
        }
        // [SCENARIO B] Existing Profile selected (REFINED: Merge Reset & Populate & Empty Fallback)
        else if (profileMode === 'existing' && selectedProfileId) {
            const isProfileChanged = selectedProfileId !== lastMappedProfileIdRef.current;
            const isEmpty = mappings.length === 0;

            if (isProfileChanged || isEmpty) {
                const p = profiles.find(x => x.id === selectedProfileId);
                if (p && p.data_points?.length > 0) {
                    if (isProfileChanged) {
                        console.log(`[Wizard] Profile changed (${lastMappedProfileIdRef.current} -> ${selectedProfileId}). Resetting targets and regenerating mappings...`);
                        setSelectedTargetIds([]); // Reset on manual profile change
                    } else {
                        console.log(`[Wizard] Mappings empty for Profile ${selectedProfileId}. Generating defaults from points...`);
                    }

                    const initialMappings = (p.data_points || []).map((dp: any) => ({
                        point_id: dp.id,
                        site_id: dp.site_id || dp.building_id || 0,
                        target_field_name: dp.name,
                        is_enabled: true,
                        conversion_config: { scale: 1, offset: 0 }
                    }));

                    setMappings(initialMappings);
                    lastMappedProfileIdRef.current = selectedProfileId;
                }
            }
        }
    }, [profileMode, newProfileData.data_points, selectedProfileId, profiles]);


    const steps = [
        { title: '게이트웨이', description: '기본 정보' },
        { title: '프로파일', description: '데이터 포인트' },
        { title: '템플릿', description: '전송 포맷' },
        { title: '전송 타겟', description: '프로토콜/매핑' },
        { title: '스케줄', description: '전송 주기' }
    ];

    const handleNext = () => {
        // Validation logic
        if (currentStep === 0) {
            if (!gatewayData.name) { confirm({ title: '입력 오류', message: '게이트웨이 명칭을 입력해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
        }
        if (currentStep === 1) {
            if (profileMode === 'new') {
                if (!newProfileData.name) { confirm({ title: '입력 오류', message: '프로파일 명칭을 입력해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
                if (newProfileData.data_points.length === 0) { confirm({ title: '입력 오류', message: '최소 1개 이상의 데이터 포인트를 선택해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
            } else {
                if (!selectedProfileId) { confirm({ title: '입력 오류', message: '프로파일을 선택해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
            }
        }
        if (currentStep === 2) {
            if (templateMode === 'new' && !newTemplateData.name) { confirm({ title: '입력 오류', message: '템플릿 명칭을 입력해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
            if (templateMode === 'existing' && !selectedTemplateId) { confirm({ title: '입력 오류', message: '템플릿을 선택해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
        }
        if (currentStep === 3) {
            if (targetMode === 'new') {
                if (selectedProtocols.length === 0) { confirm({ title: '입력 오류', message: '하나 이상의 전송 프로토콜을 선택해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
                // Detailed validation for each protocol... omitted for brevity but strictly required in prod
            } else {
                if (selectedTargetIds.length === 0) { confirm({ title: '입력 오류', message: '전송 타겟을 하나 이상 선택해주세요.', showCancelButton: false, confirmButtonType: 'danger' }); return; }
            }
        }

        setCurrentStep(currentStep + 1);
    };

    const handlePrev = () => setCurrentStep(currentStep - 1);

    const handleFinish = async () => {
        setLoading(true);
        let gatewayId = editingGateway?.id;
        const priorityMap: Record<string, number> = {};

        try {
            // 1. Create/Update Gateway (Ensure ID exists first)
            if (!gatewayId) {
                const res = await exportGatewayApi.registerGateway(gatewayData);
                gatewayId = res.data?.id;
            }

            if (!gatewayId) throw new Error("게이트웨이 ID 확보 실패");

            // 2. Profile
            let profileId = selectedProfileId;
            if (profileMode === 'new') {
                const pRes = await exportGatewayApi.createProfile({
                    name: newProfileData.name,
                    description: newProfileData.description,
                    data_points: newProfileData.data_points,
                    is_enabled: true
                });
                profileId = pRes.data?.id;
            }

            if (!profileId) throw new Error("프로파일 ID 확보 실패");

            // 2. Profile Assignment (Single-Profile Triage)
            if (profileId) {
                try {
                    const assignmentsRes = await exportGatewayApi.getAssignments(gatewayId);
                    if (assignmentsRes.success && Array.isArray(assignmentsRes.data)) {
                        const oldAssignments = assignmentsRes.data;
                        // Unassign all BUT the current desired profile
                        for (const old of oldAssignments) {
                            if (old.profile_id !== profileId) {
                                await exportGatewayApi.unassignProfile(gatewayId, old.profile_id);
                            }
                        }

                        // Only assign if it's not already there
                        const alreadyAssigned = oldAssignments.some(a => a.profile_id === profileId);
                        if (!alreadyAssigned) {
                            await exportGatewayApi.assignProfile(gatewayId, profileId);
                        }
                    } else {
                        // Fallback
                        await exportGatewayApi.assignProfile(gatewayId, profileId).catch(() => { });
                    }
                } catch (err) {
                    console.error("Assignment triage failed", err);
                    await exportGatewayApi.assignProfile(gatewayId, profileId).catch(() => { });
                }
            }

            // 3. Template
            let templateId = selectedTemplateId;
            if (templateMode === 'new') {
                const tRes = await exportGatewayApi.createTemplate({
                    ...newTemplateData,
                    is_active: true
                });
                templateId = tRes.data?.id;
            }

            // 4. Target(s)
            let finalTargetIds: number[] = [];
            if (targetMode === 'existing') {
                finalTargetIds = selectedTargetIds;

                // [FIXED] Update existing targets with ALL fields (profile, template, mode, config)
                await Promise.all(selectedTargetIds.map(async (tid) => {
                    const editedConfigs = editingTargets[tid]; // This is an Array
                    if (editedConfigs && editedConfigs.length > 0) {
                        try {
                            // Store priority for gateway map
                            const priority = editedConfigs[0]?.execution_order || 100;
                            priorityMap[tid.toString()] = priority;

                            await exportGatewayApi.updateTarget(tid, {
                                profile_id: profileId, // [NEW] Ensure target stays on current profile
                                config: editedConfigs,
                                template_id: templateId || undefined,
                                export_mode: transmissionMode === 'EVENT' ? 'REALTIME' : 'batched'
                            });
                        } catch (err) {
                            console.error(`Failed to update target for ${tid}`, err);
                        }
                    }

                    // [NEW] Always save current mappings to selected targets 
                    if (mappings.length > 0) {
                        try {
                            await exportGatewayApi.saveTargetMappings(tid, mappings);
                        } catch (err) {
                            console.error(`Failed to save mappings for target ${tid}`, err);
                        }
                    }
                }));
            } else {
                // Create targets for each configured protocol and each config item
                for (const proto of selectedProtocols) {
                    const protoKey = `config_${proto.toLowerCase()}` as 'config_http' | 'config_mqtt' | 'config_s3';
                    const configs = targetData[protoKey] || [];

                    for (const [cIdx, cfg] of configs.entries()) {
                        const targetName = configs.length > 1 ? `${targetData.name}_${proto}_${cIdx + 1}` : `${targetData.name}_${proto}`;

                        const tRes = await exportGatewayApi.createTarget({
                            name: targetName,
                            target_type: proto,
                            profile_id: profileId,
                            template_id: templateId || undefined,
                            config: [cfg], // Save as array of one
                            is_enabled: true,
                            export_mode: transmissionMode === 'EVENT' ? 'REALTIME' : 'batched'
                        });

                        if (tRes.data?.id) {
                            finalTargetIds.push(tRes.data.id);
                            // Store priority for gateway map
                            priorityMap[tRes.data.id.toString()] = cfg.execution_order || (cIdx + 1);
                            // Save Mappings
                            if (mappings.length > 0) {
                                await exportGatewayApi.saveTargetMappings(tRes.data.id, mappings);
                            }
                        }
                    }
                }
            }

            // 5. Schedule
            // If INTERVAL mode, create schedule for each target
            if (transmissionMode === 'INTERVAL') {
                for (const tid of finalTargetIds) {
                    await exportGatewayApi.createSchedule({
                        target_id: tid,
                        schedule_name: scheduleData.schedule_name || `${gatewayData.name}_Schedule`,
                        cron_expression: scheduleData.cron_expression,
                        data_range: scheduleData.data_range,
                        lookback_periods: 1,
                        is_enabled: true
                    });
                }
            }

            // 6. Finalize/Update Gateway with the Priority Map
            const finalGatewayData = {
                ...gatewayData,
                config: {
                    ...(gatewayData.config || {}),
                    target_priorities: priorityMap
                }
            };

            // Ensure ID is passed for update
            await exportGatewayApi.updateGateway(gatewayId, finalGatewayData);

            await confirm({ title: '설정 완료', message: '게이트웨이 및 전송 설정이 완료되었습니다.', showCancelButton: false, confirmButtonType: 'success' });
            onSuccess();
        } catch (e) {
            console.error(e);
            await confirm({ title: '설정 실패', message: '저장 중 오류가 발생했습니다. 로그를 확인해주세요.', showCancelButton: false, confirmButtonType: 'danger' });
        } finally {
            setLoading(false);
        }
    };


    return (
        <Modal
            title={<><i className={`fas ${editingGateway ? 'fa-edit' : 'fa-magic'} `} style={{ marginRight: '8px' }} />{editingGateway ? '게이트웨이 설정 수정' : '게이트웨이 등록 마법사'}</>}
            open={visible}
            onCancel={onClose}
            width={900}
            footer={null}
            maskClosable={false}
            destroyOnClose
            className="wizard-modal"
        >
            <div style={{ display: 'flex', flexDirection: 'column', height: '650px' }}>
                <div style={{ padding: '0 0 24px 0' }}>
                    <Steps current={currentStep} size="small" className="site-navigation-steps">
                        {steps.map(s => <Steps.Step key={s.title} title={s.title} description={s.description} />)}
                    </Steps>
                </div>

                <div className="wizard-content" style={{ flex: 1, overflowY: 'auto', padding: '16px', border: '1px solid #f0f0f0', borderRadius: '12px', background: 'white' }}>
                    {/* STEP 0: Gateway Info */}
                    {currentStep === 0 && (
                        <div style={{
                            padding: '32px',
                            background: 'white',
                            borderRadius: '12px',
                            height: '100%',
                            display: 'flex',
                            flexDirection: 'column',
                            gap: '24px',
                            boxShadow: '0 4px 12px rgba(0,0,0,0.03)',
                            border: '1px solid #f0f0f0'
                        }}>
                            <div style={{ fontSize: '18px', fontWeight: 800, color: '#1a1a1a', display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
                                <i className="fas fa-server" style={{ marginRight: '12px', color: '#1890ff' }} />
                                게이트웨이 기본 정보 설정
                            </div>

                            <Form layout="vertical">
                                <Form.Item label="게이트웨이 명칭 (Unique Name)" required tooltip="시스템 내에서 유일한 식별자입니다.">
                                    <Input
                                        size="large"
                                        autoFocus
                                        placeholder="예: GW_Factory_A_Line1"
                                        value={gatewayData.name}
                                        onChange={e => setGatewayData({ ...gatewayData, name: e.target.value })}
                                    />
                                </Form.Item>
                                <Form.Item label="IP 주소 (접속 허용 주소)" required tooltip="데이터를 수신할 허용 IP입니다. Docker 내부 통신시 127.0.0.1 권장">
                                    <Input
                                        size="large"
                                        placeholder="127.0.0.1"
                                        value={gatewayData.ip_address}
                                        onChange={e => setGatewayData({ ...gatewayData, ip_address: e.target.value })}
                                    />
                                </Form.Item>
                                <Form.Item label="알람 구독 모드 (Alarm Subscription)" required tooltip="데이터 수신 범위를 설정합니다. 'Selective' 선택 시 할당된 디바이스 알람만 처리합니다.">
                                    <Radio.Group
                                        size="large"
                                        value={gatewayData.subscription_mode}
                                        onChange={e => setGatewayData({ ...gatewayData, subscription_mode: e.target.value as 'all' | 'selective' })}
                                    >
                                        <Radio value="all">전체 수신 (All)</Radio>
                                        <Radio value="selective">이용 디바이스 한정 (Selective)</Radio>
                                    </Radio.Group>
                                </Form.Item>
                                <Form.Item label="설명 (Description)">
                                    <Input.TextArea
                                        placeholder="관리용 상세 설명"
                                        rows={4}
                                        value={gatewayData.description}
                                        onChange={e => setGatewayData({ ...gatewayData, description: e.target.value })}
                                    />
                                </Form.Item>
                            </Form>
                        </div>
                    )}

                    {/* STEP 1: Profile */}
                    {currentStep === 1 && (
                        <div>
                            <Radio.Group
                                value={profileMode}
                                onChange={e => setProfileMode(e.target.value)}
                                style={{ marginBottom: '16px', width: '100%' }}
                                buttonStyle="solid"
                            >
                                <Radio.Button value="existing" style={{ width: '50%', textAlign: 'center' }}>기존 프로파일 선택</Radio.Button>
                                <Radio.Button value="new" style={{ width: '50%', textAlign: 'center' }}>새 프로파일 생성</Radio.Button>
                            </Radio.Group>

                            {profileMode === 'existing' ? (
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
                                        <i className="fas fa-search" style={{ marginRight: '8px', color: '#1890ff' }} />
                                        사용할 프로파일 선택
                                    </div>
                                    <Select
                                        showSearch
                                        style={{ width: '100%', marginBottom: '20px' }}
                                        size="large"
                                        placeholder="설정할 프로파일을 검색하거나 선택하세요"
                                        optionFilterProp="children"
                                        value={selectedProfileId}
                                        onChange={setSelectedProfileId}
                                        getPopupContainer={triggerNode => triggerNode.parentNode}
                                        filterOption={(input, option: any) => (option?.label ?? '').toLowerCase().includes(input.toLowerCase())}
                                        options={profiles.map(p => ({ value: p.id, label: `${p.name} (${p.data_points?.length || 0} points)` }))}
                                    />
                                    {selectedProfileId && (() => {
                                        const p = profiles.find(p => p.id === selectedProfileId);
                                        return p ? (
                                            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                                                <div style={{
                                                    marginBottom: '20px',
                                                    padding: '16px',
                                                    background: 'linear-gradient(135deg, #e6f7ff 0%, #f0f5ff 100%)',
                                                    border: '1px solid #91d5ff',
                                                    borderRadius: '10px',
                                                    flexShrink: 0,
                                                    boxShadow: '0 2px 4px rgba(24, 144, 255, 0.1)'
                                                }}>
                                                    <div style={{ fontWeight: 800, color: '#0050b3', display: 'flex', alignItems: 'center', fontSize: '16px' }}>
                                                        <i className="fas fa-file-contract" style={{ marginRight: '10px', fontSize: '18px' }} />
                                                        {p.name}
                                                    </div>
                                                    <div style={{ fontSize: '13px', marginTop: '6px', color: '#434343', lineHeight: '1.5' }}>
                                                        {p.description || '이 프로파일에 대한 상세 설명이 없습니다.'}
                                                    </div>
                                                </div>


                                                {/* [MOVED] Mapping UI REFINEMENT - High Fidelity Table & Bulk Ops in Step 2 */}
                                                <div style={{
                                                    marginTop: '10px',
                                                    border: '1px solid #f0f0f0',
                                                    borderRadius: '12px',
                                                    background: 'white',
                                                    padding: '24px',
                                                    display: 'flex',
                                                    flexDirection: 'column',
                                                    overflow: 'hidden',
                                                    boxShadow: '0 4px 12px rgba(0,0,0,0.05)'
                                                }}>
                                                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px', borderBottom: '1px solid #f5f5f5', paddingBottom: '16px' }}>
                                                        <div style={{ fontSize: '16px', fontWeight: 800, color: '#1a1a1a', display: 'flex', alignItems: 'center' }}>
                                                            <i className="fas fa-exchange-alt" style={{ marginRight: '10px', color: '#1890ff' }} />
                                                            데이터 매핑 및 외부 필드명 설정
                                                        </div>
                                                        <div style={{ display: 'flex', gap: '12px', alignItems: 'center', background: '#f0f5ff', padding: '8px 16px', borderRadius: '10px', border: '1px solid #adc6ff' }}>
                                                            <span style={{ fontSize: '12px', fontWeight: 700, color: '#1d39c4' }}>SITE ID 일괄 적용:</span>
                                                            <Input
                                                                size="small"
                                                                placeholder="ID 입력"
                                                                style={{ width: '80px', borderRadius: '4px' }}
                                                                value={bulkSiteId}
                                                                onChange={e => setBulkSiteId(e.target.value)}
                                                            />
                                                            <Button
                                                                size="small"
                                                                type="primary"
                                                                onClick={() => {
                                                                    const bid = parseInt(bulkSiteId);
                                                                    if (isNaN(bid)) return;
                                                                    setMappings(mappings.map(m => ({ ...m, site_id: bid })));
                                                                }}
                                                            >
                                                                적용
                                                            </Button>
                                                        </div>
                                                    </div>

                                                    <div style={{ overflowX: 'auto' }}>
                                                        <table style={{ width: '100%', borderCollapse: 'separate', borderSpacing: '0 4px', fontSize: '12px' }}>
                                                            <thead>
                                                                <tr style={{ textAlign: 'left', color: '#8c8c8c' }}>
                                                                    <th style={{ padding: '8px 12px', width: '20%' }}>내부 포인트명</th>
                                                                    <th style={{ padding: '8px 12px', width: '35%' }}>매핑 명칭 (TARGET KEY)</th>
                                                                    <th style={{ padding: '8px 12px', width: '15%' }}>SITE ID</th>
                                                                    <th style={{ padding: '8px 12px', width: '10%' }}>SCALE</th>
                                                                    <th style={{ padding: '8px 12px', width: '10%' }}>OFFSET</th>
                                                                    <th style={{ padding: '8px 12px', width: '10%', textAlign: 'center' }}>삭제</th>
                                                                </tr>
                                                            </thead>
                                                            <tbody>
                                                                {mappings.map((m, i) => {
                                                                    const point = allPoints.find(p => p.id === m.point_id) ||
                                                                        p.data_points?.find((dp: any) => dp.id === m.point_id);

                                                                    let conv: any = {};
                                                                    try { conv = typeof m.conversion_config === 'string' ? JSON.parse(m.conversion_config) : (m.conversion_config || {}); } catch { }

                                                                    return (
                                                                        <tr key={i} style={{ background: '#fafafa', borderRadius: '8px' }}>
                                                                            <td style={{ padding: '12px', borderRadius: '8px 0 0 8px', fontWeight: 600, color: '#1a1a1a' }}>
                                                                                {point?.name || `Point ${m.point_id} `}
                                                                            </td>
                                                                            <td style={{ padding: '8px' }}>
                                                                                <Input
                                                                                    size="small"
                                                                                    style={{ borderRadius: '4px' }}
                                                                                    value={m.target_field_name}
                                                                                    onChange={e => {
                                                                                        const next = [...mappings];
                                                                                        next[i] = { ...m, target_field_name: e.target.value };
                                                                                        setMappings(next);
                                                                                    }}
                                                                                />
                                                                            </td>
                                                                            <td style={{ padding: '8px' }}>
                                                                                <Input
                                                                                    size="small"
                                                                                    type="number"
                                                                                    style={{ borderRadius: '4px' }}
                                                                                    value={m.site_id}
                                                                                    onChange={e => {
                                                                                        const next = [...mappings];
                                                                                        next[i] = { ...m, site_id: parseInt(e.target.value) || 0 };
                                                                                        setMappings(next);
                                                                                    }}
                                                                                />
                                                                            </td>
                                                                            <td style={{ padding: '8px' }}>
                                                                                <Input
                                                                                    size="small"
                                                                                    type="number"
                                                                                    step="0.01"
                                                                                    style={{ borderRadius: '4px' }}
                                                                                    value={conv.scale ?? 1}
                                                                                    onChange={e => {
                                                                                        const next = [...mappings];
                                                                                        const nextConv = { ...conv, scale: parseFloat(e.target.value) || 1 };
                                                                                        next[i] = { ...m, conversion_config: nextConv };
                                                                                        setMappings(next);
                                                                                    }}
                                                                                />
                                                                            </td>
                                                                            <td style={{ padding: '8px' }}>
                                                                                <Input
                                                                                    size="small"
                                                                                    type="number"
                                                                                    step="0.01"
                                                                                    style={{ borderRadius: '4px' }}
                                                                                    value={conv.offset ?? 0}
                                                                                    onChange={e => {
                                                                                        const next = [...mappings];
                                                                                        const nextConv = { ...conv, offset: parseFloat(e.target.value) || 0 };
                                                                                        next[i] = { ...m, conversion_config: nextConv };
                                                                                        setMappings(next);
                                                                                    }}
                                                                                />
                                                                            </td>
                                                                            <td style={{ padding: '8px', textAlign: 'center', borderRadius: '0 8px 8px 0' }}>
                                                                                <Button
                                                                                    size="small"
                                                                                    danger
                                                                                    type="text"
                                                                                    icon={<i className="fas fa-trash-alt" />}
                                                                                    onClick={() => setMappings(mappings.filter((_, idx) => idx !== i))}
                                                                                />
                                                                            </td>
                                                                        </tr>
                                                                    );
                                                                })}
                                                            </tbody>
                                                        </table>
                                                    </div>
                                                </div>
                                            </div>
                                        ) : null;
                                    })()}
                                </div>
                            ) : (
                                <div style={{ display: 'flex', gap: '24px', height: '400px' }}>
                                    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                        <Input
                                            placeholder="프로파일 명칭 (예: Factory A Standard)"
                                            value={newProfileData.name}
                                            onChange={e => setNewProfileData({ ...newProfileData, name: e.target.value })}
                                        />
                                        <Input.TextArea
                                            placeholder="설명..."
                                            rows={2}
                                            value={newProfileData.description}
                                            onChange={e => setNewProfileData({ ...newProfileData, description: e.target.value })}
                                        />
                                        <div style={{ flex: 1, border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '12px', overflowY: 'auto', background: 'white' }}>
                                            <div style={{ fontSize: '12px', color: 'var(--neutral-500)', marginBottom: '8px' }}>선택된 포인트 ({newProfileData.data_points.length})</div>
                                            {newProfileData.data_points.map((p, i) => (
                                                <div key={i} style={{ display: 'flex', justifyContent: 'space-between', padding: '4px 0', borderBottom: '1px solid #f0f0f0' }}>
                                                    <span style={{ fontSize: '12px' }}>{p.name}</span>
                                                    <i className="fas fa-times" style={{ cursor: 'pointer', color: '#ff4d4f' }} onClick={() => setNewProfileData({ ...newProfileData, data_points: newProfileData.data_points.filter((_, idx) => idx !== i) })} />
                                                </div>
                                            ))}
                                        </div>
                                    </div>
                                    <div style={{ flex: 1.5, height: '100%', overflow: 'hidden' }}>
                                        <DataPointSelector
                                            selectedPoints={newProfileData.data_points}
                                            onSelect={(point) => setNewProfileData({ ...newProfileData, data_points: [...newProfileData.data_points, point] })}
                                            onRemove={(pointId) => setNewProfileData({ ...newProfileData, data_points: newProfileData.data_points.filter(p => p.id !== pointId) })}
                                            onAddAll={(points) => {
                                                const currentIds = new Set(newProfileData.data_points.map(p => p.id));
                                                const toAdd = points.filter(p => !currentIds.has(p.id));
                                                setNewProfileData({ ...newProfileData, data_points: [...newProfileData.data_points, ...toAdd] });
                                            }}
                                        />
                                    </div>
                                </div>
                            )}
                        </div>
                    )}

                    {/* STEP 2: Template */}
                    {currentStep === 2 && (
                        <div>
                            <Radio.Group value={templateMode} onChange={e => setTemplateMode(e.target.value)} style={{ marginBottom: '16px', width: '100%' }} buttonStyle="solid">
                                <Radio.Button value="existing" style={{ width: '50%', textAlign: 'center' }}>기존 템플릿 사용</Radio.Button>
                                <Radio.Button value="new" style={{ width: '50%', textAlign: 'center' }}>새 템플릿 정의</Radio.Button>
                            </Radio.Group>
                            {templateMode === 'existing' ? (
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
                                        사용할 템플릿 선택
                                    </div>
                                    <Select
                                        style={{ width: '100%', marginBottom: '20px' }}
                                        size="large"
                                        placeholder="템플릿을 선택하여 전송 형식을 결정하세요"
                                        value={selectedTemplateId}
                                        onChange={setSelectedTemplateId}
                                        options={templates.map(t => ({ value: t.id, label: t.name }))}
                                        getPopupContainer={trigger => trigger.parentElement}
                                    />
                                    {selectedTemplateId && (
                                        <div style={{ marginTop: '10px', flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                                            <div style={{ marginBottom: '8px', fontSize: '12px', fontWeight: 700, color: '#8c8c8c', display: 'flex', alignItems: 'center' }}>
                                                <i className="fas fa-code" style={{ marginRight: '6px' }} />
                                                템플릿 JSON 미리보기
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
                                                        if (raw === undefined || raw === null) return '선택된 템플릿 정보가 없습니다.';
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
                                <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: '15px' }}>
                                    <div style={{ display: 'flex', gap: '15px' }}>
                                        <div style={{ flex: 1 }}>
                                            <div style={{ fontSize: '12px', fontWeight: 600, marginBottom: '4px' }}>템플릿 명칭</div>
                                            <Input
                                                size="large"
                                                placeholder="예: 표준 JSON 페이로드"
                                                value={newTemplateData.name}
                                                onChange={e => setNewTemplateData({ ...newTemplateData, name: e.target.value })}
                                            />
                                        </div>
                                        <div style={{ flex: 1 }}>
                                            <div style={{ fontSize: '12px', fontWeight: 600, marginBottom: '4px' }}>시스템 유형</div>
                                            <Input
                                                size="large"
                                                placeholder="예: AWS IoT, MS Azure 등"
                                                value={newTemplateData.system_type}
                                                onChange={e => setNewTemplateData({ ...newTemplateData, system_type: e.target.value })}
                                            />
                                        </div>
                                    </div>

                                    <div style={{ padding: '12px', background: '#f0faff', borderRadius: '8px', border: '1px solid #e6f7ff', fontSize: '11px', color: '#0050b3', lineHeight: '1.6' }}>
                                        <i className="fas fa-info-circle" style={{ marginRight: '8px' }} />
                                        <strong>엔지니어 가이드:</strong> <code>{"{{target_key}}"}</code>를 사용하여 프로파일의 <b>Target Key</b>를 주입하세요. 범용 설계를 위해 사이트별 고정 변수가 아닌 메타데이터 중심의 스키마를 권장합니다.
                                    </div>

                                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '15px' }}>
                                            <span style={{ fontWeight: 700, fontSize: '13px' }}>페이로드 구성</span>
                                            <Radio.Group
                                                size="small"
                                                value={templateEditMode}
                                                onChange={e => setTemplateEditMode(e.target.value)}
                                            >
                                                <Radio.Button value="simple">빌더 (Simple)</Radio.Button>
                                                <Radio.Button value="advanced">코드 (Advanced)</Radio.Button>
                                            </Radio.Group>
                                        </div>
                                    </div>

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
                                                                onClick={() => {
                                                                    const insertVar = v.value;
                                                                    if (templateEditMode === 'advanced') {
                                                                        const textarea = document.getElementById('new-template-textarea') as HTMLTextAreaElement;
                                                                        if (textarea) {
                                                                            const start = textarea.selectionStart;
                                                                            const end = textarea.selectionEnd;
                                                                            textarea.setRangeText(insertVar, start, end, 'end');
                                                                            textarea.focus();
                                                                            setNewTemplateData({ ...newTemplateData, template_json: textarea.value });
                                                                        }
                                                                    }
                                                                    // Simple mode insertion logic could be added here if templateSimpleMappings is used
                                                                }}
                                                                title={v.desc}
                                                            >
                                                                {v.label}
                                                            </button>
                                                        ))}
                                                    </div>
                                                </div>
                                            ))}
                                        </div>
                                    </div>

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
                                                    마법사에서는 'Advanced' 모드만 지원합니다.<br />상세 빌더는 [템플릿 관리] 탭을 이용해주세요.
                                                </div>
                                            )}
                                        </div>
                                        <div style={{ flex: 1, background: '#1e272e', borderRadius: '8px', padding: '12px', overflowY: 'auto' }}>
                                            <div style={{ fontSize: '11px', color: '#55efc4', fontWeight: 700, marginBottom: '8px' }}>PREVIEW</div>
                                            <pre style={{ margin: 0, fontSize: '11px', color: '#abb2bf', whiteSpace: 'pre-wrap' }}>
                                                {(() => {
                                                    try {
                                                        let preview = typeof newTemplateData.template_json === 'string' ? newTemplateData.template_json : JSON.stringify(newTemplateData.template_json, null, 2);
                                                        Object.entries(SAMPLE_DATA).forEach(([k, v]) => {
                                                            preview = preview.replace(new RegExp(`\\{\\{${k}\\}\\}`, 'g'), String(v));
                                                            preview = preview.replace(new RegExp(`\\{${k}\\}`, 'g'), String(v));
                                                        });
                                                        try {
                                                            return JSON.stringify(JSON.parse(preview), null, 2);
                                                        } catch {
                                                            return preview;
                                                        }
                                                    } catch {
                                                        return "JSON Error";
                                                    }
                                                })()}
                                            </pre>
                                        </div>
                                    </div>
                                </div>
                            )}
                        </div>
                    )}

                    {/* STEP 3: Target & Mapping */}
                    {currentStep === 3 && (
                        <div style={{ height: '480px', display: 'flex', flexDirection: 'column' }}>
                            <Radio.Group value={targetMode} onChange={e => setTargetMode(e.target.value)} style={{ marginBottom: '16px' }}>
                                <Radio.Button value="existing">기존 타겟 연결</Radio.Button>
                                <Radio.Button value="new">새 타겟 생성</Radio.Button>
                            </Radio.Group>

                            {targetMode === 'existing' ? (
                                <div style={{ padding: '24px', textAlign: 'left', background: 'white', borderRadius: '8px', height: '100%', display: 'flex', flexDirection: 'column' }}>
                                    <div style={{ marginBottom: '8px', fontWeight: 600 }}>사용할 타겟 선택 및 수정</div>
                                    <Select
                                        mode="multiple"
                                        style={{ width: '100%' }}
                                        placeholder="타겟 선택 (다중 선택 가능)"
                                        value={selectedTargetIds}
                                        onChange={setSelectedTargetIds}
                                        options={targets.map(t => ({
                                            value: t.id,
                                            label: `${t.name} [${t.target_type}]`
                                        }))}
                                        getPopupContainer={trigger => trigger.parentElement}
                                    />
                                    {selectedTargetIds.length > 0 && (
                                        <div style={{ marginTop: '16px', flex: 1, overflowY: 'auto', border: '1px solid #f0f0f0', borderRadius: '8px', background: '#fafafa', padding: '12px' }}>
                                            <div style={{ fontSize: '12px', color: '#888', marginBottom: '8px' }}>선택된 타겟 설정 수정 ({selectedTargetIds.length})</div>
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
                                                                    <span style={{ fontSize: '12px', color: '#666' }}>전송 우선순위:</span>
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
                                                                                    <option value="POST">POST</option>
                                                                                    <option value="PUT">PUT</option>
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
                                                                                    next[tid][cIdx] = {
                                                                                        ...c,
                                                                                        headers: { ...c.headers, Authorization: e.target.value }
                                                                                    };
                                                                                    setEditingTargets(next);
                                                                                }}
                                                                            />
                                                                            <Input.Password
                                                                                placeholder="인증 키 (x-api-key 또는 Token)"
                                                                                value={c.auth?.apiKey || ''}
                                                                                onChange={e => {
                                                                                    const next = { ...editingTargets };
                                                                                    next[tid][cIdx] = {
                                                                                        ...c,
                                                                                        auth: { ...c.auth, type: 'x-api-key', apiKey: e.target.value }
                                                                                    };
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
                                                                                <Input
                                                                                    placeholder="Bucket Name"
                                                                                    value={c.BucketName}
                                                                                    onChange={e => {
                                                                                        const next = { ...editingTargets };
                                                                                        next[tid][cIdx] = { ...c, BucketName: e.target.value };
                                                                                        setEditingTargets(next);
                                                                                    }}
                                                                                />
                                                                                <Input
                                                                                    placeholder="Folder Path"
                                                                                    value={c.Folder}
                                                                                    onChange={e => {
                                                                                        const next = { ...editingTargets };
                                                                                        next[tid][cIdx] = { ...c, Folder: e.target.value };
                                                                                        setEditingTargets(next);
                                                                                    }}
                                                                                />
                                                                            </div>
                                                                            <div style={{ display: 'flex', gap: '8px' }}>
                                                                                <Input.Password
                                                                                    placeholder="Access Key ID"
                                                                                    value={c.AccessKeyID}
                                                                                    onChange={e => {
                                                                                        const next = { ...editingTargets };
                                                                                        next[tid][cIdx] = { ...c, AccessKeyID: e.target.value };
                                                                                        setEditingTargets(next);
                                                                                    }}
                                                                                />
                                                                                <Input.Password
                                                                                    placeholder="Secret Access Key"
                                                                                    value={c.SecretAccessKey}
                                                                                    onChange={e => {
                                                                                        const next = { ...editingTargets };
                                                                                        next[tid][cIdx] = { ...c, SecretAccessKey: e.target.value };
                                                                                        setEditingTargets(next);
                                                                                    }}
                                                                                />
                                                                            </div>
                                                                        </div>
                                                                    )}
                                                                    {t.target_type === 'MQTT' && (
                                                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                                                            <Input
                                                                                placeholder="Broker URL"
                                                                                value={c.url}
                                                                                onChange={e => {
                                                                                    const next = { ...editingTargets };
                                                                                    next[tid][cIdx] = { ...c, url: e.target.value };
                                                                                    setEditingTargets(next);
                                                                                }}
                                                                            />
                                                                            <Input
                                                                                placeholder="Topic"
                                                                                value={c.topic}
                                                                                onChange={e => {
                                                                                    const next = { ...editingTargets };
                                                                                    next[tid][cIdx] = { ...c, topic: e.target.value };
                                                                                    setEditingTargets(next);
                                                                                }}
                                                                            />
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
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', height: '100%', overflowY: 'auto' }}>
                                    <Input
                                        placeholder="타겟 명칭 (자동으로 프로토콜 접미사가 붙습니다. 예: TargetA -> TargetA_HTTP)"
                                        value={targetData.name}
                                        onChange={e => setTargetData({ ...targetData, name: e.target.value })}
                                    />
                                    <div className="mgmt-modal-form-group">
                                        <label>전송 프로토콜 (다중 선택 가능)</label>
                                        <Checkbox.Group
                                            options={['HTTP', 'MQTT', 'S3']}
                                            value={selectedProtocols}
                                            onChange={list => setSelectedProtocols(list as string[])}
                                        />
                                    </div>

                                    {/* Config Forms */}
                                    {selectedProtocols.includes('HTTP') && (
                                        <div style={{ padding: '16px', border: '1px solid #ddd', borderRadius: '8px', background: 'white' }}>
                                            <div style={{ fontWeight: 600, marginBottom: '12px', borderBottom: '1px solid #eee', paddingBottom: '8px' }}>HTTP 연결 설정</div>
                                            {targetData.config_http.map((c: any, i: number) => (
                                                <div key={i} style={{ marginBottom: '16px', padding: '12px', background: '#f9f9f9', borderRadius: '8px' }}>
                                                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', alignItems: 'center' }}>
                                                        <span style={{ fontWeight: 600, color: '#666', fontSize: '12px' }}>Mirror #{i + 1}</span>
                                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                            <span style={{ fontSize: '11px', color: '#999' }}>전송 우선순위:</span>
                                                            <Select
                                                                size="small"
                                                                style={{ width: '60px' }}
                                                                value={c.execution_order || i + 1}
                                                                getPopupContainer={trigger => trigger.parentElement}
                                                                onChange={(val) => updateTargetPriority('new', 'config_http', i, val)}
                                                            >
                                                                {Array.from({ length: totalTargetCount }).map((_, idx) => (
                                                                    <Select.Option key={idx + 1} value={idx + 1}>{idx + 1}</Select.Option>
                                                                ))}
                                                            </Select>
                                                        </div>
                                                    </div>
                                                    <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
                                                        <Select
                                                            style={{ width: '100px' }}
                                                            value={c.method}
                                                            getPopupContainer={trigger => trigger.parentElement}
                                                            onChange={val => {
                                                                const next = [...targetData.config_http];
                                                                next[i].method = val;
                                                                setTargetData({ ...targetData, config_http: next });
                                                            }}
                                                        >
                                                            <Select.Option value="POST">POST</Select.Option>
                                                            <Select.Option value="PUT">PUT</Select.Option>
                                                        </Select>
                                                        <Input
                                                            placeholder="Endpoint URL (예: http://api.server.com/ingest)"
                                                            value={c.url}
                                                            onChange={e => {
                                                                const next = [...targetData.config_http];
                                                                next[i].url = e.target.value;
                                                                setTargetData({ ...targetData, config_http: next });
                                                            }}
                                                        />
                                                    </div>
                                                    <div style={{ marginBottom: '8px' }}>
                                                        <Input.Password
                                                            placeholder="Authorization Header (Token / Signature)"
                                                            value={c.headers?.['Authorization'] || ''}
                                                            onChange={e => {
                                                                const next = [...targetData.config_http];
                                                                if (!next[i].headers) next[i].headers = {};
                                                                if (e.target.value) {
                                                                    next[i].headers['Authorization'] = e.target.value;
                                                                } else {
                                                                    delete next[i].headers['Authorization'];
                                                                }
                                                                setTargetData({ ...targetData, config_http: next });
                                                            }}
                                                        />
                                                    </div>
                                                    <div style={{ marginBottom: '8px' }}>
                                                        <Input.Password
                                                            placeholder="인증 키 (x-api-key 또는 Token)"
                                                            value={c.auth?.apiKey || ''}
                                                            onChange={e => {
                                                                const next = [...targetData.config_http];
                                                                // Sync both auth object and headers for compatibility
                                                                if (!next[i].auth) next[i].auth = { type: 'x-api-key' };
                                                                next[i].auth.apiKey = e.target.value;

                                                                if (!next[i].headers) next[i].headers = {};
                                                                if (e.target.value) {
                                                                    next[i].headers['x-api-key'] = e.target.value;
                                                                } else {
                                                                    delete next[i].headers['x-api-key'];
                                                                }
                                                                setTargetData({ ...targetData, config_http: next });
                                                            }}
                                                        />
                                                    </div>
                                                    {targetData.config_http.length > 1 && (
                                                        <Button danger size="small" onClick={() => {
                                                            const next = targetData.config_http.filter((_, idx) => idx !== i);
                                                            setTargetData({ ...targetData, config_http: next });
                                                        }}>삭제</Button>
                                                    )}
                                                </div>
                                            ))}
                                            <Button type="dashed" block onClick={() => setTargetData({ ...targetData, config_http: [...targetData.config_http, { url: '', method: 'POST', auth: { type: 'x-api-key', apiKey: '' }, headers: {}, execution_order: totalTargetCount + 1 }] })}>
                                                + HTTP 타겟/Mirror 추가
                                            </Button>
                                        </div>
                                    )}

                                    {selectedProtocols.includes('MQTT') && (
                                        <div style={{ padding: '16px', border: '1px solid #ddd', borderRadius: '8px', background: 'white', marginTop: '16px' }}>
                                            <div style={{ fontWeight: 600, marginBottom: '12px', borderBottom: '1px solid #eee', paddingBottom: '8px' }}>MQTT 연결 설정</div>
                                            {targetData.config_mqtt.map((c: any, i: number) => (
                                                <div key={i} style={{ marginBottom: '16px', padding: '12px', background: '#f9f9f9', borderRadius: '8px' }}>
                                                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', alignItems: 'center' }}>
                                                        <span style={{ fontWeight: 600, color: '#666', fontSize: '12px' }}>MQTT 타겟 #{i + 1}</span>
                                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                            <span style={{ fontSize: '11px', color: '#999' }}>전송 우선순위:</span>
                                                            <Select
                                                                size="small"
                                                                style={{ width: '60px' }}
                                                                value={c.execution_order || i + 1}
                                                                getPopupContainer={trigger => trigger.parentElement}
                                                                onChange={(val) => updateTargetPriority('new', 'config_mqtt', i, val)}
                                                            >
                                                                {Array.from({ length: totalTargetCount }).map((_, idx) => (
                                                                    <Select.Option key={idx + 1} value={idx + 1}>{idx + 1}</Select.Option>
                                                                ))}
                                                            </Select>
                                                        </div>
                                                    </div>
                                                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                                        <Input
                                                            placeholder="Broker URL (예: mqtt://broker.hivemq.com:1883)"
                                                            value={c.url}
                                                            onChange={e => {
                                                                const next = [...targetData.config_mqtt];
                                                                next[i].url = e.target.value;
                                                                setTargetData({ ...targetData, config_mqtt: next });
                                                            }}
                                                        />
                                                        <Input
                                                            placeholder="Topic (예: pulseone/factory/data)"
                                                            value={c.topic}
                                                            onChange={e => {
                                                                const next = [...targetData.config_mqtt];
                                                                next[i].topic = e.target.value;
                                                                setTargetData({ ...targetData, config_mqtt: next });
                                                            }}
                                                        />
                                                    </div>
                                                    {targetData.config_mqtt.length > 1 && (
                                                        <Button danger size="small" style={{ marginTop: '8px' }} onClick={() => {
                                                            const next = targetData.config_mqtt.filter((_, idx) => idx !== i);
                                                            setTargetData({ ...targetData, config_mqtt: next });
                                                        }}>삭제</Button>
                                                    )}
                                                </div>
                                            ))}
                                            <Button type="dashed" block onClick={() => setTargetData({ ...targetData, config_mqtt: [...targetData.config_mqtt, { url: '', topic: 'pulseone/data', execution_order: totalTargetCount + 1 }] })}>
                                                + MQTT 타겟 추가
                                            </Button>
                                        </div>
                                    )}

                                    {selectedProtocols.includes('S3') && (
                                        <div style={{ padding: '16px', border: '1px solid #ddd', borderRadius: '8px', background: 'white', marginTop: '16px' }}>
                                            <div style={{ fontWeight: 600, marginBottom: '12px', borderBottom: '1px solid #eee', paddingBottom: '8px' }}>S3 / MinIO 연결 설정</div>
                                            {targetData.config_s3.map((c: any, i: number) => (
                                                <div key={i} style={{ marginBottom: '16px', padding: '12px', background: '#f9f9f9', borderRadius: '8px' }}>
                                                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', alignItems: 'center' }}>
                                                        <span style={{ fontWeight: 600, color: '#666', fontSize: '12px' }}>S3 타겟 #{i + 1}</span>
                                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                            <span style={{ fontSize: '11px', color: '#999' }}>전송 우선순위:</span>
                                                            <Select
                                                                size="small"
                                                                style={{ width: '60px' }}
                                                                value={c.execution_order || i + 1}
                                                                getPopupContainer={trigger => trigger.parentElement}
                                                                onChange={(val) => updateTargetPriority('new', 'config_s3', i, val)}
                                                            >
                                                                {Array.from({ length: totalTargetCount }).map((_, idx) => (
                                                                    <Select.Option key={idx + 1} value={idx + 1}>{idx + 1}</Select.Option>
                                                                ))}
                                                            </Select>
                                                        </div>
                                                    </div>
                                                    <Input
                                                        style={{ marginBottom: '8px' }}
                                                        placeholder="S3 Endpoint / Service URL (예: https://s3.ap-northeast-2.amazonaws.com)"
                                                        value={c.endpoint || c.S3ServiceUrl || ''}
                                                        onChange={e => {
                                                            const next = [...targetData.config_s3];
                                                            next[i].endpoint = e.target.value;
                                                            setTargetData({ ...targetData, config_s3: next });
                                                        }}
                                                    />
                                                    <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
                                                        <Input
                                                            placeholder="Bucket Name"
                                                            value={c.BucketName}
                                                            onChange={e => {
                                                                const next = [...targetData.config_s3];
                                                                next[i].BucketName = e.target.value;
                                                                setTargetData({ ...targetData, config_s3: next });
                                                            }}
                                                        />
                                                        <Input
                                                            placeholder="Folder Path"
                                                            value={c.Folder}
                                                            onChange={e => {
                                                                const next = [...targetData.config_s3];
                                                                next[i].Folder = e.target.value;
                                                                setTargetData({ ...targetData, config_s3: next });
                                                            }}
                                                        />
                                                    </div>
                                                    <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
                                                        <Input.Password
                                                            placeholder="Access Key ID"
                                                            value={c.AccessKeyID}
                                                            onChange={e => {
                                                                const next = [...targetData.config_s3];
                                                                next[i].AccessKeyID = e.target.value;
                                                                setTargetData({ ...targetData, config_s3: next });
                                                            }}
                                                        />
                                                        <Input.Password
                                                            placeholder="Secret Access Key"
                                                            value={c.SecretAccessKey}
                                                            onChange={e => {
                                                                const next = [...targetData.config_s3];
                                                                next[i].SecretAccessKey = e.target.value;
                                                                setTargetData({ ...targetData, config_s3: next });
                                                            }}
                                                        />
                                                    </div>
                                                    {targetData.config_s3.length > 1 && (
                                                        <Button danger size="small" onClick={() => {
                                                            const next = targetData.config_s3.filter((_, idx) => idx !== i);
                                                            setTargetData({ ...targetData, config_s3: next });
                                                        }}>삭제</Button>
                                                    )}
                                                </div>
                                            ))}
                                            <Button type="dashed" block onClick={() => setTargetData({ ...targetData, config_s3: [...targetData.config_s3, { endpoint: '', BucketName: '', Folder: '', AccessKeyID: '', SecretAccessKey: '', execution_order: totalTargetCount + 1 }] })}>
                                                + S3 스토리지 추가
                                            </Button>
                                        </div>
                                    )}
                                </div>
                            )}

                            {/* Removed Mapping UI from Step 4 - now in Step 2 */}
                        </div>
                    )}

                    {/* STEP 4: Schedule */}
                    {currentStep === 4 && (
                        <div style={{ minHeight: '380px', display: 'flex', flexDirection: 'column' }}>
                            <div style={{ marginBottom: '20px', padding: '16px', background: '#f8faff', borderRadius: '12px', border: '1px solid #e6f7ff' }}>
                                <div style={{ fontWeight: 700, color: '#0050b3', marginBottom: '4px', fontSize: '14px' }}>
                                    <i className="fas fa-info-circle" style={{ marginRight: '8px' }} />
                                    전송 모드 가이드
                                </div>
                                <div style={{ fontSize: '12.5px', color: '#434343', lineHeight: '1.6' }}>
                                    {transmissionMode === 'INTERVAL' ? (
                                        <><strong>정기 전송:</strong> 지정된 Cron 스케줄에 맞춰 데이터를 주기적으로 일괄 전송합니다. 트래픽의 예측이 가능하며 리포트 생성에 적합합니다.</>
                                    ) : (
                                        <><strong>이벤트 기반:</strong> 수집된 데이터에 변화가 생기거나 특정 임계치를 넘는 이벤트가 발생할 때 즉시 전송합니다. 실시간 알람 및 즉각적인 대응이 필요한 경우에 사용합니다.</>
                                    )}
                                </div>
                            </div>

                            <Form layout="vertical">
                                <Form.Item label={<span style={{ fontWeight: 600 }}>전송 모드 선택</span>}>
                                    <Radio.Group
                                        value={transmissionMode}
                                        onChange={e => setTransmissionMode(e.target.value)}
                                        optionType="button"
                                        buttonStyle="solid"
                                        style={{ width: '100%' }}
                                    >
                                        <Radio.Button value="INTERVAL" style={{ width: '50%', textAlign: 'center' }}>정기 전송 (Scheduled)</Radio.Button>
                                        <Radio.Button value="EVENT" style={{ width: '50%', textAlign: 'center' }}>이벤트 기반 (Real-time)</Radio.Button>
                                    </Radio.Group>
                                </Form.Item>

                                {transmissionMode === 'INTERVAL' ? (
                                    <div style={{ animation: 'fadeIn 0.3s ease-in-out' }}>
                                        <Form.Item label={<span style={{ fontWeight: 600 }}>스케줄 명칭</span>} required>
                                            <Input
                                                placeholder="예: 5분 주기 정기 전송"
                                                value={scheduleData.schedule_name}
                                                onChange={e => setScheduleData({ ...scheduleData, schedule_name: e.target.value })}
                                            />
                                        </Form.Item>
                                        <Form.Item
                                            label={<span style={{ fontWeight: 600 }}>Cron 표현식</span>}
                                            required
                                            extra={<span style={{ fontSize: '11px' }}>* 표준 Cron 형식을 지원합니다 (분 시 일 월 요일)</span>}
                                        >
                                            <Input
                                                placeholder="*/5 * * * *"
                                                value={scheduleData.cron_expression}
                                                onChange={e => setScheduleData({ ...scheduleData, cron_expression: e.target.value })}
                                                style={{ fontFamily: 'monospace', fontSize: '15px' }}
                                            />
                                            <div style={{ marginTop: '12px' }}>
                                                <div style={{ fontSize: '11px', color: '#8c8c8c', marginBottom: '8px', fontWeight: 600 }}>간편 프리셋 선택</div>
                                                <Space wrap>
                                                    {[
                                                        { label: '1분 주기', value: '*/1 * * * *' },
                                                        { label: '5분 주기', value: '*/5 * * * *' },
                                                        { label: '10분 주기', value: '*/10 * * * *' },
                                                        { label: '30분 주기', value: '*/30 * * * *' },
                                                        { label: '1시간 주기', value: '0 * * * *' },
                                                        { label: '12시간 주기', value: '0 */12 * * *' },
                                                        { label: '매일 자정', value: '0 0 * * *' },
                                                        { label: '매주 일요일', value: '0 0 * * 0' }
                                                    ].map(preset => (
                                                        <Button
                                                            key={preset.value}
                                                            size="small"
                                                            type={scheduleData.cron_expression === preset.value ? 'primary' : 'default'}
                                                            onClick={() => setScheduleData({
                                                                ...scheduleData,
                                                                cron_expression: preset.value,
                                                                schedule_name: preset.label
                                                            })}
                                                            style={{ fontSize: '11px', borderRadius: '4px' }}
                                                        >
                                                            {preset.label}
                                                        </Button>
                                                    ))}
                                                </Space>
                                            </div>
                                        </Form.Item>
                                    </div>
                                ) : (
                                    <div style={{
                                        padding: '40px 20px',
                                        textAlign: 'center',
                                        background: '#fafafa',
                                        borderRadius: '12px',
                                        border: '1px dashed #d9d9d9',
                                        animation: 'fadeIn 0.3s ease-in-out'
                                    }}>
                                        <i className="fas fa-bolt" style={{ fontSize: '32px', color: '#faad14', marginBottom: '16px' }} />
                                        <div style={{ fontWeight: 600, color: '#262626', fontSize: '15px' }}>이벤트 기반 전송 활성</div>
                                        <div style={{ color: '#8c8c8c', fontSize: '13px', marginTop: '8px' }}>
                                            별도의 스케줄링 없이 데이터 수집 및 가공 직후 즉시 전송을 시도합니다.<br />
                                            (추가 설정 없이 바로 저장이 가능합니다)
                                        </div>
                                    </div>
                                )}
                            </Form>
                        </div>
                    )}
                </div>

                <div className="wizard-footer" style={{ padding: '16px 32px', borderTop: '1px solid var(--neutral-100)', display: 'flex', justifyContent: 'space-between', marginTop: 'auto' }}>
                    <div style={{ display: 'flex', gap: '8px' }}>
                        <Button onClick={onClose} disabled={loading}>취소</Button>
                        {editingGateway && onDelete && (
                            <Button
                                danger
                                onClick={async () => {
                                    const confirmed = await confirm({
                                        title: '게이트웨이 삭제',
                                        message: `"${editingGateway.name}" 게이트웨이를 정말 삭제하시겠습니까?\n이 작업은 되돌릴 수 없습니다.`,
                                        confirmText: '삭제',
                                        confirmButtonType: 'danger'
                                    });
                                    if (confirmed && onDelete) {
                                        await onDelete(editingGateway);
                                        onClose();
                                    }
                                }}
                            >
                                삭제
                            </Button>
                        )}
                    </div>
                    <Space>
                        {currentStep > 0 && <Button onClick={handlePrev} disabled={loading}>이전</Button>}
                        {currentStep < steps.length - 1 ? (
                            <Button type="primary" onClick={handleNext}>다음 단계</Button>
                        ) : (
                            <Button type="primary" onClick={handleFinish} loading={loading}>구성 완료 및 저장</Button>
                        )}
                    </Space>
                </div>
            </div>
        </Modal>
    );
};

export default ExportGatewayWizard;
