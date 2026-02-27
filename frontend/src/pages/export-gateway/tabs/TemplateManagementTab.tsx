import React, { useState, useEffect } from 'react';
import exportGatewayApi, { PayloadTemplate } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

interface TemplateManagementTabProps {
    siteId?: number | null;
    tenantId?: number | null;
    isAdmin?: boolean;
    tenants?: any[];
}

const TemplateManagementTab: React.FC<TemplateManagementTabProps> = ({ siteId, tenantId, isAdmin, tenants = [] }) => {
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingTemplate, setEditingTemplate] = useState<Partial<PayloadTemplate> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
    const [editMode, setEditMode] = useState<'simple' | 'advanced'>('advanced');
    const [simpleMappings, setSimpleMappings] = useState<{ key: string; value: string }[]>([]);
    const [isJsonValid, setIsJsonValid] = useState(true);
    const [lastFocusedElement, setLastFocusedElement] = useState<{ id: string, index?: number, field?: 'key' | 'value' } | null>(null);
    const { confirm } = useConfirmContext();

    const [isWrappedInArray, setIsWrappedInArray] = useState(false);
    const [logicSource, setLogicSource] = useState('measured_value');
    const [logicTrueVal, setLogicTrueVal] = useState('1');
    const [logicFalseVal, setLogicFalseVal] = useState('0');

    // [v3.6.12] Smart Import from Sample JSON
    const [isImportVisible, setIsImportVisible] = useState(false);
    const [sampleInput, setSampleInput] = useState('');

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
            name: '데이터 속성 (Attributes)',
            items: [
                { label: '데이터 타입', value: '{{type}}', desc: 'num:숫자, bit:디지털, str:문자열 ({{data_type}}도 가능)' },
                { label: '제어 가능 여부', value: '{{is_control}}', desc: '1:제어 가능, 0:읽기 전용 ({{is_writable}}도 가능)' }
            ]
        },
        {
            name: '메타데이터 (Metadata)',
            items: [
                { label: '사이트 ID', value: '{{site_id}}', desc: '표준 사이트 숫자 ID (Legacy: {{bd}})' },
                { label: '사이트 명칭', value: '{{site_name}}', desc: '실제 사이트/건물 이름 (예: 구로본사)' },
                { label: '디바이스 명칭', value: '{{device_name}}', desc: '실제 장비 이름 (예: 냉동기-01)' },
                { label: '포인트 ID', value: '{{point_id}}', desc: '원본 포인트(Point) ID' },
                { label: '원본 이름', value: '{{original_name}}', desc: '수집기 내부 원본 포인트명' }
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
                { label: '계측 범위(Min)', value: '{{mi|array}}', desc: '미터링 최소 한계값 (배열 강제)' },
                { label: '계측 범위(Max)', value: '{{mx|array}}', desc: '미터링 최대 한계값 (배열 강제)' },
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
        },
        {
            name: '스마트 로직 (Smart Logic)',
            items: [
                { label: '값 치환 (MAPPING)', value: '{{map:measured_value:참일때값:거짓일때값}}', desc: '값이 1이면 참, 0이면 거짓값을 출력 예: {map:target_key:A:B}' },
                { label: '디지털 반전 (INVERT)', value: '{{map:measured_value:0:1}}', desc: '1일때 0, 0일때 1로 반전하여 전송' },
                { label: '상태 텍스트 (TEXT)', value: '{{map:status_code:정상:장애}}', desc: '숫자 상태값을 읽기 쉬운 한글로 변환' }
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
        site_id: "101",
        site_name: "구로본사 (Headquarters)",
        device_name: "냉동기-01 (AHU)",
        type: "num",
        data_type: "num",
        is_control: 1,
        is_writable: 1,
        status_code: 0,
        alarm_level: 1,
        timestamp: new Date().toISOString().replace('T', ' ').split('.')[0],

        // Backward Compatibility (Old names)
        nm: "PRO_FILE_TARGET_KEY",
        vl: "45.2 (MEASURED_VALUE)",
        tm: new Date().toISOString().replace('T', ' ').split('.')[0],
        des: "프로파일에 정의된 데이터 설명",
        bd: "101",
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
        mi: 0,
        mx: 100
    };

    // [v3.6.10] Refined preview renderer to correctly handle numeric/array types
    const getSafePreview = (template_json: any, editMode: 'simple' | 'advanced', simpleMappings: { key: string, value: string }[], isWrappedInArray: boolean) => {
        try {
            let jsonText = "";
            if (editMode === 'simple') {
                const obj: Record<string, string> = {};
                simpleMappings.forEach(m => { if (m.key) obj[m.key] = m.value; });
                jsonText = isWrappedInArray ? JSON.stringify([obj], null, 2) : JSON.stringify(obj, null, 2);
            } else {
                jsonText = typeof template_json === 'string' ? template_json : JSON.stringify(template_json || {}, null, 2);
            }

            let preview = jsonText;
            Object.entries(SAMPLE_DATA).forEach(([k, v]) => {
                const escapedVal = String(v);

                // 1. Handle array filter: "{{var|array}}" or {{var|array}}
                // We replace the quoted version first to ensure it becomes a literal array in preview
                preview = preview.replace(new RegExp(`\"\\{\\{${k}\\|array\\}\\}\"`, 'g'), `[${escapedVal}]`);
                preview = preview.replace(new RegExp(`\\{\\{${k}\\|array\\}\\}`, 'g'), `[${escapedVal}]`);

                // 2. Handle numeric substitutions for common fields if they are quoted
                const numericFields = ['site_id', 'measured_value', 'status_code', 'alarm_level', 'is_control', 'is_writable', 'point_id', 'mi', 'mx', 'bd', 'st', 'al', 'vl'];
                if (numericFields.includes(k)) {
                    preview = preview.replace(new RegExp(`\"\\{\\{${k}\\}\\}\"`, 'g'), escapedVal);
                }

                // 3. Standard substitution for anything else
                preview = preview.replace(new RegExp(`\\{\\{${k}\\}\\}`, 'g'), escapedVal);
                preview = preview.replace(new RegExp(`\\{${k}\\}`, 'g'), escapedVal);
            });

            try {
                const parsedPreview = JSON.parse(preview);
                return JSON.stringify(parsedPreview, null, 2);
            } catch {
                return String(preview);
            }
        } catch (e) {
            console.error("Preview rendering failed", e);
            return "JSON 형식이 올바르지 않거나 렌더링 중 오류가 발생했습니다.";
        }
    };

    const fetchTemplates = async () => {
        setLoading(true);
        try {
            const response = await exportGatewayApi.getTemplates({ tenantId });
            const data: any = response.data;
            // Handle various API formats (items, rows, or direct array) to prevent .map() crash
            const list = Array.isArray(data) ? data : (data?.items || data?.rows || []);
            setTemplates(list);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    const insertAtCursor = (insertText: string) => {
        const checkInsertion = (currentVal: string, pos: number) => {
            const beforeSelection = currentVal.substring(pos - insertText.length, pos);
            if (beforeSelection === insertText) return false;
            return true;
        };

        if (editMode === 'advanced') {
            const textarea = document.getElementById('template-textarea') as HTMLTextAreaElement;
            if (textarea) {
                const start = textarea.selectionStart;
                const end = textarea.selectionEnd;
                if (!checkInsertion(textarea.value, start)) return;
                textarea.setRangeText(insertText, start, end, 'end');
                textarea.focus();
                setEditingTemplate({ ...editingTemplate, template_json: textarea.value });
                setHasChanges(true);
            }
        } else if (editMode === 'simple' && lastFocusedElement) {
            const { index, field } = lastFocusedElement;
            if (index !== undefined && field) {
                const el = document.getElementById(`mapping-${field}-${index}`) as HTMLInputElement;
                if (el) {
                    const start = el.selectionStart || 0;
                    const end = el.selectionEnd || 0;
                    if (!checkInsertion(el.value, start)) return;
                    const val = el.value;
                    const newVal = val.substring(0, start) + insertText + val.substring(end);
                    const newMappings = [...simpleMappings];
                    newMappings[index][field] = newVal;
                    setSimpleMappings(newMappings);
                    setHasChanges(true);
                    setTimeout(() => {
                        el.focus();
                        const nextPos = start + insertText.length;
                        el.setSelectionRange(nextPos, nextPos);
                    }, 0);
                }
            }
        }
    };

    useEffect(() => { fetchTemplates(); }, [siteId, tenantId]);

    const handleCloseModal = async () => {
        if (hasChanges) {
            const confirmed = await confirm({
                title: '변경사항 유실 주의',
                message: '수정 중인 내용이 있습니다. 저장하지 않고 닫으시면 모든 데이터가 사라집니다. 정말 닫으시겠습니까?',
                confirmText: '닫기',
                cancelText: '취소',
                confirmButtonType: 'warning'
            });
            if (!confirmed) return;
        }
        setIsModalOpen(false);
        setHasChanges(false);
    };

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();

        if (!hasChanges && editingTemplate?.id) {
            await confirm({
                title: '수정사항 없음',
                message: '수정된 정보가 없습니다.',
                showCancelButton: false,
                confirmButtonType: 'primary'
            });
            setIsModalOpen(false);
            return;
        }

        try {
            if (editMode === 'advanced' && !isJsonValid) {
                await confirm({
                    title: 'JSON 형식 오류',
                    message: '유효하지 않은 JSON 형식입니다. 코드를 수정해 주세요.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
                return;
            }

            // Simple Mode일 경우 JSON으로 변환
            let templateJson = editingTemplate?.template_json;
            if (editMode === 'simple') {
                const obj: Record<string, string> = {};
                simpleMappings.forEach(m => {
                    if (m.key) obj[m.key] = m.value;
                });
                templateJson = isWrappedInArray ? JSON.stringify([obj]) : JSON.stringify(obj);
            }

            if (typeof templateJson === 'string') {
                try {
                    // [v3.6.11] Lax parsing for save: replace placeholders so JSON.parse doesn't fail
                    const placeholderFree = templateJson
                        .replace(/\{\{[^}]+\}\}/g, '0')
                        .replace(/\{[^}]+\}/g, '0');
                    JSON.parse(placeholderFree);
                    // Note: We still save the original templateJson with placeholders as a STRING
                } catch (parseError) {
                    await confirm({
                        title: 'JSON 형식 오류',
                        message: `유효하지 않은 JSON 형식입니다.\n\n[상세 내역]\n${parseError instanceof Error ? parseError.message : String(parseError)}\n\n팁: 중괄호를 제외한 나머지 구조가 올바른 JSON인지 확인해 주세요.`,
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                    return;
                }
            }

            const dataToSave = {
                ...editingTemplate,
                template_json: templateJson
            };
            let response;
            const targetTenantId = editingTemplate?.tenant_id || tenantId;
            if (editingTemplate?.id) {
                response = await exportGatewayApi.updateTemplate(editingTemplate.id, dataToSave, targetTenantId);
            } else {
                response = await exportGatewayApi.createTemplate(dataToSave as PayloadTemplate, targetTenantId);
            }

            await confirm({
                title: '저장 완료',
                message: '템플릿이 성공적으로 저장되었습니다.',
                showCancelButton: false,
                confirmButtonType: 'success'
            });
            setIsModalOpen(false);
            setHasChanges(false);
            setIsJsonValid(true);
            fetchTemplates();
        } catch (error) {
            await confirm({
                title: '저장 실패',
                message: '템플릿을 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const syncAdvancedToSimple = (jsonStr: string) => {
        try {
            // Lax parsing: replace placeholders so JSON.parse doesn't fail on unquoted tokens
            const placeholderFree = (jsonStr || '{}')
                .replace(/\{\{[^}]+\}\}/g, '"TEMPLATE_VAR"')
                .replace(/\{[^}]+\}/g, '"TEMPLATE_VAR"');
            const parsed = JSON.parse(placeholderFree);
            let isWrapped = false;
            let mappings: { key: string; value: string }[] = [];

            if (Array.isArray(parsed)) {
                if (parsed.length > 0 && typeof parsed[0] === 'object') {
                    isWrapped = true;
                    mappings = Object.entries(parsed[0]).map(([k, v]) => ({
                        key: k,
                        value: typeof v === 'string' ? v : JSON.stringify(v)
                    }));
                }
            } else if (typeof parsed === 'object' && parsed !== null) {
                isWrapped = false;
                mappings = Object.entries(parsed).map(([k, v]) => ({
                    key: k,
                    value: typeof v === 'string' ? v : JSON.stringify(v)
                }));
            }

            if (mappings.length > 0) {
                setSimpleMappings(mappings);
                setIsWrappedInArray(isWrapped);
            }
        } catch (e) {
            console.warn("Could not sync Advanced to Simple due to JSON errors", e);
        }
    };

    // [v3.6.12] Smart Inference: Parse Sample JSON and map to PulseOne placeholders
    const handleImportFromSample = () => {
        try {
            let parsed = JSON.parse(sampleInput);
            let isWrapped = false;
            if (Array.isArray(parsed)) {
                if (parsed.length > 0) {
                    parsed = parsed[0];
                    isWrapped = true;
                } else {
                    throw new Error("Empty array");
                }
            }

            const mappings: { key: string; value: string }[] = [];

            // Mapper: Key Patterns -> Placeholder
            const KEY_MAP: Record<string, string> = {
                'bd': '{{site_id}}',
                'site_id': '{{site_id}}',
                'vl': '{{measured_value}}',
                'measured_value': '{{measured_value}}',
                'tm': '{{timestamp}}',
                'timestamp': '{{timestamp}}',
                'st': '{{status_code}}',
                'status_code': '{{status_code}}',
                'al': '{{alarm_level}}',
                'alarm_level': '{{alarm_level}}',
                'ty': '{{type}}',
                'type': '{{type}}',
                'nm': '{{target_key}}',
                'target_key': '{{target_key}}',
                'des': '{{target_description}}',
                'target_description': '{{target_description}}',
                'mi': '{{mi|array}}',
                'mx': '{{mx|array}}'
            };

            Object.entries(parsed).forEach(([k, v]) => {
                let placeholder = KEY_MAP[k] || `{{${k}}}`;
                // Auto-detect array types for variables not in KEY_MAP
                if (Array.isArray(v) && !placeholder.includes('|array')) {
                    placeholder = placeholder.replace('}}', '|array}}');
                }
                mappings.push({ key: k, value: placeholder });
            });

            if (mappings.length > 0) {
                setSimpleMappings(mappings);
                setIsWrappedInArray(isWrapped);
                setEditMode('simple');
                setIsImportVisible(false);
                setHasChanges(true);
                setSampleInput('');
            }
        } catch (e) {
            confirm({
                title: '분석 실패',
                message: '유효한 JSON 샘플을 입력해 주세요. (배열 형태도 지원합니다)',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '템플릿 삭제 확인',
            message: '이 페이로드 템플릿을 삭제하시겠습니까? 이 템플릿을 사용하는 타겟들의 전송이 실패할 수 있습니다.',
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteTemplate(id, tenantId);
            fetchTemplates();
        } catch (error) {
            await confirm({ title: '삭제 실패', message: '템플릿을 삭제하는 중 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>페이로드 템플릿 설정</h3>
                <button className="btn btn-primary btn-sm" onClick={() => {
                    setEditingTemplate({
                        name: '',
                        system_type: 'insite',
                        template_json: '{\n  "bd": {{site_id}},\n  "ty": "{{type}}",\n  "nm": "{{target_key}}",\n  "vl": {{measured_value}},\n  "il": "{{il}}",\n  "xl": "{{xl}}",\n  "mi": {{mi|array}},\n  "mx": {{mx|array}},\n  "tm": "{{timestamp}}",\n  "st": {{status_code}},\n  "al": {{alarm_level}},\n  "des": "{{target_description}}"\n}',
                        is_active: true
                    });
                    setSimpleMappings([
                        { key: 'bd', value: '{{site_id}}' },
                        { key: 'ty', value: '{{type}}' },
                        { key: 'nm', value: '{{target_key}}' },
                        { key: 'vl', value: '{{measured_value}}' },
                        { key: 'il', value: '{{il}}' },
                        { key: 'xl', value: '{{xl}}' },
                        { key: 'mi', value: '{{mi|array}}' },
                        { key: 'mx', value: '{{mx|array}}' },
                        { key: 'tm', value: '{{timestamp}}' },
                        { key: 'st', value: '{{status_code}}' },
                        { key: 'al', value: '{{alarm_level}}' },
                        { key: 'des', value: '{{target_description}}' }
                    ]);
                    setEditMode('simple');
                    setIsWrappedInArray(false);
                    setIsModalOpen(true);
                    setHasChanges(false);
                }}>
                    <i className="fas fa-plus" /> 템플릿 추가
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>이름</th>
                            <th>시스템 타입</th>
                            <th>상태</th>
                            <th>관리</th>
                        </tr>
                    </thead>
                    <tbody>
                        {templates.map(t => (
                            <tr key={t.id}>
                                <td>{t.name}</td>
                                <td><span className="mgmt-badge neutral">{t.system_type}</span></td>
                                <td>
                                    <span className={`mgmt-badge ${t.is_active ? 'success' : 'neutral'}`}>
                                        {t.is_active ? '활성' : '비활성'}
                                    </span>
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline" onClick={() => {
                                            const rawJson = t.template_json;
                                            const jsonStr = typeof rawJson === 'string' ? rawJson : JSON.stringify(rawJson, null, 2);
                                            setEditingTemplate({ ...t, template_json: jsonStr });

                                            try {
                                                let parsed = typeof rawJson === 'string' ? JSON.parse(rawJson) : rawJson;
                                                let isWrapped = false;

                                                if (Array.isArray(parsed) && parsed.length === 1 && typeof parsed[0] === 'object') {
                                                    parsed = parsed[0];
                                                    isWrapped = true;
                                                }

                                                setIsWrappedInArray(isWrapped);

                                                if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
                                                    const mappings = Object.entries(parsed).map(([key, value]) => ({
                                                        key,
                                                        value: typeof value === 'string' ? value : JSON.stringify(value)
                                                    }));
                                                    setSimpleMappings(mappings);
                                                    setEditMode('simple');
                                                } else {
                                                    setEditMode('advanced');
                                                }
                                            } catch (e) {
                                                setEditMode('advanced');
                                            }

                                            setIsModalOpen(true);
                                            setIsJsonValid(true);
                                        }} style={{ width: 'auto' }}>수정</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-error" onClick={() => handleDelete(t.id)} style={{ width: 'auto' }}>삭제</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                        {templates.length === 0 && !loading && (
                            <tr>
                                <td colSpan={4} style={{ textAlign: 'center', padding: '40px', color: 'var(--neutral-400)' }}>
                                    등록된 템플릿이 없습니다.
                                </td>
                            </tr>
                        )}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-content" style={{ maxWidth: '900px', width: '95%' }}>
                        <div className="mgmt-modal-header" style={{ padding: '15px 20px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <h3 className="mgmt-modal-title" style={{ fontSize: '16px', margin: 0 }}>{editingTemplate?.id ? "템플릿 수정" : "페이로드 템플릿 추가"}</h3>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                <button
                                    type="button"
                                    className={`mgmt-btn btn-primary btn-sm ${editMode === 'advanced' && !isJsonValid ? 'disabled' : ''}`}
                                    disabled={editMode === 'advanced' && !isJsonValid}
                                    onClick={(e) => handleSave(e as any)}
                                    style={{ height: '32px', fontSize: '13px', padding: '0 16px', borderRadius: '6px' }}
                                >
                                    <i className="fas fa-save" style={{ marginRight: '6px' }} />
                                    저장
                                </button>
                                <button className="mgmt-modal-close" onClick={handleCloseModal}>&times;</button>
                            </div>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body" style={{ padding: 0, display: 'flex', height: '750px', overflow: 'hidden' }}>
                                {/* Sidebar: Explorer & Helpers */}
                                <div style={{ width: '280px', background: 'var(--neutral-50)', borderRight: '1px solid var(--neutral-200)', display: 'flex', flexDirection: 'column', overflowY: 'auto' }}>
                                    <div style={{ padding: '20px' }}>
                                        <div style={{ fontSize: '12px', fontWeight: 700, color: 'var(--neutral-800)', marginBottom: '15px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <i className="fas fa-database" style={{ color: 'var(--primary-500)' }} />
                                            데이터 탐색기
                                        </div>

                                        {/* Recommended Section: Prominent for quick access */}
                                        <div style={{ marginBottom: '20px', padding: '12px', background: 'var(--primary-50)', borderRadius: '10px', border: '1px solid var(--primary-100)' }}>
                                            <div style={{ fontSize: '10px', fontWeight: 800, color: 'var(--primary-700)', marginBottom: '8px', display: 'flex', alignItems: 'center', gap: '5px' }}>
                                                <i className="fas fa-star" /> 추천 항목
                                            </div>
                                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                                                <button
                                                    type="button"
                                                    className="mgmt-badge primary"
                                                    style={{ cursor: 'pointer', border: 'none', fontSize: '11px', padding: '5px 10px', borderRadius: '6px', fontWeight: 600, background: 'var(--primary-500)', color: '#fff', boxShadow: '0 2px 4px rgba(0,0,0,0.1)' }}
                                                    onClick={() => insertAtCursor('{{target_key}}')}
                                                    title="매핑 명칭 (TARGET KEY)"
                                                >
                                                    매핑 명칭
                                                </button>
                                                <button
                                                    type="button"
                                                    className="mgmt-badge primary"
                                                    style={{ cursor: 'pointer', border: 'none', fontSize: '11px', padding: '5px 10px', borderRadius: '6px', fontWeight: 600, background: 'var(--primary-500)', color: '#fff', boxShadow: '0 2px 4px rgba(0,0,0,0.1)' }}
                                                    onClick={() => insertAtCursor('{{measured_value}}')}
                                                    title="데이터 값 (VALUE)"
                                                >
                                                    데이터 값
                                                </button>
                                            </div>
                                        </div>

                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '15px' }}>
                                            {VARIABLE_CATEGORIES.map(cat => (
                                                <div key={cat.name}>
                                                    <div style={{ fontSize: '10px', fontWeight: 700, color: 'var(--neutral-400)', marginBottom: '8px', textTransform: 'uppercase', letterSpacing: '0.05em' }}>{cat.name}</div>
                                                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                                                        {cat.items.map(v => (
                                                            <button
                                                                key={v.value}
                                                                type="button"
                                                                className="mgmt-badge neutral"
                                                                style={{ cursor: 'pointer', border: '1px solid var(--neutral-200)', fontSize: '10px', padding: '3px 8px', borderRadius: '4px', fontWeight: 500, background: '#fff', color: 'var(--neutral-600)', transition: 'all 0.2s' }}
                                                                onClick={() => insertAtCursor(v.value)}
                                                                title={v.desc}
                                                            >
                                                                {v.label}
                                                            </button>
                                                        ))}
                                                    </div>
                                                </div>
                                            ))}
                                        </div>

                                        <hr style={{ margin: '20px 0', border: 'none', borderTop: '1px solid var(--neutral-200)' }} />

                                        {/* Logic Assistant: Collapsible to reduce noise */}
                                        <details style={{ cursor: 'pointer' }}>
                                            <summary style={{ fontSize: '12px', fontWeight: 700, color: 'var(--neutral-500)', display: 'flex', alignItems: 'center', gap: '8px', outline: 'none', marginBottom: '10px' }}>
                                                <i className="fas fa-magic" />
                                                <span>고급 스마트 로직</span>
                                                <i className="fas fa-chevron-down" style={{ fontSize: '10px', marginLeft: 'auto', opacity: 0.5 }} />
                                            </summary>

                                            <div className="logic-sidebar-form" style={{ display: 'flex', flexDirection: 'column', gap: '12px', padding: '10px', background: 'var(--neutral-100)', borderRadius: '8px', marginTop: '5px' }}>
                                                <div>
                                                    <label style={{ fontSize: '10px', color: 'var(--neutral-500)', fontWeight: 600, display: 'block', marginBottom: '4px' }}>대상 데이터</label>
                                                    <select
                                                        className="mgmt-input"
                                                        style={{ height: '32px', fontSize: '12px', padding: '2px 8px', width: '100%' }}
                                                        value={logicSource}
                                                        onChange={e => setLogicSource(e.target.value)}
                                                    >
                                                        <option value="measured_value">측정값 (measured_value)</option>
                                                        <option value="status_code">통신상태 (status_code)</option>
                                                        <option value="alarm_level">알람등급 (alarm_level)</option>
                                                    </select>
                                                </div>
                                                <div style={{ display: 'flex', gap: '8px' }}>
                                                    <div style={{ flex: 1 }}>
                                                        <label style={{ fontSize: '10px', color: 'var(--success-600)', fontWeight: 600, display: 'block', marginBottom: '4px' }}>참(1)일 때</label>
                                                        <input
                                                            type="text"
                                                            className="mgmt-input"
                                                            style={{ height: '32px', fontSize: '12px' }}
                                                            value={logicTrueVal}
                                                            onChange={e => setLogicTrueVal(e.target.value)}
                                                            placeholder="예: On"
                                                        />
                                                    </div>
                                                    <div style={{ flex: 1 }}>
                                                        <label style={{ fontSize: '10px', color: 'var(--danger-600)', fontWeight: 600, display: 'block', marginBottom: '4px' }}>거짓(0)일 때</label>
                                                        <input
                                                            type="text"
                                                            className="mgmt-input"
                                                            style={{ height: '32px', fontSize: '12px' }}
                                                            value={logicFalseVal}
                                                            onChange={e => setLogicFalseVal(e.target.value)}
                                                            placeholder="예: Off"
                                                        />
                                                    </div>
                                                </div>
                                                <button
                                                    type="button"
                                                    className="mgmt-btn btn-primary"
                                                    style={{ width: '100%', height: '32px', fontSize: '12px', marginTop: '4px' }}
                                                    onClick={() => {
                                                        const tag = `{{map:${logicSource}:${logicTrueVal}:${logicFalseVal}}}`;
                                                        insertAtCursor(tag);
                                                    }}
                                                >
                                                    태그 삽입
                                                </button>

                                                <div style={{ marginTop: '10px', borderTop: '1px solid var(--neutral-200)', paddingTop: '10px' }}>
                                                    <div style={{ fontSize: '10px', color: 'var(--neutral-400)', fontWeight: 600, marginBottom: '6px' }}>포맷 필터</div>
                                                    <button
                                                        type="button"
                                                        className="mgmt-badge primary"
                                                        style={{ width: '100%', cursor: 'pointer', border: 'none', fontSize: '10px', background: 'var(--primary-600)', color: '#fff', padding: '6px 8px', borderRadius: '4px', fontWeight: 600 }}
                                                        onClick={() => {
                                                            if (lastFocusedElement) {
                                                                const { index, field } = lastFocusedElement;
                                                                if (index !== undefined && field === 'value') {
                                                                    const mappings = [...simpleMappings];
                                                                    let val = mappings[index].value;
                                                                    if (val.includes('{{') && val.includes('}}') && !val.includes('|array')) {
                                                                        val = val.replace('}}', '|array}}');
                                                                        mappings[index].value = val;
                                                                        setSimpleMappings(mappings);
                                                                        setHasChanges(true);
                                                                    }
                                                                }
                                                            } else if (editMode === 'advanced') {
                                                                const textarea = document.getElementById('template-textarea') as HTMLTextAreaElement;
                                                                if (textarea) {
                                                                    const start = textarea.selectionStart;
                                                                    const end = textarea.selectionEnd;
                                                                    const currentVal = textarea.value;
                                                                    const selectedText = currentVal.substring(start, end);
                                                                    if (selectedText.includes('{{') && selectedText.includes('}}')) {
                                                                        const newVal = selectedText.replace('}}', '|array}}');
                                                                        textarea.setRangeText(newVal, start, end, 'end');
                                                                        setEditingTemplate({ ...editingTemplate, template_json: textarea.value });
                                                                    } else {
                                                                        insertAtCursor('|array');
                                                                    }
                                                                }
                                                            }
                                                        }}
                                                    >
                                                        JSON 배열 강제 (|array)
                                                    </button>
                                                </div>
                                            </div>
                                        </details>
                                    </div>
                                </div>

                                {/* Main Content Area */}
                                <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflowY: 'auto', background: '#fff' }}>
                                    <div style={{ padding: '25px' }}>
                                        {/* Name & System Type Row */}
                                        {/* Focus Guide Banner: Clear 1-2-3 Step Instructions */}
                                        <div style={{ marginBottom: '25px', padding: '20px', background: 'linear-gradient(135deg, var(--primary-500) 0%, var(--primary-600) 100%)', borderRadius: '12px', color: '#fff', boxShadow: '0 4px 15px rgba(0,0,0,0.1)', display: 'flex', alignItems: 'center', gap: '20px' }}>
                                            <div style={{ background: 'rgba(255,255,255,0.2)', width: '50px', height: '50px', borderRadius: '50%', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '24px', flexShrink: 0 }}>
                                                ✨
                                            </div>
                                            <div style={{ flex: 1 }}>
                                                <div style={{ fontSize: '14px', fontWeight: 800, marginBottom: '6px', letterSpacing: '-0.02em' }}>템플릿 생성 가이드: 3단계면 충분합니다!</div>
                                                <div style={{ display: 'flex', gap: '15px' }}>
                                                    <div style={{ fontSize: '11px', display: 'flex', alignItems: 'center', gap: '6px' }}><span style={{ background: '#fff', color: 'var(--primary-600)', width: '16px', height: '16px', borderRadius: '50%', display: 'inline-flex', alignItems: 'center', justifyContent: 'center', fontWeight: 900 }}>1</span> 왼쪽에서 데이터 선택</div>
                                                    <div style={{ fontSize: '11px', display: 'flex', alignItems: 'center', gap: '6px' }}><span style={{ background: '#fff', color: 'var(--primary-600)', width: '16px', height: '16px', borderRadius: '50%', display: 'inline-flex', alignItems: 'center', justifyContent: 'center', fontWeight: 900 }}>2</span> 표에 JSON 이름 입력</div>
                                                    <div style={{ fontSize: '11px', display: 'flex', alignItems: 'center', gap: '6px' }}><span style={{ background: '#fff', color: 'var(--primary-600)', width: '16px', height: '16px', borderRadius: '50%', display: 'inline-flex', alignItems: 'center', justifyContent: 'center', fontWeight: 900 }}>3</span> 미리보기 확인 후 저장</div>
                                                </div>
                                            </div>
                                        </div>
                                        <div style={{ padding: '0 0 25px 0' }}>
                                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', background: 'var(--neutral-50)', padding: '15px 20px', borderRadius: '12px', border: '1px solid var(--neutral-200)' }}>
                                                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                                    <div style={{ background: 'var(--primary-100)', color: 'var(--primary-600)', width: '36px', height: '36px', borderRadius: '50%', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                                                        <i className="fas fa-bolt" />
                                                    </div>
                                                    <div>
                                                        <div style={{ fontSize: '13px', fontWeight: 800, color: 'var(--neutral-800)' }}>스마트 페이로드 생성기</div>
                                                        <div style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>기존 JSON 샘플을 붙여넣으면 PulseOne 템플릿으로 자동 변환합니다.</div>
                                                    </div>
                                                </div>
                                                <button
                                                    type="button"
                                                    className="mgmt-btn mgmt-btn-outline"
                                                    style={{ height: '36px', fontSize: '12px', width: 'auto', padding: '0 15px', borderColor: 'var(--primary-300)', color: 'var(--primary-600)' }}
                                                    onClick={() => setIsImportVisible(!isImportVisible)}
                                                >
                                                    {isImportVisible ? '숨기기' : '샘플 붙여넣기'}
                                                </button>
                                            </div>

                                            {isImportVisible && (
                                                <div style={{ marginTop: '15px', background: '#fff', border: '1px solid var(--primary-200)', borderRadius: '10px', padding: '15px', boxShadow: '0 4px 12px rgba(0,0,0,0.05)' }}>
                                                    <textarea
                                                        className="mgmt-input"
                                                        style={{ height: '120px', fontFamily: '"Fira Code", monospace', fontSize: '12px', marginBottom: '10px' }}
                                                        placeholder='[{"bd":9, "ty":"num", ...}] 형태의 JSON 샘플을 여기에 붙여넣으세요.'
                                                        value={sampleInput}
                                                        onChange={e => setSampleInput(e.target.value)}
                                                    />
                                                    <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '10px' }}>
                                                        <button type="button" className="mgmt-btn mgmt-btn-outline" style={{ height: '32px', fontSize: '12px', width: '80px' }} onClick={() => setIsImportVisible(false)}>취소</button>
                                                        <button type="button" className="mgmt-btn btn-primary" style={{ height: '32px', fontSize: '12px', width: '100px' }} onClick={handleImportFromSample}>분석 및 적용</button>
                                                    </div>
                                                </div>
                                            )}
                                        </div>

                                        {/* Name & System Type Row: Softer Inputs */}
                                        <div style={{ display: 'flex', gap: '20px', marginBottom: '25px' }}>
                                            <div style={{ flex: 2 }}>
                                                <label style={{ fontSize: '12px', fontWeight: 700, color: 'var(--neutral-500)', marginBottom: '6px', display: 'block' }}>템플릿 명칭</label>
                                                <input
                                                    type="text"
                                                    className="mgmt-input"
                                                    required
                                                    style={{ height: '44px', fontSize: '14px', borderRadius: '10px', background: 'var(--neutral-50)', border: '1px solid var(--neutral-200)' }}
                                                    value={editingTemplate?.name || ''}
                                                    onChange={e => { setEditingTemplate({ ...editingTemplate, name: e.target.value }); setHasChanges(true); }}
                                                    placeholder="예: 표준 JSON 페이로드"
                                                />
                                            </div>
                                            <div style={{ flex: 1 }}>
                                                <label style={{ fontSize: '12px', fontWeight: 700, color: 'var(--neutral-500)', marginBottom: '6px', display: 'block' }}>시스템 유형</label>
                                                <input
                                                    type="text"
                                                    className="mgmt-input"
                                                    style={{ height: '44px', fontSize: '14px', borderRadius: '10px', background: 'var(--neutral-50)', border: '1px solid var(--neutral-200)' }}
                                                    value={editingTemplate?.system_type || ''}
                                                    onChange={e => { setEditingTemplate({ ...editingTemplate, system_type: e.target.value }); setHasChanges(true); }}
                                                    placeholder="Insite, AWS 등"
                                                />
                                            </div>
                                            {isAdmin && !tenantId && (
                                                <div style={{ flex: 1 }}>
                                                    <label style={{ fontSize: '12px', fontWeight: 700, color: 'var(--neutral-500)', marginBottom: '6px', display: 'block' }}>소속 테넌트 <span style={{ color: 'red' }}>*</span></label>
                                                    <select
                                                        className="mgmt-select"
                                                        required
                                                        style={{ height: '44px', fontSize: '14px', borderRadius: '10px', background: 'var(--neutral-50)', border: '1px solid var(--neutral-200)', width: '100%' }}
                                                        value={editingTemplate?.tenant_id || ''}
                                                        onChange={e => { setEditingTemplate({ ...editingTemplate, tenant_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                                    >
                                                        <option value="">(테넌트 선택)</option>
                                                        {tenants.map(t => (
                                                            <option key={t.id} value={t.id}>{t.company_name}</option>
                                                        ))}
                                                    </select>
                                                </div>
                                            )}
                                        </div>

                                        {/* Editor Section Header */}
                                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
                                            <label style={{ margin: 0, fontWeight: 700, fontSize: '13px', color: 'var(--neutral-800)' }}>페이로드 구성 에디터</label>
                                            <div className="mgmt-toggle-group" style={{ background: 'var(--neutral-100)', padding: '3px', borderRadius: '8px', display: 'flex' }}>
                                                <button
                                                    type="button"
                                                    className={`mgmt-btn btn-xs ${editMode === 'simple' ? 'btn-primary' : 'mgmt-btn-flat'}`}
                                                    style={{ height: '28px', fontSize: '11px', padding: '0 12px', borderRadius: '6px', fontWeight: editMode === 'simple' ? 600 : 400 }}
                                                    onClick={() => {
                                                        if (editMode === 'advanced') {
                                                            syncAdvancedToSimple(typeof editingTemplate?.template_json === 'string' ? editingTemplate.template_json : JSON.stringify(editingTemplate?.template_json || {}));
                                                        }
                                                        setEditMode('simple');
                                                        setIsJsonValid(true);
                                                    }}
                                                >빌더 (Simple)</button>
                                                <button
                                                    type="button"
                                                    className={`mgmt-btn btn-xs ${editMode === 'advanced' ? 'btn-primary' : 'mgmt-btn-flat'}`}
                                                    style={{ height: '28px', fontSize: '11px', padding: '0 12px', borderRadius: '6px', fontWeight: editMode === 'advanced' ? 600 : 400 }}
                                                    onClick={() => setEditMode('advanced')}
                                                >코드 (Advanced)</button>
                                            </div>
                                        </div>

                                        {/* Main Editor Content */}
                                        <div style={{ marginBottom: '25px' }}>
                                            {editMode === 'simple' ? (
                                                <div style={{ border: '1px solid var(--neutral-200)', borderRadius: '10px', overflow: 'hidden', background: '#fff' }}>
                                                    <div style={{ padding: '10px 15px', background: 'var(--neutral-50)', borderBottom: '1px solid var(--neutral-200)', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                        <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>빌더 설정</span>
                                                        <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', margin: 0 }}>
                                                            <input
                                                                type="checkbox"
                                                                checked={isWrappedInArray}
                                                                onChange={e => { setIsWrappedInArray(e.target.checked); setHasChanges(true); }}
                                                                style={{ width: '14px', height: '14px' }}
                                                            />
                                                            <span style={{ fontSize: '12px', color: 'var(--neutral-700)', fontWeight: 600 }}>결과물을 배열 ([ ])로 감싸기</span>
                                                        </label>
                                                    </div>
                                                    <table className="mgmt-table" style={{ margin: 0, tableLayout: 'fixed', width: '100%' }}>
                                                        <thead style={{ background: 'var(--neutral-50)' }}>
                                                            <tr>
                                                                <th style={{ padding: '10px 15px', fontSize: '12px', fontWeight: 600, width: '40%' }}>JSON 키 (Key)</th>
                                                                <th style={{ padding: '10px 15px', fontSize: '12px', fontWeight: 600, width: '50%' }}>데이터 값 (Value)</th>
                                                                <th style={{ padding: '10px 15px', width: '50px' }}></th>
                                                            </tr>
                                                        </thead>
                                                        <tbody>
                                                            {simpleMappings.map((m, idx) => (
                                                                <tr key={idx}>
                                                                    <td style={{ padding: '8px 15px' }}>
                                                                        <input
                                                                            id={`mapping-key-${idx}`}
                                                                            className="mgmt-input"
                                                                            style={{ height: '36px', fontSize: '13px' }}
                                                                            value={m.key}
                                                                            onChange={e => {
                                                                                const newMappings = [...simpleMappings];
                                                                                newMappings[idx].key = e.target.value;
                                                                                setSimpleMappings(newMappings);
                                                                                setHasChanges(true);
                                                                            }}
                                                                            onFocus={() => setLastFocusedElement({ id: `mapping-key-${idx}`, index: idx, field: 'key' })}
                                                                            placeholder="key"
                                                                        />
                                                                    </td>
                                                                    <td style={{ padding: '8px 15px' }}>
                                                                        <input
                                                                            id={`mapping-value-${idx}`}
                                                                            className="mgmt-input"
                                                                            style={{ height: '36px', fontSize: '13px' }}
                                                                            value={m.value}
                                                                            onChange={e => {
                                                                                const newMappings = [...simpleMappings];
                                                                                newMappings[idx].value = e.target.value;
                                                                                setSimpleMappings(newMappings);
                                                                                setHasChanges(true);
                                                                            }}
                                                                            onFocus={() => setLastFocusedElement({ id: `mapping-value-${idx}`, index: idx, field: 'value' })}
                                                                            placeholder="텍스트 또는 변수 클릭"
                                                                        />
                                                                    </td>
                                                                    <td style={{ padding: '8px 15px', textAlign: 'center' }}>
                                                                        <button type="button" className="mgmt-btn-icon" style={{ fontSize: '18px', color: 'var(--neutral-400)' }} onClick={() => { setSimpleMappings(simpleMappings.filter((_, i) => i !== idx)); setHasChanges(true); }}>&times;</button>
                                                                    </td>
                                                                </tr>
                                                            ))}
                                                            <tr>
                                                                <td colSpan={3} style={{ padding: '15px' }}>
                                                                    <button type="button" className="mgmt-btn mgmt-btn-outline" style={{ width: '100%', height: '36px', fontSize: '13px', borderStyle: 'dashed', background: 'var(--neutral-50)' }} onClick={() => setSimpleMappings([...simpleMappings, { key: '', value: '' }])}>
                                                                        <i className="fas fa-plus" style={{ marginRight: '8px' }} /> 필드 추가
                                                                    </button>
                                                                </td>
                                                            </tr>
                                                        </tbody>
                                                    </table>
                                                </div>
                                            ) : (
                                                <div style={{ position: 'relative' }}>
                                                    <textarea
                                                        id="template-textarea"
                                                        className={`mgmt-input ${!isJsonValid ? 'error' : ''}`}
                                                        required
                                                        value={typeof editingTemplate?.template_json === 'string' ? editingTemplate.template_json : JSON.stringify(editingTemplate?.template_json || {})}
                                                        onChange={e => {
                                                            const val = e.target.value;
                                                            setEditingTemplate({ ...editingTemplate, template_json: val });
                                                            setHasChanges(true);
                                                            try {
                                                                // [v3.6.11] Lax validation: support unquoted placeholders for numeric/array types
                                                                const placeholderFree = val
                                                                    .replace(/\{\{[^}]+\}\}/g, '0')
                                                                    .replace(/\{[^}]+\}/g, '0');
                                                                JSON.parse(placeholderFree);
                                                                setIsJsonValid(true);
                                                            } catch {
                                                                setIsJsonValid(false);
                                                            }
                                                        }}
                                                        style={{
                                                            height: '350px',
                                                            fontFamily: '"Fira Code", monospace',
                                                            fontSize: '13px',
                                                            padding: '15px',
                                                            background: '#fafafa',
                                                            border: isJsonValid ? '1px solid var(--neutral-300)' : '2px solid var(--danger-500)',
                                                            borderRadius: '10px',
                                                            lineHeight: '1.6'
                                                        }}
                                                    />
                                                    {!isJsonValid && (
                                                        <div style={{ position: 'absolute', bottom: '15px', right: '15px', background: 'var(--danger-500)', color: '#fff', padding: '4px 10px', borderRadius: '4px', fontSize: '11px', fontWeight: 600 }}>
                                                            JSON 문법 오류
                                                        </div>
                                                    )}
                                                </div>
                                            )}
                                        </div>

                                        {/* Real-time Preview Section */}
                                        <div>
                                            <div style={{ fontSize: '13px', color: 'var(--neutral-800)', marginBottom: '10px', display: 'flex', justifyContent: 'space-between', fontWeight: 700 }}>
                                                <span>실시간 전송 데이터 미리보기</span>
                                                <span style={{ fontSize: '11px', padding: '2px 8px', borderRadius: '4px', background: isJsonValid ? 'var(--success-100)' : 'var(--danger-100)', color: isJsonValid ? 'var(--success-700)' : 'var(--danger-700)' }}>
                                                    {isJsonValid ? 'VALID JSON' : 'INVALID'}
                                                </span>
                                            </div>
                                            <div style={{ background: '#1e272e', borderRadius: '10px', padding: '20px', border: '1px solid #10171d', boxShadow: 'inset 0 2px 10px rgba(0,0,0,0.2)' }}>
                                                <pre style={{ margin: 0, fontSize: '12px', color: '#55efc4', whiteSpace: 'pre-wrap', lineHeight: '1.6', fontFamily: '"Fira Code", monospace' }}>
                                                    {getSafePreview(editingTemplate?.template_json, editMode, simpleMappings, isWrappedInArray)}
                                                </pre>
                                            </div>
                                        </div>

                                        {/* Advanced Usage Guide (Collapsed/Small) */}
                                        <div style={{ marginTop: '25px', padding: '12px', background: 'var(--neutral-50)', borderRadius: '8px', border: '1px solid var(--neutral-200)' }}>
                                            <div style={{ fontSize: '11px', color: 'var(--neutral-600)', fontWeight: 700, marginBottom: '6px' }}>스마트 매핑 ({'{'}map{'}'}) 고급 구문</div>
                                            <div style={{ fontSize: '10px', color: 'var(--neutral-500)', lineHeight: '1.5' }}>
                                                <code>{'{'}{'{'}map:변수명:참값:거짓값{'}'}{'}'}</code> &rarr; 변수가 1(참)이면 참값을, 0(거짓)이면 거짓값을 출력합니다.<br />
                                                예: <code>{'{'}{'{'}map:alarm_level:ALARM:NORMAL{'}'}{'}'}</code> &rarr; 알람 시 "ALARM", 평상시 "NORMAL" 문자열 출력
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer" style={{ padding: '15px 20px', borderTop: '1px solid var(--neutral-200)', display: 'flex', gap: '10px', justifyContent: 'flex-end' }}>
                                <button type="button" className="mgmt-btn mgmt-btn-outline" style={{ height: '40px', fontSize: '14px', width: '100px' }} onClick={handleCloseModal}>취소</button>
                                <button type="submit" className={`mgmt-btn btn-primary ${editMode === 'advanced' && !isJsonValid ? 'disabled' : ''}`} disabled={editMode === 'advanced' && !isJsonValid} style={{ height: '40px', fontSize: '14px', minWidth: '120px' }}>템플릿 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default TemplateManagementTab;
