import React, { useState, useEffect, useCallback } from 'react';
import { Select, Tooltip, Tag, notification } from 'antd';
import exportGatewayApi, { DataPoint, Gateway, ExportProfile, ExportTarget, Assignment, PayloadTemplate, ExportTargetMapping } from '../api/services/exportGatewayApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import '../styles/management.css';

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

    // Mapping state
    const [isMappingModalOpen, setIsMappingModalOpen] = useState(false);
    const [mappingTargetId, setMappingTargetId] = useState<number | null>(null);
    // Extend type to support temporary invalid state
    const [targetMappings, setTargetMappings] = useState<(Partial<ExportTargetMapping> & { _temp_name?: string })[]>([]);
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [pasteData, setPasteData] = useState('');

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
            setTargetMappings(Array.isArray(data) ? data : (data && (data as any).rows ? (data as any).rows : []));
        } catch (error) {
            console.error(error);
        }
    };

    const handleSaveMappings = async () => {
        if (!mappingTargetId) return;

        // Validation 1: Check for unapplied paste data
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

        // Validation 2: Check for unmapped points
        const invalidMappings = targetMappings.filter(m => !m.point_id);
        if (invalidMappings.length > 0) {
            notification.warning({
                message: '매핑 누락',
                description: `매핑되지 않은 포인트가 ${invalidMappings.length}개 있습니다. 목록에서 빨간색으로 표시된 항목을 수정하거나 삭제해주세요.`,
                placement: 'topRight'
            });
            return;
        }

        try {
            // Clean up internal flags before sending
            const payload = targetMappings.map(({ _temp_name, ...rest }) => rest);
            await exportGatewayApi.saveTargetMappings(mappingTargetId, payload);
            setIsMappingModalOpen(false);
        } catch (error) {
            notification.error({ message: '저장 실패', description: '매핑 정보를 저장하는 중 오류가 발생했습니다.' });
        }
    };

    useEffect(() => { fetchData(); }, []);

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();
        try {
            if (editingTarget?.id) {
                await exportGatewayApi.updateTarget(editingTarget.id, editingTarget);
            } else {
                await exportGatewayApi.createTarget(editingTarget!);
            }
            setIsModalOpen(false);
            fetchData();
        } catch (error) {
            notification.error({ message: '저장 실패', description: '전송 타겟 정보를 저장하는 중 오류가 발생했습니다.' });
        }
    };

    const handleTestConnection = async () => {
        if (!editingTarget?.target_type || !editingTarget?.config) {
            notification.warning({ message: '입력 부족', description: '테스트할 정보가 설정되지 않았습니다.' });
            return;
        }

        setIsTesting(true);
        try {
            const response = await exportGatewayApi.testTargetConnection({
                target_type: editingTarget.target_type,
                config: editingTarget.config
            });

            if (response.success) {
                notification.success({
                    message: '테스트 성공',
                    description: response.message,
                    placement: 'topRight'
                });
            } else {
                notification.error({
                    message: '테스트 실패',
                    description: response.message || '알 수 없는 이유로 실패했습니다.',
                    placement: 'topRight'
                });
            }
        } catch (error: any) {
            notification.error({
                message: '테스트 오류',
                description: error.message || '서버와 통신 중 오류가 발생했습니다.',
                placement: 'topRight'
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
            notification.error({ message: '삭제 실패', description: '타겟을 삭제하는 중 오류가 발생했습니다.' });
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
                            <button className="mgmt-close-btn" onClick={() => setIsModalOpen(false)} style={{ fontSize: '24px' }}>&times;</button>
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
                                                onChange={e => setEditingTarget({ ...editingTarget, name: e.target.value })}
                                                placeholder="예: 실시간 전력 데이터 수집기"
                                            />
                                            <div className="mgmt-modal-form-hint">시스템에서 식별하기 위한 고유 이름입니다.</div>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>적용 프로파일</label>
                                            <select
                                                className="mgmt-select"
                                                required
                                                value={editingTarget?.profile_id || ''}
                                                onChange={e => setEditingTarget({ ...editingTarget, profile_id: parseInt(e.target.value) })}
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
                                                onChange={e => setEditingTarget({ ...editingTarget, target_type: e.target.value })}
                                            >
                                                <option value="http">HTTP POST (JSON)</option>
                                                <option value="mqtt">MQTT Publisher</option>
                                                <option value="influxdb">InfluxDB Line Protocol</option>
                                                <option value="s3">AWS S3 Storage</option>
                                                <option value="kafka">Apache Kafka</option>
                                            </select>
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
                                                        }}
                                                    />
                                                    <div className="mgmt-modal-form-hint">필요한 경우 Bearer Token 등을 입력하세요.</div>
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
                    <div className="mgmt-modal-content" style={{ maxWidth: '98vw', width: '98%', height: '95vh', display: 'flex', flexDirection: 'column' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">데이터 포인트 매핑 설정</h3>
                            <button className="mgmt-close-btn" onClick={() => setIsMappingModalOpen(false)}>&times;</button>
                        </div>
                        <div className="mgmt-modal-body" style={{ display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                            <div className="mgmt-alert info" style={{ marginBottom: '15px', flexShrink: 0 }}>
                                <i className="fas fa-info-circle" /> 이 타겟으로 전송할 데이터 포인트와 전송될 필드명을 매핑합니다.
                            </div>

                            {/* Bulk Paste Section */}
                            <div style={{ marginBottom: '15px', padding: '15px', backgroundColor: '#f8f9fa', borderRadius: '4px', border: '1px solid #ddd' }}>
                                <h4 style={{ margin: '0 0 10px 0', fontSize: '14px' }}>📋 엑셀 붙여넣기 (일괄 등록)</h4>
                                <div style={{ display: 'flex', gap: '10px', alignItems: 'flex-start', flexDirection: 'column' }}>
                                    <textarea
                                        className="mgmt-input"
                                        style={{ height: '200px', resize: 'vertical', width: '100%', fontFamily: 'monospace', fontSize: '13px', lineHeight: '1.5' }}
                                        placeholder={`엑셀 데이터 붙여넣기 예시:\n[PulseOne 포인트명] (탭) [외부 시스템 필드명] (탭) [설명]\n\nSensor_A_01\tFactory_Temp_01\t1공장 온도\nSensor_A_02\tFactory_Humid_01\t1공장 습도`}
                                        value={pasteData}
                                        onChange={(e) => setPasteData(e.target.value)}
                                    />
                                    <div style={{ display: 'flex', justifyContent: 'flex-end', width: '100%' }}>
                                        <button
                                            className="mgmt-btn mgmt-btn-primary mgmt-btn-sm"
                                            onClick={() => {
                                                if (!pasteData.trim()) {
                                                    notification.info({ message: '데이터 없음', description: '붙여넣을 데이터가 없습니다.' });
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

                                                        // 중복 체크 (이미 등록된 포인트인지)
                                                        const isAlreadyMapped = newMappings.some(m => m.point_id && m.point_id === point?.id);

                                                        if (point && !isAlreadyMapped) {
                                                            newMappings.push({
                                                                point_id: point.id,
                                                                target_field_name: targetField,
                                                                target_description: desc,
                                                                is_enabled: true
                                                            });
                                                            addedCount++;
                                                        } else if (!point) {
                                                            // 포인트가 없으면 에러 상태로 추가 (사용자가 수정하도록 유도)
                                                            newMappings.push({
                                                                point_id: undefined,
                                                                target_field_name: targetField,
                                                                target_description: desc,
                                                                is_enabled: true,
                                                                _temp_name: pointName // 원래 입력한 잘못된 이름 저장
                                                            });
                                                            failedCount++;
                                                        }
                                                    }
                                                });

                                                if (addedCount > 0 || failedCount > 0) {
                                                    setTargetMappings(newMappings);
                                                    setPasteData(''); // Clear on success
                                                }

                                                let message = `결과 리포트:\n- 자동 매칭 성공: ${addedCount}개`;
                                                if (failedCount > 0) {
                                                    message += `\n- 매칭 실패 (확인 필요): ${failedCount}개\n\n매칭되지 않은 항목은 테이블에 빨간색으로 표시됩니다.\n드롭다운에서 올바른 포인트를 선택해주세요.`;
                                                } else if (addedCount === 0) {
                                                    message += `\n(추가된 항목이 없습니다)`;
                                                }

                                                notification.success({
                                                    message: '매핑 적용 결과',
                                                    description: message,
                                                    placement: 'topRight',
                                                    duration: 6
                                                });
                                            }}
                                        >
                                            <i className="fas fa-magic" style={{ marginRight: '5px' }} />
                                            매핑 적용 및 결과 확인
                                        </button>
                                    </div>
                                </div>
                                <p style={{ margin: '5px 0 0 0', fontSize: '12px', color: '#666' }}>
                                    * <b>PulseOne 포인트명</b>은 시스템에 등록된 이름과 정확히 일치해야 합니다. (대소문자 구분 없음)
                                </p>
                            </div>

                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px', marginTop: '10px' }}>
                                <h4 style={{ margin: 0, fontSize: '14px', color: '#333' }}>📋 매핑 리스트 ({targetMappings.length})</h4>
                                <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => {
                                    setTargetMappings([...targetMappings, { point_id: undefined, target_field_name: '', target_description: '', is_enabled: true }]);
                                }}>
                                    <i className="fas fa-plus" /> 1행 추가
                                </button>
                            </div>

                            <div className="mgmt-table-container" style={{ flex: 1, overflowY: 'auto', border: '1px solid #eee', borderRadius: '4px' }}>
                                <table className="mgmt-table">
                                    <thead>
                                        <tr>
                                            <th>PulseOne 포인트</th>
                                            <th>외부 전송 필드명 (Target Key)</th>
                                            <th>설명</th>
                                            <th>관리</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {targetMappings.map((mapping, idx) => (
                                            <tr key={idx}>
                                                <td>
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
                                                                    _temp_name: undefined // 수정했으므로 에러 임시값 제거
                                                                };
                                                                setTargetMappings(newMappings);
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
                                                <td>
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
                                                <td>
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
                                                <td>
                                                    <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs mgmt-btn-error" onClick={() => {
                                                        const newMappings = targetMappings.filter((_, i) => i !== idx);
                                                        setTargetMappings(newMappings);
                                                    }}>
                                                        <i className="fas fa-trash" />
                                                    </button>
                                                </td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            </div>
                        </div>
                        <div className="mgmt-modal-footer">
                            <div className="mgmt-footer-right">
                                <button className="mgmt-btn mgmt-btn-outline" onClick={() => setIsMappingModalOpen(false)}>닫기</button>
                                <button className="mgmt-btn mgmt-btn-primary" onClick={handleSaveMappings}>저장하기</button>
                            </div>
                        </div>
                    </div>
                </div>
            )}
        </div >
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

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();
        try {
            // JSON 유효성 검사
            let templateJson = editingTemplate?.template_json;
            if (typeof templateJson === 'string') {
                try {
                    templateJson = JSON.parse(templateJson);
                } catch (e) {
                    alert('유효하지 않은 JSON 형식입니다.');
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
            setIsModalOpen(false);
            fetchTemplates();
        } catch (error) {
            notification.error({ message: '저장 실패', description: '템플릿을 저장하는 중 오류가 발생했습니다.' });
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
            notification.error({ message: '삭제 실패', description: '템플릿을 삭제하는 중 오류가 발생했습니다.' });
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
                            <button className="mgmt-modal-close" onClick={() => setIsModalOpen(false)}>&times;</button>
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
                                            onChange={e => setEditingTemplate({ ...editingTemplate, name: e.target.value })}
                                            placeholder="예: 표준 JSON 전송 포맷"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>시스템 유형</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            value={editingTemplate?.system_type || ''}
                                            onChange={e => setEditingTemplate({ ...editingTemplate, system_type: e.target.value })}
                                            placeholder="예: AWS IoT, Azure EventHub 등"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>템플릿 구조 (JSON)</label>
                                        <textarea
                                            className="mgmt-input"
                                            required
                                            value={editingTemplate?.template_json || ''}
                                            onChange={e => setEditingTemplate({ ...editingTemplate, template_json: e.target.value })}
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
                                <button type="button" className="btn-outline" onClick={() => setIsModalOpen(false)}>취소</button>
                                <button type="submit" className="btn-primary">저장하기</button>
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
    onRemove: (pointId: number) => void;
}> = ({ selectedPoints, onSelect, onRemove }) => {
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [loading, setLoading] = useState(false);
    const [searchTerm, setSearchTerm] = useState('');

    useEffect(() => {
        const fetchPoints = async () => {
            setLoading(true);
            try {
                const points = await exportGatewayApi.getDataPoints();
                setAllPoints(points);
            } catch (e) {
                console.error(e);
            } finally {
                setLoading(false);
            }
        };
        fetchPoints();
    }, []);

    const filteredPoints = allPoints.filter(p =>
        p.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        p.device_name.toLowerCase().includes(searchTerm.toLowerCase())
    );

    const isSelected = (id: number) => selectedPoints.some(p => p.id === id);

    return (
        <div className="point-selector-container" style={{ border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '12px', background: 'var(--neutral-50)' }}>
            <div className="search-box" style={{ position: 'relative', marginBottom: '12px' }}>
                <i className="fas fa-search" style={{ position: 'absolute', left: '10px', top: '10px', color: 'var(--neutral-400)' }} />
                <input
                    type="text"
                    className="form-control"
                    placeholder="포인트 또는 장치 이름 검색..."
                    value={searchTerm}
                    onChange={e => setSearchTerm(e.target.value)}
                    style={{ paddingLeft: '32px' }}
                />
            </div>

            <div className="point-list" style={{ maxHeight: '200px', overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: '4px', background: 'white', border: '1px solid var(--neutral-200)', borderRadius: '6px' }}>
                {loading ? (
                    <div style={{ textAlign: 'center', padding: '20px', color: 'var(--neutral-500)' }}>
                        <i className="fas fa-spinner fa-spin" /> 로딩 중...
                    </div>
                ) : filteredPoints.length === 0 ? (
                    <div style={{ textAlign: 'center', padding: '20px', color: 'var(--neutral-500)' }}>검색 결과가 없습니다.</div>
                ) : (
                    filteredPoints.map(p => {
                        const selected = isSelected(p.id);
                        return (
                            <div key={p.id} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '8px 12px', borderBottom: '1px solid var(--neutral-100)' }}>
                                <div style={{ fontSize: '13px' }}>
                                    <div style={{ fontWeight: 600, color: 'var(--neutral-800)' }}>{p.name}</div>
                                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>{p.device_name} ({p.site_name})</div>
                                </div>
                                <button
                                    className={`btn btn-xs ${selected ? 'btn-danger btn-outline' : 'btn-outline'}`}
                                    onClick={() => selected ? onRemove(p.id) : onSelect(p)}
                                    type="button"
                                >
                                    {selected ? '제거' : '추가'}
                                </button>
                            </div>
                        );
                    })
                )}
            </div>

            <div className="selected-points-summary" style={{ marginTop: '12px', paddingTop: '12px', borderTop: '1px solid var(--neutral-200)' }}>
                <div style={{ fontSize: '12px', fontWeight: 600, marginBottom: '8px', color: 'var(--neutral-700)' }}>
                    선택된 포인트 ({selectedPoints.length})
                </div>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                    {selectedPoints.map(p => (
                        <span key={p.id} className="badge primary" style={{ display: 'flex', alignItems: 'center', gap: '6px', padding: '4px 8px' }}>
                            {p.name}
                            <i className="fas fa-times" style={{ cursor: 'pointer', opacity: 0.8 }} onClick={() => onRemove(p.id)} />
                        </span>
                    ))}
                    {selectedPoints.length === 0 && <span style={{ color: 'var(--neutral-400)', fontSize: '12px', fontStyle: 'italic' }}>선택된 포인트 없음</span>}
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
    const { confirm } = useConfirmContext();

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
        try {
            if (editingProfile?.id) {
                await exportGatewayApi.updateProfile(editingProfile.id, editingProfile);
            } else {
                await exportGatewayApi.createProfile(editingProfile!);
            }
            setIsModalOpen(false);
            fetchProfiles();
        } catch (error) {
            notification.error({ message: '저장 실패', description: '프로파일을 저장하는 중 오류가 발생했습니다.' });
        }
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
            notification.error({ message: '삭제 실패', description: '프로파일을 삭제하는 중 오류가 발생했습니다.' });
        }
    };

    const handlePointSelect = (point: any) => {
        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: [...currentPoints, { id: point.id, name: point.name, device: point.device_name, target_field_name: point.name }]
        });
    };

    const handlePointRemove = (pointId: number) => {
        const currentPoints = editingProfile?.data_points || [];
        setEditingProfile({
            ...editingProfile,
            data_points: currentPoints.filter((p: any) => p.id !== pointId)
        });
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
                                        <button className="btn btn-outline btn-xs" onClick={() => { setEditingProfile(p); setIsModalOpen(true); }}>수정</button>
                                        <button className="btn btn-outline btn-xs btn-danger" onClick={() => handleDelete(p.id)}>삭제</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-content" style={{ maxWidth: '650px' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">{editingProfile?.id ? "프로파일 수정" : "신규 프로파일 생성"}</h3>
                            <button className="mgmt-modal-close" onClick={() => setIsModalOpen(false)}>&times;</button>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body">
                                <div className="mgmt-modal-form-section">
                                    <div className="mgmt-modal-form-group">
                                        <label>프로파일 명칭</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            required
                                            value={editingProfile?.name || ''}
                                            onChange={e => setEditingProfile({ ...editingProfile, name: e.target.value })}
                                            placeholder="예: 공장A 전력량 실시간 전송"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>상세 설명</label>
                                        <textarea
                                            className="mgmt-input"
                                            value={editingProfile?.description || ''}
                                            onChange={e => setEditingProfile({ ...editingProfile, description: e.target.value })}
                                            placeholder="이 프로파일의 용도와 구성을 설명해주세요"
                                            style={{ height: '60px', padding: '10px', resize: 'none' }}
                                        />
                                    </div>
                                </div>

                                <div className="mgmt-modal-form-section">
                                    <h3 style={{ marginBottom: '12px' }}>
                                        <i className="fas fa-list-ul" style={{ marginRight: '8px', color: 'var(--primary-500)' }} />
                                        데이터 포인트 구성
                                    </h3>
                                    <DataPointSelector
                                        selectedPoints={editingProfile?.data_points || []}
                                        onSelect={handlePointSelect}
                                        onRemove={handlePointRemove}
                                    />
                                </div>
                            </div>
                            <div className="mgmt-modal-footer">
                                <button type="button" className="btn-outline" onClick={() => setIsModalOpen(false)}>취소</button>
                                <button type="submit" className="btn-primary">프로파일 저장</button>
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
    onManageProfile: (gateway: Gateway) => void;
    onDeploy: (gateway: Gateway) => void;
    onStart: (gateway: Gateway) => void;
    onStop: (gateway: Gateway) => void;
    onRestart: (gateway: Gateway) => void;
}> = ({ gateways, assignments, onRefresh, onManageProfile, onDeploy, onStart, onStop, onRestart }) => {
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
                                    <div style={{ display: 'flex', gap: '4px' }}>
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
                                            {(!gw.processes || gw.processes.length === 0) ? (
                                                <button className="btn btn-outline btn-xs" style={{ flex: 1 }} onClick={() => onStart(gw)}>
                                                    <i className="fas fa-play" style={{ fontSize: '10px' }} /> 시작
                                                </button>
                                            ) : (
                                                <>
                                                    <button className="btn btn-outline btn-xs btn-danger" style={{ flex: 1 }} onClick={() => onStop(gw)}>
                                                        <i className="fas fa-stop" style={{ fontSize: '10px' }} /> 중지
                                                    </button>
                                                    <button className="btn btn-outline btn-xs" style={{ flex: 1 }} onClick={() => onRestart(gw)}>
                                                        <i className="fas fa-redo" style={{ fontSize: '10px' }} /> 재시작
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
                                <button className="btn btn-outline btn-sm" onClick={() => onManageProfile(gw)} style={{ flex: 1 }}>
                                    <i className="fas fa-cog" /> 관리
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

const ExportGatewaySettings: React.FC = () => {
    const [activeTab, setActiveTab] = useState<'gateways' | 'profiles' | 'targets' | 'templates'>('gateways');
    const [gateways, setGateways] = useState<Gateway[]>([]);
    const [assignments, setAssignments] = useState<Record<number, Assignment[]>>({});
    const [loading, setLoading] = useState(false);
    const { confirm } = useConfirmContext();

    // Registration Modal State
    const [isRegModalOpen, setIsRegModalOpen] = useState(false);
    const [newGateway, setNewGateway] = useState<Partial<Gateway>>({
        name: '',
        ip_address: '127.0.0.1'
    });

    // Assignment Modal State
    const [isAssignModalOpen, setIsAssignModalOpen] = useState(false);
    const [selectedGateway, setSelectedGateway] = useState<Gateway | null>(null);
    const [allProfiles, setAllProfiles] = useState<ExportProfile[]>([]);

    const fetchData = useCallback(async () => {
        if (gateways.length === 0) setLoading(true);
        try {
            const response = await exportGatewayApi.getGateways();
            // No filter needed as this API is dedicated to export gateways
            const gwList = response.data || [];
            setGateways(gwList);

            const assignMap: Record<number, Assignment[]> = {};
            await Promise.all((gwList || []).map(async (gw: Gateway) => {
                const response = await exportGatewayApi.getAssignments(gw.id);
                const data = response.data;
                assignMap[gw.id] = Array.isArray(data) ? data : (data && (data as any).rows ? (data as any).rows : []);
            }));
            setAssignments(assignMap);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    }, [gateways.length]);

    useEffect(() => {
        fetchData();
        const interval = setInterval(fetchData, 5000);
        return () => clearInterval(interval);
    }, [fetchData]);

    const handleManageProfile = async (gw: Gateway) => {
        setSelectedGateway(gw);
        try {
            const response = await exportGatewayApi.getProfiles();
            setAllProfiles(response.data || []);
            setIsAssignModalOpen(true);
        } catch (error) {
            notification.error({ message: '로딩 실패', description: '프로파일 목록을 불러오지 못했습니다.' });
        }
    };

    const handleToggleAssignment = async (profileId: number, isAssigned: boolean) => {
        if (!selectedGateway) return;
        try {
            if (isAssigned) {
                await exportGatewayApi.unassignProfile(selectedGateway.id, profileId);
            } else {
                await exportGatewayApi.assignProfile(selectedGateway.id, profileId);
            }
            const response = await exportGatewayApi.getAssignments(selectedGateway.id);
            setAssignments(prev => ({ ...prev, [selectedGateway.id]: response.data || [] }));
        } catch (error) {
            notification.error({ message: '할당 실패', description: '프로파일 할당을 변경하는 중 오류가 발생했습니다.' });
        }
    };

    const handleRegister = async () => {
        if (!newGateway.name || !newGateway.ip_address) {
            notification.warning({ message: '입력 부족', description: '모든 필드를 입력해주세요.' });
            return;
        }
        try {
            await exportGatewayApi.registerGateway(newGateway);
            setIsRegModalOpen(false);
            setNewGateway({ name: '', ip_address: '' });
            fetchData();
        } catch (error) {
            notification.error({ message: '등록 실패', description: (error as any).message });
        }
    };

    const handleDeploy = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: '설정 배포 확인',
            message: `${gw.server_name} 게이트웨이에 설정을 배포하시겠습니까? 이 작업은 실시간으로 구동 중인 게이트웨이에 리로드 명령을 전송합니다.`,
            confirmText: '배포 시작',
            confirmButtonType: 'primary'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deployConfig(gw.id);
            await confirm({
                title: '배포 완료',
                message: '게이트웨이로 배포 명령이 전송되었습니다.',
                showCancelButton: false,
                confirmText: '확인'
            });
        } catch (error) {
            notification.error({ message: '배포 실패', description: (error as any).message });
        }
    };

    const onlineCount = gateways.filter(gw => gw.live_status?.status === 'online').length;

    return (
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
                </div>

                <div className="tab-actions">
                    {activeTab === 'gateways' && (
                        <button className="btn btn-primary btn-sm" onClick={() => setIsRegModalOpen(true)}>
                            <i className="fas fa-plus" style={{ marginRight: '8px' }} /> 게이트웨이 추가
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
                        <GatewayList
                            gateways={gateways}
                            assignments={assignments}
                            onRefresh={fetchData}
                            onManageProfile={handleManageProfile}
                            onDeploy={handleDeploy}
                            onStart={async (gw) => {
                                try {
                                    await exportGatewayApi.startGatewayProcess(gw.id);
                                    fetchData();
                                } catch (e) { notification.error({ message: '시작 실패', description: '게이트웨이 프로세스를 시작하는 중 오류가 발생했습니다.' }); }
                            }}
                            onStop={async (gw) => {
                                try {
                                    await exportGatewayApi.stopGatewayProcess(gw.id);
                                    fetchData();
                                } catch (e) { notification.error({ message: '중지 실패', description: '게이트웨이 프로세스를 중지하는 중 오류가 발생했습니다.' }); }
                            }}
                            onRestart={async (gw) => {
                                try {
                                    await exportGatewayApi.restartGatewayProcess(gw.id);
                                    fetchData();
                                } catch (e) { notification.error({ message: '재시작 실패', description: '게이트웨이 프로세스를 재시작하는 중 오류가 발생했습니다.' }); }
                            }}
                        />
                    )
                )}

                {activeTab === 'profiles' && <ExportProfileBuilder />}
                {activeTab === 'targets' && <ExportTargetManager />}
                {activeTab === 'templates' && <PayloadTemplateManager />}
            </div>

            {/* Registration Modal */}
            {isRegModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-container" style={{ maxWidth: '450px' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">신규 게이트웨이 등록</h3>
                            <button className="mgmt-close-btn" onClick={() => setIsRegModalOpen(false)}>&times;</button>
                        </div>
                        <div className="mgmt-modal-body">
                            <div className="mgmt-modal-form-section">
                                <div className="mgmt-modal-form-group">
                                    <label>게이트웨이 식별 명칭</label>
                                    <input
                                        className="mgmt-input"
                                        type="text"
                                        required
                                        placeholder="예: Factory-A-Gateway"
                                        value={newGateway.name}
                                        onChange={e => setNewGateway({ ...newGateway, name: e.target.value })}
                                    />
                                    <div className="mgmt-modal-form-hint">네트워크 상에서 이 게이트웨이를 구분할 이름입니다.</div>
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>게이트웨이 IP 주소 (자신의 주소)</label>
                                    <input
                                        className="mgmt-input"
                                        type="text"
                                        required
                                        placeholder="예: 127.0.0.1"
                                        value={newGateway.ip_address}
                                        onChange={e => setNewGateway({ ...newGateway, ip_address: e.target.value })}
                                    />
                                    <div className="mgmt-modal-form-hint">이 게이트웨이 기기(서버)의 IP 주소를 입력하세요. (기본값: 127.0.0.1)</div>
                                </div>
                            </div>
                        </div>
                        <div className="mgmt-modal-footer">
                            <button className="btn-outline" onClick={() => setIsRegModalOpen(false)}>닫기</button>
                            <button className="btn-primary" onClick={handleRegister}>게이트웨이 등록</button>
                        </div>
                    </div>
                </div>
            )}

            {/* Assignment Modal */}
            {isAssignModalOpen && (
                <div className="mgmt-modal-overlay" style={{ display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <div className="mgmt-modal-container" style={{ width: '550px' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">프로파일 할당: {selectedGateway?.server_name}</h3>
                            <button className="mgmt-close-btn" onClick={() => setIsAssignModalOpen(false)}>&times;</button>
                        </div>
                        <div className="mgmt-modal-body">
                            <p style={{ marginBottom: '20px', color: 'var(--neutral-500)', fontSize: '14px' }}>
                                게이트웨이에 적용할 데이터 내보내기 프로파일을 선택하세요.
                            </p>
                            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                {allProfiles.map(p => {
                                    const isAssigned = (assignments[selectedGateway?.id || 0] || []).some(a => a.profile_id === p.id);
                                    return (
                                        <div key={p.id} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '16px', border: '1px solid var(--neutral-100)', borderRadius: '10px', background: 'var(--neutral-50)' }}>
                                            <div>
                                                <div style={{ fontWeight: 600, color: 'var(--neutral-800)' }}>{p.name}</div>
                                                <div style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{p.description}</div>
                                            </div>
                                            <button
                                                className={`btn btn-sm ${isAssigned ? 'btn-outline btn-danger' : 'btn-primary'}`}
                                                onClick={() => handleToggleAssignment(p.id, isAssigned)}
                                            >
                                                {isAssigned ? '해제' : '할당'}
                                            </button>
                                        </div>
                                    );
                                })}
                                {allProfiles.length === 0 && (
                                    <div style={{ textAlign: 'center', padding: '100px', background: 'var(--neutral-50)', borderRadius: '12px' }}>
                                        <i className="fas fa-info-circle" style={{ fontSize: '24px', color: 'var(--neutral-300)', marginBottom: '12px' }} />
                                        <p style={{ color: 'var(--neutral-400)', fontSize: '14px' }}>생성된 프로파일이 없습니다.</p>
                                        <button className="btn btn-outline btn-sm" style={{ marginTop: '12px' }} onClick={() => { setIsAssignModalOpen(false); setActiveTab('profiles'); }}>
                                            프로파일 만들러 가기
                                        </button>
                                    </div>
                                )}
                            </div>
                        </div>
                        <div className="mgmt-modal-footer">
                            <button className="btn btn-primary" onClick={() => setIsAssignModalOpen(false)}>확인</button>
                        </div>
                    </div>
                </div>
            )}
        </ManagementLayout>
    );
};

export default ExportGatewaySettings;
