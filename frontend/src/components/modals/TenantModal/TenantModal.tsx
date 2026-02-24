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
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container tenant-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{tenant ? '고객사 정보 수정' : '새 고객사 등록'}</h2>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body">
                    {error && <div className="mgmt-alert mgmt-alert-danger">{error}</div>}

                    <form id="tenant-form" onSubmit={handleSubmit}>
                        {(() => {
                            const sectionStyle: React.CSSProperties = { border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '12px 14px' };
                            const sectionTitleStyle: React.CSSProperties = { fontSize: '13px', fontWeight: 700, color: 'var(--neutral-700)', marginBottom: '10px', display: 'flex', alignItems: 'center', gap: '6px' };
                            const labelStyle: React.CSSProperties = { fontSize: '11px', fontWeight: 600, color: 'var(--neutral-400)', textTransform: 'uppercase', letterSpacing: '0.03em', marginBottom: '2px' };
                            const inputStyle: React.CSSProperties = { width: '100%', padding: '6px 10px', border: '1px solid var(--neutral-200)', borderRadius: '6px', fontSize: '13px', outline: 'none' };
                            return (
                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                                    {/* 기본 정보 */}
                                    <div style={sectionStyle}>
                                        <div style={sectionTitleStyle}><i className="fas fa-building"></i> 기본 정보</div>
                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                            <div>
                                                <div style={labelStyle}>회사명 <span style={{ color: 'var(--error-500)' }}>*</span></div>
                                                <input type="text" style={inputStyle} value={formData.company_name} onChange={e => setFormData({ ...formData, company_name: e.target.value })} placeholder="회사명을 입력하세요" disabled={saving} required />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>회사 코드 <span style={{ color: 'var(--error-500)' }}>*</span></div>
                                                <input type="text" style={{ ...inputStyle, ...(tenant ? { background: 'var(--neutral-50)', color: 'var(--neutral-500)' } : {}) }} value={formData.company_code} onChange={e => setFormData({ ...formData, company_code: e.target.value })} placeholder="ABC" disabled={saving || !!tenant} required />
                                                {tenant && <span style={{ fontSize: '10px', color: 'var(--neutral-400)', marginTop: '2px' }}>회사 코드는 변경할 수 없습니다.</span>}
                                            </div>
                                            <div>
                                                <div style={labelStyle}>도메인</div>
                                                <input type="text" style={inputStyle} value={formData.domain} onChange={e => setFormData({ ...formData, domain: e.target.value })} placeholder="example.com" disabled={saving} />
                                            </div>
                                        </div>
                                    </div>

                                    {/* 담당자 정보 */}
                                    <div style={sectionStyle}>
                                        <div style={sectionTitleStyle}><i className="fas fa-user-tie"></i> 담당자 정보</div>
                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                            <div>
                                                <div style={labelStyle}>이름</div>
                                                <input type="text" style={inputStyle} value={formData.contact_name} onChange={e => setFormData({ ...formData, contact_name: e.target.value })} placeholder="담당자 이름" disabled={saving} />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>이메일</div>
                                                <input type="email" style={inputStyle} value={formData.contact_email} onChange={e => setFormData({ ...formData, contact_email: e.target.value })} placeholder="contact@company.com" disabled={saving} />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>연락처</div>
                                                <input type="text" style={inputStyle} value={formData.contact_phone} onChange={e => setFormData({ ...formData, contact_phone: e.target.value })} placeholder="010-0000-0000" disabled={saving} />
                                            </div>
                                        </div>
                                    </div>

                                    {/* 구독 및 사용량 */}
                                    <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                                        <div style={sectionTitleStyle}><i className="fas fa-credit-card"></i> 구독 및 사용량</div>
                                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
                                            <div>
                                                <div style={labelStyle}>구독 플랜</div>
                                                <select style={inputStyle} value={formData.subscription_plan} onChange={e => setFormData({ ...formData, subscription_plan: e.target.value as any })} disabled={saving}>
                                                    <option value="starter">스타터 (Starter)</option>
                                                    <option value="professional">프로페셔널 (Professional)</option>
                                                    <option value="enterprise">엔터프라이즈 (Enterprise)</option>
                                                </select>
                                            </div>
                                            <div>
                                                <div style={labelStyle}>구독 상태</div>
                                                <select style={inputStyle} value={formData.subscription_status} onChange={e => setFormData({ ...formData, subscription_status: e.target.value as any })} disabled={saving}>
                                                    <option value="active">활성 (Active)</option>
                                                    <option value="trial">트라이얼 (Trial)</option>
                                                    <option value="suspended">정지 (Suspended)</option>
                                                    <option value="cancelled">취소 (Cancelled)</option>
                                                </select>
                                            </div>
                                            <div>
                                                <div style={labelStyle}>계정 상태</div>
                                                <div style={{ padding: '6px 10px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-200)', height: '32px', display: 'flex', alignItems: 'center' }}>
                                                    <label style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '13px', cursor: 'pointer', margin: 0 }}>
                                                        <input type="checkbox" checked={formData.is_active} onChange={e => setFormData({ ...formData, is_active: e.target.checked })} disabled={saving} />
                                                        계정 활성화
                                                    </label>
                                                </div>
                                            </div>
                                        </div>
                                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginTop: '10px' }}>
                                            <div>
                                                <div style={labelStyle}>Edge Server 할당 한도 <span style={{ fontSize: '10px', color: 'var(--neutral-400)', fontWeight: 'normal' }}>(사이트 등록 시 1개씩 사용)</span></div>
                                                <input type="number" style={inputStyle} value={formData.max_edge_servers} onChange={e => setFormData({ ...formData, max_edge_servers: parseInt(e.target.value) })} disabled={saving} min="1" />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>최대 데이터 포인트</div>
                                                <input type="number" style={inputStyle} value={formData.max_data_points} onChange={e => setFormData({ ...formData, max_data_points: parseInt(e.target.value) })} disabled={saving} min="100" />
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            );
                        })()}
                    </form>
                </div>

                <div className="mgmt-modal-footer">
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
