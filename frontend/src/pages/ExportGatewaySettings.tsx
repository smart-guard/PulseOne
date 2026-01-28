import React, { useState, useEffect, useCallback } from 'react';
import { Select, Tooltip, Tag, Modal, Steps, Form, Input, Space, Divider, Radio, Checkbox, Row, Col, Button } from 'antd';
import exportGatewayApi, { DataPoint, Gateway, ExportProfile, ExportTarget, Assignment, PayloadTemplate, ExportTargetMapping, ExportSchedule } from '../api/services/exportGatewayApi';
import { DeviceApiService, Device } from '../api/services/deviceApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { Pagination } from '../components/common/Pagination';
import '../styles/management.css';
import '../styles/pagination.css';

// Helper to safely extract array from various API response formats
const extractItems = (data: any): any[] => {
    if (!data) return [];
    if (Array.isArray(data)) return data;
    if (Array.isArray(data.items)) return data.items;
    if (Array.isArray(data.rows)) return data.rows;
    return [];
};

// =============================================================================
// Helper Components & Types
// =============================================================================

const Card: React.FC<{ children: React.ReactNode; style?: React.CSSProperties; className?: string }> = ({ children, style, className }) => (
    <div className={`mgmt-card ${className || ''}`} style={style}>
        {children}
    </div>
);

// =============================================================================
// Sub-Components: Target Manager
// =============================================================================

const ExportTargetManager: React.FC = () => {
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
            setTargets(targetsRes.data || []);
            setTemplates(templatesRes.data || []);
            setAllPoints(pointsRes || []);
            setProfiles(profilesRes.data || []);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    const handleOpenMapping = async (targetId: number) => {
        setMappingTargetId(targetId);
        setIsMappingModalOpen(true);
        try {
            const response = await exportGatewayApi.getTargetMappings(targetId);
            const data = response.data;
            const existingMappings = Array.isArray(data) ? data : (data && (data as any).rows ? (data as any).rows : []);

            // [Auto-Sync Logic] 만약 매핑 데이터가 전혀 없다면, 타겟에 지정된 프로파일에서 포인트를 가져와 초기화합니다.
            if (existingMappings.length === 0) {
                const target = targets.find(t => t.id === targetId);
                if (target && target.profile_id) {
                    const profile = profiles.find(p => p.id === target.profile_id);
                    if (profile && Array.isArray(profile.data_points) && profile.data_points.length > 0) {
                        const initialMappings = profile.data_points.map((p: any) => ({
                            point_id: p.id,
                            site_id: p.site_id || p.building_id, // [RESTORE] site_id focus
                            target_field_name: p.target_field_name || p.name,
                            target_description: '',
                            is_enabled: true,
                            conversion_config: { scale: 1, offset: 0 }
                        }));
                        setTargetMappings(initialMappings);
                        return;
                    }
                }
            }

            setTargetMappings(existingMappings);
            setHasMappingChanges(false);
        } catch (error) {
            console.error(error);
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

    const handleCloseMapping = async () => {
        if (hasMappingChanges || pasteData.trim()) {
            const confirmed = await confirm({
                title: '변경사항 유실 주의',
                message: '수정 중인 내용이 있습니다. 저장하지 않고 닫으시면 모든 데이터가 사라집니다. 정말 닫으시겠습니까?',
                confirmText: '닫기',
                cancelText: '취소',
                confirmButtonType: 'warning'
            });
            if (!confirmed) return;
        }
        setIsMappingModalOpen(false);
        setPasteData('');
        setTargetMappings([]);
        setMappingTargetId(null);
        setHasMappingChanges(false);
    };

    const handleSaveMappings = async () => {
        if (!mappingTargetId) return;

        // 0. Empty check
        if (targetMappings.length === 0) {
            await confirm({
                title: '데이터 없음',
                message: '저장할 매핑 데이터가 없습니다. 한 개 이상의 포인트를 매핑하거나 엑셀 데이터를 붙여넣어주세요.',
                showCancelButton: false,
                confirmButtonType: 'warning'
            });
            return;
        }

        // 1. Check for unapplied paste data
        if (pasteData.trim()) {
            const confirmed = await confirm({
                title: '적용되지 않은 데이터',
                message: '엑셀 붙여넣기 창에 아직 "매핑 적용"되지 않은 데이터가 있습니다.\n이 데이터는 저장되지 않습니다. 무시하고 진행하시겠습니까?',
                confirmText: '무시하고 저장',
                cancelText: '취소 (적용하러 가기)',
                confirmButtonType: 'warning'
            });
            if (!confirmed) return;
        }

        // 2. Check for unmapped points
        const invalidMappings = targetMappings.filter(m => !m.point_id);
        if (invalidMappings.length > 0) {
            await confirm({
                title: '매핑 누락',
                message: `매핑되지 않은 포인트가 ${invalidMappings.length}개 있습니다. 목록에서 빨간색으로 표시된 항목을 수정하거나 삭제해주세요.`,
                showCancelButton: false,
                confirmButtonType: 'warning'
            });
            return;
        }

        // 3. Final Confirmation
        const finalConfirm = await confirm({
            title: '최종 저장 확인',
            message: '현재 설정된 매핑 정보를 저장하시겠습니까?',
            confirmText: '저장',
            cancelText: '취소',
            confirmButtonType: 'primary'
        });
        if (!finalConfirm) return;

        try {
            const payload = targetMappings.map(({ _temp_name, ...rest }) => rest);
            await exportGatewayApi.saveTargetMappings(mappingTargetId, payload);

            await confirm({
                title: '저장 완료',
                message: '매핑 정보가 성공적으로 저장되었습니다.',
                showCancelButton: false,
                confirmButtonType: 'success'
            });

            setIsMappingModalOpen(false);
            setPasteData('');
            setTargetMappings([]);
            setMappingTargetId(null);
        } catch (error) {
            await confirm({
                title: '저장 실패',
                message: '매핑 정보를 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleImportFromProfile = async () => {
        if (!mappingTargetId) return;
        const target = targets.find(t => t.id === mappingTargetId);
        if (!target || !target.profile_id) {
            await confirm({ title: '프로파일 없음', message: '이 타겟에 연결된 프로파일이 없습니다.', showCancelButton: false, confirmButtonType: 'warning' });
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
                response = await exportGatewayApi.updateTarget(editingTarget.id, editingTarget);
            } else {
                response = await exportGatewayApi.createTarget(editingTarget!);
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
                                    <div style={{ flex: 1, minWidth: '280px' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>기본 정보</div>
                                        <div className="mgmt-modal-form-group">
                                            <label>타겟 명칭</label>
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                required
                                                value={editingTarget?.name || ''}
                                                onChange={e => { setEditingTarget({ ...editingTarget, name: e.target.value }); setHasChanges(true); }}
                                                placeholder="예: 실시간 전력 데이터 수집기"
                                            />
                                            <div className="mgmt-modal-form-hint">시스템에서 식별하기 위한 고유 이름입니다.</div>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>상세 설명</label>
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                value={editingTarget?.description || ''}
                                                onChange={e => { setEditingTarget({ ...editingTarget, description: e.target.value }); setHasChanges(true); }}
                                                placeholder="타겟에 대한 설명을 입력하세요"
                                            />
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>적용 프로파일</label>
                                            <select
                                                className="mgmt-select"
                                                required
                                                value={editingTarget?.profile_id || ''}
                                                onChange={e => { setEditingTarget({ ...editingTarget, profile_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                            >
                                                <option value="">(프로파일 선택)</option>
                                                {profiles.map(p => (
                                                    <option key={p.id} value={p.id}>{p.name}</option>
                                                ))}
                                            </select>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>전송 프로토콜</label>
                                            <select
                                                className="mgmt-select"
                                                value={editingTarget?.target_type || 'http'}
                                                onChange={e => { setEditingTarget({ ...editingTarget, target_type: e.target.value }); setHasChanges(true); }}
                                            >
                                                <option value="http">HTTP POST (JSON)</option>
                                                <option value="mqtt">MQTT Publisher</option>
                                                <option value="influxdb">InfluxDB Line Protocol</option>
                                                <option value="s3">AWS S3 Storage</option>
                                                <option value="kafka">Apache Kafka</option>
                                            </select>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>전송 모드 (Export Mode)</label>
                                            <select
                                                className="mgmt-select"
                                                value={editingTarget?.export_mode || 'on_change'}
                                                onChange={e => { setEditingTarget({ ...editingTarget, export_mode: e.target.value }); setHasChanges(true); }}
                                            >
                                                <option value="on_change">즉시 전송 (Event Triggered)</option>
                                                <option value="batch">배치 전송 (Buffered Batch)</option>
                                            </select>
                                            <div className="mgmt-modal-form-hint">
                                                {editingTarget?.export_mode === 'batch'
                                                    ? '데이터를 버퍼에 모아서 일괄적으로 전송합니다. (데이터 효율 최적화)'
                                                    : '데이터가 수집될 때마다 즉시 전송합니다. (실시간성 강조)'}
                                            </div>
                                        </div>
                                        {editingTarget?.export_mode === 'batch' && (
                                            <div className="mgmt-modal-form-group">
                                                <label>배치 크기 (Batch Size)</label>
                                                <input
                                                    type="number"
                                                    className="mgmt-input"
                                                    value={editingTarget?.batch_size || 100}
                                                    onChange={e => setEditingTarget({ ...editingTarget, batch_size: parseInt(e.target.value) })}
                                                    min={1}
                                                    max={5000}
                                                />
                                                <div className="mgmt-modal-form-hint">최대 몇 개의 메시지를 버퍼링한 후 전송할지 설정합니다.</div>
                                            </div>
                                        )}
                                        <div className="mgmt-modal-form-group">
                                            <label>최소 전송 간격 (Interval, ms) - 옵션</label>
                                            <input
                                                type="number"
                                                className="mgmt-input"
                                                value={editingTarget?.export_interval || 0}
                                                onChange={e => setEditingTarget({ ...editingTarget, export_interval: parseInt(e.target.value) })}
                                                min={0}
                                                step={100}
                                            />
                                            <div className="mgmt-modal-form-hint">배치 모드에서 타임아웃으로도 사용됩니다. (0: 사용 안함)</div>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>페이로드 템플릿</label>
                                            <select
                                                className="mgmt-select"
                                                value={editingTarget?.template_id || ''}
                                                onChange={e => setEditingTarget({ ...editingTarget, template_id: e.target.value ? parseInt(e.target.value) : undefined })}
                                            >
                                                <option value="">(템플릿 사용 안함 - 기본 포맷)</option>
                                                {templates.filter(t => t.is_active).map(t => (
                                                    <option key={t.id} value={t.id}>{t.name}</option>
                                                ))}
                                            </select>
                                            {templates.length === 0 && <div className="mgmt-modal-form-hint error">등록된 활성 템플릿이 없습니다.</div>}
                                        </div>
                                    </div>
                                    {/* Column 2: Specific Inputs */}
                                    <div style={{ flex: 1.2, borderLeft: '1px solid #eee', paddingLeft: '20px', minWidth: '320px', display: 'flex', flexDirection: 'column' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>상세 설정 / 연결 정보</div>
                                        {(editingTarget?.target_type || 'http') === 'http' ? (
                                            <>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Endpoint URL (전송 주소)</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        required
                                                        placeholder="예: http://api.myserver.com/data"
                                                        value={(() => {
                                                            try {
                                                                const c = typeof editingTarget?.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget?.config || {});
                                                                return c.url || '';
                                                            } catch { return ''; }
                                                        })()}
                                                        onChange={e => {
                                                            const val = e.target.value;
                                                            let c = {};
                                                            try {
                                                                c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {});
                                                            } catch { }
                                                            c = { ...c, url: val };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>인증 키 (Authorization Header)</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="예: Bearer eyJhbGciOi..."
                                                        value={(() => {
                                                            try {
                                                                const c = typeof editingTarget?.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget?.config || {});
                                                                return c.headers?.Authorization || '';
                                                            } catch { return ''; }
                                                        })()}
                                                        onChange={e => {
                                                            const val = e.target.value;
                                                            let c: any = {};
                                                            try {
                                                                c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {});
                                                            } catch { }

                                                            const headers = { ...(c.headers || {}), Authorization: val };
                                                            c = { ...c, headers };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                    <div className="mgmt-modal-form-hint">필요한 경우 Bearer Token 등을 입력하세요.</div>
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>X-API-KEY (전용 헤더)</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="API Gateway용 키를 입력하세요"
                                                        value={(() => {
                                                            try {
                                                                const c = typeof editingTarget?.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget?.config || {});
                                                                return c.auth?.apiKey || '';
                                                            } catch { return ''; }
                                                        })()}
                                                        onChange={e => {
                                                            const val = e.target.value;
                                                            let c: any = {};
                                                            try {
                                                                c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {});
                                                            } catch { }

                                                            const auth = { ...(c.auth || {}), type: 'x-api-key', apiKey: val };
                                                            c = { ...c, auth };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                        }}
                                                    />
                                                    <div className="mgmt-modal-form-hint">AWS API Gateway 등에서 사용하는 x-api-key 헤더로 전송됩니다.</div>
                                                </div>
                                            </>
                                        ) : editingTarget?.target_type === 's3' ? (
                                            <>
                                                <div className="mgmt-modal-form-group">
                                                    <label>AWS S3 Bucket</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="예: my-data-bucket"
                                                        value={(() => { try { return (typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : editingTarget.config).bucket || ''; } catch { return ''; } })()}
                                                        onChange={e => {
                                                            let c: any = {};
                                                            try { c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {}); } catch { }
                                                            c = { ...c, bucket: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                        }}
                                                    />
                                                </div>
                                                <div style={{ display: 'flex', gap: '10px' }}>
                                                    <div className="mgmt-modal-form-group" style={{ flex: 1 }}>
                                                        <label>Region</label>
                                                        <input
                                                            type="text"
                                                            className="mgmt-input"
                                                            placeholder="예: ap-northeast-2"
                                                            value={(() => { try { return (typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : editingTarget.config).region || ''; } catch { return ''; } })()}
                                                            onChange={e => {
                                                                let c: any = {};
                                                                try { c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {}); } catch { }
                                                                c = { ...c, region: e.target.value };
                                                                setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                            }}
                                                        />
                                                    </div>
                                                    <div className="mgmt-modal-form-group" style={{ flex: 1 }}>
                                                        <label>Path Prefix (Folder)</label>
                                                        <input
                                                            type="text"
                                                            className="mgmt-input"
                                                            placeholder="예: data/logs/"
                                                            value={(() => { try { return (typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : editingTarget.config).prefix || ''; } catch { return ''; } })()}
                                                            onChange={e => {
                                                                let c: any = {};
                                                                try { c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {}); } catch { }
                                                                c = { ...c, prefix: e.target.value };
                                                                setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                            }}
                                                        />
                                                    </div>
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Access Key ID</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="AKIA..."
                                                        value={(() => { try { return (typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : editingTarget.config).accessKey || ''; } catch { return ''; } })()}
                                                        onChange={e => {
                                                            let c: any = {};
                                                            try { c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {}); } catch { }
                                                            c = { ...c, accessKey: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>Secret Access Key</label>
                                                    <input
                                                        type="password"
                                                        className="mgmt-input"
                                                        placeholder="Secret Key..."
                                                        value={(() => { try { return (typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : editingTarget.config).secretKey || ''; } catch { return ''; } })()}
                                                        onChange={e => {
                                                            let c: any = {};
                                                            try { c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {}); } catch { }
                                                            c = { ...c, secretKey: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                        }}
                                                    />
                                                </div>
                                            </>
                                        ) : (
                                            <div className="mgmt-alert info">
                                                이 타겟 유형은 별도의 입력 폼을 제공하지 않습니다.
                                                오른쪽의 JSON 설정 창을 사용하여 직접 구성하세요.
                                            </div>
                                        )}
                                        {/* Activation Checkbox Moved below Column 2 inputs */}
                                        <div style={{ marginTop: 'auto', paddingTop: '15px', borderTop: '1px solid #eee' }}>
                                            <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center' }}>
                                                <input
                                                    type="checkbox"
                                                    checked={editingTarget?.is_enabled ?? true}
                                                    onChange={e => setEditingTarget({ ...editingTarget, is_enabled: e.target.checked })}
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
                                            style={{ flex: 1, fontFamily: 'monospace', fontSize: '12px', resize: 'none', minHeight: '300px' }}
                                            value={typeof editingTarget?.config === 'string' ? editingTarget.config : JSON.stringify(editingTarget?.config || {}, null, 2)}
                                            onChange={e => setEditingTarget({ ...editingTarget, config: e.target.value })}
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
                                                                    style={{ width: '100%' }}
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

// =============================================================================
// Sub-Components: Payload Template Manager
// =============================================================================

const PayloadTemplateManager: React.FC = () => {
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingTemplate, setEditingTemplate] = useState<Partial<PayloadTemplate> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
    const { confirm } = useConfirmContext();

    const fetchTemplates = async () => {
        setLoading(true);
        try {
            const response = await exportGatewayApi.getTemplates();
            setTemplates(response.data || []);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => { fetchTemplates(); }, []);

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
            // JSON 유효성 검사
            let templateJson = editingTemplate?.template_json;
            if (typeof templateJson === 'string') {
                try {
                    templateJson = JSON.parse(templateJson);
                } catch (parseError) {
                    await confirm({
                        title: 'JSON 형식 오류',
                        message: `유효하지 않은 JSON 형식입니다.\n\n[상세 내역]\n${parseError instanceof Error ? parseError.message : String(parseError)}\n\n팁: {{bd}}와 같은 숫지형 변수도 "{{bd}}"와 같이 따옴표로 감싸야 유효한 JSON이 됩니다.`,
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

            if (editingTemplate?.id) {
                await exportGatewayApi.updateTemplate(editingTemplate.id, dataToSave);
            } else {
                await exportGatewayApi.createTemplate(dataToSave);
            }

            await confirm({
                title: '저장 완료',
                message: '템플릿이 성공적으로 저장되었습니다.',
                showCancelButton: false,
                confirmButtonType: 'success'
            });
            setIsModalOpen(false);
            setHasChanges(false);
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

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '템플릿 삭제 확인',
            message: '이 페이로드 템플릿을 삭제하시겠습니까? 이 템플릿을 사용하는 타겟들의 전송이 실패할 수 있습니다.',
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteTemplate(id);
            fetchTemplates();
        } catch (error) {
            await confirm({ title: '삭제 실패', message: '템플릿을 삭제하는 중 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>페이로드 템플릿 설정</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingTemplate({ name: '', system_type: 'custom', template_json: '{\n  "device": "{device_name}",\n  "point": "{point_name}",\n  "value": {value},\n  "timestamp": "{timestamp}"\n}', is_active: true }); setIsModalOpen(true); }}>
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
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => {
                                            const jsonStr = typeof t.template_json === 'string' ? t.template_json : JSON.stringify(t.template_json, null, 2);
                                            setEditingTemplate({ ...t, template_json: jsonStr });
                                            setIsModalOpen(true);
                                        }} style={{ width: 'auto' }}>수정</button>
                                        <button className="btn btn-outline btn-xs btn-danger" onClick={() => handleDelete(t.id)} style={{ width: 'auto' }}>삭제</button>
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
                    <div className="mgmt-modal-content" style={{ maxWidth: '600px' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">{editingTemplate?.id ? "템플릿 수정" : "페이로드 템플릿 추가"}</h3>
                            <button className="mgmt-modal-close" onClick={handleCloseModal}>&times;</button>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body">
                                <div className="mgmt-modal-form-section">
                                    <div className="mgmt-modal-form-group">
                                        <label>템플릿 명칭</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            required
                                            value={editingTemplate?.name || ''}
                                            onChange={e => { setEditingTemplate({ ...editingTemplate, name: e.target.value }); setHasChanges(true); }}
                                            placeholder="예: 표준 JSON 전송 포맷"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>시스템 유형</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            value={editingTemplate?.system_type || ''}
                                            onChange={e => { setEditingTemplate({ ...editingTemplate, system_type: e.target.value }); setHasChanges(true); }}
                                            placeholder="예: AWS IoT, Azure EventHub 등"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>템플릿 구조 (JSON)</label>
                                        <textarea
                                            className="mgmt-input"
                                            required
                                            value={editingTemplate?.template_json || ''}
                                            onChange={e => { setEditingTemplate({ ...editingTemplate, template_json: e.target.value }); setHasChanges(true); }}
                                            placeholder='{ "key": "{value}" }'
                                            style={{ height: '200px', fontFamily: 'monospace', fontSize: '12px', padding: '10px' }}
                                        />
                                        <div className="mgmt-modal-form-hint">
                                            치환자 사용 가능: {"{device_name}"}, {"{point_name}"}, {"{value}"}, {"{timestamp}"}, {"{description}"}
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer">
                                <button type="button" className="mgmt-btn mgmt-btn-outline" style={{ height: '36px' }} onClick={handleCloseModal}>취소</button>
                                <button type="submit" className="mgmt-btn mgmt-btn-primary" style={{ height: '36px' }}>템플릿 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

// =============================================================================
// Sub-Components: Profile Builder
// =============================================================================

// =============================================================================
// Sub-Components: Data Point Selector
// =============================================================================

const DataPointSelector: React.FC<{
    selectedPoints: any[];
    onSelect: (point: any) => void;
    onAddAll?: (points: any[]) => void;
    onRemove: (pointId: number) => void;
}> = ({ selectedPoints, onSelect, onAddAll, onRemove }) => {
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [devices, setDevices] = useState<Device[]>([]);
    const [selectedDeviceId, setSelectedDeviceId] = useState<number | undefined>();
    const [loading, setLoading] = useState(false);
    const [searchTerm, setSearchTerm] = useState('');
    const [debouncedSearch, setDebouncedSearch] = useState('');

    // Fetch devices on mount
    useEffect(() => {
        const fetchDevices = async () => {
            console.log('--- [DataPointSelector] Fetching devices START ---');
            try {
                // 1. Try standard Service call
                const res = await DeviceApiService.getDevices({ limit: 1000 });
                let items: Device[] = [];
                if (res && res.success) {
                    const rawData: any = res.data;
                    // Standard: res.data.items
                    if (rawData && Array.isArray(rawData.items)) {
                        items = rawData.items;
                    }
                    // Fallback 1: res.data is the array
                    else if (Array.isArray(rawData)) {
                        items = rawData;
                    }
                    // Fallback 2: res.data.data.items
                    else if (rawData && rawData.data && Array.isArray(rawData.data.items)) {
                        items = rawData.data.items;
                    }
                }

                console.log(`--- [DataPointSelector] Found ${items.length} items. ---`);

                // Force a test item if empty to verify UI
                if (items.length === 0) {
                    console.warn('--- [DataPointSelector] API returned empty, adding test item ---');
                }

                setDevices(items);
            } catch (e) {
                console.error('--- [DataPointSelector] Device fetch CRASH: ---', e);
            }
        };
        fetchDevices();
    }, []);

    // Debounce search term
    useEffect(() => {
        const timer = setTimeout(() => {
            setDebouncedSearch(searchTerm);
        }, 300);
        return () => clearTimeout(timer);
    }, [searchTerm]);

    // Generic fetch function so it can be called on mount and on updates
    const fetchPoints = useCallback(async (search: string = debouncedSearch, deviceId?: number) => {
        console.log('--- [DataPointSelector] Fetching points with:', { search, deviceId });
        setLoading(true);
        try {
            const result = await exportGatewayApi.getDataPoints(search, deviceId);
            // Since result is now DataPoint[] or {items: [] } depending on my last edit attempt
            // I standardized exportGatewayApi.getDataPoints to return items object
            let points: DataPoint[] = [];
            if (Array.isArray(result)) {
                points = result;
            } else if (result && (result as any).items) {
                points = (result as any).items;
            }

            console.log(`--- [DataPointSelector] Points fetched: ${points.length} ---`);
            setAllPoints(points);
        } catch (e) {
            console.error('--- [DataPointSelector] Points fetch error: ---', e);
        } finally {
            setLoading(false);
        }
    }, [debouncedSearch]);

    // Initial fetch to show all points so users don't have to guess/search
    useEffect(() => {
        // Ensure devices are fetched first if needed, or just fetch all points
        fetchPoints('', undefined);
    }, [fetchPoints]);

    // Fetch points when debounced search or device filter changes
    useEffect(() => {
        fetchPoints(debouncedSearch, selectedDeviceId);
    }, [debouncedSearch, selectedDeviceId, fetchPoints]);

    const isSelected = (id: number) => selectedPoints.some(p => p.id === id);

    return (
        <div className="point-selector-container" style={{
            background: 'white',
            display: 'flex',
            flexDirection: 'column',
            gap: '16px',
            height: '100%',
            padding: '16px'
        }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <div>
                    <label style={{ display: 'block', fontSize: '12px', fontWeight: 600, color: '#64748b', marginBottom: '6px' }}>장치 선택 필터</label>
                    <Select
                        placeholder="장치 선택 (전체/장치별)"
                        style={{ width: '100%', height: '36px' }}
                        value={selectedDeviceId ?? null}
                        onChange={(val) => setSelectedDeviceId(val ?? undefined)}
                        getPopupContainer={trigger => trigger.parentElement}
                        options={[
                            { value: null, label: '전체 장치 (All Devices)' },
                            ...devices.map(d => ({
                                value: d.id,
                                label: d.device_type ? `${d.name} [${d.device_type}]` : d.name
                            }))
                        ]}
                    />
                </div>
                <div>
                    <label style={{ display: 'block', fontSize: '12px', fontWeight: 600, color: '#64748b', marginBottom: '6px' }}>키워드 검색</label>
                    <div className="search-box" style={{ position: 'relative' }}>
                        <i className="fas fa-search" style={{
                            position: 'absolute',
                            left: '12px',
                            top: '50%',
                            transform: 'translateY(-50%)',
                            color: '#94a3b8',
                            zIndex: 1
                        }} />
                        <input
                            type="text"
                            className="mgmt-input sm"
                            placeholder="명칭 또는 주소 입력..."
                            value={searchTerm}
                            onChange={e => setSearchTerm(e.target.value)}
                            style={{ paddingLeft: '36px', height: '36px', borderRadius: '8px', border: '1px solid #cbd5e1' }}
                        />
                    </div>
                </div>
            </div>

            <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column', border: '1px solid #e2e8f0', borderRadius: '8px', minHeight: '300px' }}>
                <div style={{ padding: '8px 12px', background: '#f8fafc', borderBottom: '1px solid #e2e8f0', fontSize: '11px', fontWeight: 700, color: '#64748b', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                        <span>탐색 결과 ({allPoints.length})</span>
                        {selectedDeviceId && allPoints.length > 0 && onAddAll && (
                            <button
                                type="button"
                                className="btn btn-xs btn-outline"
                                style={{ fontSize: '10px', height: '18px', padding: '0 6px', lineHeight: '1' }}
                                onClick={() => onAddAll(allPoints)}
                            >
                                <i className="fas fa-plus-double" /> 전체 추가
                            </button>
                        )}
                    </div>
                    <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                        {loading && <i className="fas fa-spinner fa-spin" />}
                        <button
                            type="button"
                            onClick={() => fetchPoints(debouncedSearch, selectedDeviceId)}
                            style={{
                                background: 'none',
                                border: 'none',
                                color: '#64748b',
                                cursor: 'pointer',
                                padding: '2px',
                                display: 'flex',
                                alignItems: 'center',
                                transition: 'color 0.2s'
                            }}
                            title="새로고침"
                            className="refresh-btn-hover"
                        >
                            <i className="fas fa-sync-alt" />
                        </button>
                    </div>
                </div>
                <div className="point-list" style={{ flex: 1, overflowY: 'auto', background: 'white', minHeight: '200px' }}>
                    {loading && allPoints.length === 0 ? (
                        <div style={{ textAlign: 'center', padding: '40px 20px', color: '#94a3b8' }}>
                            <i className="fas fa-circle-notch fa-spin fa-2x" style={{ marginBottom: '12px' }} /><br />
                            데이터를 불러오는 중...
                        </div>
                    ) : allPoints.length === 0 ? (
                        <div style={{ textAlign: 'center', padding: '60px 20px', color: '#94a3b8' }}>
                            <i className="fas fa-search fa-3x" style={{ marginBottom: '16px', opacity: 0.2 }} /><br />
                            검색 결과가 없습니다.<br />
                            <span style={{ fontSize: '12px', opacity: 0.8 }}>다른 키워드나 장치를 선택해 보세요.</span>
                        </div>
                    ) : (
                        allPoints.map(p => {
                            const selected = isSelected(p.id);
                            return (
                                <div key={p.id} style={{
                                    display: 'flex',
                                    justifyContent: 'space-between',
                                    alignItems: 'center',
                                    padding: '10px 14px',
                                    borderBottom: '1px solid #f1f5f9',
                                    transition: 'background 0.2s',
                                    cursor: 'pointer'
                                }}
                                    onClick={() => selected ? onRemove(p.id) : onSelect(p)}
                                    className="point-item-hover">
                                    <div style={{ fontSize: '12px', flex: 1, minWidth: 0 }}>
                                        <div style={{ fontWeight: 600, color: '#1e293b', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{p.name}</div>
                                        <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>
                                            {p.device_name} • {p.address || '-'}
                                        </div>
                                    </div>
                                    <div style={{ marginLeft: '12px' }}>
                                        {selected ? (
                                            <span style={{ color: '#ef4444', fontSize: '18px' }}><i className="fas fa-check-circle" /></span>
                                        ) : (
                                            <span style={{ color: '#3b82f6', fontSize: '18px' }}><i className="fas fa-plus-circle" /></span>
                                        )}
                                    </div>
                                </div>
                            );
                        })
                    )}
                </div>
            </div>
        </div>
    );
};

// =============================================================================
// Sub-Components: Profile Builder
// =============================================================================

const ExportProfileBuilder: React.FC = () => {
    const [profiles, setProfiles] = useState<ExportProfile[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingProfile, setEditingProfile] = useState<Partial<ExportProfile> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
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
                    // Preference: Saved mapping override > Master Metadata > Stale local snapshot > Default
                    site_id: lm?.site_id || lm?.building_id || mp?.site_id || p.site_id || p.building_id || 0,
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
                                    <div style={{ padding: '20px 24px', borderBottom: '1px solid #f1f5f9' }}>
                                        <h4 className="section-title" style={{ margin: 0 }}>데이터 매핑 및 외부 필드명 설정</h4>
                                        <p className="mgmt-modal-form-hint">추가된 포인트가 외부 시스템에서 어떤 필드명(Mapping Name)으로 표시될지 정의하세요.</p>
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

// =============================================================================
// Sub-Components: Schedule Manager
// =============================================================================

const ExportScheduleManager: React.FC = () => {
    const [schedules, setSchedules] = useState<any[]>([]);
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [profiles, setProfiles] = useState<ExportProfile[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingSchedule, setEditingSchedule] = useState<Partial<any> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
    const { confirm } = useConfirmContext();

    const fetchData = async () => {
        setLoading(true);
        try {
            const [schedulesRes, targetsRes, profilesRes] = await Promise.all([
                exportGatewayApi.getSchedules(),
                exportGatewayApi.getTargets(),
                exportGatewayApi.getProfiles()
            ]);
            setSchedules(schedulesRes.data || []);
            setTargets(targetsRes.data || []);
            setProfiles(profilesRes.data || []);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
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

    useEffect(() => { fetchData(); }, []);

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();

        if (!hasChanges && editingSchedule?.id) {
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
            if (editingSchedule?.id) {
                response = await exportGatewayApi.updateSchedule(editingSchedule.id, editingSchedule);
            } else {
                response = await exportGatewayApi.createSchedule(editingSchedule!);
            }

            if (response.success) {
                await confirm({
                    title: '저장 완료',
                    message: '스케줄이 성공적으로 저장되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
                setHasChanges(false);
                fetchData();
            } else {
                await confirm({
                    title: '저장 실패',
                    message: response.message || '스케줄을 저장하는 중 오류가 발생했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: '저장 실패',
                message: '스케줄을 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '스케줄 삭제 확인',
            message: '이 전송 스케줄을 삭제하시겠습니까?',
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteSchedule(id);
            fetchData();
        } catch (error) {
            await confirm({ title: '삭제 실패', message: '스케줄을 삭제하는 중 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>배치 전송 스케줄 관리</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingSchedule({ schedule_name: '', cron_expression: '0 0 * * *', data_range: 'hour', lookback_periods: 1, timezone: 'Asia/Seoul', is_enabled: true }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> 스케줄 추가
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>스케줄 이름</th>
                            <th>전송 타겟</th>
                            <th>Cron 표현식</th>
                            <th>상태</th>
                            <th>마지막 실행</th>
                            <th>관리</th>
                        </tr>
                    </thead>
                    <tbody>
                        {schedules.map(s => (
                            <tr key={s.id}>
                                <td>
                                    <div style={{ fontWeight: 600 }}>{s.schedule_name}</div>
                                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>{s.description}</div>
                                </td>
                                <td>{s.target_name || `ID: ${s.target_id}`}</td>
                                <td><code style={{ background: '#f0f0f0', padding: '2px 4px', borderRadius: '4px' }}>{s.cron_expression}</code></td>
                                <td>
                                    <span className={`mgmt-badge ${s.is_enabled ? 'success' : 'neutral'}`}>
                                        {s.is_enabled ? '활성' : '비활성'}
                                    </span>
                                </td>
                                <td>
                                    {s.last_run_at ? new Date(s.last_run_at).toLocaleString() : '-'}
                                    {s.last_status && <div style={{ fontSize: '10px', color: s.last_status === 'success' ? 'var(--success-600)' : 'var(--error-600)' }}>{s.last_status}</div>}
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => { setEditingSchedule(s); setIsModalOpen(true); }} style={{ width: 'auto' }}>수정</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs mgmt-btn-error" onClick={() => handleDelete(s.id)} style={{ width: 'auto' }}>삭제</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-content" style={{ maxWidth: '600px' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">{editingSchedule?.id ? "스케줄 수정" : "전송 스케줄 추가"}</h3>
                            <button className="mgmt-modal-close" onClick={() => setIsModalOpen(false)}>&times;</button>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body">
                                <div className="mgmt-modal-form-section">
                                    <div className="mgmt-modal-form-group">
                                        <label>스케줄 명칭</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            required
                                            value={editingSchedule?.schedule_name || ''}
                                            onChange={e => setEditingSchedule({ ...editingSchedule, schedule_name: e.target.value })}
                                            placeholder="예: 일일 마감 데이터 전송"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>전송 대상 타겟</label>
                                        <select
                                            className="mgmt-select"
                                            required
                                            value={editingSchedule?.target_id || ''}
                                            onChange={e => { setEditingSchedule({ ...editingSchedule, target_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                        >
                                            <option value="">(타겟 선택)</option>
                                            {targets.map(t => (
                                                <option key={t.id} value={t.id}>{t.name} ({t.target_type})</option>
                                            ))}
                                        </select>
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>Cron 표현식</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            required
                                            value={editingSchedule?.cron_expression || ''}
                                            onChange={e => { setEditingSchedule({ ...editingSchedule, cron_expression: e.target.value }); setHasChanges(true); }}
                                            placeholder="예: 0 0 * * * (매일 자정)"
                                        />
                                        <div className="mgmt-modal-form-hint">분 시 일 월 요일 형식입니다. (매분: * * * * *)</div>
                                    </div>
                                    <div style={{ display: 'flex', gap: '15px' }}>
                                        <div className="mgmt-modal-form-group" style={{ flex: 1 }}>
                                            <label>데이터 범위 (Data Range)</label>
                                            <select
                                                className="mgmt-select"
                                                value={editingSchedule?.data_range || 'hour'}
                                                onChange={e => { setEditingSchedule({ ...editingSchedule, data_range: e.target.value }); setHasChanges(true); }}
                                            >
                                                <option value="minute">분 단위 (Minutes)</option>
                                                <option value="hour">시간 단위 (Hours)</option>
                                                <option value="day">일 단위 (Days)</option>
                                            </select>
                                        </div>
                                        <div className="mgmt-modal-form-group" style={{ flex: 1 }}>
                                            <label>조회 기간 (Periods)</label>
                                            <input
                                                type="number"
                                                className="mgmt-input"
                                                value={editingSchedule?.lookback_periods || 1}
                                                onChange={e => { setEditingSchedule({ ...editingSchedule, lookback_periods: parseInt(e.target.value) }); setHasChanges(true); }}
                                                min={1}
                                            />
                                        </div>
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label className="checkbox-label">
                                            <input
                                                type="checkbox"
                                                checked={editingSchedule?.is_enabled ?? true}
                                                onChange={e => { setEditingSchedule({ ...editingSchedule, is_enabled: e.target.checked }); setHasChanges(true); }}
                                                style={{ marginRight: '8px' }}
                                            />
                                            스케줄 활성화
                                        </label>
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer">
                                <button type="button" className="btn-outline" onClick={handleCloseModal}>취소</button>
                                <button type="submit" className="btn-primary">스케줄 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};


// =============================================================================
// Sub-Components: Gateway Dashboard (Refined)
// =============================================================================

const GatewayList: React.FC<{
    gateways: Gateway[];
    assignments: Record<number, Assignment[]>;
    onRefresh: () => void;
    onDeploy: (gateway: Gateway) => void;
    onStart: (gateway: Gateway) => Promise<void>;
    onStop: (gateway: Gateway) => Promise<void>;
    onRestart: (gateway: Gateway) => Promise<void>;
    onDetails: (gateway: Gateway) => void;
}> = ({ gateways, assignments, onRefresh, onDeploy, onStart, onStop, onRestart, onDetails }) => {
    const [actionLoading, setActionLoading] = useState<number | null>(null);

    const handleAction = async (gwId: number, action: (gw: Gateway) => Promise<void>) => {
        setActionLoading(gwId);
        try {
            await action(gateways.find(g => g.id === gwId)!);
        } finally {
            setActionLoading(null);
        }
    };
    return (
        <div className="gateway-list">
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>등록된 게이트웨이</h3>
                <button className="btn btn-outline btn-sm" onClick={onRefresh}>
                    <i className="fas fa-sync-alt" /> 새로고침
                </button>
            </div>
            {gateways.length === 0 ? (
                <div className="empty-state" style={{ padding: '60px 0', textAlign: 'center', color: 'var(--neutral-400)' }}>
                    <i className="fas fa-server fa-3x" style={{ marginBottom: '16px', opacity: 0.3 }} />
                    <p>등록된 게이트웨이가 없습니다.</p>
                </div>
            ) : (
                <div className="mgmt-grid">
                    {gateways.map(gw => (
                        <div key={gw.id} className="mgmt-card gateway-card">
                            <div className="mgmt-card-header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                                <div className="mgmt-card-title" style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                                    <h4 style={{ margin: 0, fontSize: '15px' }}>{gw.name}</h4>
                                    {gw.description && (
                                        <div style={{ fontSize: '12px', color: 'var(--neutral-500)', fontWeight: 400, marginTop: '2px' }}>
                                            {gw.description}
                                        </div>
                                    )}
                                    <div style={{ display: 'flex', gap: '4px', marginTop: '4px' }}>
                                        <span className="badge neutral-light" style={{ fontSize: '10px', padding: '2px 6px' }}>
                                            <i className="fas fa-satellite-dish" style={{ marginRight: '4px' }} />
                                            EXPORT GATEWAY
                                        </span>
                                    </div>
                                </div>
                                <div className={`badge ${gw.live_status?.status === 'online' ? 'success' : 'neutral'}`}>
                                    <i className={`fas fa-circle`} style={{ fontSize: '8px', marginRight: '5px' }} />
                                    {gw.live_status?.status === 'online' ? 'ONLINE' : 'OFFLINE'}
                                </div>
                            </div>

                            <div className="mgmt-card-body" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
                                <div className="info-list" style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '13px' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                        <span style={{ color: 'var(--neutral-500)' }}>IP Address:</span>
                                        <span style={{ fontWeight: 500, fontFamily: 'monospace' }}>{gw.ip_address}</span>
                                    </div>
                                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                        <span style={{ color: 'var(--neutral-500)' }}>Last Seen:</span>
                                        <span>{gw.last_seen ? new Date(gw.last_seen).toLocaleString() : '-'}</span>
                                    </div>
                                    {gw.live_status && (
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>Memory:</span>
                                            <span>{gw.live_status.memory_usage} MB</span>
                                        </div>
                                    )}
                                    <div style={{ marginTop: '8px', padding: '8px', background: 'var(--neutral-50)', borderRadius: '4px' }}>
                                        <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '4px', display: 'flex', justifyContent: 'space-between' }}>
                                            <span>프로세스 상태</span>
                                            <span style={{ fontWeight: 600, color: gw.processes && gw.processes.length > 0 ? 'var(--success-600)' : 'var(--error-600)' }}>
                                                {gw.processes && gw.processes.length > 0 ? 'RUNNING' : 'STOPPED'}
                                            </span>
                                        </div>
                                        {gw.processes && gw.processes.length > 0 && (
                                            <div style={{ fontSize: '11px', fontFamily: 'monospace', color: 'var(--neutral-600)' }}>
                                                PID: {gw.processes[0].pid} | CPU: {gw.processes[0].cpu}%
                                            </div>
                                        )}
                                        <div style={{ display: 'flex', gap: '4px', marginTop: '8px' }}>
                                            {(gw.processes?.[0]?.status !== 'running' && gw.live_status?.status !== 'online') ? (
                                                <button
                                                    className="btn btn-outline btn-xs"
                                                    style={{ flex: 1 }}
                                                    onClick={() => handleAction(gw.id, onStart)}
                                                    disabled={actionLoading === gw.id}
                                                >
                                                    {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-play" style={{ fontSize: '10px' }} />} 시작
                                                </button>
                                            ) : (
                                                <>
                                                    <button
                                                        className="btn btn-outline btn-xs btn-danger"
                                                        style={{ flex: 1 }}
                                                        onClick={() => handleAction(gw.id, onStop)}
                                                        disabled={actionLoading === gw.id}
                                                    >
                                                        {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-stop" style={{ fontSize: '10px' }} />} 중지
                                                    </button>
                                                    <button
                                                        className="btn btn-outline btn-xs"
                                                        style={{ flex: 1 }}
                                                        onClick={() => handleAction(gw.id, onRestart)}
                                                        disabled={actionLoading === gw.id}
                                                    >
                                                        {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-redo" style={{ fontSize: '10px' }} />} 재시작
                                                    </button>
                                                </>
                                            )}
                                        </div>
                                    </div>
                                </div>

                                <div className="assigned-profiles" style={{ marginTop: '16px', paddingTop: '16px', borderTop: '1px solid var(--neutral-100)', flex: 1 }}>
                                    <div style={{ fontSize: '11px', fontWeight: 600, color: 'var(--neutral-400)', textTransform: 'uppercase', marginBottom: '8px' }}>포함된 프로파일</div>
                                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                                        {(assignments[gw.id] || []).length === 0 ? (
                                            <span style={{ fontSize: '12px', color: 'var(--neutral-400)', fontStyle: 'italic' }}>할당된 프로파일 없음</span>
                                        ) : (
                                            assignments[gw.id].map(a => (
                                                <span key={a.id} className="badge neutral-light" style={{ fontSize: '11px' }}>{a.name}</span>
                                            ))
                                        )}
                                    </div>
                                </div>
                            </div>

                            <div className="mgmt-card-footer" style={{ borderTop: '1px solid var(--neutral-100)', paddingTop: '12px', marginTop: 'auto', display: 'flex', gap: '8px' }}>
                                <button className="btn btn-outline btn-sm" onClick={() => onDetails(gw)} style={{ flex: 1 }}>
                                    <i className="fas fa-search-plus" /> 상세
                                </button>
                                <button className="btn btn-primary btn-sm" onClick={() => onDeploy(gw)} style={{ flex: 1 }} disabled={gw.live_status?.status !== 'online'}>
                                    <i className="fas fa-rocket" /> 배포
                                </button>
                            </div>
                        </div>
                    ))}
                </div>
            )}
        </div>
    );
};

// =============================================================================
// Main Page: ExportGatewaySettings
// =============================================================================

// =============================================================================
// Export Gateway Configuration Wizard
// =============================================================================

const ExportGatewayWizard: React.FC<{
    visible: boolean;
    onClose: () => void;
    onSuccess: () => void;
    profiles: ExportProfile[];
    targets: ExportTarget[];
    templates: PayloadTemplate[];
    allPoints: DataPoint[];
    editingGateway?: Gateway | null;
    assignments?: Assignment[];
}> = ({ visible, onClose, onSuccess, profiles, targets, templates, allPoints, editingGateway, assignments = [] }) => {
    const { confirm } = useConfirmContext();
    const [currentStep, setCurrentStep] = useState(0);
    const [loading, setLoading] = useState(false);

    // Track previous states to prevent redundant resets during polling
    const prevVisibleRef = React.useRef(visible);
    const prevEditingIdRef = React.useRef<number | undefined>(editingGateway?.id);
    const hydratedTargetIdRef = React.useRef<number | null>(null);

    // Step Data
    const [gatewayData, setGatewayData] = useState({ name: '', ip_address: '127.0.0.1', description: '' });

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
        template_json: '{\n  "device": "{device_name}",\n  "point": "{point_name}",\n  "value": {value},\n  "timestamp": "{timestamp}"\n}'
    });

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
            auth: { type: 'x-api-key', apiKey: '' }
        }] as any[],
        config_mqtt: [{ url: '', topic: 'pulseone/data' }] as any[],
        config_s3: [{ bucket: '', region: '', accessKeyId: '', secretAccessKey: '' }] as any[]
    });

    const [mappings, setMappings] = useState<Partial<ExportTargetMapping>[]>([]);
    const [bulkSiteId, setBulkSiteId] = useState<string>('');
    const [scheduleData, setScheduleData] = useState({
        schedule_name: '',
        cron_expression: '*/1 * * * *',
        data_range: 'minute' as const,
        lookback_periods: 1
    });

    // Aggressive Hydration & Initialization logic
    useEffect(() => {
        const isOpening = visible && !prevVisibleRef.current;
        const isEditingTargetChanged = visible && editingGateway?.id !== prevEditingIdRef.current;

        if (isOpening || isEditingTargetChanged) {
            // Reset for EVERY opening or gateway change to ensure clean state
            setCurrentStep(0);
            setSelectedProfileId(null);
            setProfileMode('existing');
            setSelectedTemplateId(null);
            setTemplateMode('existing');
            setSelectedProtocols(['HTTP']);
            setTargetMode('new');
            setSelectedTargetIds([]);
            setTransmissionMode('INTERVAL');
            setMappings([]);
            hydratedTargetIdRef.current = null; // Reset hydration tracking

            setScheduleData({
                schedule_name: '',
                cron_expression: '*/1 * * * *',
                data_range: 'minute' as const,
                lookback_periods: 1
            });
            setTargetData({
                name: '',
                config_http: [{
                    url: '',
                    method: 'POST',
                    auth_type: 'NONE',
                    headers: { Authorization: '' },
                    auth: { type: 'x-api-key', apiKey: '' }
                }],
                config_mqtt: [{ url: '', topic: 'pulseone/data' }],
                config_s3: [{ bucket: '', region: '', accessKeyId: '', secretAccessKey: '' }]
            });

            if (editingGateway) {
                // 1. Basic Gateway Data
                setGatewayData({
                    name: editingGateway.name,
                    ip_address: editingGateway.ip_address,
                    description: editingGateway.description || ''
                });
            } else {
                setGatewayData({ name: '', ip_address: '127.0.0.1', description: '' });
            }
        }

        // Reactive Hydration: Whenever visible/assignments change, populate profile/template/targets
        if (visible && editingGateway && assignments.length > 0) {
            const currentAssign = assignments[0];
            const pid = currentAssign.profile_id;

            // 1. Hydrate Profile
            if (selectedProfileId === null) {
                setSelectedProfileId(pid);
                setProfileMode('existing');
            }

            // 2. Hydrate Targets, Protocols, Template, and Mappings
            const profileTargets = targets.filter(t => String(t.profile_id) === String(pid));

            // Check if we need to hydrate (Either haven't hydrated for this gateway yet, or it's a fresh opening)
            const needsHydration = hydratedTargetIdRef.current !== editingGateway.id && profileTargets.length > 0;

            if (needsHydration) {
                const rawProtocols = Array.from(new Set(profileTargets.map(t => t.target_type.toUpperCase())));
                setSelectedProtocols(rawProtocols);

                // Try to find a target name that matches the gateway name, otherwise use the first one
                const bestTarget = profileTargets.find(t => t.name.startsWith(editingGateway.name)) || profileTargets[0];
                const integratedName = bestTarget.name.replace(/\s*\(.*\)$/, '');

                // Important: Use a clean base object for targetData to avoid merging with state from previous session
                const freshTargetData = {
                    name: integratedName,
                    config_http: [] as any[],
                    config_mqtt: [] as any[],
                    config_s3: [] as any[]
                };

                const httpTargets = profileTargets.filter(t => t.target_type.toUpperCase() === 'HTTP');
                freshTargetData.config_http = httpTargets.length > 0
                    ? httpTargets.map(t => {
                        const cfg = typeof t.config === 'string' ? JSON.parse(t.config) : t.config;
                        return {
                            ...cfg,
                            headers: cfg.headers || { Authorization: '' },
                            auth: cfg.auth || { type: 'x-api-key', apiKey: '' }
                        };
                    })
                    : [{ url: '', method: 'POST', auth_type: 'NONE', headers: { Authorization: '' }, auth: { type: 'x-api-key', apiKey: '' } }];

                const mqttTargets = profileTargets.filter(t => t.target_type.toUpperCase() === 'MQTT');
                freshTargetData.config_mqtt = mqttTargets.length > 0
                    ? mqttTargets.map(t => typeof t.config === 'string' ? JSON.parse(t.config) : t.config)
                    : [{ url: '', topic: 'pulseone/data' }];

                const s3Targets = profileTargets.filter(t => t.target_type.toUpperCase() === 'S3');
                freshTargetData.config_s3 = s3Targets.length > 0
                    ? s3Targets.map(t => typeof t.config === 'string' ? JSON.parse(t.config) : t.config)
                    : [{ bucket: '', region: '', accessKeyId: '', secretAccessKey: '' }];

                setTargetData(freshTargetData);
                hydratedTargetIdRef.current = editingGateway.id;

                // 3. Hydrate Template
                if (profileTargets[0]?.template_id) {
                    setSelectedTemplateId(profileTargets[0].template_id);
                    setTemplateMode('existing');
                }

                // Hydrate Transmission Mode from Target
                const mode = profileTargets[0].export_mode; // 'INTERVAL', 'EVENT', 'on_change', etc.
                if (mode === 'INTERVAL') {
                    setTransmissionMode('INTERVAL');
                } else {
                    setTransmissionMode('EVENT'); // Default to EVENT for 'REALTIME', 'on_change', etc.
                }

                // 4. Hydrate Mappings
                const loadMappings = async () => {
                    try {
                        const res = await exportGatewayApi.getTargetMappings(profileTargets[0].id);
                        const items = extractItems(res.data);
                        if (items.length > 0) {
                            setMappings(items.map((m: any) => ({
                                id: m.id,
                                point_id: m.point_id,
                                site_id: m.site_id || m.building_id, // [CLEANUP] Single assignment
                                target_field_name: m.target_field_name,
                                target_description: m.target_description,
                                is_enabled: m.is_enabled === 1 || m.is_enabled === true,
                                conversion_config: typeof m.conversion_config === 'string' ? JSON.parse(m.conversion_config) : (m.conversion_config || { scale: 1, offset: 0 })
                            })));
                        } else {
                            const profile = profiles.find(p => p.id === pid);
                            if (profile && profile.data_points) {
                                setMappings(profile.data_points.map((p: any) => ({
                                    point_id: p.id,
                                    site_id: p.site_id || p.building_id, // [RESTORE] site_id focus
                                    target_field_name: p.target_field_name || p.name,
                                    conversion_config: { scale: 1, offset: 0 }
                                })));
                            }
                        }
                    } catch (e) { console.error('Failed to load mappings:', e); }
                };
                loadMappings();

                // 5. Hydrate Schedule (Only if INTERVAL)
                if (mode === 'INTERVAL') {
                    const loadExtra = async () => {
                        try {
                            const schedulesRes = await exportGatewayApi.getSchedules();
                            const items = extractItems(schedulesRes.data);
                            const linkedSchedule = items.find((s: any) => String(s.target_id) === String(profileTargets[0].id));
                            if (linkedSchedule) {
                                setScheduleData({
                                    schedule_name: linkedSchedule.schedule_name,
                                    cron_expression: linkedSchedule.cron_expression,
                                    data_range: linkedSchedule.data_range as 'minute',
                                    lookback_periods: linkedSchedule.lookback_periods
                                });
                            }
                        } catch (e) { console.error('Failed to load schedule:', e); }
                    };
                    loadExtra();
                }
            }
        }

        prevVisibleRef.current = visible;
        prevEditingIdRef.current = editingGateway?.id;
    }, [visible, editingGateway, targets, assignments, templates]);

    const steps = [
        { title: '게이트웨이', icon: <i className="fas fa-server" /> },
        { title: '데이터 프로파일', icon: <i className="fas fa-file-export" /> },
        { title: '페이로드 양식', icon: <i className="fas fa-code" /> },
        { title: '전송 대상', icon: <i className="fas fa-bullseye" /> },
        { title: '스케줄링', icon: <i className="fas fa-clock" /> }
    ];

    const handleNext = async () => {
        // Step-by-step Validation
        if (currentStep === 0) {
            if (!gatewayData.name || !gatewayData.ip_address) {
                await confirm({
                    title: '입력 확인',
                    message: '게이트웨이 명칭과 IP 주소는 필수 입력 사항입니다.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
                return;
            }
        } else if (currentStep === 1) {
            if (profileMode === 'existing' && !selectedProfileId) {
                await confirm({
                    title: '선택 확인',
                    message: '기존 프로파일을 선택하거나 새로운 프로파일을 작성해 주세요.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
                return;
            }
            if (profileMode === 'new' && !newProfileData.name) {
                await confirm({
                    title: '입력 확인',
                    message: '신규 프로파일 명칭을 입력해 주세요.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
                return;
            }
        } else if (currentStep === 2) {
            if (templateMode === 'existing' && !selectedTemplateId) {
                await confirm({
                    title: '선택 확인',
                    message: '기존 페이로드 양식을 선택하거나 새로운 양식을 작성해 주세요.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
                return;
            }
            if (templateMode === 'new' && !newTemplateData.name) {
                await confirm({
                    title: '입력 확인',
                    message: '신규 페이로드 양식 명칭을 입력해 주세요.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
                return;
            }
        } else if (currentStep === 3) {
            if (targetMode === 'existing' && selectedTargetIds.length === 0) {
                await confirm({
                    title: '선택 확인',
                    message: '최소 하나 이상의 기존 타겟을 선택해 주세요.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
                return;
            }
            if (targetMode === 'new' && (!targetData.name || selectedProtocols.length === 0)) {
                await confirm({
                    title: '입력 확인',
                    message: '타겟 통합 명칭과 최소 하나의 전송 프로토콜을 선택해 주세요.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
                return;
            }
        }

        // Auto-initialize mappings if moving to step 3 (Target/Mapping stage)
        if (currentStep === 2) {
            let activePoints = [];
            if (profileMode === 'existing') {
                const profile = profiles.find(p => String(p.id) === String(selectedProfileId));
                activePoints = profile?.data_points || [];
            } else {
                activePoints = newProfileData.data_points;
            }

            // PROTECT EXISTING MAPPINGS: Only initialize if current mappings are empty 
            // OR if the mapping count doesn't match the active points (profile changed)
            if (activePoints.length > 0 && (mappings.length === 0 || mappings.length !== activePoints.length)) {
                const initialMappings = activePoints.map((p: any) => ({
                    point_id: p.id,
                    site_id: p.site_id || p.building_id, // [RESTORE] site_id focus
                    target_field_name: p.target_field_name || p.name,
                    target_description: '',
                    is_enabled: true,
                    conversion_config: { scale: 1, offset: 0 }
                }));
                setMappings(initialMappings);
            }

            if (!scheduleData.schedule_name) {
                setScheduleData(prev => ({ ...prev, schedule_name: `${gatewayData.name} 전송 스케줄` }));
            }
            if (!targetData.name) {
                setTargetData(prev => ({ ...prev, name: `${gatewayData.name} 전송 대상` }));
            }
        }

        setCurrentStep(currentStep + 1);
    };

    const handlePrev = () => setCurrentStep(currentStep - 1);

    const handleFinish = async () => {
        setLoading(true);
        try {
            // 0. Gateway Upsert
            let gatewayId = editingGateway?.id;
            if (editingGateway) {
                await exportGatewayApi.updateGateway(editingGateway.id, gatewayData);
            } else {
                const gwRes = await exportGatewayApi.registerGateway(gatewayData);
                gatewayId = gwRes.data?.id;
            }
            if (!gatewayId) throw new Error('Gateway registration failed');

            // 1. Profile Creation (if new)
            let finalProfileId = selectedProfileId;
            if (profileMode === 'new') {
                const pRes = await exportGatewayApi.createProfile(newProfileData);
                finalProfileId = pRes.data?.id || null;
                if (!finalProfileId) throw new Error('Profile creation failed');
            }

            // 2. Template Creation (if new)
            let finalTemplateId = selectedTemplateId;
            if (templateMode === 'new') {
                // Validate JSON
                let json = newTemplateData.template_json;
                try {
                    if (typeof json === 'string') JSON.parse(json);
                } catch (e) { throw new Error('Invalid JSON in Template'); }

                const tRes = await exportGatewayApi.createTemplate(newTemplateData);
                finalTemplateId = tRes.data?.id || null;
                if (!finalTemplateId) throw new Error('Template creation failed');
            }

            // 4. Register Targets and link with Profile & Template
            const targetIds: number[] = [];
            if (targetMode === 'new') {
                // [Edit Mode Cleanup] If editing an existing profile, cleanup old targets first to avoid duplicates
                if (finalProfileId) {
                    try {
                        const existingTargetsRes = await exportGatewayApi.getTargets();
                        const allTargets = extractItems(existingTargetsRes.data);
                        const oldTargets = allTargets.filter(t => String(t.profile_id) === String(finalProfileId));
                        for (const ot of oldTargets) {
                            await exportGatewayApi.deleteTarget(ot.id);
                        }
                    } catch (e) {
                        console.warn('Failed to cleanup old targets:', e);
                    }
                }

                for (const protocol of selectedProtocols) {
                    const configs = protocol === 'HTTP' ? targetData.config_http
                        : protocol === 'MQTT' ? targetData.config_mqtt
                            : targetData.config_s3;

                    for (let i = 0; i < configs.length; i++) {
                        const config = configs[i];
                        const targetPayload = {
                            name: `${targetData.name} (${protocol}${configs.length > 1 ? `-${i + 1}` : ''})`,
                            target_type: protocol,
                            config: config,
                            profile_id: finalProfileId!,
                            template_id: finalTemplateId!,
                            is_enabled: true,
                            export_mode: transmissionMode,
                            export_interval: transmissionMode === 'INTERVAL' ? 60 : 0
                        };
                        const targetRes = await exportGatewayApi.createTarget(targetPayload);
                        if (targetRes.data?.id) targetIds.push(targetRes.data.id);
                    }
                }
            } else {
                // Link Existing Targets
                for (const tId of selectedTargetIds) {
                    await exportGatewayApi.updateTarget(tId, {
                        profile_id: finalProfileId!,
                        template_id: finalTemplateId!
                    });
                    targetIds.push(tId);
                }
            }

            if (targetIds.length === 0) throw new Error('Target creation/selection failed');

            // 5. Save Mappings for each target
            for (const tId of targetIds) {
                if (mappings.length > 0) {
                    await exportGatewayApi.saveTargetMappings(tId, mappings);
                }

                // 7. Create Schedule ONLY if in INTERVAL mode
                if (transmissionMode === 'INTERVAL') {
                    await exportGatewayApi.createSchedule({
                        ...scheduleData,
                        target_id: tId,
                        is_enabled: true,
                        timezone: 'Asia/Seoul'
                    });
                }
            }

            // 8. Assign Profile to Gateway
            if (gatewayId && finalProfileId) {
                await exportGatewayApi.assignProfile(gatewayId, finalProfileId);
            }

            Modal.success({
                title: '구성 완료',
                content: '게이트웨이, 프로파일, 타겟 설정을 포함한 모든 구성이 성공적으로 완료되었습니다.',
                centered: true
            });
            onSuccess();
        } catch (error: any) {
            Modal.error({
                title: '처리 중 오류 발생',
                content: error.message || '설정 과정에서 문제가 생겼습니다.',
                centered: true
            });
        } finally {
            setLoading(false);
        }
    };

    return (
        <Modal
            open={visible}
            onCancel={onClose}
            width={900}
            footer={null}
            centered
            maskClosable={false}
            title={null}
            bodyStyle={{ padding: 0 }}
        >
            <div className="wizard-container">
                <div className="wizard-header" style={{ padding: '24px', background: 'var(--primary-50)', borderBottom: '1px solid var(--primary-100)' }}>
                    <h3 style={{ margin: 0, color: 'var(--primary-700)' }}>
                        {editingGateway ? `게이트웨이 구성 수정: ${editingGateway.name}` : '신규 데이터 수집 구성 마법사'}
                    </h3>
                    <p style={{ margin: '4px 0 0 0', color: 'var(--primary-600)', fontSize: '12px' }}>
                        {editingGateway ? '기존 게이트웨이의 데이터 전송 파이프라인 설정을 변경합니다.' : '처음부터 끝까지 데이터 연동에 필요한 모든 설정을 원스톱으로 구성합니다.'}
                    </p>
                </div>

                <div className="wizard-steps" style={{ padding: '24px 24px 0 24px' }}>
                    <Steps current={currentStep} size="small">
                        {steps.map(s => <Steps.Step key={s.title} title={s.title} icon={s.icon} />)}
                    </Steps>
                </div>

                <Divider />

                <div className="wizard-content" style={{ padding: '0 32px 32px 32px', minHeight: '350px' }}>
                    {/* STEP 0: Gateway */}
                    {currentStep === 0 && (
                        <div style={{ height: '480px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                            <Form layout="vertical" style={{ width: '100%' }}>
                                <Row gutter={24}>
                                    <Col span={14}>
                                        <Form.Item label="수집 게이트웨이 명칭" required tooltip="식별을 위한 게이트웨이 이름을 입력합니다.">
                                            <Input
                                                size="large"
                                                placeholder="예: 제1공장 Edge Server"
                                                value={gatewayData.name}
                                                onChange={e => setGatewayData({ ...gatewayData, name: e.target.value })}
                                            />
                                        </Form.Item>
                                    </Col>
                                    <Col span={10}>
                                        <Form.Item label="게이트웨이 유형">
                                            <Input size="large" value="Export Gateway" disabled />
                                        </Form.Item>
                                    </Col>
                                </Row>
                                <Form.Item label="접속 IP 주소" required tooltip="게이트웨이가 구동 중인 서버의 IP 주소를 입력합니다.">
                                    <Input
                                        size="large"
                                        placeholder="127.0.0.1"
                                        value={gatewayData.ip_address}
                                        onChange={e => setGatewayData({ ...gatewayData, ip_address: e.target.value })}
                                    />
                                </Form.Item>
                                <Form.Item label="상세 설명">
                                    <Input.TextArea
                                        rows={4}
                                        placeholder="게이트웨이에 대한 추가 정보를 입력하세요."
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
                            <div style={{ marginBottom: '24px', textAlign: 'center' }}>
                                <Radio.Group value={profileMode} onChange={e => setProfileMode(e.target.value)} size="large">
                                    <Radio.Button value="existing" style={{ padding: '0 32px' }}>기존 프로파일 선택</Radio.Button>
                                    <Radio.Button value="new" style={{ padding: '0 32px' }}>새 프로파일 생성</Radio.Button>
                                </Radio.Group>
                            </div>

                            {profileMode === 'existing' ? (
                                <div style={{ height: '480px', overflowY: 'auto', padding: '4px', border: '1px solid var(--neutral-200)', borderRadius: '8px', background: 'var(--neutral-50)' }}>
                                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', padding: '12px' }}>
                                        {profiles.map(p => (
                                            <div
                                                key={p.id}
                                                onClick={() => setSelectedProfileId(p.id)}
                                                style={{
                                                    padding: '16px',
                                                    cursor: 'pointer',
                                                    borderRadius: '8px',
                                                    border: String(selectedProfileId) === String(p.id) ? '2px solid var(--primary-500)' : '1px solid white',
                                                    background: String(selectedProfileId) === String(p.id) ? 'var(--primary-50)' : 'white',
                                                    boxShadow: '0 2px 4px rgba(0,0,0,0.02)',
                                                    transition: 'all 0.2s',
                                                    display: 'flex',
                                                    alignItems: 'center',
                                                    gap: '16px'
                                                }}
                                            >
                                                <Radio checked={String(selectedProfileId) === String(p.id)} />
                                                <div style={{ flex: 1 }}>
                                                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                        <span style={{ fontWeight: 600, fontSize: '15px' }}>{p.name}</span>
                                                        <Tag color="blue">{p.data_points?.length || 0} Points</Tag>
                                                    </div>
                                                    <div style={{ fontSize: '13px', color: 'var(--neutral-500)', marginTop: '4px' }}>
                                                        {p.description || '설명 없음'}
                                                    </div>
                                                </div>
                                            </div>
                                        ))}
                                    </div>
                                </div>
                            ) : (
                                <Row gutter={24} style={{ height: '480px' }}>
                                    <Col span={10}>
                                        <Form layout="vertical">
                                            <Form.Item label="프로파일 이름" required>
                                                <Input size="large" value={newProfileData.name} onChange={e => setNewProfileData({ ...newProfileData, name: e.target.value })} placeholder="프로파일 명칭 입력" />
                                            </Form.Item>
                                            <Form.Item label="설명">
                                                <Input.TextArea rows={4} value={newProfileData.description} onChange={e => setNewProfileData({ ...newProfileData, description: e.target.value })} placeholder="프로파일에 대한 상세 설명을 입력하세요." />
                                            </Form.Item>
                                            <div style={{ padding: '16px', background: 'var(--neutral-50)', borderRadius: '8px', border: '1px dashed var(--neutral-300)' }}>
                                                <div style={{ fontSize: '14px', fontWeight: 600, color: 'var(--neutral-700)' }}>
                                                    <i className="fas fa-check-circle" style={{ marginRight: '8px', color: 'var(--success-500)' }} />
                                                    선택된 포인트: {newProfileData.data_points.length}개
                                                </div>
                                                <div style={{ fontSize: '12px', color: 'var(--neutral-500)', marginTop: '4px' }}>우측 장치 및 포인트 목록에서 필요한 항목을 클릭하여 추가하세요.</div>
                                            </div>
                                        </Form>
                                    </Col>
                                    <Col span={14}>
                                        <div style={{ border: '1px solid var(--neutral-200)', borderRadius: '8px', height: '480px', overflow: 'hidden', boxShadow: '0 2px 8px rgba(0,0,0,0.05)' }}>
                                            <DataPointSelector
                                                selectedPoints={newProfileData.data_points}
                                                onSelect={p => setNewProfileData({ ...newProfileData, data_points: [...newProfileData.data_points, p] })}
                                                onRemove={id => setNewProfileData({ ...newProfileData, data_points: newProfileData.data_points.filter((p: any) => p.id !== id) })}
                                            />
                                        </div>
                                    </Col>
                                </Row>
                            )}
                        </div>
                    )}

                    {/* STEP 2: Payload Template */}
                    {currentStep === 2 && (
                        <div style={{ height: '480px' }}>
                            <div style={{ marginBottom: '24px', textAlign: 'center' }}>
                                <Radio.Group value={templateMode} onChange={e => setTemplateMode(e.target.value)} size="large">
                                    <Radio.Button value="existing" style={{ padding: '0 32px' }}>기존 템플릿 선택</Radio.Button>
                                    <Radio.Button value="new" style={{ padding: '0 32px' }}>새 템플릿 생성</Radio.Button>
                                </Radio.Group>
                            </div>

                            {templateMode === 'existing' ? (
                                <Select
                                    style={{ width: '100%' }}
                                    value={selectedTemplateId}
                                    onChange={val => setSelectedTemplateId(val)}
                                >
                                    {templates.map(t => <Select.Option key={t.id} value={t.id}>{t.name} ({t.system_type})</Select.Option>)}
                                </Select>
                            ) : (
                                <Form layout="vertical">
                                    <Form.Item label="템플릿 명칭" required>
                                        <Input value={newTemplateData.name} onChange={e => setNewTemplateData({ ...newTemplateData, name: e.target.value })} />
                                    </Form.Item>
                                    <Form.Item label="JSON 구조 정의">
                                        <Input.TextArea
                                            rows={8}
                                            style={{ fontFamily: 'monospace' }}
                                            value={newTemplateData.template_json}
                                            onChange={e => setNewTemplateData({ ...newTemplateData, template_json: e.target.value })}
                                        />
                                        <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '4px' }}> 치환자: {"{device_name}, {point_name}, {value}, {timestamp}"} </div>
                                    </Form.Item>
                                </Form>
                            )}
                        </div>
                    )}

                    {/* STEP 3: Target & Mapping */}
                    {currentStep === 3 && (
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '20px', height: '480px', overflowY: 'auto', paddingRight: '12px' }}>
                            <div style={{ marginBottom: '4px', textAlign: 'center' }}>
                                <Radio.Group value={targetMode} onChange={e => setTargetMode(e.target.value)} size="large">
                                    <Radio.Button value="existing" style={{ padding: '0 32px' }}>기존 타겟 선택</Radio.Button>
                                    <Radio.Button value="new" style={{ padding: '0 32px' }}>새 타겟 생성</Radio.Button>
                                </Radio.Group>
                            </div>

                            {targetMode === 'existing' ? (
                                <div style={{ background: 'var(--neutral-50)', padding: '24px', borderRadius: '12px', border: '1px solid var(--neutral-200)', minHeight: '380px' }}>
                                    <div style={{ fontWeight: 600, marginBottom: '16px', color: 'var(--neutral-700)' }}>기존 전송 타겟 선택</div>
                                    <div style={{ maxHeight: '300px', overflowY: 'auto', border: '1px solid var(--neutral-200)', borderRadius: '8px', background: 'white' }}>
                                        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                                            <thead style={{ background: 'var(--neutral-50)', position: 'sticky', top: 0, zIndex: 1 }}>
                                                <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--neutral-200)' }}>
                                                    <th style={{ padding: '12px 16px', width: '60px', textAlign: 'center' }}>선택</th>
                                                    <th style={{ padding: '12px 16px' }}>타겟 명칭</th>
                                                    <th style={{ padding: '12px 16px' }}>프로토콜</th>
                                                    <th style={{ padding: '12px 16px' }}>상태</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                {targets.map(t => (
                                                    <tr key={t.id} style={{ borderBottom: '1px solid var(--neutral-100)', cursor: 'pointer' }} onClick={() => {
                                                        if (selectedTargetIds.includes(t.id)) setSelectedTargetIds(selectedTargetIds.filter(id => id !== t.id));
                                                        else setSelectedTargetIds([...selectedTargetIds, t.id]);
                                                    }}>
                                                        <td style={{ padding: '12px 16px', textAlign: 'center' }}>
                                                            <Checkbox checked={selectedTargetIds.includes(t.id)} />
                                                        </td>
                                                        <td style={{ padding: '12px 16px', fontWeight: 500 }}>{t.name}</td>
                                                        <td style={{ padding: '12px 16px' }}><Tag color="blue">{t.target_type.toUpperCase()}</Tag></td>
                                                        <td style={{ padding: '12px 16px' }}><Tag color={t.is_enabled ? 'success' : 'default'}>{t.is_enabled ? '활성' : '비활성'}</Tag></td>
                                                    </tr>
                                                ))}
                                                {targets.length === 0 && (
                                                    <tr>
                                                        <td colSpan={4} style={{ padding: '40px', textAlign: 'center', color: 'var(--neutral-400)' }}>등록된 타겟이 없습니다.</td>
                                                    </tr>
                                                )}
                                            </tbody>
                                        </table>
                                    </div>
                                    <div style={{ marginTop: '12px', fontSize: '12px', color: 'var(--neutral-500)' }}>
                                        * 선택한 타겟들은 현재 설정 중인 프로파일에 자동으로 연결됩니다.
                                    </div>
                                </div>
                            ) : (
                                <div style={{ background: 'var(--neutral-50)', padding: '24px', borderRadius: '12px', border: '1px solid var(--neutral-200)' }}>
                                    <Form.Item label="대상(Target) 통합 명칭" required tooltip="전송 대상들의 기본 이름을 지정합니다.">
                                        <Input size="large" value={targetData.name} onChange={e => setTargetData({ ...targetData, name: e.target.value })} placeholder="예: [운영] 데이터 허브" />
                                    </Form.Item>

                                    <Form.Item label="전송 프로토콜 선택 (중복 선택 가능)" required>
                                        <Checkbox.Group
                                            value={selectedProtocols}
                                            onChange={vals => setSelectedProtocols(vals as string[])}
                                            style={{ width: '100%' }}
                                        >
                                            <div style={{ display: 'flex', gap: '12px' }}>
                                                <div style={{ flex: 1, padding: '12px', border: '1px solid var(--neutral-200)', borderRadius: '8px', background: selectedProtocols.includes('HTTP') ? 'var(--primary-50)' : 'white', textAlign: 'center' }}>
                                                    <Checkbox value="HTTP">HTTP (REST)</Checkbox>
                                                </div>
                                                <div style={{ flex: 1, padding: '12px', border: '1px solid var(--neutral-200)', borderRadius: '8px', background: selectedProtocols.includes('MQTT') ? 'var(--primary-50)' : 'white', textAlign: 'center' }}>
                                                    <Checkbox value="MQTT">MQTT</Checkbox>
                                                </div>
                                                <div style={{ flex: 1, padding: '12px', border: '1px solid var(--neutral-200)', borderRadius: '8px', background: selectedProtocols.includes('S3') ? 'var(--primary-50)' : 'white', textAlign: 'center' }}>
                                                    <Checkbox value="S3">AWS S3</Checkbox>
                                                </div>
                                            </div>
                                        </Checkbox.Group>
                                    </Form.Item>

                                    {selectedProtocols.includes('HTTP') && (
                                        <div style={{ marginTop: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                <div style={{ fontWeight: 600, fontSize: '13px', color: 'var(--primary-700)' }}><i className="fas fa-link" /> HTTP 목적지 설정</div>
                                                <Button type="link" size="small" icon={<i className="fas fa-plus" />} onClick={() => setTargetData({ ...targetData, config_http: [...targetData.config_http, { url: '', method: 'POST', auth_type: 'NONE', headers: { Authorization: '' }, auth: { type: 'x-api-key', apiKey: '' } }] })}>추가</Button>
                                            </div>
                                            {targetData.config_http.map((config, idx) => (
                                                <div key={idx} style={{ padding: '16px', background: 'white', borderRadius: '8px', border: '1px solid var(--primary-100)', position: 'relative' }}>
                                                    {targetData.config_http.length > 1 && (
                                                        <Button type="text" danger size="small" icon={<i className="fas fa-trash-alt" />} style={{ position: 'absolute', top: 8, right: 8 }} onClick={() => setTargetData({ ...targetData, config_http: targetData.config_http.filter((_, i) => i !== idx) })} />
                                                    )}
                                                    <Form.Item label={`Endpoint URL #${idx + 1}`} required style={{ marginBottom: '12px' }}>
                                                        <Input size="large" placeholder="http://api.myserver.com/v1/data" value={config.url} onChange={e => {
                                                            const next = [...targetData.config_http];
                                                            next[idx].url = e.target.value;
                                                            setTargetData({ ...targetData, config_http: next });
                                                        }} />
                                                    </Form.Item>
                                                    <Row gutter={12}>
                                                        <Col span={12}>
                                                            <Form.Item label="인증 키 (Authorization)" style={{ marginBottom: 0 }}>
                                                                <Input
                                                                    placeholder="Bearer ..."
                                                                    value={config.headers?.Authorization}
                                                                    onChange={e => {
                                                                        const next = [...targetData.config_http];
                                                                        next[idx].headers = { ...next[idx].headers, Authorization: e.target.value };
                                                                        setTargetData({ ...targetData, config_http: next });
                                                                    }}
                                                                />
                                                            </Form.Item>
                                                        </Col>
                                                        <Col span={12}>
                                                            <Form.Item label="X-API-KEY" style={{ marginBottom: 0 }}>
                                                                <Input
                                                                    placeholder="API Gateway Key"
                                                                    value={config.auth?.apiKey}
                                                                    onChange={e => {
                                                                        const next = [...targetData.config_http];
                                                                        next[idx].auth = { ...next[idx].auth, type: 'x-api-key', apiKey: e.target.value };
                                                                        setTargetData({ ...targetData, config_http: next });
                                                                    }}
                                                                />
                                                            </Form.Item>
                                                        </Col>
                                                    </Row>
                                                </div>
                                            ))}
                                        </div>
                                    )}

                                    {selectedProtocols.includes('MQTT') && (
                                        <div style={{ marginTop: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                <div style={{ fontWeight: 600, fontSize: '13px', color: 'var(--primary-700)' }}><i className="fas fa-project-diagram" /> MQTT 목적지 설정</div>
                                                <Button type="link" size="small" icon={<i className="fas fa-plus" />} onClick={() => setTargetData({ ...targetData, config_mqtt: [...targetData.config_mqtt, { url: '', topic: 'pulseone/data' }] })}>추가</Button>
                                            </div>
                                            {targetData.config_mqtt.map((config, idx) => (
                                                <div key={idx} style={{ padding: '16px', background: 'white', borderRadius: '8px', border: '1px solid var(--primary-100)', position: 'relative' }}>
                                                    {targetData.config_mqtt.length > 1 && (
                                                        <Button type="text" danger size="small" icon={<i className="fas fa-trash-alt" />} style={{ position: 'absolute', top: 8, right: 8 }} onClick={() => setTargetData({ ...targetData, config_mqtt: targetData.config_mqtt.filter((_, i) => i !== idx) })} />
                                                    )}
                                                    <Row gutter={12}>
                                                        <Col span={14}>
                                                            <Form.Item label="Broker URL" required style={{ marginBottom: 0 }}>
                                                                <Input size="large" placeholder="mqtt://localhost" value={config.url} onChange={e => {
                                                                    const next = [...targetData.config_mqtt];
                                                                    next[idx].url = e.target.value;
                                                                    setTargetData({ ...targetData, config_mqtt: next });
                                                                }} />
                                                            </Form.Item>
                                                        </Col>
                                                        <Col span={10}>
                                                            <Form.Item label="Topic" required style={{ marginBottom: 0 }}>
                                                                <Input size="large" placeholder="pulseone/data" value={config.topic} onChange={e => {
                                                                    const next = [...targetData.config_mqtt];
                                                                    next[idx].topic = e.target.value;
                                                                    setTargetData({ ...targetData, config_mqtt: next });
                                                                }} />
                                                            </Form.Item>
                                                        </Col>
                                                    </Row>
                                                </div>
                                            ))}
                                        </div>
                                    )}

                                    {selectedProtocols.includes('S3') && (
                                        <div style={{ marginTop: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                <div style={{ fontWeight: 600, fontSize: '13px', color: 'var(--primary-700)' }}><i className="fab fa-aws" /> AWS S3 목적지 설정</div>
                                                <Button type="link" size="small" icon={<i className="fas fa-plus" />} onClick={() => setTargetData({ ...targetData, config_s3: [...targetData.config_s3, { bucket: '', region: '', accessKeyId: '', secretAccessKey: '' }] })}>추가</Button>
                                            </div>
                                            {targetData.config_s3.map((config, idx) => (
                                                <div key={idx} style={{ padding: '16px', background: 'white', borderRadius: '8px', border: '1px solid var(--primary-100)', position: 'relative' }}>
                                                    {targetData.config_s3.length > 1 && (
                                                        <Button type="text" danger size="small" icon={<i className="fas fa-trash-alt" />} style={{ position: 'absolute', top: 8, right: 8 }} onClick={() => setTargetData({ ...targetData, config_s3: targetData.config_s3.filter((_, i) => i !== idx) })} />
                                                    )}
                                                    <Row gutter={12}>
                                                        <Col span={12}>
                                                            <Form.Item label="Bucket Name" required style={{ marginBottom: '12px' }}>
                                                                <Input size="large" value={config.bucket} onChange={e => {
                                                                    const next = [...targetData.config_s3];
                                                                    next[idx].bucket = e.target.value;
                                                                    setTargetData({ ...targetData, config_s3: next });
                                                                }} />
                                                            </Form.Item>
                                                        </Col>
                                                        <Col span={12}>
                                                            <Form.Item label="Region" required style={{ marginBottom: '12px' }}>
                                                                <Input size="large" value={config.region} onChange={e => {
                                                                    const next = [...targetData.config_s3];
                                                                    next[idx].region = e.target.value;
                                                                    setTargetData({ ...targetData, config_s3: next });
                                                                }} />
                                                            </Form.Item>
                                                        </Col>
                                                    </Row>
                                                    <Row gutter={12}>
                                                        <Col span={12}>
                                                            <Form.Item label="Access Key" style={{ marginBottom: 0 }}>
                                                                <Input.Password size="large" value={config.accessKeyId} onChange={e => {
                                                                    const next = [...targetData.config_s3];
                                                                    next[idx].accessKeyId = e.target.value;
                                                                    setTargetData({ ...targetData, config_s3: next });
                                                                }} />
                                                            </Form.Item>
                                                        </Col>
                                                        <Col span={12}>
                                                            <Form.Item label="Secret Key" style={{ marginBottom: 0 }}>
                                                                <Input.Password size="large" value={config.secretAccessKey} onChange={e => {
                                                                    const next = [...targetData.config_s3];
                                                                    next[idx].secretAccessKey = e.target.value;
                                                                    setTargetData({ ...targetData, config_s3: next });
                                                                }} />
                                                            </Form.Item>
                                                        </Col>
                                                    </Row>
                                                </div>
                                            ))}
                                        </div>
                                    )}
                                </div>
                            )}
                            <div style={{ padding: '0 8px' }}>
                                <div style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '12px' }}><i className="fas fa-table" /> 데이터 필드 매핑 설정</div>
                                <div style={{ background: 'white', border: '1px solid var(--neutral-200)', borderRadius: '8px', overflow: 'hidden' }}>
                                    <div style={{ padding: '8px 16px', background: 'var(--neutral-50)', borderBottom: '1px solid var(--neutral-200)', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                                        <div style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>
                                            * PulseOne 포인트의 원본 {`site_id`}가 기본으로 설정되어 있습니다. (선택 시 자동 로드)
                                        </div>
                                        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                                            <span style={{ fontSize: '11px', color: 'var(--neutral-600)' }}>Building ID 일괄 적용:</span>
                                            <Input
                                                size="small"
                                                placeholder="예: 101"
                                                style={{ width: '80px' }}
                                                value={bulkSiteId}
                                                onChange={e => setBulkSiteId(e.target.value)}
                                            />
                                            <Button
                                                size="small"
                                                onClick={() => {
                                                    const bid = parseInt(bulkSiteId);
                                                    if (isNaN(bid)) return;
                                                    setMappings(mappings.map(m => ({ ...m, site_id: bid }))); // [RESTORE] site_id focus
                                                }}
                                            >적용</Button>
                                        </div>
                                    </div>
                                    <table style={{ width: '100%', fontSize: '12px' }}>
                                        <thead style={{ background: 'var(--neutral-50)' }}>
                                            <tr style={{ textAlign: 'left' }}>
                                                <th style={{ padding: '8px 16px', width: '25%' }}>원본 포인트</th>
                                                <th style={{ padding: '8px 16px', width: '25%' }}>전송 필드명</th>
                                                <th style={{ padding: '8px 16px', width: '15%' }}>Building ID</th>
                                                <th style={{ padding: '8px 16px', width: '15%' }}>Scale</th>
                                                <th style={{ padding: '8px 16px', width: '15%' }}>Offset</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            {mappings.map((m, idx) => (
                                                <tr key={idx} style={{ borderBottom: '1px solid var(--neutral-100)' }}>
                                                    <td style={{ padding: '8px 16px' }}>
                                                        <div style={{ fontWeight: 500 }}>{allPoints.find(p => p.id === m.point_id)?.name}</div>
                                                        <div style={{ fontSize: '10px', color: 'var(--neutral-500)' }}>{allPoints.find(p => p.id === m.point_id)?.device_name}</div>
                                                    </td>
                                                    <td style={{ padding: '4px 16px' }}>
                                                        <Input
                                                            size="small"
                                                            value={m.target_field_name}
                                                            onChange={e => {
                                                                const next = [...mappings];
                                                                next[idx].target_field_name = e.target.value;
                                                                setMappings(next);
                                                            }}
                                                        />
                                                    </td>
                                                    <td style={{ padding: '4px 16px' }}>
                                                        <Input
                                                            size="small"
                                                            type="number"
                                                            value={m.site_id} // [RESTORE] site_id focus
                                                            onChange={e => {
                                                                const next = [...mappings];
                                                                next[idx].site_id = parseInt(e.target.value); // [RESTORE] site_id focus
                                                                setMappings(next);
                                                            }}
                                                        />
                                                    </td>
                                                    <td style={{ padding: '4px 16px' }}>
                                                        <Input
                                                            size="small"
                                                            type="number"
                                                            step="0.001"
                                                            value={m.conversion_config?.scale}
                                                            onChange={e => {
                                                                const next = [...mappings];
                                                                next[idx].conversion_config = { ...next[idx].conversion_config, scale: parseFloat(e.target.value) };
                                                                setMappings(next);
                                                            }}
                                                        />
                                                    </td>
                                                    <td style={{ padding: '4px 16px' }}>
                                                        <Input
                                                            size="small"
                                                            type="number"
                                                            step="0.001"
                                                            value={m.conversion_config?.offset}
                                                            onChange={e => {
                                                                const next = [...mappings];
                                                                next[idx].conversion_config = { ...next[idx].conversion_config, offset: parseFloat(e.target.value) };
                                                                setMappings(next);
                                                            }}
                                                        />
                                                    </td>
                                                </tr>
                                            ))}
                                        </tbody>
                                    </table>
                                </div>
                            </div>
                        </div>
                    )}

                    {/* STEP 4: Schedule */}
                    {currentStep === 4 && (
                        <div style={{ height: '480px', display: 'flex', flexDirection: 'column', gap: '20px' }}>
                            <Form layout="vertical">
                                <Form.Item label="전송 전략 (Transmission Mode)" required tooltip="데이터를 어떤 시점에 보낼지 결정합니다.">
                                    <Radio.Group
                                        size="large"
                                        value={transmissionMode}
                                        onChange={e => setTransmissionMode(e.target.value)}
                                        style={{ width: '100%' }}
                                    >
                                        <div style={{ display: 'flex', gap: '12px' }}>
                                            <Radio.Button value="INTERVAL" style={{ flex: 1, textAlign: 'center' }}>
                                                <i className="fas fa-clock" style={{ marginRight: '8px' }} /> 정기 전송 (Scheduled)
                                            </Radio.Button>
                                            <Radio.Button value="EVENT" style={{ flex: 1, textAlign: 'center' }}>
                                                <i className="fas fa-bolt" style={{ marginRight: '8px' }} /> 실시간/이벤트 (Event-driven)
                                            </Radio.Button>
                                        </div>
                                    </Radio.Group>
                                </Form.Item>

                                {transmissionMode === 'INTERVAL' ? (
                                    <>
                                        <Form.Item label="전송 스케줄 명칭" required>
                                            <Input size="large" value={scheduleData.schedule_name} onChange={e => setScheduleData({ ...scheduleData, schedule_name: e.target.value })} placeholder="예: [운영] 경영지원본부 매분 전송" />
                                        </Form.Item>
                                        <Row gutter={16}>
                                            <Col span={10}>
                                                <Form.Item label="전송 주기 프리셋">
                                                    <Select
                                                        size="large"
                                                        value={scheduleData.cron_expression}
                                                        onChange={val => setScheduleData({ ...scheduleData, cron_expression: val })}
                                                        placeholder="주기를 선택하세요"
                                                    >
                                                        <Select.OptGroup label="분 단위">
                                                            <Select.Option value="*/1 * * * *">매 1분</Select.Option>
                                                            <Select.Option value="*/5 * * * *">매 5분</Select.Option>
                                                            <Select.Option value="*/10 * * * *">매 10분</Select.Option>
                                                            <Select.Option value="*/30 * * * *">매 30분</Select.Option>
                                                        </Select.OptGroup>
                                                        <Select.OptGroup label="시간/일 단위">
                                                            <Select.Option value="0 * * * *">매시 정각 (1h)</Select.Option>
                                                            <Select.Option value="0 */6 * * *">6시간마다</Select.Option>
                                                            <Select.Option value="0 */12 * * *">12시간마다</Select.Option>
                                                            <Select.Option value="0 0 * * *">매일 자정 (00:00)</Select.Option>
                                                        </Select.OptGroup>
                                                    </Select>
                                                </Form.Item>
                                            </Col>
                                            <Col span={14}>
                                                <Form.Item label="Cron 표현식 직접 입력" required>
                                                    <Input
                                                        size="large"
                                                        placeholder="*/1 * * * *"
                                                        value={scheduleData.cron_expression}
                                                        onChange={e => setScheduleData({ ...scheduleData, cron_expression: e.target.value })}
                                                        suffix={<i className="fas fa-keyboard" style={{ color: 'var(--neutral-400)' }} />}
                                                    />
                                                </Form.Item>
                                            </Col>
                                        </Row>
                                    </>
                                ) : (
                                    <div style={{ padding: '32px', textAlign: 'center', background: 'var(--primary-50)', borderRadius: '12px', border: '1px dashed var(--primary-200)' }}>
                                        <i className="fas fa-bolt" style={{ fontSize: '32px', color: 'var(--primary-500)', marginBottom: '16px' }} />
                                        <div style={{ fontWeight: 600, fontSize: '16px', color: 'var(--primary-700)' }}>실시간 이벤트 전송 모드 활성</div>
                                        <div style={{ fontSize: '13px', color: 'var(--primary-600)', marginTop: '8px' }}>
                                            데이터 속성값이 변경되거나 알람이 발생한 즉시 <br />
                                            선택한 모든 전송 대상({selectedProtocols.join(', ')})으로 데이터를 송출합니다.
                                        </div>
                                    </div>
                                )}
                            </Form>
                        </div>
                    )}
                </div>

                <div className="wizard-footer" style={{ padding: '16px 32px', borderTop: '1px solid var(--neutral-100)', display: 'flex', justifyContent: 'space-between' }}>
                    <Button onClick={onClose} disabled={loading}>취소</Button>
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
            <style>{`
                .mini th { background: var(--neutral-50); padding: 8px; font-size: 11px; font-weight: 600; }
                .mini td { padding: 4px 8px; border-bottom: 1px solid var(--neutral-100); }
            `}</style>
        </Modal>
    );
};

const GatewayDetailModal: React.FC<{
    visible: boolean;
    onClose: () => void;
    gateway: Gateway | null;
    allAssignments: Record<number, Assignment[]>;
    targets: ExportTarget[];
    allProfiles: ExportProfile[];
    schedules: ExportSchedule[];
    onEdit: (gateway: Gateway) => void;
}> = ({ visible, onClose, gateway, allAssignments, targets, allProfiles, schedules, onEdit }) => {
    if (!gateway) return null;

    // Derived info with fallback for names
    const assignments = gateway ? (Object.entries(allAssignments).find(([k]) => String(k) === String(gateway.id))?.[1] || []) : [];
    const assignedProfiles = assignments.map(a => {
        const found = allProfiles.find(p => String(p.id) === String(a.profile_id));
        if (found) return found;
        if (a.name) return { id: a.profile_id, name: a.name, data_points: [] } as any;
        return null;
    }).filter(Boolean) as ExportProfile[];

    const profileIds = assignedProfiles.map(p => p.id);
    const linkedTargets = targets.filter(t => profileIds.some(pid => String(pid) === String(t.profile_id)));
    const targetIds = linkedTargets.map(t => t.id);
    const linkedSchedules = schedules.filter(s => targetIds.some(tid => String(tid) === String(s.target_id)));

    return (
        <Modal
            open={visible}
            onCancel={onClose}
            title={null}
            footer={[
                <button
                    key="edit"
                    className="btn btn-outline"
                    onClick={() => { onEdit(gateway); onClose(); }}
                    style={{ marginRight: '8px', border: '1px solid #e2e8f0' }}
                >
                    <i className="fas fa-magic" style={{ marginRight: '6px', color: 'var(--primary-600)' }} /> 설정 수정
                </button>,
                <button key="close" className="btn btn-primary" onClick={onClose} style={{ minWidth: '100px' }}>닫기</button>
            ]}
            width={800}
            centered
            getContainer={() => document.body}
            bodyStyle={{ padding: '0px', background: '#fcfcfd', borderRadius: '12px', overflow: 'hidden' }}
        >
            <div style={{ display: 'flex', flexDirection: 'column' }}>
                {/* Unified Header */}
                <div style={{ padding: '32px 40px', background: '#1e293b', color: 'white' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '24px' }}>
                        <div style={{ width: '60px', height: '60px', background: 'rgba(255,255,255,0.1)', borderRadius: '14px', display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0 }}>
                            <i className="fas fa-server" style={{ fontSize: '26px', color: '#60a5fa' }} />
                        </div>
                        <div style={{ flex: 1, minWidth: 0 }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '6px' }}>
                                <h2 style={{ margin: 0, fontSize: '22px', fontWeight: 800, color: 'white' }}>{gateway.name}</h2>
                                <Tag color={gateway.live_status?.status === 'online' ? 'success' : 'default'} style={{ border: 'none', borderRadius: '4px', fontSize: '11px', height: '22px', lineHeight: '22px', fontWeight: 700, flexShrink: 0 }}>
                                    {gateway.live_status?.status === 'online' ? 'ONLINE' : 'OFFLINE'}
                                </Tag>
                            </div>
                            <div style={{ fontSize: '14px', color: '#94a3b8', display: 'flex', alignItems: 'center', gap: '16px' }}>
                                <span><i className="fas fa-network-wired" style={{ marginRight: '8px' }} /> {gateway.ip_address}</span>
                                <span style={{ opacity: 0.5 }}>|</span>
                                <span>ID: {gateway.id}</span>
                            </div>
                            {gateway.description && (
                                <div style={{ marginTop: '12px', fontSize: '13px', color: '#cbd5e1', fontStyle: 'italic', background: 'rgba(255,255,255,0.05)', padding: '8px 12px', borderRadius: '6px', borderLeft: '3px solid #60a5fa' }}>
                                    {gateway.description}
                                </div>
                            )}
                        </div>
                    </div>
                </div>

                {/* Unified Content Body */}
                <div style={{ padding: '32px 40px' }}>
                    {/* Summary Board */}
                    <div style={{ background: 'white', padding: '24px', borderRadius: '16px', border: '1px solid #e2e8f0', boxShadow: '0 1px 3px rgba(0,0,0,0.02)', marginBottom: '32px' }}>
                        <div style={{ fontSize: '13px', fontWeight: 800, color: '#475569', marginBottom: '20px', display: 'flex', alignItems: 'center', gap: '10px', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                            <i className="fas fa-project-diagram" style={{ color: 'var(--primary-500)' }} /> 데이터 파이프라인 구성 요약
                        </div>
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr auto 1fr auto 1fr', alignItems: 'center', gap: '20px' }}>
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>DATA SOURCE</div>
                                <div style={{ fontSize: '14px', fontWeight: 800, color: '#1e293b' }}>
                                    {assignedProfiles.length > 0 ? (
                                        <>
                                            <div style={{ color: 'var(--primary-600)' }}>{assignedProfiles[0].name}</div>
                                            <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>({assignedProfiles[0].data_points?.length || 0} Points)</div>
                                        </>
                                    ) : '미지정'}
                                </div>
                            </div>
                            <i className="fas fa-long-arrow-alt-right" style={{ color: '#cbd5e1', fontSize: '18px' }} />
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>EXPORT TARGET</div>
                                <div style={{ fontSize: '15px', fontWeight: 800, color: '#1e293b' }}>{linkedTargets.length > 0 ? `${linkedTargets.length}개 엔드포인트` : '-'}</div>
                            </div>
                            <i className="fas fa-long-arrow-alt-right" style={{ color: '#cbd5e1', fontSize: '18px' }} />
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>EXPORT MODE</div>
                                <div style={{ fontSize: '15px', fontWeight: 800, color: 'var(--primary-600)' }}>{linkedSchedules.length > 0 ? '스케줄 전송' : '실시간 전송'}</div>
                            </div>
                        </div>
                    </div>

                    {/* Endpoints List */}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                        <div style={{ fontSize: '16px', fontWeight: 800, color: '#1e293b', borderLeft: '4px solid #3b82f6', paddingLeft: '12px', display: 'flex', alignItems: 'center' }}>
                            활성화된 엔드포인트 정보
                        </div>
                        {linkedTargets.length > 0 ? (
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                                {linkedTargets.map(t => {
                                    const config = t.config as any;
                                    return (
                                        <div key={t.id} style={{ border: '1px solid #e2e8f0', borderRadius: '12px', overflow: 'hidden', background: 'white', transition: 'all 0.2s', boxShadow: '0 1px 2px rgba(0,0,0,0.02)' }}>
                                            <div style={{ padding: '14px 18px', background: '#f8fafc', borderBottom: '1px solid #f1f5f9', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                <span style={{ fontWeight: 800, fontSize: '14px', color: '#334155' }}>{t.name}</span>
                                                <Tag color="blue" style={{ fontSize: '10px', height: '18px', lineHeight: '18px', margin: 0, fontWeight: 700, border: 'none', background: '#e0f2fe', color: '#0369a1' }}>{t.target_type}</Tag>
                                            </div>
                                            <div style={{ padding: '18px', fontSize: '12px', color: '#475569', wordBreak: 'break-all', fontFamily: 'monospace', lineHeight: 1.6, minHeight: '70px' }}>
                                                {t.target_type === 'HTTP' ? config.url : `${config.url} [Topic: ${config.topic}]`}
                                            </div>
                                        </div>
                                    );
                                })}
                            </div>
                        ) : (
                            <div style={{ padding: '60px', textAlign: 'center', border: '1px dashed #cbd5e1', borderRadius: '16px', background: '#f8fafc' }}>
                                <i className="fas fa-plug" style={{ fontSize: '36px', color: '#cbd5e1', marginBottom: '16px' }} />
                                <div style={{ fontSize: '15px', color: '#64748b', fontWeight: 600 }}>구성된 데이터 전송 파이프라인이 없습니다.</div>
                                <div style={{ fontSize: '13px', color: '#94a3b8', marginTop: '6px' }}>상세 전송 설정을 위해 마법사를 시작해 주세요.</div>
                            </div>
                        )}
                    </div>
                </div>
            </div>
        </Modal>
    );
};

const ExportGatewaySettings: React.FC = () => {
    const [activeTab, setActiveTab] = useState<'gateways' | 'profiles' | 'targets' | 'templates' | 'schedules'>('gateways');
    const [gateways, setGateways] = useState<Gateway[]>([]);
    const [assignments, setAssignments] = useState<Record<number, Assignment[]>>({});
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [schedules, setSchedules] = useState<ExportSchedule[]>([]);
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [loading, setLoading] = useState(false);
    const [pagination, setPagination] = useState<any>({ current_page: 1, total_pages: 1 });
    const [page, setPage] = useState(1);
    const { confirm } = useConfirmContext();

    // Registration Modal State
    const [isRegModalOpen, setIsRegModalOpen] = useState(false);

    // Edit Gateway State
    const [isEditModalOpen, setIsEditModalOpen] = useState(false);
    const [editingGateway, setEditingGateway] = useState<Gateway | null>(null);

    // Detail Modal State
    const [isDetailModalOpen, setIsDetailModalOpen] = useState(false);
    const [selectedGateway, setSelectedGateway] = useState<Gateway | null>(null);
    const [allProfiles, setAllProfiles] = useState<ExportProfile[]>([]);

    const fetchData = useCallback(async () => {
        if (gateways.length === 0) setLoading(true);
        try {
            const [gwRes, targetsRes, templatesRes, pointsRes, profilesRes, schedulesRes] = await Promise.all([
                exportGatewayApi.getGateways({ page, limit: 4 }),
                exportGatewayApi.getTargets(),
                exportGatewayApi.getTemplates(),
                exportGatewayApi.getDataPoints(),
                exportGatewayApi.getProfiles(),
                exportGatewayApi.getSchedules()
            ]);

            const data = gwRes.data;
            const gwList = data?.items || [];
            setGateways(gwList);
            if (data?.pagination) {
                setPagination(data.pagination);
            }

            setTargets(extractItems(targetsRes.data));
            setTemplates(extractItems(templatesRes.data));
            setAllPoints(pointsRes || []);
            setAllProfiles(extractItems(profilesRes.data));
            setSchedules(extractItems(schedulesRes.data));

            const assignMap: Record<number, Assignment[]> = {};
            await Promise.all((gwList || []).map(async (gw: Gateway) => {
                const response = await exportGatewayApi.getAssignments(gw.id);
                assignMap[gw.id] = extractItems(response.data);
            }));
            setAssignments(assignMap);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    }, [gateways.length, page]);

    useEffect(() => {
        fetchData();
        const interval = setInterval(fetchData, 5000);
        return () => clearInterval(interval);
    }, [fetchData]);

    const handleEditGateway = (gw: Gateway) => {
        setEditingGateway(gw);
        setIsRegModalOpen(true);
    };

    const handleDeploy = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: '배포 확인',
            message: `"${gw.name}" 게이트웨이의 설정을 실제 서버에 배포하시겠습니까?`,
            confirmText: '배포 시작',
            confirmButtonType: 'primary'
        });
        if (!confirmed) return;

        setLoading(true);
        try {
            const res = await exportGatewayApi.deployConfig(gw.id);
            if (res.success) {
                await confirm({
                    title: '배포 성공',
                    message: '최신 설정이 게이트웨이에 성공적으로 적용되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                fetchData();
            } else {
                await confirm({
                    title: '배포 실패',
                    message: res.message || '설정 적용에 실패했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (e) {
            await confirm({
                title: '에러 발생',
                message: 'API 호출 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        } finally {
            setLoading(false);
        }
    };

    const onlineCount = gateways.filter(gw => gw.live_status?.status === 'online').length;

    return (
        <>
            <ManagementLayout>
                <PageHeader
                    title="데이터 내보내기 설정"
                    description="외부 시스템(HTTP/MQTT)으로 데이터를 전송하기 위한 게이트웨이 및 데이터 매핑을 설정합니다."
                    icon="fas fa-satellite-dish"
                />

                <div className="mgmt-stats-panel" style={{ marginBottom: '24px' }}>
                    <StatCard label="전체 게이트웨이" value={gateways.length} type="neutral" />
                    <StatCard label="온라인" value={onlineCount} type="success" />
                    <StatCard label="오프라인" value={gateways.length - onlineCount} type="error" />
                </div>

                <div className="mgmt-filter-bar" style={{ marginBottom: '20px', borderBottom: '1px solid var(--neutral-200)', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', gap: '24px' }}>
                        <button
                            className={`nav-tab ${activeTab === 'gateways' ? 'active' : ''}`}
                            onClick={() => setActiveTab('gateways')}
                            style={{
                                padding: '12px 16px',
                                border: 'none',
                                background: 'none',
                                borderBottom: activeTab === 'gateways' ? '2px solid var(--primary-500)' : '2px solid transparent',
                                color: activeTab === 'gateways' ? 'var(--primary-600)' : 'var(--neutral-500)',
                                fontWeight: activeTab === 'gateways' ? 600 : 400,
                                cursor: 'pointer',
                                fontSize: '14px'
                            }}
                        >
                            <i className="fas fa-server" style={{ marginRight: '8px' }} /> 게이트웨이 설정
                        </button>
                        <button
                            className={`nav-tab ${activeTab === 'profiles' ? 'active' : ''}`}
                            onClick={() => setActiveTab('profiles')}
                            style={{
                                padding: '12px 16px',
                                border: 'none',
                                background: 'none',
                                borderBottom: activeTab === 'profiles' ? '2px solid var(--primary-500)' : '2px solid transparent',
                                color: activeTab === 'profiles' ? 'var(--primary-600)' : 'var(--neutral-500)',
                                fontWeight: activeTab === 'profiles' ? 600 : 400,
                                cursor: 'pointer',
                                fontSize: '14px'
                            }}
                        >
                            <i className="fas fa-file-export" style={{ marginRight: '8px' }} /> 데이터 매핑
                        </button>
                        <button
                            className={`nav-tab ${activeTab === 'targets' ? 'active' : ''}`}
                            onClick={() => setActiveTab('targets')}
                            style={{
                                padding: '12px 16px',
                                border: 'none',
                                background: 'none',
                                borderBottom: activeTab === 'targets' ? '2px solid var(--primary-500)' : '2px solid transparent',
                                color: activeTab === 'targets' ? 'var(--primary-600)' : 'var(--neutral-500)',
                                fontWeight: activeTab === 'targets' ? 600 : 400,
                                cursor: 'pointer',
                                fontSize: '14px'
                            }}
                        >
                            <i className="fas fa-external-link-alt" style={{ marginRight: '8px' }} /> 내보내기 대상
                        </button>
                        <button
                            className={`nav-tab ${activeTab === 'templates' ? 'active' : ''}`}
                            onClick={() => setActiveTab('templates')}
                            style={{
                                padding: '12px 16px',
                                border: 'none',
                                background: 'none',
                                borderBottom: activeTab === 'templates' ? '2px solid var(--primary-500)' : '2px solid transparent',
                                color: activeTab === 'templates' ? 'var(--primary-600)' : 'var(--neutral-500)',
                                fontWeight: activeTab === 'templates' ? 600 : 400,
                                cursor: 'pointer',
                                fontSize: '14px'
                            }}
                        >
                            <i className="fas fa-code" style={{ marginRight: '8px' }} /> 페이로드 템플릿
                        </button>
                        <button
                            className={`nav-tab ${activeTab === 'schedules' ? 'active' : ''}`}
                            onClick={() => setActiveTab('schedules')}
                            style={{
                                padding: '12px 16px',
                                border: 'none',
                                background: 'none',
                                borderBottom: activeTab === 'schedules' ? '2px solid var(--primary-500)' : '2px solid transparent',
                                color: activeTab === 'schedules' ? 'var(--primary-600)' : 'var(--neutral-500)',
                                fontWeight: activeTab === 'schedules' ? 600 : 400,
                                cursor: 'pointer',
                                fontSize: '14px'
                            }}
                        >
                            <i className="fas fa-calendar-alt" style={{ marginRight: '8px' }} /> 스케줄 관리
                        </button>
                    </div>

                    <div className="tab-actions">
                        {activeTab === 'gateways' && (
                            <button className="btn btn-primary btn-sm" onClick={() => setIsRegModalOpen(true)}>
                                <i className="fas fa-magic" style={{ marginRight: '8px' }} /> 게이트웨이 등록 (마법사)
                            </button>
                        )}
                    </div>
                </div>

                <div className="mgmt-content-area">
                    {activeTab === 'gateways' && (
                        loading && gateways.length === 0 ? (
                            <div style={{ textAlign: 'center', padding: '100px' }}>
                                <i className="fas fa-spinner fa-spin fa-2x" style={{ color: 'var(--primary-500)' }} />
                                <p style={{ marginTop: '16px', color: 'var(--neutral-500)' }}>라이브 상태 로딩 중...</p>
                            </div>
                        ) : (
                            <>
                                <div style={{ flex: 1, overflow: 'auto' }}>
                                    <GatewayList
                                        gateways={gateways}
                                        assignments={assignments}
                                        onRefresh={fetchData}
                                        onDetails={(gw) => { setSelectedGateway(gw); setIsDetailModalOpen(true); }}
                                        onDeploy={handleDeploy}
                                        onStart={async (gw) => {
                                            const confirmed = await confirm({
                                                title: '게이트웨이 시작 확인',
                                                message: `"${gw.name}" 게이트웨이 프로세스를 시작하시겠습니까?`,
                                                confirmText: '시작',
                                                confirmButtonType: 'primary'
                                            });
                                            if (!confirmed) return;

                                            try {
                                                const res = await exportGatewayApi.startGatewayProcess(gw.id);
                                                if (res.success) {
                                                    await confirm({
                                                        title: '시작 완료',
                                                        message: '게이트웨이 프로세스가 성공적으로 시작되었습니다.',
                                                        showCancelButton: false,
                                                        confirmText: '확인'
                                                    });
                                                } else {
                                                    await confirm({
                                                        title: '시작 실패',
                                                        message: res.message || '프로세스 시작에 실패했습니다.',
                                                        showCancelButton: false,
                                                        confirmButtonType: 'danger'
                                                    });
                                                }
                                                fetchData();
                                            } catch (e) {
                                                await confirm({
                                                    title: '시작 에러',
                                                    message: 'API 호출 중 오류가 발생했습니다.',
                                                    showCancelButton: false,
                                                    confirmButtonType: 'danger'
                                                });
                                            }
                                        }}
                                        onStop={async (gw) => {
                                            const confirmed = await confirm({
                                                title: '게이트웨이 중지 확인',
                                                message: `"${gw.name}" 게이트웨이 프로세스를 중지하시겠습니까?`,
                                                confirmText: '중지',
                                                confirmButtonType: 'danger'
                                            });
                                            if (!confirmed) return;

                                            try {
                                                const res = await exportGatewayApi.stopGatewayProcess(gw.id);
                                                if (res.success) {
                                                    await confirm({
                                                        title: '중지 완료',
                                                        message: '게이트웨이 프로세스를 중지했습니다.',
                                                        showCancelButton: false,
                                                        confirmText: '확인'
                                                    });
                                                } else {
                                                    await confirm({
                                                        title: '중지 실패',
                                                        message: res.message || '프로세스 중지에 실패했습니다.',
                                                        showCancelButton: false,
                                                        confirmButtonType: 'danger'
                                                    });
                                                }
                                                fetchData();
                                            } catch (e) {
                                                await confirm({
                                                    title: '중지 에러',
                                                    message: 'API 호출 중 오류가 발생했습니다.',
                                                    showCancelButton: false,
                                                    confirmButtonType: 'danger'
                                                });
                                            }
                                        }}
                                        onRestart={async (gw) => {
                                            const confirmed = await confirm({
                                                title: '게이트웨이 재시작 확인',
                                                message: `"${gw.name}" 게이트웨이 프로세스를 재시작하시겠습니까?`,
                                                confirmText: '재시작',
                                                confirmButtonType: 'warning'
                                            });
                                            if (!confirmed) return;

                                            try {
                                                const res = await exportGatewayApi.restartGatewayProcess(gw.id);
                                                if (res.success) {
                                                    await confirm({
                                                        title: '재시작 완료',
                                                        message: '게이트웨이 프로세스를 재시작했습니다.',
                                                        showCancelButton: false,
                                                        confirmText: '확인'
                                                    });
                                                } else {
                                                    await confirm({
                                                        title: '재시작 실패',
                                                        message: res.message || '프로세스 재시작에 실패했습니다.',
                                                        showCancelButton: false,
                                                        confirmButtonType: 'danger'
                                                    });
                                                }
                                                fetchData();
                                            } catch (e) {
                                                await confirm({
                                                    title: '재시작 에러',
                                                    message: 'API 호출 중 오류가 발생했습니다.',
                                                    showCancelButton: false,
                                                    confirmButtonType: 'danger'
                                                });
                                            }
                                        }}
                                    />
                                </div>
                                <Pagination
                                    current={page}
                                    total={pagination.total_count}
                                    pageSize={4}
                                    onChange={(p) => setPage(p)}
                                />
                            </>
                        )
                    )}

                    {activeTab === 'profiles' && <ExportProfileBuilder />}
                    {activeTab === 'targets' && <ExportTargetManager />}
                    {activeTab === 'templates' && <PayloadTemplateManager />}
                    {activeTab === 'schedules' && <ExportScheduleManager />}
                </div>

                {/* Replacement: Wizard used for both Register & Edit */}
                <ExportGatewayWizard
                    visible={isRegModalOpen}
                    editingGateway={editingGateway}
                    onClose={() => {
                        setIsRegModalOpen(false);
                        setEditingGateway(null);
                    }}
                    onSuccess={() => {
                        setIsRegModalOpen(false);
                        setEditingGateway(null);
                        fetchData();
                    }}
                    profiles={allProfiles}
                    targets={targets}
                    templates={templates}
                    allPoints={allPoints}
                    assignments={editingGateway ? (Object.entries(assignments).find(([k]) => String(k) === String(editingGateway.id))?.[1] || []) : []}
                />

                {/* Gateway Detail Modal */}
                <GatewayDetailModal
                    visible={isDetailModalOpen}
                    onClose={() => setIsDetailModalOpen(false)}
                    gateway={selectedGateway}
                    allAssignments={assignments}
                    targets={targets}
                    allProfiles={allProfiles}
                    schedules={schedules}
                    onEdit={handleEditGateway}
                />

            </ManagementLayout>
        </>
    );
};

export default ExportGatewaySettings;
