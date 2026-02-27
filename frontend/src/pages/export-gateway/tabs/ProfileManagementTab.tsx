import React, { useState, useEffect } from 'react';
import exportGatewayApi, { ExportProfile } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';
import DataPointSelector from '../components/DataPointSelector';

interface ProfileManagementTabProps {
    siteId?: number | null;
    tenantId?: number | null;
    isAdmin?: boolean;
    tenants?: any[];
}

const ProfileManagementTab: React.FC<ProfileManagementTabProps> = ({ siteId, tenantId, isAdmin, tenants = [] }) => {
    const [profiles, setProfiles] = useState<ExportProfile[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingProfile, setEditingProfile] = useState<Partial<ExportProfile> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
    const [bulkSiteId, setBulkSiteId] = useState<number | undefined>(undefined);
    const { confirm } = useConfirmContext();

    const handleEditProfile = async (profile: any) => {
        setLoading(true);
        try {
            // 1. Fetch master points for basic metadata (like name/device)
            const masterPoints = await exportGatewayApi.getDataPoints("", undefined, siteId, tenantId);

            // 2. Fetch all targets to find if any are using THIS profile
            const targetsRes = await exportGatewayApi.getTargets({ tenantId });
            const allTargets = (targetsRes.data as any)?.items || (Array.isArray(targetsRes.data) ? targetsRes.data : []);
            const linkedTarget = allTargets.find((t: any) => String(t.profile_id) === String(profile.id));

            let liveMappings: any[] = [];
            if (linkedTarget) {
                // 3. Pull live mapping overrides (the "Wizard" data)
                const mappingsRes = await exportGatewayApi.getTargetMappings(linkedTarget.id, siteId, tenantId);
                liveMappings = (mappingsRes.data as any)?.items || (Array.isArray(mappingsRes.data) ? mappingsRes.data : []);
            }

            const hydratedPoints = (profile.data_points || []).map((p: any) => {
                const mp = masterPoints.find((sp: any) => sp.id === p.id);
                const lm = liveMappings.find((m: any) => m.point_id === p.id);

                return {
                    ...p,
                    // Preference: Saved mapping override > Profile Snapshot > Master Metadata > Default
                    site_id: lm?.site_id || lm?.building_id || p.site_id || p.building_id || mp?.site_id || 0,
                    target_field_name: lm?.target_field_name || p.target_field_name || p.name,
                    scale: lm?.conversion_config?.scale ?? p.scale ?? 1,
                    offset: lm?.conversion_config?.offset ?? p.offset ?? 0
                };
            });

            setEditingProfile({ ...profile, data_points: hydratedPoints });
            setIsModalOpen(true);
            setHasChanges(false);
        } catch (err) {
            console.error("Hydration failed:", err);
            // Fallback to basic hydration
            setEditingProfile(profile);
            setIsModalOpen(true);
        } finally {
            setLoading(false);
        }
    };

    const fetchProfiles = async () => {
        setLoading(true);
        try {
            const response = await exportGatewayApi.getProfiles({ tenantId });
            setProfiles(response.data || []);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => { fetchProfiles(); }, [siteId, tenantId]);

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();

        // 1. Validation: Ensure at least one point is mapped
        if (!editingProfile?.data_points || editingProfile.data_points.length === 0) {
            await confirm({
                title: '포인트 부족',
                message: '최소 하나 이상의 데이터 포인트를 매핑해야 합니다.',
                showCancelButton: false,
                confirmButtonType: 'warning'
            });
            return;
        }

        // 2. Check for modifications
        if (!hasChanges && editingProfile.id) {
            await confirm({
                title: '수정사항 없음',
                message: '수정된 정보가 없습니다.',
                showCancelButton: false,
                confirmButtonType: 'primary'
            });
            setIsModalOpen(false);
            return;
        }

        // 3. Final Confirmation
        const confirmed = await confirm({
            title: '저장 확인',
            message: editingProfile.id ? '프로파일 변경 사항을 저장하시겠습니까?' : '새 프로파일을 생성하시겠습니까?',
            confirmText: '저장',
            cancelText: '취소',
            confirmButtonType: 'primary'
        });
        if (!confirmed) return;

        try {
            // Apply defaults for target_field_name if empty
            const processedProfile = {
                ...editingProfile,
                data_points: editingProfile.data_points.map((p: any) => ({
                    ...p,
                    target_field_name: p.target_field_name?.trim() || p.name
                }))
            };

            let response;
            const targetTenantId = editingProfile?.tenant_id || tenantId;
            if (processedProfile?.id) {
                response = await exportGatewayApi.updateProfile(processedProfile.id, processedProfile, targetTenantId);
            } else {
                response = await exportGatewayApi.createProfile({ ...processedProfile! }, targetTenantId);
            }

            if (response.success) {
                await confirm({
                    title: '저장 완료',
                    message: '프로파일이 성공적으로 저장되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
                setHasChanges(false);
                fetchProfiles();
            } else {
                await confirm({
                    title: '저장 실패',
                    message: response.message || '프로파일을 저장하는 중 오류가 발생했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: '저장 실패',
                message: '프로파일을 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

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

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '프로파일 삭제 확인',
            message: '이 프로파일을 삭제하시겠습니까? 관련된 게이트웨이 할당 정보도 모두 삭제됩니다.',
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteProfile(id);
            fetchProfiles();
        } catch (error) {
            await confirm({ title: '삭제 실패', message: '프로파일을 삭제하는 중 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    const handlePointSelect = (point: any) => {
        const currentPoints = editingProfile?.data_points || [];
        if (currentPoints.some((p: any) => p.id === point.id)) return;
        setEditingProfile({
            ...editingProfile,
            data_points: [...currentPoints, {
                id: point.id,
                name: point.name,
                device: point.device_name,
                address: point.address,
                site_id: point.site_id,
                scale: 1, // Default scale
                offset: 0, // Default offset
                target_field_name: point.name
            }]
        });
        setHasChanges(true);
    };

    const handleAddAllPoints = (points: any[]) => {
        const currentPoints = [...(editingProfile?.data_points || [])];
        const newPoints = points.filter(p => !currentPoints.some(cp => cp.id === p.id));

        if (newPoints.length === 0) return;

        setEditingProfile({
            ...editingProfile,
            data_points: [...currentPoints, ...newPoints.map(p => ({
                id: p.id,
                name: p.name,
                device: p.device_name,
                address: p.address,
                site_id: p.site_id,
                scale: 1,
                offset: 0,
                target_field_name: p.name
            }))]
        });
        setHasChanges(true);
    };

    const handlePointRemove = (pointId: number) => {
        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: currentPoints.filter((p: any) => p.id !== pointId)
        });
        setHasChanges(true);
    };

    const handleMappingNameChange = (pointId: number, name: string) => {
        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: currentPoints.map((p: any) =>
                p.id === pointId ? { ...p, target_field_name: name } : p
            )
        });
        setHasChanges(true);
    };

    const handleSiteIdChange = (pointId: number, siteId: number | undefined) => {
        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: currentPoints.map((p: any) =>
                p.id === pointId ? { ...p, site_id: siteId } : p
            )
        });
        setHasChanges(true);
    };

    const handleScaleChange = (pointId: number, scale: number) => {
        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: currentPoints.map((p: any) =>
                p.id === pointId ? { ...p, scale } : p
            )
        });
        setHasChanges(true);
    };

    const handleOffsetChange = (pointId: number, offset: number) => {
        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: currentPoints.map((p: any) =>
                p.id === pointId ? { ...p, offset } : p
            )
        });
        setHasChanges(true);
    };

    const handleBulkSiteIdApply = () => {
        if (bulkSiteId === undefined || isNaN(bulkSiteId)) return;

        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: currentPoints.map((p: any) => ({ ...p, site_id: bulkSiteId }))
        });
        setHasChanges(true);
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>내보내기 프로파일</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingProfile({ name: '', description: '', data_points: [], is_enabled: true }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> 프로파일 생성
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>이름</th>
                            <th>설명</th>
                            <th>포인트 수</th>
                            <th>관리</th>
                        </tr>
                    </thead>
                    <tbody>
                        {profiles.map(p => (
                            <tr key={p.id}>
                                <td>{p.name}</td>
                                <td>{p.description}</td>
                                <td>
                                    <span className="badge neutral">
                                        {Array.isArray(p.data_points) ? p.data_points.length : 0}개
                                    </span>
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="btn btn-outline btn-xs" onClick={() => handleEditProfile(p)}>수정</button>
                                        <button className="btn btn-outline btn-xs btn-danger" onClick={() => handleDelete(p.id)}>삭제</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="ultra-wide-overlay">
                    <style>{`
                        .ultra-wide-overlay {
                            position: fixed;
                            top: 0; left: 0; right: 0; bottom: 0;
                            background: rgba(15, 23, 42, 0.7);
                            backdrop-filter: blur(8px);
                            display: flex;
                            align-items: center;
                            justify-content: center;
                            z-index: 2000;
                        }
                        .ultra-wide-container {
                            background: white;
                            width: 90vw;
                            height: 92vh;
                            max-width: 1600px;
                            display: flex;
                            flex-direction: column;
                            border-radius: 16px;
                            overflow: hidden;
                            box-shadow: 0 25px 50px -12px rgba(0,0,0,0.3);
                        }
                        /* Isolated overrides for Management design system within this modal */
                        .ultra-wide-container .mgmt-input {
                            width: 100% !important; 
                            height: 38px !important; /* Standard comfortable height */
                            border: 1.5px solid #cbd5e1 !important;
                            border-radius: 8px !important;
                            background-color: #ffffff !important;
                            font-size: 14px !important;
                            transition: all 0.2s ease !important;
                        }
                        .ultra-wide-container .mgmt-input.bulk-site-input {
                             width: 110px !important; 
                             height: 44px !important; /* ONLY THIS ONE IS TALLER as requested */
                             font-size: 16px !important;
                             font-weight: 700 !important;
                        }
                        .ultra-wide-container .mgmt-input:focus {
                            border-color: var(--primary-500) !important;
                            box-shadow: 0 0 0 4px rgba(14, 165, 233, 0.15) !important;
                            background-color: #fff !important;
                        }
                        .ultra-wide-container .mgmt-table th {
                            background: #f8fafc !important;
                            color: #475569 !important;
                            font-weight: 700 !important;
                            font-size: 11px !important;
                            letter-spacing: 0.05em !important;
                            padding: 12px 16px !important;
                        }
                        .ultra-wide-container .mgmt-table td {
                            padding: 10px 12px !important;
                            border-bottom: 1px solid #f1f5f9 !important;
                        }
                        .ultra-wide-body {
                            flex: 1;
                            display: flex;
                            overflow: hidden;
                            background: #f8fafc;
                            padding: 20px;
                            gap: 20px;
                        }
                        .side-setup-panel {
                            width: 380px;
                            display: flex;
                            flex-direction: column;
                            flex-shrink: 0;
                            background: white;
                            border-radius: 12px;
                            border: 1px solid #e2e8f0;
                            overflow: hidden;
                            box-shadow: var(--shadow-sm);
                        }
                        .center-mapping-panel {
                            flex: 1;
                            background: white;
                            border-radius: 12px;
                            border: 1px solid #e2e8f0;
                            display: flex;
                            flex-direction: column;
                            overflow: hidden;
                            box-shadow: var(--shadow-sm);
                        }
                        .side-guide-panel {
                            width: 320px;
                            background: white;
                            border-radius: 12px;
                            border: 1px solid #e2e8f0;
                            padding: 28px;
                            overflow-y: auto;
                            flex-shrink: 0;
                            box-shadow: var(--shadow-sm);
                        }
                        .ultra-wide-header {
                            padding: 24px 32px;
                            background: white;
                            border-bottom: 1px solid #e2e8f0;
                            display: flex;
                            justify-content: space-between;
                            align-items: center;
                        }
                        .ultra-wide-footer {
                            padding: 20px 32px;
                            background: #f8fafc;
                            border-top: 1px solid #e2e8f0;
                            display: flex;
                            justify-content: flex-end;
                            align-items: center;
                            gap: 16px;
                        }
                    `}</style>
                    <div className="ultra-wide-container">
                        <form onSubmit={handleSave} style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
                            <div className="ultra-wide-header">
                                <h3 style={{ margin: 0, fontSize: '22px', fontWeight: 800, color: '#0f172a' }}>
                                    <i className={`fas ${editingProfile?.id ? 'fa-edit' : 'fa-plus-circle'} `} style={{ marginRight: '12px', color: 'var(--primary-600)' }} />
                                    {editingProfile?.id ? "프로파일 수정" : "신규 프로파일 생성"}
                                </h3>
                                <button type="button" className="mgmt-modal-close" onClick={handleCloseModal} style={{ fontSize: '28px', color: '#94a3b8' }}>&times;</button>
                            </div>

                            {/* Top Setup Bar (Horizontal) */}
                            <div style={{ background: 'white', padding: '20px 32px', borderBottom: '1px solid #e2e8f0', display: 'flex', gap: '40px', alignItems: 'flex-start' }}>
                                <div style={{ flex: '0 0 400px' }}>
                                    <label style={{ display: 'block', fontSize: '13px', fontWeight: 700, color: '#444', marginBottom: '8px' }}>프로파일 명칭</label>
                                    <input
                                        type="text"
                                        className="mgmt-input"
                                        required
                                        value={editingProfile?.name || ''}
                                        onChange={e => { setEditingProfile({ ...editingProfile, name: e.target.value }); setHasChanges(true); }}
                                        placeholder="예: 공장 데이터 전송 프로파일"
                                        style={{ height: '42px', fontSize: '15px' }}
                                    />
                                </div>
                                <div style={{ flex: 1 }}>
                                    <label style={{ display: 'block', fontSize: '13px', fontWeight: 700, color: '#444', marginBottom: '8px' }}>상세 설명</label>
                                    <input
                                        type="text"
                                        className="mgmt-input"
                                        value={editingProfile?.description || ''}
                                        onChange={e => { setEditingProfile({ ...editingProfile, description: e.target.value }); setHasChanges(true); }}
                                        placeholder="이 프로파일의 용도를 간단히 적어주세요"
                                        style={{ height: '42px', fontSize: '15px' }}
                                    />
                                </div>
                                {isAdmin && !tenantId && (
                                    <div style={{ flex: '0 0 250px' }}>
                                        <label style={{ display: 'block', fontSize: '13px', fontWeight: 700, color: '#444', marginBottom: '8px' }}>소속 테넌트 <span style={{ color: 'red' }}>*</span></label>
                                        <select
                                            className="mgmt-select"
                                            required
                                            value={editingProfile?.tenant_id || ''}
                                            onChange={e => { setEditingProfile({ ...editingProfile, tenant_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                            style={{ width: '100%', height: '42px', fontSize: '15px', borderRadius: '8px', border: '1.5px solid #cbd5e1' }}
                                        >
                                            <option value="">(테넌트 선택)</option>
                                            {tenants.map(t => (
                                                <option key={t.id} value={t.id}>{t.company_name}</option>
                                            ))}
                                        </select>
                                    </div>
                                )}
                            </div>

                            <div className="ultra-wide-body">
                                {/* Column 1: Data Discovery (Full Height) */}
                                <div className="side-setup-panel">
                                    <div style={{ padding: '20px', borderBottom: '1px solid #f1f5f9', background: '#f8fafc', display: 'flex', alignItems: 'center', gap: '10px' }}>
                                        <i className="fas fa-search-plus" style={{ color: 'var(--primary-600)' }} />
                                        <h4 style={{ margin: 0, fontSize: '16px', fontWeight: 800 }}>포인트 탐색</h4>
                                    </div>
                                    <div style={{ flex: 1, overflow: 'hidden' }}>
                                        <DataPointSelector
                                            siteId={siteId}
                                            selectedPoints={editingProfile?.data_points || []}
                                            onSelect={handlePointSelect}
                                            onAddAll={handleAddAllPoints}
                                            onRemove={handlePointRemove}
                                        />
                                    </div>
                                </div>

                                {/* Column 2: Large Mapping Table */}
                                <div className="center-mapping-panel">
                                    <div style={{ padding: '24px', borderBottom: '1px solid #f1f5f9', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                        <div>
                                            <h4 style={{ margin: 0, fontSize: '18px', fontWeight: 800, color: '#1e293b' }}>데이터 매핑 및 외부 필드명 설정</h4>
                                            <p style={{ margin: '4px 0 0 0', fontSize: '13px', color: '#64748b' }}>추가된 포인트가 외부 시스템에서 어떤 이름으로 표시될지 정의하세요.</p>
                                        </div>
                                        <div style={{ display: 'flex', alignItems: 'flex-end', gap: '12px', padding: '12px 20px', background: '#f0f9ff', borderRadius: '12px', border: '1.5px solid #bae6fd' }}>
                                            <div style={{ display: 'flex', flexDirection: 'column' }}>
                                                <span style={{ fontSize: '11px', fontWeight: 800, color: '#0369a1', marginBottom: '2px', textTransform: 'uppercase' }}>Site ID 일괄 적용</span>
                                                <input
                                                    type="number"
                                                    className="mgmt-input sm bulk-site-input"
                                                    placeholder="ID"
                                                    value={bulkSiteId ?? ''}
                                                    onChange={e => {
                                                        const val = parseInt(e.target.value);
                                                        setBulkSiteId(isNaN(val) ? undefined : val);
                                                    }}
                                                    style={{ borderColor: '#7dd3fc' }}
                                                />
                                            </div>
                                            <button
                                                type="button"
                                                className="btn btn-primary"
                                                onClick={handleBulkSiteIdApply}
                                                style={{ height: '36px', fontSize: '13px', fontWeight: 700, padding: '0 20px' }}
                                            >
                                                적용
                                            </button>
                                        </div>
                                    </div>
                                    <div style={{ flex: 1, overflow: 'auto', padding: '0 8px' }}>
                                        <table className="mgmt-table">
                                            <thead>
                                                <tr>
                                                    <th style={{ width: '22%' }}>내부 포인트명</th>
                                                    <th style={{ width: '38%' }}>매핑 명칭 (TARGET KEY)</th>
                                                    <th style={{ width: '100px', textAlign: 'center' }}>SITE ID</th>
                                                    <th style={{ width: '90px', textAlign: 'center' }}>SCALE</th>
                                                    <th style={{ width: '90px', textAlign: 'center' }}>OFFSET</th>
                                                    <th style={{ width: '60px', textAlign: 'center' }}>삭제</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                {(editingProfile?.data_points || []).map((p: any) => (
                                                    <tr key={p.id}>
                                                        <td>
                                                            <div style={{ fontWeight: 700, color: '#334155', fontSize: '14px' }}>{p.name}</div>
                                                            <div style={{ fontSize: '11px', color: '#94a3b8', marginTop: '2px' }}>ID: {p.id}</div>
                                                        </td>
                                                        <td>
                                                            <input
                                                                type="text"
                                                                className="mgmt-input"
                                                                value={p.target_field_name || ''}
                                                                onChange={e => handleMappingNameChange(p.id, e.target.value)}
                                                                placeholder="예: TEMPERATURE_SENSOR_1"
                                                                style={{ height: '38px', fontSize: '13px' }}
                                                            />
                                                        </td>
                                                        <td style={{ textAlign: 'center' }}>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input"
                                                                value={p.site_id || ''}
                                                                onChange={e => {
                                                                    const val = parseInt(e.target.value);
                                                                    handleSiteIdChange(p.id, isNaN(val) ? undefined : val);
                                                                }}
                                                                style={{ height: '38px', textAlign: 'center', fontSize: '13px' }}
                                                            />
                                                        </td>
                                                        <td>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input"
                                                                step="0.001"
                                                                value={p.scale ?? 1}
                                                                onChange={e => handleScaleChange(p.id, parseFloat(e.target.value))}
                                                                style={{ height: '38px', textAlign: 'center', fontSize: '13px' }}
                                                            />
                                                        </td>
                                                        <td>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input"
                                                                step="0.001"
                                                                value={p.offset ?? 0}
                                                                onChange={e => handleOffsetChange(p.id, parseFloat(e.target.value))}
                                                                style={{ height: '38px', textAlign: 'center', fontSize: '13px' }}
                                                            />
                                                        </td>
                                                        <td style={{ textAlign: 'center' }}>
                                                            <button
                                                                type="button"
                                                                className="mgmt-btn-icon error"
                                                                onClick={() => handlePointRemove(p.id)}
                                                                style={{ background: '#fff1f2', color: '#e11d48', border: '1px solid #fecaca' }}
                                                            >
                                                                <i className="fas fa-trash-alt" />
                                                            </button>
                                                        </td>
                                                    </tr>
                                                ))}
                                            </tbody>
                                        </table>
                                    </div>
                                </div>

                                {/* Column 3: Guide */}
                                <div className="side-guide-panel">
                                    <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '24px', borderBottom: '2px solid #e2e8f0', paddingBottom: '12px' }}>
                                        <i className="fas fa-book-reader" style={{ color: 'var(--primary-600)', fontSize: '20px' }} />
                                        <h4 style={{ margin: 0, fontSize: '18px', fontWeight: 800 }}>Engineer's Guide</h4>
                                    </div>
                                    <div style={{ fontSize: '14px', lineHeight: '1.7', display: 'flex', flexDirection: 'column', gap: '24px' }}>
                                        <section>
                                            <div style={{ fontWeight: 800, color: '#1e293b', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                <span style={{ background: 'var(--primary-100)', color: 'var(--primary-700)', width: '22px', height: '22px', borderRadius: '50%', display: 'inline-flex', alignItems: 'center', justifyContent: 'center', fontSize: '12px' }}>1</span>
                                                프로파일 정의
                                            </div>
                                            <p style={{ margin: '8px 0 0 30px', color: '#64748b' }}>데이터 수집의 **템플릿** 역할을 수행합니다. 내부 관리용 명칭을 입력하세요.</p>
                                        </section>
                                        <section>
                                            <div style={{ fontWeight: 800, color: '#1e293b', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                <span style={{ background: 'var(--primary-100)', color: 'var(--primary-700)', width: '22px', height: '22px', borderRadius: '50%', display: 'inline-flex', alignItems: 'center', justifyContent: 'center', fontSize: '12px' }}>2</span>
                                                포인트 탐색
                                            </div>
                                            <p style={{ margin: '8px 0 0 30px', color: '#64748b' }}>왼쪽 패널에서 필요한 포인트를 선택하여 우측 매핑 리스트에 추가하세요.</p>
                                        </section>
                                        <section>
                                            <div style={{ fontWeight: 800, color: '#1e293b', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                <span style={{ background: 'var(--primary-100)', color: 'var(--primary-700)', width: '22px', height: '22px', borderRadius: '50%', display: 'inline-flex', alignItems: 'center', justifyContent: 'center', fontSize: '12px' }}>3</span>
                                                매핑 키 설정
                                            </div>
                                            <p style={{ margin: '8px 0 0 30px', color: '#64748b' }}>외부 시스템(AWS S3 등)에서 보여질 **최종 필드 명칭**을 정의합니다.</p>
                                            <div style={{ margin: '12px 0 0 30px', padding: '12px', background: '#f8fafc', borderRadius: '8px', border: '1px dashed #cbd5e1', fontSize: '12px' }}>
                                                <i className="fas fa-info-circle" style={{ color: '#0ea5e9', marginRight: '6px' }} />
                                                **Scale/Offset**: 수집된 원본 수치 데이터를 변환할 때 사용합니다.
                                            </div>
                                        </section>
                                    </div>
                                </div>
                            </div>

                            <div className="ultra-wide-footer">
                                <button type="button" className="btn btn-outline" style={{ height: '40px', padding: '0 24px', borderRadius: '10px', fontWeight: 600 }} onClick={handleCloseModal}>취소</button>
                                <button type="submit" className="btn btn-primary" style={{ height: '40px', padding: '0 32px', borderRadius: '10px', fontWeight: 700, boxShadow: '0 4px 10px rgba(14, 165, 233, 0.3)' }}>프로파일 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default ProfileManagementTab;
