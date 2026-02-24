import React, { useState, useEffect } from 'react';
import { Site } from '../../../types/common';
import { SiteApiService } from '../../../api/services/siteApi';
import { CollectorApiService, EdgeServer } from '../../../api/services/collectorApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import './SiteModal.css';

interface SiteDetailModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    siteId: number | null;
}

export const SiteDetailModal: React.FC<SiteDetailModalProps> = ({
    isOpen,
    onClose,
    onSave,
    siteId
}) => {
    const { confirm } = useConfirmContext();
    const [site, setSite] = useState<Site | null>(null);
    const [isEditing, setIsEditing] = useState(false);
    const [formData, setFormData] = useState<Partial<Site>>({});
    const [allSites, setAllSites] = useState<Site[]>([]);
    const [collectors, setCollectors] = useState<EdgeServer[]>([]);
    const [availableCollectors, setAvailableCollectors] = useState<EdgeServer[]>([]);
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        if (isOpen && siteId) {
            loadAll(siteId);
            setIsEditing(false);
        }
    }, [isOpen, siteId]);

    const loadAll = async (id: number) => {
        try {
            setLoading(true);
            const [siteRes, collectorsRes, sitesRes] = await Promise.all([
                SiteApiService.getSite(id),
                CollectorApiService.getCollectorsBySite(id),
                SiteApiService.getSites()
            ]);
            if (siteRes.success && siteRes.data) {
                setSite(siteRes.data);
                setFormData(siteRes.data);
                setError(null);
            } else {
                setError('사이트 정보를 불러오는데 실패했습니다.');
            }
            if (collectorsRes.success && collectorsRes.data) {
                setCollectors(collectorsRes.data);
            } else {
                setCollectors([]);
            }
            if (sitesRes.success && sitesRes.data) {
                setAllSites(sitesRes.data.items.filter(s => s.id !== id));
            }
        } catch (err: any) {
            setError('데이터를 불러오는데 실패했습니다.');
        } finally {
            setLoading(false);
        }
    };

    const loadAvailableCollectors = async () => {
        try {
            const res = await CollectorApiService.getUnassignedCollectors();
            if (res.success && res.data) {
                setAvailableCollectors(res.data);
            }
        } catch (e) {
            setAvailableCollectors([]);
        }
    };

    const refreshCollectors = async () => {
        if (!siteId) return;
        try {
            const res = await CollectorApiService.getCollectorsBySite(siteId);
            if (res.success && res.data) setCollectors(res.data);
        } catch (e) { setCollectors([]); }
    };

    const handleAssignCollector = async (collectorId: number) => {
        if (!siteId) return;
        try {
            setSaving(true);
            const res = await CollectorApiService.reassignCollector(collectorId, siteId);
            if (res.success) {
                await refreshCollectors();
                await loadAvailableCollectors();
            } else {
                setError(res.message || 'Collector 배정에 실패했습니다.');
            }
        } catch (err: any) {
            setError(err.message || 'Collector 배정 중 오류가 발생했습니다.');
        } finally {
            setSaving(false);
        }
    };

    const handleUnassignCollector = async (collectorId: number) => {
        try {
            setSaving(true);
            const res = await CollectorApiService.reassignCollector(collectorId, 0);
            if (res.success) {
                await refreshCollectors();
                await loadAvailableCollectors();
            } else {
                setError(res.message || 'Collector 해제에 실패했습니다.');
            }
        } catch (err: any) {
            setError(err.message || 'Collector 해제 중 오류가 발생했습니다.');
        } finally {
            setSaving(false);
        }
    };

    const loadSite = async (id: number) => {
        try {
            setLoading(true);
            const res = await SiteApiService.getSite(id);
            if (res.success && res.data) {
                setSite(res.data);
                setFormData(res.data);
                setError(null);
            }
        } catch (err: any) {
            setError('사이트 정보를 불러오는데 실패했습니다.');
        } finally {
            setLoading(false);
        }
    };

    const loadPotentialParents = async (currentSiteId: number) => {
        try {
            const res = await SiteApiService.getSites();
            if (res.success && res.data) {
                setAllSites(res.data.items.filter(s => s.id !== currentSiteId));
            }
        } catch (err) {
            console.error('Failed to load sites:', err);
        }
    };

    const handleEditToggle = () => {
        const next = !isEditing;
        setIsEditing(next);
        setError(null);
        if (next) {
            loadAvailableCollectors();
        }
    };

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!siteId) return;

        const isConfirmed = await confirm({
            title: '변경 사항 저장',
            message: '입력하신 변경 내용을 저장하시겠습니까?',
            confirmText: '저장',
            confirmButtonType: 'primary'
        });

        if (!isConfirmed) return;

        try {
            setSaving(true);
            setError(null);
            const res = await SiteApiService.updateSite(siteId, formData);
            if (res.success) {
                await confirm({
                    title: '저장 완료',
                    message: '사이트 정보가 성공적으로 수정되었습니다.',
                    confirmText: '확인',
                    showCancelButton: false,
                    confirmButtonType: 'primary'
                });
                await loadSite(siteId);
                onSave();
                onClose();
            } else {
                setError(res.message || '업데이트에 실패했습니다.');
            }
        } catch (err: any) {
            setError(err.message || '서버 통신 중 오류가 발생했습니다.');
        } finally {
            setSaving(false);
        }
    };

    const handleDelete = async () => {
        if (!site) return;

        const confirmed = await confirm({
            title: '사이트 삭제 확인',
            message: `'${site.name}' 사이트를 삭제하시겠습니까? 하위 사이트가 있는 경우 삭제할 수 없습니다.`,
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setSaving(true);
                const res = await SiteApiService.deleteSite(site.id);
                if (res.success) {
                    await confirm({
                        title: '삭제 완료',
                        message: '사이트가 성공적으로 삭제되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || '삭제에 실패했습니다. 하위 사이트 유무를 확인하세요.');
                }
            } catch (err: any) {
                setError(err.message || '삭제 중 오류가 발생했습니다.');
            } finally {
                setSaving(false);
            }
        }
    };

    // ── 공통 스타일 ──
    const sectionStyle: React.CSSProperties = {
        padding: '12px 14px',
        border: '1px solid var(--neutral-150, #e8e8e8)',
        borderRadius: '8px',
        background: 'var(--neutral-25, #fafafa)',
    };
    const sectionTitleStyle: React.CSSProperties = {
        fontSize: '13px', fontWeight: 600, marginBottom: '10px',
        color: 'var(--neutral-700)', display: 'flex', alignItems: 'center', gap: '6px'
    };
    const fieldLabel: React.CSSProperties = {
        fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '2px', fontWeight: 500
    };
    const fieldValue: React.CSSProperties = {
        fontSize: '13px', color: 'var(--neutral-800)', marginBottom: '8px'
    };
    const inputStyle: React.CSSProperties = {
        width: '100%', padding: '6px 10px', fontSize: '13px',
        border: '1px solid var(--neutral-300)', borderRadius: '6px',
        marginBottom: '8px', boxSizing: 'border-box'
    };
    const selectStyle: React.CSSProperties = { ...inputStyle };

    const renderContent = () => {
        if (loading) return <div style={{ textAlign: 'center', padding: '40px 0', color: 'var(--neutral-400)', fontSize: '13px' }}><i className="fas fa-spinner fa-spin" style={{ marginRight: '6px' }}></i>로딩 중...</div>;
        if (!site) return <div className="mgmt-alert mgmt-alert-danger">사이트 정보를 불러오지 못했습니다.</div>;

        if (isEditing) {
            return (
                <form id="site-detail-form" onSubmit={handleSubmit}>
                    {/* 2-column compact grid */}
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                        {/* 기본 정보 */}
                        <div style={sectionStyle}>
                            <div style={sectionTitleStyle}><i className="fas fa-info-circle"></i> 기본 정보</div>
                            <div>
                                <div style={fieldLabel}>사이트명 *</div>
                                <input style={inputStyle} type="text" value={formData.name || ''} onChange={e => setFormData({ ...formData, name: e.target.value })} required />
                            </div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                                <div>
                                    <div style={fieldLabel}>사이트 코드</div>
                                    <input style={inputStyle} type="text" value={site.code} disabled />
                                </div>
                                <div>
                                    <div style={fieldLabel}>사이트 유형</div>
                                    <select style={selectStyle} value={formData.site_type || ''} onChange={e => setFormData({ ...formData, site_type: e.target.value })}>
                                        <option value="company">본사</option>
                                        <option value="office">오피스/지사</option>
                                        <option value="factory">공장/플랜트</option>
                                        <option value="building">빌딩/건물</option>
                                        <option value="floor">층</option>
                                        <option value="room">실</option>
                                    </select>
                                </div>
                            </div>
                        </div>

                        {/* 위치 + 상위 사이트 */}
                        <div style={sectionStyle}>
                            <div style={sectionTitleStyle}><i className="fas fa-map-marker-alt"></i> 위치 정보</div>
                            <div>
                                <div style={fieldLabel}>상위 사이트</div>
                                <select style={selectStyle} value={formData.parent_site_id || ''} onChange={e => setFormData({ ...formData, parent_site_id: e.target.value ? parseInt(e.target.value) : undefined })}>
                                    <option value="">없음 (최상위)</option>
                                    {allSites.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}
                                </select>
                            </div>
                            <div>
                                <div style={fieldLabel}>주소</div>
                                <input style={inputStyle} type="text" value={formData.address || ''} onChange={e => setFormData({ ...formData, address: e.target.value })} />
                            </div>
                        </div>

                        {/* 담당자 */}
                        <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                            <div style={sectionTitleStyle}><i className="fas fa-user-tie"></i> 담당자 정보</div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
                                <div>
                                    <div style={fieldLabel}>담당자 성함</div>
                                    <input style={inputStyle} type="text" value={formData.manager_name || formData.contact_name || ''} onChange={e => setFormData({ ...formData, manager_name: e.target.value })} placeholder="담당자 성함" />
                                </div>
                                <div>
                                    <div style={fieldLabel}>이메일</div>
                                    <input style={inputStyle} type="email" value={formData.manager_email || formData.contact_email || ''} onChange={e => setFormData({ ...formData, manager_email: e.target.value })} placeholder="example@pulseone.com" />
                                </div>
                                <div>
                                    <div style={fieldLabel}>연락처</div>
                                    <input style={inputStyle} type="text" value={formData.manager_phone || formData.contact_phone || ''} onChange={e => setFormData({ ...formData, manager_phone: e.target.value })} placeholder="010-0000-0000" />
                                </div>
                            </div>
                        </div>

                        {/* Collector 배정 */}
                        <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                            <div style={sectionTitleStyle}><i className="fas fa-server"></i> Collector 배정</div>

                            {/* 현재 배정된 Collector */}
                            {collectors.length > 0 && (
                                <div style={{ marginBottom: '10px' }}>
                                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '4px', fontWeight: 600 }}>
                                        이 사이트에 배정됨 ({collectors.length}개)
                                    </div>
                                    {collectors.map(c => (
                                        <div key={c.id} style={{
                                            display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                                            padding: '5px 10px', marginBottom: '3px',
                                            background: 'white', borderRadius: '6px',
                                            border: '1px solid var(--neutral-200)'
                                        }}>
                                            <span style={{ fontSize: '12px' }}>
                                                <i className="fas fa-server" style={{ marginRight: '6px', color: 'var(--primary-500)' }}></i>
                                                {c.name}
                                                <span style={{ marginLeft: '8px', fontSize: '11px', color: 'var(--neutral-400)' }}>
                                                    장치 {(c as any).device_count ?? 0}개
                                                </span>
                                            </span>
                                            <button
                                                type="button" className="btn btn-outline"
                                                style={{ padding: '1px 6px', fontSize: '11px', lineHeight: '18px' }}
                                                onClick={() => handleUnassignCollector(c.id)}
                                                disabled={saving || ((c as any).device_count ?? 0) > 0}
                                                title={((c as any).device_count ?? 0) > 0 ? '연결된 장치가 있어 해제 불가' : '배정 해제'}
                                            >
                                                <i className="fas fa-minus-circle"></i> 해제
                                            </button>
                                        </div>
                                    ))}
                                </div>
                            )}

                            {/* 미배정 Collector */}
                            {availableCollectors.length > 0 ? (
                                <div>
                                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '4px', fontWeight: 600 }}>
                                        배정 가능 ({availableCollectors.length}개)
                                    </div>
                                    {availableCollectors.map(c => (
                                        <div key={c.id} style={{
                                            display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                                            padding: '5px 10px', marginBottom: '3px',
                                            background: 'white', borderRadius: '6px',
                                            border: '1px dashed var(--neutral-300)'
                                        }}>
                                            <span style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>
                                                <i className="fas fa-server" style={{ marginRight: '6px' }}></i>
                                                {c.name}
                                                <span style={{ marginLeft: '6px', fontSize: '10px', padding: '1px 5px', background: 'var(--neutral-100)', borderRadius: '8px' }}>미배정</span>
                                            </span>
                                            <button
                                                type="button" className="btn btn-primary"
                                                style={{ padding: '1px 6px', fontSize: '11px', lineHeight: '18px' }}
                                                onClick={() => handleAssignCollector(c.id)}
                                                disabled={saving}
                                            >
                                                <i className="fas fa-plus-circle"></i> 배정
                                            </button>
                                        </div>
                                    ))}
                                </div>
                            ) : collectors.length === 0 ? (
                                <div style={{ color: 'var(--neutral-400)', fontSize: '12px', padding: '4px 0' }}>
                                    배정 가능한 Collector가 없습니다. 고객사 설정에서 할당량을 늘려주세요.
                                </div>
                            ) : null}
                        </div>
                    </div>
                </form>
            );
        }

        // ── 상세 보기 모드 ──
        return (
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                {/* 기본 정보 */}
                <div style={sectionStyle}>
                    <div style={sectionTitleStyle}><i className="fas fa-info-circle"></i> 기본 정보</div>
                    <div style={fieldLabel}>사이트명</div>
                    <div style={{ ...fieldValue, fontWeight: 600 }}>{site.name}</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                        <div>
                            <div style={fieldLabel}>코드 / ID</div>
                            <div style={fieldValue}><code style={{ fontSize: '12px' }}>{site.code}</code> / {site.id}</div>
                        </div>
                        <div>
                            <div style={fieldLabel}>사이트 유형</div>
                            <div style={fieldValue}>{(site.site_type || '-').toUpperCase()}</div>
                        </div>
                    </div>
                </div>

                {/* 위치 정보 */}
                <div style={sectionStyle}>
                    <div style={sectionTitleStyle}><i className="fas fa-map-marker-alt"></i> 위치 정보</div>
                    <div style={fieldLabel}>상위 사이트</div>
                    <div style={fieldValue}>{site.parent_site_id ? `ID: ${site.parent_site_id}` : '최상위 사이트'}</div>
                    <div style={fieldLabel}>상세 주소</div>
                    <div style={fieldValue}>{site.address || '-'}</div>
                    <div style={fieldLabel}>타임존</div>
                    <div style={fieldValue}>{site.timezone || 'UTC'}</div>
                </div>

                {/* 담당자 */}
                <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                    <div style={sectionTitleStyle}><i className="fas fa-user-tie"></i> 담당자 정보</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
                        <div>
                            <div style={fieldLabel}>담당자</div>
                            <div style={fieldValue}>{site.manager_name || site.contact_name || '-'}</div>
                        </div>
                        <div>
                            <div style={fieldLabel}>이메일</div>
                            <div style={fieldValue}>{site.manager_email || site.contact_email || '-'}</div>
                        </div>
                        <div>
                            <div style={fieldLabel}>연락처</div>
                            <div style={fieldValue}>{site.manager_phone || site.contact_phone || '-'}</div>
                        </div>
                    </div>
                </div>

                {/* Edge Server 현황 */}
                <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                    <div style={sectionTitleStyle}><i className="fas fa-server"></i> Edge Server 현황</div>
                    {collectors.length === 0 ? (
                        <div style={{ color: 'var(--neutral-400)', fontSize: '12px', padding: '2px 0' }}>
                            이 사이트에 등록된 Edge Server가 없습니다.
                        </div>
                    ) : (
                        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px' }}>
                            <thead>
                                <tr style={{ borderBottom: '1px solid var(--neutral-200)' }}>
                                    <th style={{ textAlign: 'left', padding: '4px 8px', color: 'var(--neutral-500)' }}>서버 명</th>
                                    <th style={{ textAlign: 'center', padding: '4px 8px', color: 'var(--neutral-500)' }}>유형</th>
                                    <th style={{ textAlign: 'center', padding: '4px 8px', color: 'var(--neutral-500)' }}>상태</th>
                                    <th style={{ textAlign: 'center', padding: '4px 8px', color: 'var(--neutral-500)' }}>연결 장치</th>
                                </tr>
                            </thead>
                            <tbody>
                                {collectors.map(c => (
                                    <tr key={c.id} style={{ borderBottom: '1px solid var(--neutral-100)' }}>
                                        <td style={{ padding: '4px 8px' }}>
                                            <i className={`fas fa-${(c as any).server_type === 'gateway' ? 'network-wired' : 'server'}`} style={{ marginRight: '6px', color: 'var(--neutral-400)' }}></i>
                                            {c.name}
                                        </td>
                                        <td style={{ textAlign: 'center', padding: '4px 8px' }}>
                                            <span style={{
                                                display: 'inline-block', padding: '1px 6px', borderRadius: '10px', fontSize: '10px',
                                                background: (c as any).server_type === 'gateway' ? '#ede9fe' : '#dbeafe',
                                                color: (c as any).server_type === 'gateway' ? '#6b21a8' : '#1e40af'
                                            }}>
                                                {(c as any).server_type === 'gateway' ? 'Gateway' : 'Collector'}
                                            </span>
                                        </td>
                                        <td style={{ textAlign: 'center', padding: '4px 8px' }}>
                                            <span style={{
                                                display: 'inline-block', padding: '1px 6px', borderRadius: '10px', fontSize: '10px',
                                                background: c.status === 'online' || c.status === 'active' ? 'var(--success-100, #dcfce7)' : 'var(--neutral-100)',
                                                color: c.status === 'online' || c.status === 'active' ? 'var(--success-700, #15803d)' : 'var(--neutral-500)'
                                            }}>
                                                {c.status === 'online' || c.status === 'active' ? '🟢 온라인' : '🔴 오프라인'}
                                            </span>
                                        </td>
                                        <td style={{ textAlign: 'center', padding: '4px 8px', color: 'var(--neutral-600)' }}>
                                            {(c as any).server_type === 'gateway' ? '-' : `${(c as any).device_count ?? 0}개`}
                                        </td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    )}
                </div>
            </div>
        );
    };

    if (!isOpen) return null;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container site-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{isEditing ? '사이트 정보 수정' : '사이트 상세 정보'}</h2>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body" style={{ padding: '16px 20px' }}>
                    {error && <div className="mgmt-alert mgmt-alert-danger" style={{ marginBottom: '12px', fontSize: '13px' }}>{error}</div>}
                    {renderContent()}
                </div>

                <div className="mgmt-modal-footer">
                    {!isEditing ? (
                        <div className="footer-right" style={{ width: '100%', display: 'flex', justifyContent: 'flex-end', gap: '10px' }}>
                            <button className="btn btn-outline" onClick={onClose}>닫기</button>
                            <button className="btn btn-primary" onClick={handleEditToggle}><i className="fas fa-edit"></i> 수정</button>
                        </div>
                    ) : (
                        <div style={{ width: '100%', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <button className="btn btn-error" onClick={handleDelete} disabled={saving}><i className="fas fa-trash"></i> 삭제</button>
                            <div className="footer-right" style={{ display: 'flex', gap: '10px' }}>
                                <button className="btn btn-outline" onClick={handleEditToggle} disabled={saving}>취소</button>
                                <button type="submit" form="site-detail-form" className="btn btn-primary" disabled={saving}>
                                    <i className="fas fa-save"></i> {saving ? '저장 중...' : '저장 완료'}
                                </button>
                            </div>
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
};
