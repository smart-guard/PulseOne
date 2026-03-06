// ============================================================================
// wizards/useWizardState.ts
// ExportGatewayWizard — 모든 상태 및 비즈니스 로직 커스텀 훅
// ============================================================================

import { useState, useEffect, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';
import { apiClient } from '../../../api/client';
import exportGatewayApi, {
    ExportTargetMapping,
    ExportSchedule
} from '../../../api/services/exportGatewayApi';
import {
    ExportGatewayWizardProps,
    GatewayFormData,
    NewProfileData,
    NewTemplateData,
    TargetFormData,
    ScheduleFormData,
    extractItems
} from './types';

const DEFAULT_GATEWAY_DATA: GatewayFormData = {
    name: '',
    ip_address: '127.0.0.1',
    description: '',
    subscription_mode: 'all',
    config: {}
};

const DEFAULT_TARGET_DATA: TargetFormData = {
    name: '',
    config_http: [{ url: '', method: 'POST', auth_type: 'NONE', headers: { Authorization: '' }, auth: { type: 'x-api-key', apiKey: '' }, execution_order: 0 }],
    config_mqtt: [{ url: '', topic: 'pulseone/data', execution_order: 0 }],
    config_s3: [{ endpoint: '', BucketName: '', Folder: '', ObjectKeyTemplate: '', AccessKeyID: '', SecretAccessKey: '', execution_order: 0 }]
};

const DEFAULT_SCHEDULE_DATA: ScheduleFormData = {
    schedule_name: '',
    cron_expression: '*/1 * * * *',
    data_range: 'minute',
    lookback_periods: 1
};

export function useWizardState(props: ExportGatewayWizardProps) {
    const { visible, onClose, onSuccess, profiles, targets, templates, allPoints, editingGateway, assignments = [], schedules = [], onDelete, siteId, tenantId } = props;
    const { confirm } = useConfirmContext();
    const { t: tl } = useTranslation(['dataExport', 'common']);

    // ─── Navigation ────────────────────────────────────────────────────────────
    const [currentStep, setCurrentStep] = useState(0);
    const [loading, setLoading] = useState(false);

    // ─── Admin / Tenant / Site ─────────────────────────────────────────────────
    const [isAdmin, setIsAdmin] = useState(true);
    const [tenants, setTenants] = useState<any[]>([]);
    const [sites, setSites] = useState<any[]>([]);
    const [wizardTenantId, setWizardTenantId] = useState<number | null>(tenantId || null);
    const [wizardSiteId, setWizardSiteId] = useState<number | null>(siteId || null);

    // ─── Step 0: Gateway ───────────────────────────────────────────────────────
    const [gatewayData, setGatewayData] = useState<GatewayFormData>(DEFAULT_GATEWAY_DATA);

    // ─── Step 1: Profile ───────────────────────────────────────────────────────
    const [profileMode, setProfileMode] = useState<'existing' | 'new'>('existing');
    const [selectedProfileId, setSelectedProfileId] = useState<number | null>(null);
    const [newProfileData, setNewProfileData] = useState<NewProfileData>({ name: '', description: '', data_points: [] });
    const [mappings, setMappings] = useState<Partial<ExportTargetMapping>[]>([]);
    const [bulkSiteId, setBulkSiteId] = useState<string>('');

    // ─── Step 2: Template ──────────────────────────────────────────────────────
    const [templateMode, setTemplateMode] = useState<'existing' | 'new'>('existing');
    const [selectedTemplateId, setSelectedTemplateId] = useState<number | null>(templates[0]?.id || null);
    const [newTemplateData, setNewTemplateData] = useState<NewTemplateData>({
        name: '',
        system_type: 'custom',
        template_json: '{\n  "site_id": "{{site_id}}",\n  "type": "{{type}}",\n  "target_key": "{{target_key}}",\n  "measured_value": "{{measured_value}}",\n  "timestamp": "{{timestamp}}"\n}'
    });
    const [templateEditMode, setTemplateEditMode] = useState<'simple' | 'advanced'>('advanced');
    const [templateSimpleMappings, setTemplateSimpleMappings] = useState<{ key: string; value: string }[]>([]);
    const [templateIsWrappedInArray, setTemplateIsWrappedInArray] = useState(false);
    const [lastFocusedTemplateElement, setLastFocusedTemplateElement] = useState<{ id: string; index?: number; field?: 'key' | 'value' } | null>(null);

    // ─── Step 3: Target ────────────────────────────────────────────────────────
    const [transmissionMode, setTransmissionMode] = useState<'INTERVAL' | 'EVENT'>('INTERVAL');
    const [selectedProtocols, setSelectedProtocols] = useState<string[]>(['HTTP']);
    const [targetMode, setTargetMode] = useState<'existing' | 'new'>('new');
    const [selectedTargetIds, setSelectedTargetIds] = useState<number[]>([]);
    const [targetData, setTargetData] = useState<TargetFormData>(DEFAULT_TARGET_DATA);
    const [editingTargets, setEditingTargets] = useState<Record<number, any>>({});

    // ─── Step 4: Schedule ──────────────────────────────────────────────────────
    const [scheduleData, setScheduleData] = useState<ScheduleFormData>(DEFAULT_SCHEDULE_DATA);
    const [scheduleMode, setScheduleMode] = useState<'new' | 'existing'>('new');
    const [selectedScheduleId, setSelectedScheduleId] = useState<number | null>(null);
    const [availableSchedules, setAvailableSchedules] = useState<ExportSchedule[]>([]);

    // ─── Refs ──────────────────────────────────────────────────────────────────
    const prevVisibleRef = useRef(visible);
    const prevEditingIdRef = useRef<number | undefined>(editingGateway?.id);
    const hydratedTargetIdRef = useRef<number | null>(null);
    const lastMappedProfileIdRef = useRef<number | null>(null);

    // ─── API 헬퍼 ─────────────────────────────────────────────────────────────
    const fetchTenants = async () => {
        try {
            const res = await apiClient.get<any>('/api/tenants');
            if (res.success && res.data) setTenants(res.data.items || []);
        } catch (e) { console.error(e); }
    };

    const fetchSites = async (tId: number) => {
        try {
            const res = await apiClient.get<any>('/api/sites', { tenantId: tId, limit: 100 });
            if (res.success && res.data) setSites(res.data.items || []);
        } catch (e) { console.error(e); }
    };

    // ─── 초기화 Effect ─────────────────────────────────────────────────────────
    useEffect(() => { setIsAdmin(true); fetchTenants(); }, []);
    useEffect(() => { setWizardTenantId(tenantId || null); }, [tenantId]);
    useEffect(() => { setWizardSiteId(siteId || null); }, [siteId]);
    useEffect(() => { if (visible && wizardTenantId) fetchSites(wizardTenantId); }, [visible, wizardTenantId]);

    // templates prop이 비동기로 늦게 로드될 경우, existing 모드인데 선택값이 null이면 첫 번째 템플릿 자동 선택
    useEffect(() => {
        if (templateMode === 'existing' && selectedTemplateId === null && templates.length > 0) {
            setSelectedTemplateId(templates[0].id);
        }
    }, [templates]);

    // ─── editingTargets 자동 동기화 ────────────────────────────────────────────
    useEffect(() => {
        if (selectedTargetIds.length === 0) return;
        setEditingTargets(prev => {
            let changed = false;
            const next = { ...prev };
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
            selectedTargetIds.forEach((id, idx) => {
                const priority = idx + 1;
                if (next[id] && next[id][0] && next[id][0].execution_order !== priority) {
                    next[id][0] = { ...next[id][0], execution_order: priority };
                    changed = true;
                }
            });
            return changed ? next : prev;
        });
    }, [selectedTargetIds, targets, editingGateway]);

    // targetMode 자동 동기화
    useEffect(() => {
        if (currentStep === 3) setTargetMode(selectedTargetIds.length > 0 ? 'existing' : 'new');
    }, [currentStep, selectedTargetIds]);

    // ─── 하이드레이션 Effect (Edit 모드 상태 복원) ────────────────────────────
    useEffect(() => {
        if (!visible) {
            if (prevVisibleRef.current) {
                prevVisibleRef.current = false;
                prevEditingIdRef.current = undefined;
                hydratedTargetIdRef.current = null;
                // ── 모든 state 완전 초기화 ──
                setCurrentStep(0);
                setGatewayData(DEFAULT_GATEWAY_DATA);
                setProfileMode('existing');
                setSelectedProfileId(null);
                setNewProfileData({ name: '', description: '', data_points: [] });
                setMappings([]);
                setBulkSiteId('');
                setTemplateMode('existing');
                setSelectedTemplateId(templates[0]?.id || null);
                setNewTemplateData({ name: '', system_type: 'custom', template_json: '{\n  "site_id": "{{site_id}}",\n  "type": "{{type}}",\n  "target_key": "{{target_key}}",\n  "measured_value": "{{measured_value}}",\n  "timestamp": "{{timestamp}}"\n}' });
                setTemplateEditMode('advanced');
                setTargetMode('new');
                setSelectedProtocols(['HTTP']);
                setSelectedTargetIds([]);
                setTargetData(DEFAULT_TARGET_DATA);
                setEditingTargets({});
                setTransmissionMode('INTERVAL');
                setScheduleMode('new');
                setScheduleData(DEFAULT_SCHEDULE_DATA);
                setSelectedScheduleId(null);
            }
            return;
        }
        prevVisibleRef.current = visible;

        exportGatewayApi.getSchedules({ tenantId: wizardTenantId || undefined }).then(res => {
            if (res.success && res.data) setAvailableSchedules(extractItems(res.data));
        });

        if (!editingGateway) {
            if (hydratedTargetIdRef.current !== -1) {
                setCurrentStep(0);
                setGatewayData(DEFAULT_GATEWAY_DATA);
                setProfileMode('existing');
                setSelectedProfileId(null);
                setNewProfileData({ name: '', description: '', data_points: [] });
                setMappings([]);
                setBulkSiteId('');
                setTemplateMode('existing');
                setSelectedTemplateId(templates[0]?.id || null);
                setNewTemplateData({ name: '', system_type: 'custom', template_json: '{\n  "site_id": "{{site_id}}",\n  "type": "{{type}}",\n  "target_key": "{{target_key}}",\n  "measured_value": "{{measured_value}}",\n  "timestamp": "{{timestamp}}"\n}' });
                setTemplateEditMode('advanced');
                setTargetMode('new');
                setSelectedProtocols(['HTTP']);
                setSelectedTargetIds([]);
                setTargetData(DEFAULT_TARGET_DATA);
                setEditingTargets({});
                setTransmissionMode('INTERVAL');
                setScheduleMode('new');
                setScheduleData(DEFAULT_SCHEDULE_DATA);
                setSelectedScheduleId(null);
                hydratedTargetIdRef.current = -1;
            }
            return;
        }

        const currentId = editingGateway.id;
        if (hydratedTargetIdRef.current === currentId) return;

        const myAssignments = assignments || [];
        if (myAssignments.length === 0) {
            console.log(`[Wizard] Waiting for assignments for Gateway ${currentId}...`);
            return;
        }

        console.log(`[Wizard] Hydrating Gateway ${currentId}...`);
        prevEditingIdRef.current = currentId;

        setGatewayData({
            name: editingGateway.name,
            ip_address: editingGateway.ip_address,
            description: editingGateway.description || '',
            subscription_mode: editingGateway.subscription_mode || 'all',
            config: editingGateway.config || {}
        });

        if (editingGateway.tenant_id) {
            setWizardTenantId(editingGateway.tenant_id);
            fetchTenants();
            fetchSites(editingGateway.tenant_id);
        }
        if (editingGateway.site_id) setWizardSiteId(editingGateway.site_id);

        const sortedAssignments = [...myAssignments].sort((a, b) => (b.id || 0) - (a.id || 0));
        const assign = sortedAssignments[0];
        setSelectedProfileId(assign.profile_id);
        lastMappedProfileIdRef.current = assign.profile_id;
        setProfileMode('existing');

        const p = profiles.find(x => String(x.id) === String(assign.profile_id));
        if (p && p.data_points) {
            setMappings((p.data_points || []).map((dp: any) => ({
                point_id: dp.id,
                site_id: dp.site_id || dp.building_id || 0,
                target_field_name: dp.target_field_name || dp.name,
                is_enabled: true,
                conversion_config: {
                    scale: dp.scale !== undefined ? dp.scale : 1,
                    offset: dp.offset !== undefined ? dp.offset : 0
                }
            })));
        }

        const savedTargets = Object.keys(editingGateway.config?.target_priorities || {});
        const linkedTargets = targets.filter(t => savedTargets.includes(String(t.id)));
        if (linkedTargets.length > 0) {
            setTargetMode('existing');
            setSelectedTargetIds(linkedTargets.map(t => t.id));
            // 편집 모드: 기존 템플릿 사용 탭 유지, 첫 번째 템플릿 자동 선택
            setTemplateMode('existing');
            setSelectedTemplateId(templates[0]?.id || null);

            (async () => {
                try {
                    const mappingsRes = await exportGatewayApi.getTargetMappings(linkedTargets[0].id, wizardSiteId, wizardTenantId);
                    if (mappingsRes.success && mappingsRes.data && mappingsRes.data.length > 0) {
                        setMappings(mappingsRes.data);
                    }
                } catch (err) { console.error('Failed to fetch existing mappings', err); }
            })();

            const initialEditingTargets: Record<number, any[]> = {};
            targets.forEach(t => {
                if (t.config) initialEditingTargets[t.id] = Array.isArray(t.config) ? t.config : [t.config];
            });
            setEditingTargets(initialEditingTargets);

            const linkedSchedule = schedules.find(s => s.target_id === linkedTargets[0].id);
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
            setTargetMode('new');
        }

        hydratedTargetIdRef.current = currentId;
    }, [visible, editingGateway, assignments, targets, templates, schedules, profiles]);

    // ─── 프로필 변경 시 매핑 자동 생성 ───────────────────────────────────────
    useEffect(() => {
        if (profileMode === 'new' && newProfileData.data_points.length > 0) {
            if (mappings.length === 0) {
                setMappings(newProfileData.data_points.map(p => ({
                    point_id: p.id,
                    site_id: (p as any).site_id || (p as any).building_id || 0,
                    target_field_name: (p as any).target_field_name || p.name,
                    is_enabled: true,
                    conversion_config: {
                        scale: (p as any).scale !== undefined ? (p as any).scale : 1,
                        offset: (p as any).offset !== undefined ? (p as any).offset : 0
                    }
                })));
            }
        } else if (profileMode === 'existing' && selectedProfileId) {
            const isProfileChanged = selectedProfileId !== lastMappedProfileIdRef.current;
            const isEmpty = mappings.length === 0;
            if (isProfileChanged || isEmpty) {
                const prof = profiles.find(x => x.id === selectedProfileId);
                if (prof && prof.data_points?.length > 0) {
                    setMappings((prof.data_points || []).map((dp: any) => ({
                        point_id: dp.id,
                        site_id: dp.site_id || dp.building_id || 0,
                        target_field_name: dp.target_field_name || dp.name,
                        is_enabled: true,
                        conversion_config: {
                            scale: dp.scale !== undefined ? dp.scale : 1,
                            offset: dp.offset !== undefined ? dp.offset : 0
                        }
                    })));
                    lastMappedProfileIdRef.current = selectedProfileId;

                    const savedTargets = Object.keys(editingGateway?.config?.target_priorities || {});
                    const belongsToGateway = targets.filter(t => savedTargets.includes(String(t.id))).map(t => t.id);
                    if (belongsToGateway.length > 0) {
                        setSelectedTargetIds(belongsToGateway);
                        setTargetMode('existing');
                    } else {
                        setTargetMode('new');
                    }
                }
            }
        }
    }, [profileMode, newProfileData.data_points, selectedProfileId, profiles, targets]);

    // ─── Step 라벨 ────────────────────────────────────────────────────────────
    const steps = [
        { title: tl('gwWizard.step1', { ns: 'dataExport' }), description: tl('gwWizard.step1Desc', { ns: 'dataExport' }) },
        { title: tl('gwWizard.step2', { ns: 'dataExport' }), description: tl('gwWizard.step2Desc', { ns: 'dataExport' }) },
        { title: tl('gwWizard.step3', { ns: 'dataExport' }), description: tl('gwWizard.step3Desc', { ns: 'dataExport' }) },
        { title: tl('gwWizard.step4', { ns: 'dataExport' }), description: tl('gwWizard.step4Desc', { ns: 'dataExport' }) },
        { title: tl('gwWizard.step5', { ns: 'dataExport' }), description: tl('gwWizard.step5Desc', { ns: 'dataExport' }) }
    ];

    // ─── 타겟 우선순위 핸들러 ─────────────────────────────────────────────────
    const updateTargetPriority = (type: 'existing' | 'new', idOrProto: number | string, index: number, newPriority: number) => {
        const items: any[] = [];
        selectedTargetIds.forEach(tid => {
            const configs = editingTargets[tid] || [];
            configs.forEach((c: any, cIdx: number) => {
                items.push({ type: 'existing', id: tid, index: cIdx, priority: c.execution_order || 0 });
            });
        });
        selectedProtocols.forEach(proto => {
            const protoKey = `config_${proto.toLowerCase()}` as 'config_http' | 'config_mqtt' | 'config_s3';
            const configs = targetData[protoKey] || [];
            configs.forEach((c: any, cIdx: number) => {
                items.push({ type: 'new', protocol: protoKey, index: cIdx, priority: c.execution_order || 0 });
            });
        });

        const target = items.find(it =>
            type === 'existing'
                ? (it.type === 'existing' && it.id === idOrProto && it.index === index)
                : (it.type === 'new' && it.protocol === idOrProto && it.index === index)
        );
        if (!target) return;

        const otherItems = items.filter(it => it !== target).sort((a, b) => (a.priority || 0) - (b.priority || 0));
        const insertionIndex = Math.max(0, Math.min(newPriority - 1, otherItems.length));
        otherItems.splice(insertionIndex, 0, target);

        const nextTargetData = { ...targetData };
        const nextEditingTargets = { ...editingTargets };
        otherItems.forEach((it, idx) => {
            const priority = idx + 1;
            if (it.type === 'existing') {
                const configs = [...(nextEditingTargets[it.id] || [])];
                if (configs[it.index]) { configs[it.index] = { ...configs[it.index], execution_order: priority }; nextEditingTargets[it.id] = configs; }
            } else {
                const protoKey = it.protocol as 'config_http' | 'config_mqtt' | 'config_s3';
                const configs = [...(nextTargetData[protoKey] || [])];
                if (configs[it.index]) { configs[it.index] = { ...configs[it.index], execution_order: priority }; nextTargetData[protoKey] = configs; }
            }
        });
        setTargetData(nextTargetData);
        setEditingTargets(nextEditingTargets);
    };

    // ─── 네비게이션 핸들러 ────────────────────────────────────────────────────
    const handleNext = () => {
        if (currentStep === 0) {
            if (!wizardTenantId && !editingGateway) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.selectTenantError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
            if (!gatewayData.name) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.enterGatewayNameError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
        }
        if (currentStep === 1) {
            if (profileMode === 'new') {
                if (!newProfileData.name) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.enterProfileName', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
                if (newProfileData.data_points.length === 0) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.selectPointError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
            } else {
                if (!selectedProfileId) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.selectProfileError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
            }
        }
        if (currentStep === 2) {
            if (templateMode === 'new' && !newTemplateData.name) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.enterTemplateNameError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
            if (templateMode === 'existing' && !selectedTemplateId) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.selectTemplateError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
        }
        if (currentStep === 3) {
            if (targetMode === 'new') {
                if (selectedProtocols.length === 0) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.selectProtocolError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
            } else {
                if (selectedTargetIds.length === 0) { confirm({ title: tl('gwWizard.inputError', { ns: 'dataExport' }), message: tl('gwWizard.selectTargetError', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'danger' }); return; }
            }
        }
        setCurrentStep(currentStep + 1);
    };

    const handlePrev = () => setCurrentStep(currentStep - 1);

    // ─── 저장 핸들러 ──────────────────────────────────────────────────────────
    const handleFinish = async () => {
        setLoading(true);
        let gatewayId = editingGateway?.id;
        const priorityMap: Record<string, number> = {};

        try {
            // 1. Gateway 생성/수정
            if (!gatewayId) {
                const res = await exportGatewayApi.registerGateway({ ...gatewayData, site_id: wizardSiteId || undefined, tenant_id: wizardTenantId || undefined }, wizardSiteId, wizardTenantId);
                if (!res.success) throw new Error(res.message || 'Gateway registration failed');
                gatewayId = res.data?.id;
            } else {
                const updateRes = await exportGatewayApi.updateGateway(gatewayId, { ...gatewayData, site_id: wizardSiteId || undefined, tenant_id: wizardTenantId || undefined }, wizardSiteId, wizardTenantId);
                if (!updateRes.success) throw new Error(updateRes.message || 'Gateway update failed');
            }
            if (!gatewayId) throw new Error('Failed to obtain gateway ID');

            // 2. Profile
            let profileId = selectedProfileId;
            if (profileMode === 'new') {
                const pRes = await exportGatewayApi.createProfile({ name: newProfileData.name, description: newProfileData.description, data_points: newProfileData.data_points, is_enabled: true }, wizardTenantId);
                if (!pRes.success) throw new Error(pRes.message || 'Profile creation failed');
                profileId = pRes.data?.id;
            } else if (profileMode === 'existing' && profileId && mappings.length > 0) {
                const existingProfile = profiles.find(p => p.id === profileId);
                if (existingProfile && existingProfile.data_points) {
                    const updatedDataPoints = existingProfile.data_points.map((dp: any) => {
                        const editedMapping = mappings.find((m: any) => String(m.point_id) === String(dp.id));
                        if (editedMapping) {
                            return {
                                ...dp,
                                target_field_name: editedMapping.target_field_name ?? dp.target_field_name,
                                site_id: editedMapping.site_id ?? dp.site_id,
                                scale: (editedMapping as any).conversion_config?.scale ?? dp.scale ?? 1,
                                offset: (editedMapping as any).conversion_config?.offset ?? dp.offset ?? 0
                            };
                        }
                        return dp;
                    });
                    const upRes = await exportGatewayApi.updateProfile(profileId, { data_points: updatedDataPoints }, wizardTenantId);
                    if (!upRes.success) console.warn(`[Wizard] Profile update failed: ${upRes.message}`);
                }
            }
            if (!profileId) throw new Error('Failed to obtain profile ID');

            // Profile 할당
            try {
                const assignmentsRes = await exportGatewayApi.getAssignments(gatewayId, wizardSiteId, wizardTenantId);
                const oldAssignments = (assignmentsRes.data as any)?.items || (Array.isArray(assignmentsRes.data) ? assignmentsRes.data : []);
                let currentIsAlreadyAssigned = false;
                for (const old of oldAssignments) {
                    if (String(old.profile_id) === String(profileId)) { currentIsAlreadyAssigned = true; }
                    else {
                        const unRes = await exportGatewayApi.unassignProfile(gatewayId, old.profile_id, wizardSiteId, wizardTenantId);
                        if (!unRes.success) console.warn(`Failed to unassign profile ${old.profile_id}: ${unRes.message}`);
                    }
                }
                if (!currentIsAlreadyAssigned) {
                    const assignRes = await exportGatewayApi.assignProfile(gatewayId, profileId, wizardSiteId, wizardTenantId);
                    if (!assignRes.success) throw new Error(assignRes.message || 'Profile assignment failed');
                }
            } catch (err) {
                console.error('Assignment triage failed', err);
                const fbRes = await exportGatewayApi.assignProfile(gatewayId, profileId, wizardSiteId, wizardTenantId);
                if (!fbRes.success) throw new Error(fbRes.message || 'Final profile assignment failed');
            }

            // 3. Template
            let templateId = selectedTemplateId;
            if (templateMode === 'new') {
                const tRes = await exportGatewayApi.createTemplate({ ...newTemplateData, is_active: true }, wizardTenantId);
                if (!tRes.success) throw new Error(tRes.message || 'Template creation failed');
                templateId = tRes.data?.id;
            }

            // 4. Target
            let finalTargetIds: number[] = [];
            if (targetMode === 'existing') {
                finalTargetIds = selectedTargetIds;
                await Promise.all(selectedTargetIds.map(async (tid) => {
                    const editedConfigs = editingTargets[tid];
                    try {
                        priorityMap[tid.toString()] = editedConfigs?.[0]?.execution_order || 100;
                        const uRes = await exportGatewayApi.updateTarget(tid, { config: editedConfigs || [], export_mode: transmissionMode === 'EVENT' ? 'REALTIME' : 'batched' }, wizardTenantId);
                        if (!uRes.success) console.error(`Failed to update target ${tid}: ${uRes.message}`);
                    } catch (err) { console.error(`Failed to update target ${tid}`, err); }
                    if (mappings.length > 0) {
                        try {
                            const mRes = await exportGatewayApi.saveTargetMappings(tid, mappings, wizardSiteId, wizardTenantId);
                            if (!mRes.success) console.error(`Failed to save mappings for target ${tid}: ${mRes.message}`);
                        } catch (err) { console.error(`Failed to save mappings for target ${tid}`, err); }
                    }
                }));
            } else {
                for (const proto of selectedProtocols) {
                    const protoKey = `config_${proto.toLowerCase()}` as 'config_http' | 'config_mqtt' | 'config_s3';
                    const configs = targetData[protoKey] || [];
                    for (const [cIdx, cfg] of configs.entries()) {
                        const targetName = configs.length > 1 ? `${targetData.name}_${proto}_${cIdx + 1}` : `${targetData.name}_${proto}`;
                        const tRes = await exportGatewayApi.createTarget({ name: targetName, target_type: proto, config: [cfg], is_enabled: true, export_mode: transmissionMode === 'EVENT' ? 'REALTIME' : 'batched' }, wizardTenantId);
                        if (!tRes.success) throw new Error(tRes.message || 'Transmission target creation failed');
                        if (tRes.data?.id) {
                            finalTargetIds.push(tRes.data.id);
                            priorityMap[tRes.data.id.toString()] = cfg.execution_order || (cIdx + 1);
                            if (mappings.length > 0) {
                                const mRes = await exportGatewayApi.saveTargetMappings(tRes.data.id, mappings, wizardSiteId, wizardTenantId);
                                if (!mRes.success) throw new Error(mRes.message || 'Mapping save failed');
                            }
                        }
                    }
                }
            }

            // 5. Schedule
            if (transmissionMode === 'INTERVAL') {
                for (const tid of finalTargetIds) {
                    let schedToCreate: Partial<ExportSchedule> = {
                        target_id: tid,
                        schedule_name: scheduleData.schedule_name || `${gatewayData.name}_Schedule`,
                        cron_expression: scheduleData.cron_expression,
                        data_range: scheduleData.data_range,
                        lookback_periods: 1,
                        is_enabled: true
                    };
                    if (scheduleMode === 'existing' && selectedScheduleId) {
                        const existingSched = availableSchedules.find(s => s.id === selectedScheduleId);
                        if (existingSched) {
                            schedToCreate = {
                                target_id: tid,
                                schedule_name: existingSched.schedule_name,
                                cron_expression: existingSched.cron_expression,
                                data_range: existingSched.data_range || 'minute',
                                lookback_periods: existingSched.lookback_periods || 1,
                                is_enabled: true
                            };
                        }
                    }
                    await exportGatewayApi.createSchedule(schedToCreate, wizardTenantId);
                }
            }

            // 6. Gateway priority 최종 업데이트
            await exportGatewayApi.updateGateway(gatewayId, { ...gatewayData, config: { ...(gatewayData.config || {}), target_priorities: priorityMap } }, wizardSiteId, wizardTenantId);

            await confirm({ title: tl('gwWizard.completeTitle', { ns: 'dataExport' }), message: tl('gwWizard.completeMsg', { ns: 'dataExport' }), showCancelButton: false, confirmButtonType: 'success' });
            onSuccess();
        } catch (e: any) {
            console.error('GATEWAY_SAVE_ERROR:', e);
            const errMsg = e?.response?.data?.message || e?.message || 'Error during save. Please check the logs.';
            await confirm({ title: tl('gwWizard.failedTitle', { ns: 'dataExport' }), message: errMsg, showCancelButton: false, confirmButtonType: 'danger' });
        } finally {
            setLoading(false);
        }
    };

    // ─── 계산된 값 ────────────────────────────────────────────────────────────
    const totalTargetCount = selectedTargetIds.length
        + (selectedProtocols.includes('HTTP') ? targetData.config_http.length : 0)
        + (selectedProtocols.includes('MQTT') ? targetData.config_mqtt.length : 0)
        + (selectedProtocols.includes('S3') ? targetData.config_s3.length : 0);

    return {
        // Navigation
        currentStep, setCurrentStep,
        loading,
        steps,
        handleNext, handlePrev, handleFinish,

        // Step 0
        isAdmin, tenants, sites,
        wizardTenantId, setWizardTenantId,
        wizardSiteId, setWizardSiteId,
        fetchSites,
        gatewayData, setGatewayData,

        // Step 1
        profileMode, setProfileMode,
        selectedProfileId, setSelectedProfileId,
        newProfileData, setNewProfileData,
        mappings, setMappings,
        bulkSiteId, setBulkSiteId,

        // Step 2
        templateMode, setTemplateMode,
        selectedTemplateId, setSelectedTemplateId,
        newTemplateData, setNewTemplateData,
        templateEditMode, setTemplateEditMode,
        templateSimpleMappings, setTemplateSimpleMappings,
        templateIsWrappedInArray, setTemplateIsWrappedInArray,
        lastFocusedTemplateElement, setLastFocusedTemplateElement,

        // Step 3
        transmissionMode, setTransmissionMode,
        selectedProtocols, setSelectedProtocols,
        targetMode, setTargetMode,
        selectedTargetIds, setSelectedTargetIds,
        targetData, setTargetData,
        editingTargets, setEditingTargets,
        updateTargetPriority,
        totalTargetCount,

        // Step 4
        scheduleData, setScheduleData,
        scheduleMode, setScheduleMode,
        selectedScheduleId, setSelectedScheduleId,
        availableSchedules,

        // Context
        editingGateway, onClose, onDelete,
        profiles, targets, templates, allPoints,
        tl,
        confirm
    };
}
