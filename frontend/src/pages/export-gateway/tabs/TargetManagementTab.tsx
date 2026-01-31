import React, { useState, useEffect } from 'react';
import { Select, Tooltip, Input, Button } from 'antd';
import exportGatewayApi, { ExportTarget, ExportProfile, Assignment, DataPoint, PayloadTemplate, ExportTargetMapping } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

// Local helper for extracting items from API response
const extractItems = (data: any) => {
    if (Array.isArray(data)) return data;
    if (data && Array.isArray(data.items)) return data.items;
    if (data && data.data && Array.isArray(data.data.items)) return data.data.items;
    return [];
};

// [NEW] Helper to extract a single config object from various formats (object or [object])
const getConfigObject = (config: any): any => {
    try {
        const parsed = typeof config === 'string' ? JSON.parse(config) : (config || {});
        // If it's an array, take the first element (common legacy pattern in HDCL-CSP)
        if (Array.isArray(parsed)) return parsed[0] || {};
        return parsed;
    } catch {
        return {};
    }
};

// [NEW] Helper to mask sensitive keys in a config object
const maskSensitiveData = (config: any): any => {
    const obj = getConfigObject(config);
    const masked = JSON.parse(JSON.stringify(obj)); // Deep clone
    const sensitiveKeys = [
        'AccessKeyID', 'SecretAccessKey', 'apiKey', 'Authorization',
        'x-api-key', 'token', 'password', 'secret', 'secretKey'
    ];

    const maskValue = (o: any) => {
        if (!o || typeof o !== 'object') return;
        Object.keys(o).forEach(key => {
            if (sensitiveKeys.some(sk => sk.toLowerCase() === key.toLowerCase())) {
                if (typeof o[key] === 'string' && o[key].length > 0) {
                    o[key] = '********';
                }
            } else if (typeof o[key] === 'object') {
                maskValue(o[key]);
            }
        });
    };

    maskValue(masked);

    // Explicitly remove obsolete fields from preview
    delete masked.execution_order;

    return masked;
};

// [NEW] Helper to cleanup config for saving
const cleanupConfig = (config: any): any => {
    const obj = getConfigObject(config);
    // Remove obsolete fields
    delete obj.execution_order;
    return obj;
};

const TargetManagementTab: React.FC = () => {
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [profiles, setProfiles] = useState<ExportProfile[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingTarget, setEditingTarget] = useState<Partial<ExportTarget> | null>(null);
    const [isTesting, setIsTesting] = useState(false);
    const [hasChanges, setHasChanges] = useState(false);

    // Mapping state
    const [isMappingModalOpen, setIsMappingModalOpen] = useState(false);
    const [mappingTargetId, setMappingTargetId] = useState<number | null>(null);
    const [hasMappingChanges, setHasMappingChanges] = useState(false);
    // Extend type to support temporary invalid state
    const [targetMappings, setTargetMappings] = useState<(Partial<ExportTargetMapping> & { _temp_name?: string })[]>([]);
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [pasteData, setPasteData] = useState('');
    const [bulkSiteIdForMapping, setBulkSiteIdForMapping] = useState(''); // [NEW] Bulk Site ID state

    const { confirm } = useConfirmContext();

    const fetchData = async () => {
        setLoading(true);
        try {
            const [targetsRes, templatesRes, pointsRes, profilesRes] = await Promise.all([
                exportGatewayApi.getTargets(),
                exportGatewayApi.getTemplates(),
                exportGatewayApi.getDataPoints(),
                exportGatewayApi.getProfiles()
            ]);
            setTargets(extractItems(targetsRes.data));
            setTemplates(extractItems(templatesRes.data));
            setAllPoints(Array.isArray(pointsRes) ? pointsRes : (pointsRes as any)?.items || []);
            setProfiles(extractItems(profilesRes.data));
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    const handleOpenMapping = async (targetId: number) => {
        setMappingTargetId(targetId);
        setIsMappingModalOpen(true);
        setLoading(true);
        try {
            const response = await exportGatewayApi.getTargetMappings(targetId);
            setTargetMappings(extractItems(response.data));
            setHasMappingChanges(false);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    const handleCloseMapping = async () => {
        if (hasMappingChanges) {
            const confirmed = await confirm({
                title: '매핑 변경사항 유실 주의',
                message: '저장하지 않은 매핑 변경사항이 있습니다. 정말 닫으시겠습니까?',
                confirmText: '닫기',
                cancelText: '취소',
                confirmButtonType: 'warning'
            });
            if (!confirmed) return;
        }
        setIsMappingModalOpen(false);
        setMappingTargetId(null);
        setTargetMappings([]);
        setHasMappingChanges(false);
        setPasteData(''); // Clear paste area
    };

    const handleSaveMappings = async () => {
        if (!mappingTargetId) return;

        // Validation for missing points
        const invalidMappings = targetMappings.filter(m => !m.point_id);
        if (invalidMappings.length > 0) {
            await confirm({
                title: '매핑 오류',
                message: `${invalidMappings.length}개의 항목이 데이터 포인트와 연결되지 않았습니다. 삭제하거나 포인트를 선택해주세요.`,
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
            return;
        }

        try {
            const mappingsPayload = targetMappings.map(m => ({
                point_id: m.point_id!,
                target_field_name: m.target_field_name || '',
                target_description: m.target_description || '',
                site_id: m.site_id, // [FIX] Include site_id
                start_bit: m.start_bit,
                bit_length: m.bit_length,
                is_enabled: m.is_enabled !== false,
                conversion_config: m.conversion_config // { scale, offset }
            }));

            const response = await exportGatewayApi.updateTargetMappings(mappingTargetId, mappingsPayload);

            if (response.success) {
                await confirm({
                    title: '매핑 저장 완료',
                    message: '데이터 포인트 매핑이 성공적으로 저장되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsMappingModalOpen(false);
                setHasMappingChanges(false);
            } else {
                throw new Error(response.message);
            }
        } catch (error: any) {
            await confirm({
                title: '저장 실패',
                message: error.message || '매핑 정보를 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleTriggerManualExport = async (mapping: any) => {
        if (!mapping.point_id || !mappingTargetId) return;

        try {
            // Find current value if available
            const pointFn = allPoints.find(p => p.id === mapping.point_id);
            const val = pointFn ? (pointFn as any).latest_value : 0;

            const res = await exportGatewayApi.triggerManualExport(0, { // 0 = broadcast or handled by backend logic for target-specific
                target_id: mappingTargetId,
                point_id: mapping.point_id,
                value: val || 0
            });

            if (res.success) {
                await confirm({
                    title: '수동 전송 성공',
                    message: `포인트 "${pointFn?.name}"의 현재값(${val})이 전송되었습니다.`,
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
            } else {
                throw new Error(res.message);
            }
        } catch (e: any) {
            await confirm({
                title: '전송 실패',
                message: e.message || '수동 전송에 실패했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    }

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

    const handleImportFromProfile = async () => {
        if (!mappingTargetId) return;
        const target = targets.find(t => t.id === mappingTargetId);
        if (!target?.profile_id) {
            await confirm({
                title: '프로파일 없음',
                message: '이 타겟에는 연결된 프로파일이 없습니다. 먼저 타겟 설정에서 프로파일을 선택해주세요.',
                showCancelButton: false,
                confirmButtonType: 'warning'
            });
            return;
        }

        const profile = profiles.find(p => p.id === target.profile_id);
        if (!profile) return;

        const confirmed = await confirm({
            title: '프로파일 데이터 불러오기',
            message: `현재 설정된 매핑 리스트를 삭제하고, 프로파일 "${profile.name}"의 모든 포인트를 불러오시겠습니까?`,
            confirmText: '불러오기',
            cancelText: '취소',
            confirmButtonType: 'warning'
        });

        if (!confirmed) return;

        if (Array.isArray(profile.data_points)) {
            const initialMappings = profile.data_points.map((p: any) => ({
                point_id: p.id,
                site_id: p.site_id || p.building_id, // [RESTORE] site_id focus
                target_field_name: p.target_field_name || p.name,
                target_description: '',
                is_enabled: true,
                conversion_config: { scale: 1, offset: 0 }
            }));
            setTargetMappings(initialMappings);
            setHasMappingChanges(true);
            await confirm({
                title: '동기화 완료',
                message: '프로파일에서 포인트 목록을 성공적으로 가져왔습니다.',
                showCancelButton: false,
                confirmButtonType: 'success'
            });
        }
    };

    useEffect(() => { fetchData(); }, []);

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();

        if (!hasChanges && editingTarget?.id) {
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
            let response;
            if (editingTarget?.id) {
                // [CLEANUP] Ensure config is an object and stripped of obsolete fields
                const cleanTarget = {
                    ...editingTarget,
                    config: cleanupConfig(editingTarget.config)
                };
                response = await exportGatewayApi.updateTarget(editingTarget.id, cleanTarget);
            } else {
                const cleanTarget = {
                    ...editingTarget,
                    config: cleanupConfig(editingTarget.config)
                };
                response = await exportGatewayApi.createTarget(cleanTarget as ExportTarget);
            }

            if (response.success) {
                await confirm({
                    title: '저장 완료',
                    message: '전송 타겟 정보가 성공적으로 저장되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
                setHasChanges(false);
                fetchData();
            } else {
                await confirm({
                    title: '저장 실패',
                    message: response.message || '정보를 저장하는 중 오류가 발생했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: '저장 실패',
                message: '전송 타겟 정보를 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleTestConnection = async () => {
        if (!editingTarget?.target_type || !editingTarget?.config) {
            await confirm({ title: '입력 부족', message: '테스트할 정보가 설정되지 않았습니다.', showCancelButton: false, confirmButtonType: 'warning' });
            return;
        }

        setIsTesting(true);
        try {
            const response = await exportGatewayApi.testTargetConnection({
                target_type: editingTarget.target_type,
                config: editingTarget.config
            });

            if (response.success) {
                await confirm({
                    title: '테스트 성공',
                    message: response.message || '연결 테스트가 성공적으로 완료되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
            } else {
                await confirm({
                    title: '테스트 실패',
                    message: response.message || '알 수 없는 이유로 실패했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error: any) {
            await confirm({
                title: '테스트 오류',
                message: error.message || '서버와 통신 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        } finally {
            setIsTesting(false);
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '타겟 삭제 확인',
            message: '정말 이 전송 타겟을 삭제하시겠습니까?',
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteTarget(id);
            fetchData();
        } catch (error) {
            await confirm({ title: '삭제 실패', message: '타겟을 삭제하는 중 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>전송 타겟 설정</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingTarget({ target_type: 'http', is_enabled: true, config: {} }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> 타겟 추가
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>이름</th>
                            <th>타입</th>
                            <th>상태</th>
                            <th>관리</th>
                        </tr>
                    </thead>
                    <tbody>
                        {targets.map(t => (
                            <tr key={t.id}>
                                <td>{t.name}</td>
                                <td>
                                    <span className="mgmt-badge neutral" style={{ textTransform: 'uppercase' }}>
                                        {t.target_type}
                                    </span>
                                </td>
                                <td>
                                    <span className={`mgmt-badge ${t.is_enabled ? 'success' : 'neutral'}`}>
                                        {t.is_enabled ? '활성화' : '비활성'}
                                    </span>
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => handleOpenMapping(t.id)} style={{ width: 'auto' }}>매핑 설정</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => { setEditingTarget(t); setIsModalOpen(true); }} style={{ width: 'auto' }}>수정</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs mgmt-btn-error" onClick={() => handleDelete(t.id)} style={{ width: 'auto' }}>삭제</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-container" style={{ width: '95vw', maxWidth: '1600px', display: 'flex', flexDirection: 'column', height: '75vh', maxHeight: '85vh', background: 'white', borderRadius: '12px', boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.3)', overflow: 'hidden', border: '1px solid #e2e8f0' }}>
                        <div className="mgmt-modal-header">
                            <div className="mgmt-modal-title">
                                <h2 style={{ fontSize: '18px', fontWeight: 700, margin: 0 }}>{editingTarget?.id ? "전송 타겟 수정" : "전송 타겟 추가"}</h2>
                            </div>
                            <button className="mgmt-close-btn" onClick={handleCloseModal} style={{ fontSize: '24px' }}>&times;</button>
                        </div>
                        <form onSubmit={handleSave} style={{ display: 'flex', flexDirection: 'column', flex: 1, overflow: 'hidden' }}>
                            <div className="mgmt-modal-body" style={{ overflowY: 'auto', overflowX: 'hidden', flex: 1, paddingBottom: '20px' }}>
                                <div style={{ display: 'flex', gap: '20px', alignItems: 'flex-start', height: '100%' }}>
                                    {/* Column 1: Basic Info */}
                                    <div style={{ flex: 1, maxWidth: '400px', borderRight: '1px solid #eee', paddingRight: '20px', display: 'flex', flexDirection: 'column' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>기본 정보</div>
                                        <div className="mgmt-modal-form-group">
                                            <label>타겟 명칭</label>
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                required
                                                value={editingTarget?.name || ''}
                                                onChange={e => { setEditingTarget({ ...editingTarget, name: e.target.value }); setHasChanges(true); }}
                                                placeholder="예: 본사 관제 시스템"
                                            />
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>전송 프로토콜</label>
                                            <select
                                                className="mgmt-select"
                                                required
                                                value={editingTarget?.target_type || 'http'}
                                                onChange={e => { setEditingTarget({ ...editingTarget, target_type: e.target.value }); setHasChanges(true); }}
                                            >
                                                <option value="http">HTTP / HTTPS (Rest API)</option>
                                                <option value="mqtt">MQTT Broker</option>
                                                <option value="s3">AWS S3 / MinIO (File)</option>
                                                <option value="ftp">FTP / SFTP (File)</option>
                                                <option value="database">External Database (SQL)</option>
                                            </select>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>사용할 페이로드 템플릿</label>
                                            <select
                                                className="mgmt-select"
                                                required
                                                value={editingTarget?.template_id || ''}
                                                onChange={e => { setEditingTarget({ ...editingTarget, template_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                            >
                                                <option value="">(기본 형식 사용)</option>
                                                {templates.map(t => (
                                                    <option key={t.id} value={t.id}>{t.name} (v{t.version})</option>
                                                ))}
                                            </select>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>연결 프로파일 (데이터 소스)</label>
                                            <select
                                                className="mgmt-select"
                                                value={editingTarget?.profile_id || ''}
                                                onChange={e => { setEditingTarget({ ...editingTarget, profile_id: parseInt(e.target.value) || undefined }); setHasChanges(true); }}
                                            >
                                                <option value="">(연결된 프로파일 없음)</option>
                                                {profiles.map(p => (
                                                    <option key={p.id} value={p.id}>{p.name}</option>
                                                ))}
                                            </select>
                                            <div className="mgmt-modal-form-hint">프로파일을 연결하면 매핑 설정 시 포인트를 자동으로 불러올 수 있습니다.</div>
                                        </div>
                                    </div>

                                    {/* Column 2: Protocol Specific Config */}
                                    <div style={{ flex: 1, paddingRight: '20px', display: 'flex', flexDirection: 'column' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>전송 설정 상세</div>
                                        {editingTarget?.target_type?.toLowerCase() === 'http' ? (
                                            <>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Endpoint URL</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="http://api.example.com/v1/data"
                                                        value={getConfigObject(editingTarget.config).url || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, url: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>HTTP Method</label>
                                                    <select
                                                        className="mgmt-select"
                                                        value={(() => { try { return (typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : editingTarget.config).method || 'POST'; } catch { return 'POST'; } })()}
                                                        onChange={e => {
                                                            let c: any = {};
                                                            try { c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {}); } catch { }
                                                            c = { ...c, method: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    >
                                                        <option value="POST">POST</option>
                                                        <option value="PUT">PUT</option>
                                                        <option value="PATCH">PATCH</option>
                                                    </select>
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Headers (JSON)</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder='{"Content-Type": "application/json"}'
                                                        value={(() => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            return JSON.stringify(c.headers || {});
                                                        })()}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            try {
                                                                const newHeaders = JSON.parse(e.target.value);
                                                                const newConfig = { ...c, headers: newHeaders };
                                                                setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                                setHasChanges(true);
                                                            } catch { }
                                                        }}
                                                    />
                                                </div>
                                                {/* [NEW] Authorization Header (First) */}
                                                <div className="mgmt-modal-form-group">
                                                    <label>Authorization (Token / Signature)</label>
                                                    <Input.Password
                                                        className="mgmt-input"
                                                        placeholder="Bearer Token, AWS Signature, or custom auth value"
                                                        value={getConfigObject(editingTarget.config).headers?.['Authorization'] || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const headers = { ...(c.headers || {}) };
                                                            if (e.target.value) {
                                                                headers['Authorization'] = e.target.value;
                                                            } else {
                                                                delete headers['Authorization'];
                                                            }
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify({ ...c, headers }, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                {/* Access Key / Auth Token (Second) */}
                                                <div className="mgmt-modal-form-group">
                                                    <label>인증 키 (API Key / Token)</label>
                                                    <Input.Password
                                                        className="mgmt-input"
                                                        placeholder="x-api-key 또는 Authorization Bearer 토큰 입력"
                                                        value={(() => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            return c.auth?.apiKey || c.headers?.['x-api-key'] || '';
                                                        })()}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const auth = { ...(c.auth || { type: 'x-api-key' }), apiKey: e.target.value };
                                                            const headers = { ...(c.headers || {}) };

                                                            if (e.target.value) {
                                                                headers['x-api-key'] = e.target.value;
                                                            } else {
                                                                delete headers['x-api-key'];
                                                            }

                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify({ ...c, auth, headers }, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                    <div className="mgmt-modal-form-hint">입력 시 x-api-key 헤더 및 auth 설정에 자동 반영됩니다.</div>
                                                </div>
                                            </>
                                        ) : editingTarget?.target_type?.toLowerCase() === 's3' ? (
                                            <>
                                                <div className="mgmt-modal-form-group">
                                                    <label>S3 Service URL</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="https://s3.ap-northeast-2.amazonaws.com"
                                                        value={getConfigObject(editingTarget.config).S3ServiceUrl || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, S3ServiceUrl: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Bucket Name</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="my-export-bucket"
                                                        value={getConfigObject(editingTarget.config).BucketName || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, BucketName: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Folder Path</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="data/exports"
                                                        value={getConfigObject(editingTarget.config).Folder || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, Folder: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Object Key Template (Optional)</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="예: {year}/{month}/{day}/{hour}/{minute}/{second}_{building_id}.json"
                                                        value={getConfigObject(editingTarget.config).ObjectKeyTemplate || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, ObjectKeyTemplate: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                    <div className="mgmt-modal-form-hint">사용 가능 변수: {'{year}, {month}, {day}, {hour}, {minute}, {second}, {building_id}'}</div>
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Access Key ID</label>
                                                    <Input.Password
                                                        className="mgmt-input"
                                                        placeholder="AKIA..."
                                                        value={getConfigObject(editingTarget.config).AccessKeyID || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, AccessKeyID: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Secret Access Key</label>
                                                    <Input.Password
                                                        className="mgmt-input"
                                                        placeholder="Secret Key..."
                                                        value={getConfigObject(editingTarget.config).SecretAccessKey || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, SecretAccessKey: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                            </>
                                        ) : (
                                            <div className="mgmt-alert info">
                                                이 타겟 유형은 별도의 입력 폼을 제공하지 않습니다 ({editingTarget?.target_type}).
                                                오른쪽의 JSON 설정 창을 사용하여 직접 구성하세요.
                                            </div>
                                        )}
                                        {/* Activation Checkbox Moved below Column 2 inputs */}
                                        <div style={{ marginTop: 'auto', paddingTop: '15px', borderTop: '1px solid #eee' }}>
                                            <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center' }}>
                                                <input
                                                    type="checkbox"
                                                    checked={editingTarget?.is_enabled ?? true}
                                                    onChange={e => { setEditingTarget({ ...editingTarget, is_enabled: e.target.checked }); setHasChanges(true); }}
                                                    style={{ marginRight: '8px' }}
                                                />
                                                이 타겟 활성화
                                            </label>
                                        </div>
                                    </div>

                                    {/* Column 3: JSON Config */}
                                    <div style={{ flex: 1.2, minWidth: '300px', borderLeft: '1px solid #eee', paddingLeft: '20px', display: 'flex', flexDirection: 'column' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>고급 설정 (JSON)</div>
                                        <div className="modal-form-hint" style={{ marginBottom: '10px' }}>
                                            왼쪽 입력값과 자동 동기화됩니다. 세부 조정이 필요할 때 직접 편집하세요.
                                        </div>
                                        <textarea
                                            className="mgmt-input"
                                            style={{ flex: 1, fontFamily: 'monospace', fontSize: '12px', resize: 'none', minHeight: '300px', backgroundColor: '#fdfdfd' }}
                                            value={JSON.stringify(maskSensitiveData(editingTarget?.config), null, 2)}
                                            readOnly // Masked version should be read-only to avoid overwriting real keys with asterisks
                                            placeholder='{ "url": ..., "bucket": ... }'
                                        />
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer" style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%' }}>
                                <button type="button" className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={handleTestConnection} disabled={isTesting} style={{ marginRight: 'auto', borderColor: 'var(--primary-500)', color: 'var(--primary-600)', display: 'flex', alignItems: 'center', gap: '8px', width: 'auto' }}>
                                    {isTesting ? (
                                        <>
                                            <i className="fas fa-spinner fa-spin" /> 테스트 중...
                                        </>
                                    ) : (
                                        <>
                                            <i className="fas fa-plug" /> 접속 테스트
                                        </>
                                    )}
                                </button>
                                <div className="mgmt-footer-right" style={{ display: 'flex', gap: '8px' }}>
                                    <button type="button" className="mgmt-btn mgmt-btn-outline" onClick={() => setIsModalOpen(false)} style={{ width: 'auto' }}>취소</button>
                                    <button type="submit" className="mgmt-btn mgmt-btn-primary" style={{ width: 'auto' }}>저장하기</button>
                                </div>
                            </div>
                        </form>
                    </div>
                </div>
            )}
            {isMappingModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-content wide-modal" style={{ display: 'flex', flexDirection: 'column' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">데이터 포인트 매핑 설정</h3>
                            <button className="mgmt-close-btn" onClick={handleCloseMapping}>&times;</button>
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '20px', flex: 1, overflow: 'hidden', padding: '20px' }}>
                            {/* 1. Guidance Section (Top) */}
                            <div style={{
                                background: 'var(--warning-50)',
                                border: '1px solid var(--warning-100)',
                                borderRadius: '8px',
                                padding: '15px',
                                flexShrink: 0
                            }}>
                                <div style={{ fontWeight: 600, color: 'var(--warning-700)', fontSize: '14px', marginBottom: '8px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <i className="fas fa-exchange-alt" /> 물리 전송 매핑 가이드
                                </div>
                                <div style={{ fontSize: '13px', color: 'var(--warning-600)', lineHeight: '1.6' }}>
                                    실제 외부 시스템으로 전송할 때의 <strong>최종 설정</strong>입니다.
                                    데이터 단위 변환(Scale, Offset)이 필요한 경우 테이블에서 설정하세요.
                                    엑셀에서 [포인트명][탭][필드명][탭][설명] 순으로 복사해 아래 영역에 붙여넣으면 자동 매핑됩니다.
                                </div>
                            </div>

                            {/* 2. Excel Paste Section (Middle) */}
                            <div style={{ padding: '15px', backgroundColor: '#f8f9fa', borderRadius: '8px', border: '1px solid #ddd', display: 'flex', flexDirection: 'column', gap: '10px', flexShrink: 0 }}>
                                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                    <h4 style={{ margin: 0, fontSize: '14px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                        <i className="fas fa-file-excel" style={{ color: '#1d6f42' }} /> 엑셀 붙여넣기 (일괄 등록)
                                    </h4>
                                    <button
                                        className="mgmt-btn mgmt-btn-primary mgmt-btn-sm"
                                        style={{ width: 'auto' }}
                                        onClick={async () => {
                                            if (!pasteData.trim()) {
                                                await confirm({
                                                    title: '데이터 없음',
                                                    message: '붙여넣을 데이터가 없습니다.',
                                                    showCancelButton: false,
                                                    confirmButtonType: 'primary'
                                                });
                                                return;
                                            }

                                            const lines = pasteData.split(/\r?\n/);
                                            let addedCount = 0;
                                            let failedCount = 0;
                                            const newMappings = [...targetMappings];

                                            lines.forEach(line => {
                                                if (!line.trim()) return;
                                                const parts = line.split('\t');
                                                const pointName = parts[0]?.trim();
                                                const targetField = parts[1]?.trim() || '';
                                                const desc = parts[2]?.trim() || '';

                                                if (pointName) {
                                                    const point = allPoints.find(p => p.name.toLowerCase() === pointName.toLowerCase());
                                                    const isAlreadyMapped = newMappings.some(m => m.point_id && m.point_id === point?.id);

                                                    if (point && !isAlreadyMapped) {
                                                        newMappings.push({
                                                            site_id: point.site_id, // [FIX] Include site_id
                                                            point_id: point.id,
                                                            target_field_name: targetField,
                                                            target_description: desc,
                                                            is_enabled: true
                                                        });
                                                        addedCount++;
                                                    } else if (!point) {
                                                        newMappings.push({
                                                            point_id: undefined,
                                                            target_field_name: targetField,
                                                            site_id: undefined, // [FIX] Include site_id
                                                            target_description: desc,
                                                            is_enabled: true,
                                                            _temp_name: pointName
                                                        });
                                                        failedCount++;
                                                    }
                                                }
                                            });

                                            if (addedCount > 0 || failedCount > 0) {
                                                setTargetMappings(newMappings);
                                                setPasteData('');
                                                setHasMappingChanges(true);
                                            }

                                            await confirm({
                                                title: '매핑 적용 완료',
                                                message: `성공: ${addedCount}개\n실패(확인 필요): ${failedCount}개`,
                                                showCancelButton: false,
                                                confirmButtonType: 'success'
                                            });
                                        }}
                                    >
                                        <i className="fas fa-magic" /> 매핑 적용하기
                                    </button>
                                    <button
                                        className="mgmt-btn mgmt-btn-outline mgmt-btn-sm"
                                        style={{ width: 'auto', borderColor: 'var(--primary-300)', color: 'var(--primary-600)' }}
                                        onClick={handleImportFromProfile}
                                    >
                                        <i className="fas fa-sync-alt" /> 프로파일에서 불러오기
                                    </button>
                                </div>
                                <textarea
                                    className="mgmt-input"
                                    style={{ height: '100px', resize: 'vertical', width: '100%', fontFamily: 'monospace', fontSize: '12px', lineHeight: '1.4' }}
                                    placeholder={`[PulseOne 포인트명] (탭) [외부 시스템 필드명] (탭) [설명]\n\nSensor_A_01\tFactory_Temp_01\t1공장 온도`}
                                    value={pasteData}
                                    onChange={(e) => setPasteData(e.target.value)}
                                />
                            </div>

                            {/* [NEW] Bulk Site ID Application Area */}
                            <div style={{ background: '#f8fafc', padding: '12px 16px', borderRadius: '8px', border: '1px solid #e2e8f0', marginBottom: '20px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                <div style={{ fontSize: '12px', color: '#64748b' }}>
                                    <i className="fas fa-info-circle" /> 포인트 정보 로드시 기본 Site ID가 설정됩니다. 필요시 아래에서 일괄 변경 가능합니다.
                                </div>
                                <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                                    <span style={{ fontSize: '13px', fontWeight: 600 }}>Site ID 일괄 적용:</span>
                                    <Input
                                        size="small"
                                        placeholder="빌딩 번호"
                                        style={{ width: '100px' }}
                                        value={bulkSiteIdForMapping}
                                        onChange={e => setBulkSiteIdForMapping(e.target.value)}
                                    />
                                    <Button
                                        size="small"
                                        onClick={async () => {
                                            const bid = parseInt(bulkSiteIdForMapping);
                                            if (isNaN(bid)) {
                                                await confirm({
                                                    title: '입력 확인',
                                                    message: '숫자 형태의 Site ID를 입력해주세요.',
                                                    showCancelButton: false,
                                                    confirmButtonType: 'warning'
                                                });
                                                return;
                                            }
                                            setTargetMappings(targetMappings.map(m => ({ ...m, site_id: bid })));
                                            setHasMappingChanges(true);
                                        }}
                                    >적용하기</Button>
                                </div>
                            </div>

                            {/* 3. Mapping List Section (Bottom) */}
                            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', borderTop: '1px solid #eee', paddingTop: '20px' }}>
                                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
                                    <h4 style={{ margin: 0, fontSize: '15px', fontWeight: 600, color: '#333' }}>
                                        <i className="fas fa-list-ul" style={{ marginRight: '8px' }} /> 최종 매핑 리스트 ({targetMappings.length})
                                    </h4>
                                    <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" style={{ width: 'auto' }} onClick={() => {
                                        setTargetMappings([...targetMappings, { point_id: undefined, target_field_name: '', target_description: '', is_enabled: true }]);
                                        setHasMappingChanges(true);
                                    }}>
                                        <i className="fas fa-plus" /> 1행 추가
                                    </button>
                                </div>

                                <div className="mgmt-table-container" style={{ flex: 1, overflowY: 'auto', border: '1px solid #eee', borderRadius: '8px' }}>
                                    <table className="mgmt-table">
                                        <thead>
                                            <tr>
                                                <th style={{ width: '20%', padding: '8px 8px' }}>PulseOne 포인트 (소스)</th>
                                                <th style={{ width: '25%', padding: '8px 8px' }}>외부 필드명 (Key)</th>
                                                <th style={{ width: '60px', padding: '8px 4px' }}>Site ID</th>
                                                <th style={{ width: '60px', padding: '8px 4px', whiteSpace: 'nowrap' }}>Scale</th>
                                                <th style={{ width: '60px', padding: '8px 4px', whiteSpace: 'nowrap' }}>Offset</th>
                                                <th style={{ width: '30%', padding: '8px 8px' }}>상세 설명</th>
                                                <th style={{ width: '50px', padding: '8px 8px', textAlign: 'center' }}>관리</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            {targetMappings.map((mapping, idx) => {
                                                // conversion_config 파싱 (없으면 기본값)
                                                let config: any = {};
                                                try {
                                                    config = typeof mapping.conversion_config === 'string'
                                                        ? JSON.parse(mapping.conversion_config)
                                                        : (mapping.conversion_config || {});
                                                } catch { }

                                                return (
                                                    <tr key={idx}>
                                                        <td style={{ padding: '8px 8px' }}>
                                                            <div style={{ display: 'flex', alignItems: 'center' }}>
                                                                <Select
                                                                    showSearch
                                                                    style={{ flex: 1 }}
                                                                    placeholder={mapping._temp_name ? `찾을 수 없음: "${mapping._temp_name}"` : "포인트 선택"}
                                                                    optionFilterProp="children"
                                                                    filterOption={(input, option: any) =>
                                                                        (option?.label ?? '').toLowerCase().includes(input.toLowerCase())
                                                                    }
                                                                    value={mapping.point_id}
                                                                    onChange={(val) => {
                                                                        const newMappings = [...targetMappings];
                                                                        newMappings[idx] = {
                                                                            ...newMappings[idx],
                                                                            point_id: val,
                                                                            _temp_name: undefined
                                                                        };
                                                                        setTargetMappings(newMappings);
                                                                        setHasMappingChanges(true);
                                                                    }}
                                                                    status={!mapping.point_id ? 'error' : ''}
                                                                    options={allPoints.map(p => ({
                                                                        value: p.id,
                                                                        label: `${p.name} [${p.address || '?'}] (${p.device_name})`
                                                                    }))}
                                                                />
                                                                {mapping.point_id && (
                                                                    <Tooltip title="현재 데이터 즉시 전송 (Manual Export)">
                                                                        <button
                                                                            type="button"
                                                                            className="mgmt-btn mgmt-btn-outline"
                                                                            style={{
                                                                                width: '32px',
                                                                                height: '32px',
                                                                                marginLeft: '8px',
                                                                                padding: 0,
                                                                                display: 'flex',
                                                                                alignItems: 'center',
                                                                                justifyContent: 'center',
                                                                                color: 'var(--primary-600)',
                                                                                borderColor: 'var(--primary-200)'
                                                                            }}
                                                                            onClick={() => handleTriggerManualExport(mapping)}
                                                                        >
                                                                            <i className="fas fa-paper-plane" />
                                                                        </button>
                                                                    </Tooltip>
                                                                )}
                                                            </div>
                                                            {!mapping.point_id && mapping._temp_name && (
                                                                <div style={{ color: '#ff4d4f', fontSize: '11px', marginTop: '2px' }}>
                                                                    <i className="fas fa-exclamation-circle" /> 매칭 실패: "{mapping._temp_name}"
                                                                </div>
                                                            )}
                                                        </td>
                                                        <td style={{ padding: '8px 8px' }}>
                                                            <input
                                                                type="text"
                                                                className="mgmt-input"
                                                                value={mapping.target_field_name || ''}
                                                                onChange={e => {
                                                                    const newMappings = [...targetMappings];
                                                                    newMappings[idx] = { ...newMappings[idx], target_field_name: e.target.value };
                                                                    setTargetMappings(newMappings);
                                                                }}
                                                                placeholder="예: voltage_l1"
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 4px' }}>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input"
                                                                value={mapping.site_id || ''}
                                                                onChange={e => {
                                                                    const val = parseInt(e.target.value);
                                                                    const newMappings = [...targetMappings];
                                                                    newMappings[idx] = { ...newMappings[idx], site_id: isNaN(val) ? undefined : val };
                                                                    setTargetMappings(newMappings);
                                                                }}
                                                                placeholder="ID"
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 4px' }}>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input"
                                                                step="0.01"
                                                                value={config.scale !== undefined ? config.scale : 1}
                                                                onChange={e => {
                                                                    const val = parseFloat(e.target.value);
                                                                    const newConfig = { ...config, scale: isNaN(val) ? 1 : val };
                                                                    const newMappings = [...targetMappings];
                                                                    newMappings[idx] = { ...newMappings[idx], conversion_config: newConfig };
                                                                    setTargetMappings(newMappings);
                                                                }}
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 4px' }}>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input"
                                                                step="0.01"
                                                                value={config.offset !== undefined ? config.offset : 0}
                                                                onChange={e => {
                                                                    const val = parseFloat(e.target.value);
                                                                    const newConfig = { ...config, offset: isNaN(val) ? 0 : val };
                                                                    const newMappings = [...targetMappings];
                                                                    newMappings[idx] = { ...newMappings[idx], conversion_config: newConfig };
                                                                    setTargetMappings(newMappings);
                                                                }}
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 8px' }}>
                                                            <input
                                                                type="text"
                                                                className="mgmt-input"
                                                                value={mapping.target_description || ''}
                                                                onChange={e => {
                                                                    const newMappings = [...targetMappings];
                                                                    newMappings[idx] = { ...newMappings[idx], target_description: e.target.value };
                                                                    setTargetMappings(newMappings);
                                                                }}
                                                                placeholder="설명"
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 8px', textAlign: 'center' }}>
                                                            <button
                                                                className="btn btn-xs btn-outline btn-danger"
                                                                style={{ width: 'auto', padding: '0 8px' }}
                                                                onClick={() => {
                                                                    const newMappings = targetMappings.filter((_, i) => i !== idx);
                                                                    setTargetMappings(newMappings);
                                                                }}
                                                            >
                                                                <i className="fas fa-trash" />
                                                            </button>
                                                        </td>
                                                    </tr>
                                                );
                                            })}
                                        </tbody>
                                    </table>
                                </div>

                                <div className="mgmt-modal-footer" style={{ marginTop: '20px', flexShrink: 0 }}>
                                    <div className="mgmt-footer-right" style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
                                        <button className="mgmt-btn mgmt-btn-outline" onClick={handleCloseMapping}>닫기</button>
                                        <button className="mgmt-btn mgmt-btn-primary" onClick={handleSaveMappings}>저장하기</button>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
};

export default TargetManagementTab;
