import React, { useState, useEffect } from 'react';
import exportGatewayApi, { ExportProfile } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';
import DataPointSelector from '../components/DataPointSelector';

const ProfileManagementTab: React.FC = () => {
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
            const masterPoints = await exportGatewayApi.getDataPoints("");

            // 2. Fetch all targets to find if any are using THIS profile
            const targetsRes = await exportGatewayApi.getTargets();
            const allTargets = (targetsRes.data as any)?.items || (Array.isArray(targetsRes.data) ? targetsRes.data : []);
            const linkedTarget = allTargets.find((t: any) => String(t.profile_id) === String(profile.id));

            let liveMappings: any[] = [];
            if (linkedTarget) {
                // 3. Pull live mapping overrides (the "Wizard" data)
                const mappingsRes = await exportGatewayApi.getTargetMappings(linkedTarget.id);
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
            const response = await exportGatewayApi.getProfiles();
            setProfiles(response.data || []);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => { fetchProfiles(); }, []);

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
            if (processedProfile?.id) {
                response = await exportGatewayApi.updateProfile(processedProfile.id, processedProfile);
            } else {
                response = await exportGatewayApi.createProfile(processedProfile!);
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
                        .refresh-btn-hover:hover {
                            color: var(--primary-500) !important;
                        }
                        .ultra-wide-body {
                            flex: 1;
                            display: flex;
                            overflow: hidden;
                            background: #f1f5f9;
                            padding: 16px;
                            gap: 16px;
                        }
                        .side-setup-panel {
                            width: 350px; /* Slimmer sidebar for discovery */
                            display: flex;
                            flex-direction: column;
                            flex-shrink: 0;
                            background: white;
                            border-radius: 12px;
                            border: 1px solid #e2e8f0;
                            overflow: hidden;
                        }
                        .center-mapping-panel {
                            flex: 1;
                            background: white;
                            border-radius: 12px;
                            border: 1px solid #e2e8f0;
                            display: flex;
                            flex-direction: column;
                            overflow: hidden;
                        }
                        .side-guide-panel {
                            width: 300px;
                            background: white;
                            border-radius: 12px;
                            border: 1px solid #e2e8f0;
                            padding: 24px;
                            overflow-y: auto;
                            flex-shrink: 0;
                        }
                        .setup-card {
                            background: white;
                            border-radius: 12px;
                            border: 1px solid #e2e8f0;
                            padding: 24px;
                        }
                        .ultra-wide-header {
                            padding: 20px 32px;
                            background: white;
                            border-bottom: 1px solid #e2e8f0;
                            display: flex;
                            justify-content: space-between;
                            align-items: center;
                        }
                        .ultra-wide-footer {
                            padding: 16px 32px;
                            background: #f8fafc;
                            border-top: 1px solid #e2e8f0;
                            display: flex;
                            justify-content: flex-end; /* Ensure buttons are on the right */
                            align-items: center;
                            gap: 12px;
                        }
                        .ultra-wide-footer button {
                            flex: 0 0 auto; /* Prevent buttons from stretching */
                            width: auto;
                        }
                    `}</style>
                    <div className="ultra-wide-container">
                        <form onSubmit={handleSave} style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
                            <div className="ultra-wide-header">
                                <h3 style={{ margin: 0, fontSize: '20px', fontWeight: 700 }}>{editingProfile?.id ? "프로파일 수정" : "신규 프로파일 생성"}</h3>
                                <button type="button" className="mgmt-modal-close" onClick={handleCloseModal} style={{ fontSize: '28px' }}>&times;</button>
                            </div>

                            {/* Top Setup Bar (Horizontal) */}
                            <div style={{ background: 'white', padding: '16px 24px', borderBottom: '1px solid #e2e8f0', display: 'flex', gap: '32px', alignItems: 'flex-start' }}>
                                <div style={{ flex: '0 0 350px' }}>
                                    <label style={{ display: 'block', fontSize: '12px', fontWeight: 700, color: '#64748b', marginBottom: '6px' }}>프로파일 명칭</label>
                                    <input
                                        type="text"
                                        className="mgmt-input"
                                        required
                                        value={editingProfile?.name || ''}
                                        onChange={e => { setEditingProfile({ ...editingProfile, name: e.target.value }); setHasChanges(true); }}
                                        placeholder="예: 공장 데이터 전송"
                                        style={{ height: '36px' }}
                                    />
                                </div>
                                <div style={{ flex: 1 }}>
                                    <label style={{ display: 'block', fontSize: '12px', fontWeight: 700, color: '#64748b', marginBottom: '6px' }}>상세 설명</label>
                                    <input
                                        type="text"
                                        className="mgmt-input"
                                        value={editingProfile?.description || ''}
                                        onChange={e => { setEditingProfile({ ...editingProfile, description: e.target.value }); setHasChanges(true); }}
                                        placeholder="프로파일 용도 요약"
                                        style={{ height: '36px' }}
                                    />
                                </div>
                            </div>

                            <div className="ultra-wide-body">
                                {/* Column 1: Data Discovery (Full Height) */}
                                <div className="side-setup-panel">
                                    <div style={{ padding: '16px 20px', borderBottom: '1px solid #f1f5f9', background: '#f8fafc' }}>
                                        <h4 className="section-title" style={{ margin: 0 }}><i className="fas fa-search-plus" /> 포인트 탐색</h4>
                                    </div>
                                    <div style={{ flex: 1, overflow: 'hidden' }}>
                                        <DataPointSelector
                                            selectedPoints={editingProfile?.data_points || []}
                                            onSelect={handlePointSelect}
                                            onAddAll={handleAddAllPoints}
                                            onRemove={handlePointRemove}
                                        />
                                    </div>
                                </div>

                                {/* Column 2: Large Mapping Table */}
                                <div className="center-mapping-panel">
                                    <div style={{ padding: '20px 24px', borderBottom: '1px solid #f1f5f9', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                        <div>
                                            <h4 className="section-title" style={{ margin: 0 }}>데이터 매핑 및 외부 필드명 설정</h4>
                                            <p className="mgmt-modal-form-hint">추가된 포인트가 외부 시스템에서 어떤 필드명(Mapping Name)으로 표시될지 정의하세요.</p>
                                        </div>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', padding: '12px', background: 'var(--primary-50)', borderRadius: '12px', border: '1px solid var(--primary-100)' }}>
                                            <div style={{ display: 'flex', flexDirection: 'column' }}>
                                                <span style={{ fontSize: '10px', fontWeight: 700, color: 'var(--primary-600)', marginBottom: '2px', textTransform: 'uppercase', letterSpacing: '0.05em' }}>Bulk Site ID</span>
                                                <input
                                                    type="number"
                                                    className="mgmt-input sm"
                                                    placeholder="Site ID"
                                                    value={bulkSiteId ?? ''}
                                                    onChange={e => {
                                                        const val = parseInt(e.target.value);
                                                        setBulkSiteId(isNaN(val) ? undefined : val);
                                                    }}
                                                    style={{ width: '100px', height: '32px', border: '1px solid var(--primary-200)', borderRadius: '6px' }}
                                                />
                                            </div>
                                            <button
                                                type="button"
                                                className="btn btn-primary btn-sm"
                                                onClick={handleBulkSiteIdApply}
                                                style={{ height: '32px', marginTop: '12px', borderRadius: '6px', fontWeight: 600 }}
                                            >
                                                적용
                                            </button>
                                        </div>
                                    </div>
                                    <div style={{ flex: 1, overflow: 'auto' }}>
                                        <table className="mgmt-table">
                                            <thead style={{ position: 'sticky', top: 0, zIndex: 1, background: '#f8fafc' }}>
                                                <tr>
                                                    <th style={{ width: '18%', padding: '8px 20px' }}>내부 포인트명</th>
                                                    <th style={{ width: '35%', padding: '8px 10px' }}>매핑 명칭 (TARGET KEY)</th>
                                                    <th style={{ width: '100px', padding: '8px 10px' }}>SITE ID</th>
                                                    <th style={{ width: '90px', padding: '8px 10px' }}>SCALE</th>
                                                    <th style={{ width: '90px', padding: '8px 10px' }}>OFFSET</th>
                                                    <th style={{ width: '60px', padding: '8px 10px', textAlign: 'center' }}>삭제</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                {(editingProfile?.data_points || []).map((p: any) => (
                                                    <tr key={p.id}>
                                                        <td style={{ padding: '8px 20px' }}>
                                                            <div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>{p.name}</div>
                                                        </td>
                                                        <td style={{ padding: '8px 10px' }}>
                                                            <input
                                                                type="text"
                                                                className="mgmt-input sm"
                                                                value={p.target_field_name || ''}
                                                                onChange={e => handleMappingNameChange(p.id, e.target.value)}
                                                                placeholder="Target Key"
                                                                style={{ height: '36px', border: '1px solid #cbd5e1' }}
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 10px' }}>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input sm"
                                                                value={p.site_id || ''}
                                                                onChange={e => {
                                                                    const val = parseInt(e.target.value);
                                                                    handleSiteIdChange(p.id, isNaN(val) ? undefined : val);
                                                                }}
                                                                placeholder="ID"
                                                                style={{ height: '36px', border: '1px solid #cbd5e1' }}
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 10px' }}>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input sm"
                                                                step="0.001"
                                                                value={p.scale ?? 1}
                                                                onChange={e => handleScaleChange(p.id, parseFloat(e.target.value))}
                                                                style={{ height: '36px', border: '1px solid #cbd5e1' }}
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 10px' }}>
                                                            <input
                                                                type="number"
                                                                className="mgmt-input sm"
                                                                step="0.001"
                                                                value={p.offset ?? 0}
                                                                onChange={e => handleOffsetChange(p.id, parseFloat(e.target.value))}
                                                                style={{ height: '36px', border: '1px solid #cbd5e1' }}
                                                            />
                                                        </td>
                                                        <td style={{ padding: '8px 10px', textAlign: 'center' }}>
                                                            <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '100%' }}>
                                                                <button
                                                                    type="button"
                                                                    className="mgmt-btn-icon error"
                                                                    onClick={() => handlePointRemove(p.id)}
                                                                    style={{ margin: 0, padding: 0, width: '32px', height: '32px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}
                                                                >
                                                                    <i className="fas fa-trash" />
                                                                </button>
                                                            </div>
                                                        </td>
                                                    </tr>
                                                ))}
                                            </tbody>
                                        </table>
                                    </div>
                                </div>

                                {/* Column 3: Guide */}
                                <div className="side-guide-panel">
                                    <h4 style={{ borderBottom: '2px solid var(--primary-500)', paddingBottom: '8px' }}>Engineer's Guide</h4>
                                    <div style={{ fontSize: '13px', lineHeight: '1.6', display: 'flex', flexDirection: 'column', gap: '20px', marginTop: '16px' }}>
                                        <section>
                                            <div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>1. 프로파일 정의</div>
                                            <p style={{ margin: '4px 0' }}>데이터 수집의 **템플릿** 역할을 합니다. 의미 있는 이름을 지정해 주세요.</p>
                                        </section>
                                        <section>
                                            <div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>2. 포인트 탐색</div>
                                            <p style={{ margin: '4px 0' }}>디바이스 필터를 사용하면 대량의 포인트도 쉽게 관리할 수 있습니다.</p>
                                        </section>
                                        <section>
                                            <div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>3. 매핑 키(Key) 및 타겟 설정</div>
                                            <p style={{ margin: '4px 0' }}>외부 시스템에서 수집할 **최종 필드 명칭**입니다. </p>
                                            <p style={{ fontSize: '11px', color: '#64748b', marginTop: '8px' }}>
                                                <i className="fas fa-lightbulb" /> **매핑 구조 참고**<br />
                                                - **프로파일 매핑**: 논리적 수집 대상 정의 (이곳)<br />
                                                - **내보내기 타겟 매핑**: 목적지별 상세 변환 (Scale/Offset/필드명 재정의)
                                            </p>
                                        </section>
                                    </div>
                                </div>
                            </div>

                            <div className="ultra-wide-footer">
                                <button type="button" className="mgmt-btn-outline" style={{ height: '32px', fontSize: '12px', padding: '0 16px' }} onClick={handleCloseModal}>취소</button>
                                <button type="submit" className="mgmt-btn-primary" style={{ height: '32px', fontSize: '12px', padding: '0 20px' }}>프로파일 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default ProfileManagementTab;
