import React, { useState, useEffect } from 'react';
import { Tenant } from '../../../types/common';
import { TenantApiService } from '../../../api/services/tenantApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import './TenantModal.css';

interface TenantModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    tenant: Tenant | null;
}

export const TenantModal: React.FC<TenantModalProps> = ({
    isOpen,
    onClose,
    onSave,
    tenant
}) => {
    const { confirm } = useConfirmContext();
    const [formData, setFormData] = useState<Partial<Tenant>>({
        company_name: '',
        company_code: '',
        domain: '',
        contact_name: '',
        contact_email: '',
        contact_phone: '',
        subscription_plan: 'starter',
        subscription_status: 'trial',
        max_edge_servers: 1,
        max_data_points: 100,
        max_users: 5,
        is_active: true
    });
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        if (tenant) {
            setFormData({
                company_name: tenant.company_name,
                company_code: tenant.company_code,
                domain: tenant.domain || '',
                contact_name: tenant.contact_name || '',
                contact_email: tenant.contact_email || '',
                contact_phone: tenant.contact_phone || '',
                subscription_plan: tenant.subscription_plan || 'starter',
                subscription_status: tenant.subscription_status || 'active',
                max_edge_servers: tenant.max_edge_servers || 1,
                max_data_points: tenant.max_data_points || 100,
                max_users: tenant.max_users || 5,
                is_active: tenant.is_active !== false
            });
        } else {
            setFormData({
                company_name: '',
                company_code: '',
                domain: '',
                contact_name: '',
                contact_email: '',
                contact_phone: '',
                subscription_plan: 'starter',
                subscription_status: 'trial',
                max_edge_servers: 1,
                max_data_points: 100,
                max_users: 5,
                is_active: true
            });
        }
        setError(null);
    }, [tenant, isOpen]);

    if (!isOpen) return null;

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setError(null);

        if (!formData.company_name || !formData.company_code) {
            setError('회사명과 회사 코드는 필수 항목입니다.');
            return;
        }

        try {
            setSaving(true);

            if (tenant) {
                const isConfirmed = await confirm({
                    title: '변경 사항 저장',
                    message: '입력하신 변경 내용을 저장하시겠습니까?',
                    confirmText: '저장',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await TenantApiService.updateTenant(tenant.id, formData);
                if (res.success) {
                    await confirm({
                        title: '저장 완료',
                        message: '고객사 정보가 성공적으로 수정되었습니다.',
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
                    title: '고객사 등록',
                    message: '새로운 고객사를 등록하시겠습니까?',
                    confirmText: '등록',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await TenantApiService.createTenant(formData);
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
        <div className="modal-overlay">
            <div className="modal-container tenant-modal">
                <div className="modal-header">
                    <div className="modal-title">
                        <h2>{tenant ? '고객사 정보 수정' : '새 고객사 등록'}</h2>
                    </div>
                    <button className="close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="modal-body">
                    {error && <div className="alert alert-error">{error}</div>}

                    <form id="tenant-form" onSubmit={handleSubmit}>
                        <div className="modal-form-grid">
                            <div className="modal-form-section">
                                <h3><i className="fas fa-building"></i> 기본 정보</h3>
                                <div className="modal-form-group">
                                    <label className="required">회사명</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.company_name}
                                        onChange={e => setFormData({ ...formData, company_name: e.target.value })}
                                        placeholder="회사명을 입력하세요"
                                        disabled={saving}
                                        required
                                    />
                                </div>
                                <div className="modal-form-group">
                                    <label className="required">회사 코드</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.company_code}
                                        onChange={e => setFormData({ ...formData, company_code: e.target.value })}
                                        placeholder="회사 식별 코드를 입력하세요 (예: ABC)"
                                        disabled={saving || !!tenant}
                                        required
                                    />
                                    {tenant && <span className="input-hint">회사 코드는 변경할 수 없습니다.</span>}
                                </div>
                                <div className="modal-form-group">
                                    <label>도메인</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.domain}
                                        onChange={e => setFormData({ ...formData, domain: e.target.value })}
                                        placeholder="example.com"
                                        disabled={saving}
                                    />
                                </div>
                            </div>

                            <div className="modal-form-section">
                                <h3><i className="fas fa-user-tie"></i> 담당자 정보</h3>
                                <div className="modal-form-group">
                                    <label>이름</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.contact_name}
                                        onChange={e => setFormData({ ...formData, contact_name: e.target.value })}
                                        placeholder="담당자 이름을 입력하세요"
                                        disabled={saving}
                                    />
                                </div>
                                <div className="modal-form-group">
                                    <label>이메일</label>
                                    <input
                                        type="email"
                                        className="form-control"
                                        value={formData.contact_email}
                                        onChange={e => setFormData({ ...formData, contact_email: e.target.value })}
                                        placeholder="contact@company.com"
                                        disabled={saving}
                                    />
                                </div>
                                <div className="modal-form-group">
                                    <label>연락처</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.contact_phone}
                                        onChange={e => setFormData({ ...formData, contact_phone: e.target.value })}
                                        placeholder="010-0000-0000"
                                        disabled={saving}
                                    />
                                </div>
                            </div>

                            <div className="modal-form-section span-full">
                                <h3><i className="fas fa-credit-card"></i> 구독 및 사용량</h3>
                                <div className="modal-form-row">
                                    <div className="modal-form-group">
                                        <label>구독 플랜</label>
                                        <select
                                            className="form-control"
                                            value={formData.subscription_plan}
                                            onChange={e => setFormData({ ...formData, subscription_plan: e.target.value as any })}
                                            disabled={saving}
                                        >
                                            <option value="starter">스타터 (Starter)</option>
                                            <option value="professional">프로페셔널 (Professional)</option>
                                            <option value="enterprise">엔터프라이즈 (Enterprise)</option>
                                        </select>
                                    </div>
                                    <div className="modal-form-group">
                                        <label>구독 상태</label>
                                        <select
                                            className="form-control"
                                            value={formData.subscription_status}
                                            onChange={e => setFormData({ ...formData, subscription_status: e.target.value as any })}
                                            disabled={saving}
                                        >
                                            <option value="active">Active</option>
                                            <option value="trial">Trial</option>
                                            <option value="suspended">Suspended</option>
                                            <option value="cancelled">Cancelled</option>
                                        </select>
                                    </div>
                                    <div className="modal-form-group">
                                        <label>계정 상태</label>
                                        <div className="checkbox-group" style={{ height: '38px', display: 'flex', alignItems: 'center' }}>
                                            <label className="checkbox-label" style={{ margin: 0 }}>
                                                <input
                                                    type="checkbox"
                                                    checked={formData.is_active}
                                                    onChange={e => setFormData({ ...formData, is_active: e.target.checked })}
                                                    disabled={saving}
                                                />
                                                계정 활성화
                                            </label>
                                        </div>
                                    </div>
                                </div>

                                <div className="modal-form-row" style={{ marginTop: '12px' }}>
                                    <div className="modal-form-group">
                                        <label>최대 에지 서버</label>
                                        <input
                                            type="number"
                                            className="form-control"
                                            value={formData.max_edge_servers}
                                            onChange={e => setFormData({ ...formData, max_edge_servers: parseInt(e.target.value) })}
                                            disabled={saving}
                                            min="1"
                                        />
                                    </div>
                                    <div className="modal-form-group">
                                        <label>최대 데이터 포인트</label>
                                        <input
                                            type="number"
                                            className="form-control"
                                            value={formData.max_data_points}
                                            onChange={e => setFormData({ ...formData, max_data_points: parseInt(e.target.value) })}
                                            disabled={saving}
                                            min="100"
                                        />
                                    </div>
                                </div>
                            </div>
                        </div>
                    </form>
                </div>

                <div className="modal-footer">
                    <button type="button" className="btn btn-outline" onClick={onClose} disabled={saving}>
                        취소
                    </button>
                    <button type="submit" form="tenant-form" className="btn btn-primary" disabled={saving}>
                        <i className="fas fa-check"></i> {saving ? '저장 중...' : (tenant ? '수정 완료' : '등록 완료')}
                    </button>
                </div>
            </div>
        </div>
    );
};
