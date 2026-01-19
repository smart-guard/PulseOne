import React, { useState, useEffect, useCallback } from 'react';
import { Site } from '../../../types/common';
import { SiteApiService } from '../../../api/services/siteApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import './SiteModal.css';

interface SiteModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    site: Site | null;
}

export const SiteModal: React.FC<SiteModalProps> = ({
    isOpen,
    onClose,
    onSave,
    site
}) => {
    const { confirm } = useConfirmContext();
    const [formData, setFormData] = useState<Partial<Site>>({
        name: '',
        code: '',
        site_type: 'office',
        description: '',
        location: '',
        address: '',
        timezone: 'Asia/Seoul',
        parent_site_id: undefined,
        is_active: true
    });
    const [sites, setSites] = useState<Site[]>([]);
    const [saving, setSaving] = useState(false);
    const [loadingSites, setLoadingSites] = useState(false);
    const [error, setError] = useState<string | null>(null);

    const loadPotentialParents = useCallback(async () => {
        try {
            setLoadingSites(true);
            const res = await SiteApiService.getSites();
            if (res.success && res.data) {
                // Exclude self from potential parents if editing
                const filtered = site
                    ? res.data.items.filter(s => s.id !== site.id)
                    : res.data.items;
                setSites(filtered);
            }
        } catch (err) {
            console.error('Failed to load sites:', err);
        } finally {
            setLoadingSites(false);
        }
    }, [site]);

    useEffect(() => {
        if (isOpen) {
            loadPotentialParents();
            if (site) {
                setFormData({
                    name: site.name,
                    code: site.code,
                    site_type: site.site_type,
                    description: site.description || '',
                    location: site.location || '',
                    address: site.address || '',
                    timezone: site.timezone,
                    parent_site_id: site.parent_site_id,
                    is_active: site.is_active
                });
            } else {
                setFormData({
                    name: '',
                    code: '',
                    site_type: 'office',
                    description: '',
                    location: '',
                    address: '',
                    timezone: 'Asia/Seoul',
                    parent_site_id: undefined,
                    is_active: true
                });
            }
            setError(null);
        }
    }, [site, isOpen, loadPotentialParents]);

    if (!isOpen) return null;

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!formData.name || !formData.code) {
            setError('사이트명과 코드는 필수 항목입니다.');
            return;
        }

        try {
            setSaving(true);
            setError(null);

            if (site) {
                const isConfirmed = await confirm({
                    title: '변경 사항 저장',
                    message: '입력하신 변경 내용을 저장하시겠습니까?',
                    confirmText: '저장',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await SiteApiService.updateSite(site.id, formData);
                if (res.success) {
                    await confirm({
                        title: '저장 완료',
                        message: '사이트 정보가 성공적으로 수정되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || '업데이트에 실패했습니다.');
                }
            } else {
                const isConfirmed = await confirm({
                    title: '사이트 등록',
                    message: '새로운 사이트를 등록하시겠습니까?',
                    confirmText: '등록',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await SiteApiService.createSite(formData);
                if (res.success) {
                    await confirm({
                        title: '등록 완료',
                        message: '성공적으로 등록되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || '등록에 실패했습니다.');
                }
            }
        } catch (err: any) {
            setError(err.message || '서버 통신 중 오류가 발생했습니다.');
        } finally {
            setSaving(false);
        }
    };

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container site-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{site ? '사이트 정보 수정' : '새 사이트 등록'}</h2>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body">
                    {error && <div className="mgmt-alert mgmt-alert-danger">{error}</div>}

                    <form id="site-form" onSubmit={handleSubmit}>
                        <div className="mgmt-modal-form-grid">
                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">사이트명</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.name}
                                        onChange={e => setFormData({ ...formData, name: e.target.value })}
                                        placeholder="사이트 이름을 입력하세요 (예: 서울 본사)"
                                        disabled={saving}
                                        required
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">사이트 코드</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.code}
                                        onChange={e => setFormData({ ...formData, code: e.target.value })}
                                        placeholder="식별 코드를 입력하세요 (예: HQ-001)"
                                        disabled={saving}
                                        required
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>사이트 유형</label>
                                    <select
                                        className="form-control"
                                        value={formData.site_type}
                                        onChange={e => setFormData({ ...formData, site_type: e.target.value })}
                                        disabled={saving}
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
                                <h3><i className="fas fa-sitemap"></i> 계층 및 위치</h3>
                                <div className="mgmt-modal-form-group">
                                    <label>상위 사이트</label>
                                    <select
                                        className="form-control"
                                        value={formData.parent_site_id || ''}
                                        onChange={e => setFormData({ ...formData, parent_site_id: e.target.value ? parseInt(e.target.value) : undefined })}
                                        disabled={saving || loadingSites}
                                    >
                                        <option value="">없음 (최상위 사이트)</option>
                                        {sites.map(s => (
                                            <option key={s.id} value={s.id}>{s.name} ({s.code})</option>
                                        ))}
                                    </select>
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>주소</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.address}
                                        onChange={e => setFormData({ ...formData, address: e.target.value })}
                                        placeholder="상세 주소를 입력하세요"
                                        disabled={saving}
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>타임존</label>
                                    <select
                                        className="form-control"
                                        value={formData.timezone}
                                        onChange={e => setFormData({ ...formData, timezone: e.target.value })}
                                        disabled={saving}
                                    >
                                        <option value="Asia/Seoul">Asia/Seoul (KST)</option>
                                        <option value="UTC">UTC</option>
                                        <option value="America/New_York">America/New_York (EST/EDT)</option>
                                        <option value="Europe/London">Europe/London (GMT/BST)</option>
                                    </select>
                                </div>
                                <div className="checkbox-group">
                                    <label className="checkbox-label">
                                        <input
                                            type="checkbox"
                                            checked={formData.is_active}
                                            onChange={e => setFormData({ ...formData, is_active: e.target.checked })}
                                            disabled={saving}
                                        />
                                        활성화 상태
                                    </label>
                                </div>
                            </div>
                        </div>
                    </form>
                </div>

                <div className="mgmt-modal-footer">
                    <button type="button" className="btn btn-outline" onClick={onClose} disabled={saving}>
                        취소
                    </button>
                    <button type="submit" form="site-form" className="btn btn-primary" disabled={saving}>
                        <i className="fas fa-check"></i> {saving ? '저장 중...' : (site ? '수정 완료' : '등록 완료')}
                    </button>
                </div>
            </div>
        </div>
    );
};
