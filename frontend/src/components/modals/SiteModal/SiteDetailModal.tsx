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
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        if (isOpen && siteId) {
            loadSite(siteId);
            loadPotentialParents(siteId);
            loadCollectors(siteId);
            setIsEditing(false);
        }
    }, [isOpen, siteId]);

    const loadCollectors = async (id: number) => {
        try {
            const res = await CollectorApiService.getCollectorsBySite(id);
            if (res.success && res.data) setCollectors(res.data);
        } catch (e) {
            setCollectors([]);
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
        setIsEditing(!isEditing);
        setError(null);
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

    const renderContent = () => {
        if (loading) return <div className="loading-spinner"><i className="fas fa-spinner fa-spin"></i> 로딩 중...</div>;
        if (!site) return <div className="mgmt-alert mgmt-alert-danger">사이트 정보를 불러오지 못했습니다.</div>;

        if (isEditing) {
            return (
                <form id="site-detail-form" onSubmit={handleSubmit}>
                    <div className="mgmt-modal-form-grid">
                        <div className="mgmt-modal-form-section">
                            <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                            <div className="mgmt-modal-form-group">
                                <label className="required">사이트명</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.name || ''}
                                    onChange={e => setFormData({ ...formData, name: e.target.value })}
                                    required
                                />
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>사이트 코드</label>
                                <input type="text" className="form-control" value={site.code} disabled />
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>사이트 유형</label>
                                <select
                                    className="form-control"
                                    value={formData.site_type || ''}
                                    onChange={e => setFormData({ ...formData, site_type: e.target.value })}
                                >
                                    <option value="company">본사 (Business Unit)</option>
                                    <option value="office">오피스/지사</option>
                                    <option value="factory">공장/플랜트</option>
                                    <option value="building">빌딩/건물</option>
                                    <option value="floor">층 (Floor)</option>
                                    <option value="room">실 (Room)</option>
                                </select>
                            </div>
                        </div>
                        <div className="mgmt-modal-form-section">
                            <h3><i className="fas fa-map-marker-alt"></i> 위치 정보</h3>
                            <div className="mgmt-modal-form-group">
                                <label>상위 사이트</label>
                                <select
                                    className="form-control"
                                    value={formData.parent_site_id || ''}
                                    onChange={e => setFormData({ ...formData, parent_site_id: e.target.value ? parseInt(e.target.value) : undefined })}
                                >
                                    <option value="">없음 (최상위)</option>
                                    {allSites.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}
                                </select>
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>주소</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.address || ''}
                                    onChange={e => setFormData({ ...formData, address: e.target.value })}
                                />
                            </div>
                        </div>
                        <div className="mgmt-modal-form-section">
                            <h3><i className="fas fa-user-tie"></i> 담당자 정보</h3>
                            <div className="mgmt-modal-form-group">
                                <label>담당자 성함</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.manager_name || formData.contact_name || ''}
                                    onChange={e => setFormData({ ...formData, manager_name: e.target.value })}
                                    placeholder="담당자 성함을 입력하세요"
                                />
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>이메일</label>
                                <input
                                    type="email"
                                    className="form-control"
                                    value={formData.manager_email || formData.contact_email || ''}
                                    onChange={e => setFormData({ ...formData, manager_email: e.target.value })}
                                    placeholder="example@pulseone.com"
                                />
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>연락처</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.manager_phone || formData.contact_phone || ''}
                                    onChange={e => setFormData({ ...formData, manager_phone: e.target.value })}
                                    placeholder="010-0000-0000"
                                />
                            </div>
                        </div>
                    </div>
                </form>
            );
        }

        return (
            <div className="mgmt-modal-form-grid">
                <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                    <div className="detail-item">
                        <div className="detail-label">사이트명</div>
                        <div className="detail-value highlight">{site.name}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">사이트 코드 / ID</div>
                        <div className="detail-value"><code>{site.code}</code> / ID: {site.id}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">사이트 유형</div>
                        <div className="detail-value">{site.site_type.toUpperCase()}</div>
                    </div>
                </div>
                <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-map-marker-alt"></i> 위치 정보</h3>
                    <div className="detail-item">
                        <div className="detail-label">상위 사이트</div>
                        <div className="detail-value">{site.parent_site_id ? `ID: ${site.parent_site_id}` : '최상위 사이트'}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">상세 주소</div>
                        <div className="detail-value">{site.address || '-'}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">타임존</div>
                        <div className="detail-value">{site.timezone}</div>
                    </div>
                </div>
                <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-user-tie"></i> 담당자 정보</h3>
                    <div className="detail-item">
                        <div className="detail-label">담당자</div>
                        <div className="detail-value">{site.manager_name || site.contact_name || '-'}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">이메일</div>
                        <div className="detail-value">{site.manager_email || site.contact_email || '-'}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">연락처</div>
                        <div className="detail-value">{site.manager_phone || site.contact_phone || '-'}</div>
                    </div>
                </div>
                <div className="mgmt-modal-form-section mgmt-span-full" style={{ marginTop: '16px' }}>
                    <h3><i className="fas fa-server"></i> Collector 현황</h3>
                    {collectors.length === 0 ? (
                        <div style={{ color: 'var(--neutral-400)', fontSize: '13px', padding: '8px 0' }}>
                            이 사이트에 등록된 Collector가 없습니다.
                        </div>
                    ) : (
                        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '13px' }}>
                            <thead>
                                <tr style={{ borderBottom: '1px solid var(--neutral-200)' }}>
                                    <th style={{ textAlign: 'left', padding: '6px 8px', color: 'var(--neutral-500)' }}>Collector 명</th>
                                    <th style={{ textAlign: 'center', padding: '6px 8px', color: 'var(--neutral-500)' }}>상태</th>
                                    <th style={{ textAlign: 'center', padding: '6px 8px', color: 'var(--neutral-500)' }}>연결 장치</th>
                                </tr>
                            </thead>
                            <tbody>
                                {collectors.map(c => (
                                    <tr key={c.id} style={{ borderBottom: '1px solid var(--neutral-100)' }}>
                                        <td style={{ padding: '6px 8px' }}>
                                            <i className="fas fa-server" style={{ marginRight: '6px', color: 'var(--neutral-400)' }}></i>
                                            {c.name}
                                        </td>
                                        <td style={{ textAlign: 'center', padding: '6px 8px' }}>
                                            <span style={{
                                                display: 'inline-block', padding: '2px 8px', borderRadius: '12px', fontSize: '11px',
                                                background: c.status === 'online' ? 'var(--success-100)' : 'var(--neutral-100)',
                                                color: c.status === 'online' ? 'var(--success-700)' : 'var(--neutral-500)'
                                            }}>
                                                {c.status === 'online' ? '🟢 온라인' : '🔴 오프라인'}
                                            </span>
                                        </td>
                                        <td style={{ textAlign: 'center', padding: '6px 8px', color: 'var(--neutral-600)' }}>
                                            {(c as any).device_count ?? 0}개
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

                <div className="mgmt-modal-body">
                    {error && <div className="mgmt-alert mgmt-alert-danger">{error}</div>}
                    {renderContent()}
                </div>

                <div className="mgmt-modal-footer">
                    {!isEditing ? (
                        <div className="footer-right" style={{ width: '100%', display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                            <button className="btn btn-outline" onClick={onClose}>닫기</button>
                            <button className="btn btn-primary" onClick={handleEditToggle}><i className="fas fa-edit"></i> 수정</button>
                        </div>
                    ) : (
                        <div style={{ width: '100%', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <button className="btn btn-error" onClick={handleDelete} disabled={saving}><i className="fas fa-trash"></i> 삭제</button>
                            <div className="footer-right" style={{ display: 'flex', gap: '12px' }}>
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
