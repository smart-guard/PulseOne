import React, { useState, useEffect } from 'react';
import { Card, Space, Select, Button, Tag, Modal, Input, Radio } from 'antd';
import exportGatewayApi, { Gateway, Assignment, ExportTarget, DataPoint, PayloadTemplate } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';


// Helper if utility not imported
const extractItemsRef = (data: any) => {
    if (Array.isArray(data)) return data;
    if (data && Array.isArray(data.items)) return data.items;
    if (data && data.data && Array.isArray(data.data.items)) return data.data.items;
    return [];
}

const getLocalISOString = () => {
    const now = new Date();
    const offset = now.getTimezoneOffset() * 60000;
    const localIso = new Date(now.getTime() - offset).toISOString();
    return `${localIso.replace('T', ' ').split('.')[0]}.000`;
};

const ManualTestTab: React.FC = () => {
    const [gateways, setGateways] = useState<Gateway[]>([]);
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [assignments, setAssignments] = useState<Record<number, Assignment[]>>({});

    const [selectedGatewayId, setSelectedGatewayId] = useState<number | null>(null);
    const [testMappings, setTestMappings] = useState<any[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [currentMapping, setCurrentMapping] = useState<any>(null);
    const [testValue, setTestValue] = useState<string | number>('');
    const [testAlValue, setTestAlValue] = useState<number>(0);
    const [showJsonPreview, setShowJsonPreview] = useState(false);
    const [editableJson, setEditableJson] = useState<string>('');

    const { confirm } = useConfirmContext();

    // Fetch dependencies on mount
    useEffect(() => {
        const fetchInitialData = async () => {
            try {
                const [gwRes, targetsRes, pointsRes, templatesRes] = await Promise.all([
                    exportGatewayApi.getGateways({ page: 1, limit: 100 }),
                    exportGatewayApi.getTargets(),
                    exportGatewayApi.getDataPoints(),
                    exportGatewayApi.getTemplates()
                ]);

                setGateways(gwRes.data?.items || []);
                targetsRes.data ? setTargets(extractItemsRef(targetsRes.data)) : setTargets([]);
                setTemplates(templatesRes.data || []);
                // DataPoints might be array or items
                if (pointsRes) {
                    if (Array.isArray(pointsRes)) setAllPoints(pointsRes);
                    else if ((pointsRes as any).items) setAllPoints((pointsRes as any).items);
                    else setAllPoints([]);
                }
            } catch (e) {
                console.error("ManualTestTab init failed:", e);
            }
        };
        fetchInitialData();
    }, []);

    // Specific logic to fetch assignments for a gateway when needed, IF not already fetched.
    // In original code, it passed `assignments` prop which had ALL assignments.
    // Here we will fetch assignments for the selected gateway on demand if we don't have them.
    const ensureAssignments = async (gwId: number) => {
        if (assignments[gwId]) return assignments[gwId];
        try {
            const res = await exportGatewayApi.getAssignments(gwId);
            const list = extractItemsRef(res.data);
            setAssignments(prev => ({ ...prev, [gwId]: list }));
            return list;
        } catch (e) {
            console.error(e);
            return [];
        }
    };

    // Dynamic Payload Generation based on Template
    useEffect(() => {
        if (!currentMapping) return;

        const val = typeof testValue === 'string' ? parseFloat(testValue) || 0 : testValue;

        // 1. Find the target to get the template_id
        // We use the first target in the list as the primary one for the template preview
        const firstTargetInfo = currentMapping.target_names?.[0];
        const targetId = firstTargetInfo?.id; // We need to ensure we pass ID in fetchGatewayMappings
        const target = targets.find(t => t.id === targetId);

        let templateJson: any = null;

        // 2. Find the template
        if (target && target.template_id) {
            const template = templates.find(t => t.id === target.template_id);
            if (template) {
                templateJson = template.template_json;
            }
        }

        // 3. Construct Payload
        let finalPayload: any;

        if (templateJson) {
            // Use Template Structure
            let jsonStr = typeof templateJson === 'string' ? templateJson : JSON.stringify(templateJson);

            // Basic Replacements (This makes it consistent with TemplateManagementTab preview)
            // Note: In a real C++ engine, this is done more robustly. Here we do simple string replacement for preview.

            // Find point to get data_type
            const point = allPoints.find(p => p.id === currentMapping.point_id);
            const dataType = point?.data_type || 'num';

            // Substitutions (Supporting both {} and {{}} for C++ Engine format)
            const substitutions: Record<string, any> = {
                'site_id': currentMapping.site_id || 1,
                'bd': currentMapping.site_id || 1,
                'point_name': point?.name || 'Unknown',
                'nm': point?.name || 'Unknown',
                'target_key': currentMapping.target_field_name,
                'measured_value': val,
                'vl': val,
                'timestamp': getLocalISOString(),
                'tm': getLocalISOString(),
                'status_code': 0,
                'st': 1,
                'alarm_level': testAlValue,
                'al': testAlValue,
                'description': "Manual Export Triggered",
                'des': "Manual Export Triggered",
                'target_description': currentMapping.target_description || "Manual Export Triggered",
                'type': dataType,
                'ty': dataType,
                'device_name': point?.device_name || '',
                'unit': point?.unit || '',
                'is_control': 1,
                'il': "-",
                'xl': "-",
                'mi': 0,
                'mx': 100
            };

            // Replace all placeholders
            Object.entries(substitutions).forEach(([key, value]) => {
                const escapedVal = String(value);

                // 1. Double braces with |array filter: {{key|array}} -> [value]
                const arrayPattern = new RegExp(`\\{\\{${key}\\|array\\}\\}`, 'g');
                jsonStr = jsonStr.replace(arrayPattern, `[${escapedVal}]`);

                // 2. Double braces {{key}}
                jsonStr = jsonStr.replace(new RegExp(`\\{\\{${key}\\}\\}`, 'g'), escapedVal);
                // 3. Single braces {key} (C++ Engine Format)
                jsonStr = jsonStr.replace(new RegExp(`\\{${key}\\}`, 'g'), escapedVal);
            });

            // Handle Smart Logic {{map:source:trueVal:falseVal}}
            // Regex: {{map:([^:]+):([^:]+):([^:]+)}}
            jsonStr = jsonStr.replace(/\{\{map:([^:]+):([^:]+):([^:]+)\}\}/g, (match, source, trueVal, falseVal) => {
                let sourceVal: any;

                // Determine source value based on variable name
                if (source === 'alarm_level' || source === 'al') sourceVal = testAlValue;
                else if (source === 'status_code' || source === 'st') sourceVal = 0; // Default normal
                else if (source === 'measured_value' || source === 'vl') sourceVal = val;
                else sourceVal = 0;

                // Logic: 0 is false, anything else is true
                const isTrue = Number(sourceVal) !== 0;
                return isTrue ? trueVal : falseVal;
            });

            try {
                finalPayload = JSON.parse(jsonStr);
            } catch (e) {
                console.error("Failed to parse template JSON after substitution", e);
                // Fallback if template is broken
                finalPayload = {
                    "error": "Template parsing failed",
                    "raw": jsonStr
                };
            }
        } else {
            // Fallback: Default Insite Structure (Object, No Array) - Matching the previous 'fix'
            finalPayload = {
                "bd": currentMapping.site_id || 1,
                "ty": "num",
                "nm": currentMapping.target_field_name,
                "vl": val,
                "il": "-",
                "xl": "-",
                "mi": [0],
                "mx": [100],
                "tm": getLocalISOString(),
                "st": 1,
                "al": testAlValue,
                "des": "Manual Export Triggered"
            };
        }

        setEditableJson(JSON.stringify(finalPayload, null, 4));
    }, [currentMapping, templates, targets, testValue, testAlValue, allPoints]);

    const fetchGatewayMappings = async (gwId: number) => {
        setLoading(true);
        try {
            // Ensure we have assignments
            const gwAssignments = await ensureAssignments(gwId);

            const profileIds = gwAssignments.map(a => String(a.profile_id));
            const linkedTargets = targets.filter(t => profileIds.includes(String(t.profile_id)));

            const pointGroups: Record<number, any> = {};
            await Promise.all(linkedTargets.map(async (target) => {
                const res = await exportGatewayApi.getTargetMappings(target.id);
                const items = extractItemsRef(res.data);
                items.forEach((m: any) => {
                    if (!pointGroups[m.point_id]) {
                        pointGroups[m.point_id] = {
                            ...m,
                            target_names: []
                        };
                    }
                    if (!pointGroups[m.point_id].target_names.some((tn: any) => tn.name === target.name)) {
                        pointGroups[m.point_id].target_names.push({
                            id: target.id,  // Add ID so we can lookup template later
                            name: target.name,
                            type: target.target_type
                        });
                    }
                });
            }));

            const sortedMappings = Object.values(pointGroups).sort((a: any, b: any) => {
                const nameA = allPoints.find(p => p.id === a.point_id)?.name || '';
                const nameB = allPoints.find(p => p.id === b.point_id)?.name || '';
                return nameA.localeCompare(nameB);
            });

            setTestMappings(sortedMappings);
        } catch (error) {
            console.error('Failed to fetch mappings for manual test:', error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        if (selectedGatewayId) {
            fetchGatewayMappings(selectedGatewayId);
        } else {
            setTestMappings([]);
        }
    }, [selectedGatewayId]);

    const handleOpenTest = (mapping: any) => {
        setCurrentMapping(mapping);
        const point = allPoints.find(p => p.id === mapping.point_id);
        const latestInfo = (point as any)?.latest_value;
        const initialVal = latestInfo !== undefined ? latestInfo : 0;
        setTestValue(initialVal);
        setTestAlValue(0);
        setIsModalOpen(true);
    };

    const isSubmittingRef = React.useRef(false);

    const handleSendTest = async () => {
        if (!currentMapping || !selectedGatewayId || isSubmittingRef.current) return;

        isSubmittingRef.current = true;
        setLoading(true);
        try {
            let overrides: any = {};
            try {
                const parsed = JSON.parse(editableJson);
                const first = Array.isArray(parsed) ? parsed[0] : parsed;
                if (first) {
                    overrides = {
                        al: first.al,
                        st: first.st,
                        des: first.des,
                        ty: first.ty,
                        il: first.il,
                        xl: first.xl
                    };
                }
            } catch (e) {
                console.error("Failed to parse JSON for overrides", e);
            }

            const res = await exportGatewayApi.triggerManualExport(selectedGatewayId, {
                target_name: "ALL",
                point_id: currentMapping.point_id,
                value: typeof testValue === 'string' ? parseFloat(testValue) : testValue,
                al: testAlValue,
                raw_payload: JSON.parse(editableJson),
                ...overrides
            });

            if (res.success) {
                await confirm({
                    title: '전송 요청 성공',
                    message: `데이터 "${testValue}"가 관련 타겟(${currentMapping.target_names.map((t: any) => t.name).join(', ')})으로 전송 요청되었습니다.`,
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
            } else {
                await confirm({
                    title: '전송 실패',
                    message: res.message || '데이터 전송에 실패했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: '에러',
                message: '수동 전송 요청 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        } finally {
            setLoading(false);
            isSubmittingRef.current = false;
        }
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '20px', flex: 1, overflow: 'auto', paddingBottom: '30px' }}>
            <Card style={{ padding: '24px 24px 40px 24px', minHeight: 'fit-content' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px' }}>
                    <div>
                        <h3 style={{ margin: 0, fontSize: '18px', fontWeight: 700 }}>수동 데이터 전송 테스트</h3>
                        <p style={{ margin: '4px 0 0', color: 'var(--neutral-500)', fontSize: '13px' }}>등록된 게이트웨이의 매핑 정보를 기반으로 데이터를 수동으로 전송해 봅니다.</p>
                    </div>
                    <Space size="large">
                        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                            <span style={{ fontWeight: 600, fontSize: '14px' }}>게이트웨이 선택:</span>
                            <Select
                                style={{ width: 250 }}
                                placeholder="게이트웨이를 선택하세요"
                                value={selectedGatewayId}
                                onChange={setSelectedGatewayId}
                                size="large"
                            >
                                {gateways.map(gw => (
                                    <Select.Option key={gw.id} value={gw.id}>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <i className="fas fa-server" style={{ color: gw.live_status?.status === 'online' ? 'var(--success-500)' : 'var(--neutral-400)' }} />
                                            {gw.name}
                                        </div>
                                    </Select.Option>
                                ))}
                            </Select>
                        </div>
                        <Button
                            icon={<i className="fas fa-sync-alt" />}
                            onClick={() => selectedGatewayId && fetchGatewayMappings(selectedGatewayId)}
                            disabled={!selectedGatewayId || loading}
                        >
                            새로고침
                        </Button>
                    </Space>
                </div>

                {!selectedGatewayId ? (
                    <div style={{ padding: '100px', textAlign: 'center', background: 'var(--neutral-50)', borderRadius: '12px', border: '1px dashed var(--neutral-300)' }}>
                        <i className="fas fa-mouse-pointer" style={{ fontSize: '48px', color: 'var(--neutral-300)', marginBottom: '16px' }} />
                        <div style={{ fontSize: '15px', color: 'var(--neutral-500)', fontWeight: 600 }}>테스트를 진행할 게이트웨이를 선택해 주세요.</div>
                    </div>
                ) : loading && testMappings.length === 0 ? (
                    <div style={{ textAlign: 'center', padding: '100px' }}>
                        <i className="fas fa-spinner fa-spin fa-2x" style={{ color: 'var(--primary-500)' }} />
                        <p style={{ marginTop: '16px', color: 'var(--neutral-500)' }}>매핑 정보 로딩 중...</p>
                    </div>
                ) : (
                    <div style={{
                        border: '1px solid var(--neutral-200)',
                        borderRadius: '12px',
                        overflowX: 'auto',
                        overflowY: 'scroll',
                        background: 'white',
                        maxHeight: '380px',
                        boxShadow: '0 2px 10px rgba(0,0,0,0.05)',
                        position: 'relative',
                        marginBottom: '30px'
                    }}>
                        <table style={{ width: '100%', borderCollapse: 'collapse', minWidth: '800px' }}>
                            <thead style={{ background: 'var(--neutral-100)', position: 'sticky', top: 0, zIndex: 10 }}>
                                <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--neutral-200)' }}>
                                    <th style={{ padding: '12px 16px', background: 'var(--neutral-100)' }}>포인트 정보</th>
                                    <th style={{ padding: '12px 16px', background: 'var(--neutral-100)' }}>전송 대상 (Multi-Target)</th>
                                    <th style={{ padding: '12px 16px', background: 'var(--neutral-100)' }}>전송 필드명</th>
                                    <th style={{ padding: '12px 16px', background: 'var(--neutral-100)' }}>Building ID</th>
                                    <th style={{ padding: '12px 16px', background: 'var(--neutral-100)', textAlign: 'center' }}>조작</th>
                                </tr>
                            </thead>
                            <tbody>
                                {testMappings.map((m, idx) => {
                                    const point = allPoints.find(p => p.id === m.point_id);
                                    return (
                                        <tr key={idx} style={{ borderBottom: '1px solid var(--neutral-50)', transition: 'background 0.2s' }}>
                                            <td style={{ padding: '12px 16px' }}>
                                                <div style={{ fontWeight: 600, color: 'var(--primary-600)' }}>{point?.name || `Point #${m.point_id}`}</div>
                                                <div style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>{point?.device_name}</div>
                                            </td>
                                            <td style={{ padding: '12px 16px' }}>
                                                <div style={{ fontWeight: 600, color: 'var(--neutral-800)', marginBottom: '4px' }}>
                                                    {m.target_names?.[0]?.name?.split('(')[0]?.trim() || 'Unknown Target'}
                                                    {m.target_names?.length > 1 && <span style={{ color: 'var(--neutral-400)', fontSize: '12px', fontWeight: 400, marginLeft: '4px' }}>외 {m.target_names.length - 1}건</span>}
                                                </div>
                                                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                                                    {m.target_names && m.target_names.map((tn: any, i: number) => (
                                                        <Tag key={i} color="blue" style={{ fontSize: '10px', margin: 0, padding: '0 4px', borderRadius: '4px' }}>
                                                            {tn.type}
                                                        </Tag>
                                                    ))}
                                                </div>
                                            </td>
                                            <td style={{ padding: '12px 16px' }}><code>{m.target_field_name}</code></td>
                                            <td style={{ padding: '12px 16px' }}><Tag>{m.site_id || '-'}</Tag></td>
                                            <td style={{ padding: '12px 16px', textAlign: 'center' }}>
                                                <Button
                                                    type="primary"
                                                    size="small"
                                                    icon={<i className="fas fa-paper-plane" />}
                                                    onClick={() => handleOpenTest(m)}
                                                >
                                                    전송 테스트
                                                </Button>
                                            </td>
                                        </tr>
                                    );
                                })}
                                {testMappings.length === 0 && (
                                    <tr>
                                        <td colSpan={5} style={{ padding: '60px', textAlign: 'center', color: 'var(--neutral-400)' }}>
                                            할당된 매핑 정보가 없습니다. 해당 게이트웨이의 데이터 매핑 설정을 확인해 주세요.
                                        </td>
                                    </tr>
                                )}
                            </tbody>
                        </table>
                    </div>
                )
                }
            </Card>

            <Modal
                title={
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', width: '95%' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                            <i className="fas fa-flask" style={{ color: 'var(--primary-500)' }} />
                            <span>데이터 전송 테스트 (Multi-Target)</span>
                        </div>
                        <Button
                            size="small"
                            icon={<i className="fas fa-code" />}
                            onClick={() => setShowJsonPreview(!showJsonPreview)}
                            type={showJsonPreview ? "primary" : "default"}
                        >
                            전송 데이터 확인
                        </Button>
                    </div>
                }
                open={isModalOpen}
                onCancel={() => setIsModalOpen(false)}
                footer={[
                    <Button key="cancel" onClick={() => setIsModalOpen(false)}>취소</Button>,
                    <Button key="send" type="primary" icon={<i className="fas fa-paper-plane" />} loading={loading} onClick={handleSendTest}>
                        모든 타겟으로 전송
                    </Button>
                ]}
                width={showJsonPreview ? 800 : 450}
                centered
                destroyOnClose
            >
                {currentMapping && (
                    <div style={{ display: 'flex', flexDirection: showJsonPreview ? 'row' : 'column', gap: '20px', padding: '10px 0' }}>
                        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '16px' }}>
                            <div style={{ background: 'var(--neutral-50)', padding: '16px', borderRadius: '8px', fontSize: '13px' }}>
                                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                                    <span style={{ color: 'var(--neutral-500)' }}>대상 타겟:</span>
                                    <span style={{ fontWeight: 600, textAlign: 'right' }}>
                                        {currentMapping.target_names && currentMapping.target_names.map((t: any) => t.name).join(', ')}
                                    </span>
                                </div>
                                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                                    <span style={{ color: 'var(--neutral-500)' }}>전송 필드:</span>
                                    <span style={{ fontWeight: 600, color: 'var(--primary-600)' }}>{currentMapping.target_field_name}</span>
                                </div>
                                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                    <span style={{ color: 'var(--neutral-500)' }}>원본 포인트:</span>
                                    <span style={{ fontWeight: 600 }}>{allPoints.find(p => p.id === currentMapping.point_id)?.name || currentMapping.point_id}</span>
                                </div>
                            </div>

                            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                <label style={{ fontWeight: 700 }}>전송할 데이터 값 (Override)</label>
                                <Input
                                    size="large"
                                    type="number"
                                    value={testValue}
                                    onChange={e => setTestValue(e.target.value)}
                                    placeholder="숫자 값을 입력하세요"
                                    style={{ width: '100%' }}
                                    autoFocus
                                />
                                <div style={{ fontSize: '12px', color: '#94a3b8' }}>
                                    실제 포인트의 최근 값을 불러왔습니다. 변경하여 전송할 수 있습니다.
                                </div>
                            </div>

                            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                <label style={{ fontWeight: 700 }}>알람 상태 값 (al Override)</label>
                                <Radio.Group
                                    value={testAlValue}
                                    onChange={e => setTestAlValue(e.target.value)}
                                    buttonStyle="solid"
                                    size="large"
                                >
                                    <Radio.Button value={0}>0 (정상)</Radio.Button>
                                    <Radio.Button value={1}>1 (알람)</Radio.Button>
                                    <Radio.Button value={2}>2 (주의)</Radio.Button>
                                </Radio.Group>
                                <div style={{ fontSize: '12px', color: '#94a3b8' }}>
                                    알람 상태(al) 값을 강제로 지정하여 전송합니다.
                                </div>
                            </div>
                        </div>

                        {showJsonPreview && (
                            <div style={{ flex: 1, borderLeft: '1px solid var(--neutral-200)', paddingLeft: '20px' }}>
                                <div style={{ fontWeight: 700, marginBottom: '8px', fontSize: '12px', color: 'var(--neutral-500)', textTransform: 'uppercase' }}>
                                    전송 페이로드 미리보기 (C++ ENGINE FORMAT)
                                </div>
                                <div style={{ position: 'relative' }}>
                                    <Input.TextArea
                                        value={editableJson}
                                        onChange={e => setEditableJson(e.target.value)}
                                        rows={15}
                                        style={{
                                            background: '#1e1e1e',
                                            color: '#d4d4d4',
                                            padding: '16px',
                                            borderRadius: '8px',
                                            fontFamily: 'monospace',
                                            fontSize: '12px',
                                            border: 'none',
                                            resize: 'none'
                                        }}
                                        spellCheck={false}
                                    />
                                    <div style={{ position: 'absolute', top: '10px', right: '10px', display: 'flex', gap: '8px' }}>
                                        <Tag color="blue" style={{ margin: 0, opacity: 0.7, fontSize: '10px' }}>EDITABLE</Tag>
                                    </div>
                                </div>
                                <div style={{ marginTop: '12px', fontSize: '11px', color: 'var(--neutral-400)' }}>
                                    * 위 JSON은 C++ 엔진에서 타겟으로 전송할 때 사용하는 기본 데이터 구조입니다. 템플릿 설정에 따라 최종 형태는 달라질 수 있습니다.
                                </div>
                            </div>
                        )}
                    </div>
                )}
            </Modal>
        </div >
    );
};

export default ManualTestTab;
