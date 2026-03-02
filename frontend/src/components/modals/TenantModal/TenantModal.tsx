import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
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
    const { t } = useTranslation(['tenants', 'common']);
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
            setError(t('modal.errRequired', { ns: 'tenants' }));
            return;
        }

        try {
            setSaving(true);

            if (tenant) {
                const isConfirmed = await confirm({
                    title: t('modal.confirmSaveTitle', { ns: 'tenants' }),
                    message: t('modal.confirmSaveMsg', { ns: 'tenants' }),
                    confirmText: t('modal.confirmSaveBtn', { ns: 'tenants' }),
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await TenantApiService.updateTenant(tenant.id, formData);
                if (res.success) {
                    await confirm({
                        title: t('modal.confirmSaveCompleteTitle', { ns: 'tenants' }),
                        message: t('modal.confirmSaveCompleteMsg', { ns: 'tenants' }),
                        confirmText: t('modal.confirmSaveCompleteBtn', { ns: 'tenants' }),
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || t('modal.errUpdateFailed', { ns: 'tenants' }));
                }
            } else {
                const isConfirmed = await confirm({
                    title: t('modal.confirmCreateTitle', { ns: 'tenants' }),
                    message: t('modal.confirmCreateMsg', { ns: 'tenants' }),
                    confirmText: t('modal.confirmCreateBtn', { ns: 'tenants' }),
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await TenantApiService.createTenant(formData);
                if (res.success) {
                    await confirm({
                        title: t('modal.confirmCreateCompleteTitle', { ns: 'tenants' }),
                        message: t('modal.confirmCreateCompleteMsg', { ns: 'tenants' }),
                        confirmText: t('modal.confirmCreateCompleteBtn', { ns: 'tenants' }),
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || t('modal.errCreateFailed', { ns: 'tenants' }));
                }
            }
        } catch (err: any) {
            setError(err.message || t('modal.errServerComm', { ns: 'tenants' }));
        } finally {
            setSaving(false);
        }
    };

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container tenant-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{tenant ? t('modal.editTitle', { ns: 'tenants' }) : t('modal.createTitle', { ns: 'tenants' })}</h2>
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
                                    {/* Basic Info */}
                                    <div style={sectionStyle}>
                                        <div style={sectionTitleStyle}><i className="fas fa-building"></i> {t('modal.sectionBasic', { ns: 'tenants' })}</div>
                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                            <div>
                                                <div style={labelStyle}>{t('labels.companyName', { ns: 'tenants' })} <span style={{ color: 'var(--error-500)' }}>*</span></div>
                                                <input type="text" style={inputStyle} value={formData.company_name} onChange={e => setFormData({ ...formData, company_name: e.target.value })} placeholder="Enter company name" disabled={saving} required />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>{t('labels.companyCode', { ns: 'tenants' })} <span style={{ color: 'var(--error-500)' }}>*</span></div>
                                                <input type="text" style={{ ...inputStyle, ...(tenant ? { background: 'var(--neutral-50)', color: 'var(--neutral-500)' } : {}) }} value={formData.company_code} onChange={e => setFormData({ ...formData, company_code: e.target.value })} placeholder="ABC" disabled={saving || !!tenant} required />
                                                {tenant && <span style={{ fontSize: '10px', color: 'var(--neutral-400)', marginTop: '2px' }}>{t('labels.companyCodeCannotBeChanged', { ns: 'tenants' })}</span>}
                                            </div>
                                            <div>
                                                <div style={labelStyle}>{t('table.domain', { ns: 'tenants' })}</div>
                                                <input type="text" style={inputStyle} value={formData.domain} onChange={e => setFormData({ ...formData, domain: e.target.value })} placeholder="example.com" disabled={saving} />
                                            </div>
                                        </div>
                                    </div>

                                    {/* Contact Info */}
                                    <div style={sectionStyle}>
                                        <div style={sectionTitleStyle}><i className="fas fa-user-tie"></i> {t('modal.sectionContact', { ns: 'tenants' })}</div>
                                        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                            <div>
                                                <div style={labelStyle}>{t('labels.name', { ns: 'tenants' })}</div>
                                                <input type="text" style={inputStyle} value={formData.contact_name} onChange={e => setFormData({ ...formData, contact_name: e.target.value })} placeholder="Contact person name" disabled={saving} />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>{t('labels.email', { ns: 'tenants' })}</div>
                                                <input type="email" style={inputStyle} value={formData.contact_email} onChange={e => setFormData({ ...formData, contact_email: e.target.value })} placeholder="contact@company.com" disabled={saving} />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>{t('labels.contact', { ns: 'tenants' })}</div>
                                                <input type="text" style={inputStyle} value={formData.contact_phone} onChange={e => setFormData({ ...formData, contact_phone: e.target.value })} placeholder="010-0000-0000" disabled={saving} />
                                            </div>
                                        </div>
                                    </div>

                                    {/* Subscription & Usage */}
                                    <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                                        <div style={sectionTitleStyle}><i className="fas fa-credit-card"></i> {t('modal.sectionSubscription', { ns: 'tenants' })}</div>
                                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
                                            <div>
                                                <div style={labelStyle}>{t('table.subscriptionPlan', { ns: 'tenants' })}</div>
                                                <select style={inputStyle} value={formData.subscription_plan} onChange={e => setFormData({ ...formData, subscription_plan: e.target.value as any })} disabled={saving}>
                                                    <option value="starter">{t('labels.starter', { ns: 'tenants' })}</option>
                                                    <option value="professional">{t('labels.professional', { ns: 'tenants' })}</option>
                                                    <option value="enterprise">{t('labels.enterprise', { ns: 'tenants' })}</option>
                                                </select>
                                            </div>
                                            <div>
                                                <div style={labelStyle}>{t('labels.subscriptionStatus', { ns: 'tenants' })}</div>
                                                <select style={inputStyle} value={formData.subscription_status} onChange={e => setFormData({ ...formData, subscription_status: e.target.value as any })} disabled={saving}>
                                                    <option value="active">{t('filter.active', { ns: 'tenants' })}</option>
                                                    <option value="trial">{t('stats.trial', { ns: 'tenants' })}</option>
                                                    <option value="suspended">{t('labels.suspended', { ns: 'tenants' })}</option>
                                                    <option value="cancelled">{t('labels.cancelled', { ns: 'tenants' })}</option>
                                                </select>
                                            </div>
                                            <div>
                                                <div style={labelStyle}>{t('labels.accountStatus', { ns: 'tenants' })}</div>
                                                <div style={{ padding: '6px 10px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-200)', height: '32px', display: 'flex', alignItems: 'center' }}>
                                                    <label style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '13px', cursor: 'pointer', margin: 0 }}>
                                                        <input type="checkbox" checked={formData.is_active} onChange={e => setFormData({ ...formData, is_active: e.target.checked })} disabled={saving} />
                                                        {t('labels.accountActive', { ns: 'tenants' })}
                                                    </label>
                                                </div>
                                            </div>
                                        </div>
                                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginTop: '10px' }}>
                                            <div>
                                                <div style={labelStyle}>{t('labels.edgeServerLimit', { ns: 'tenants' })} <span style={{ fontSize: '10px', color: 'var(--neutral-400)', fontWeight: 'normal' }}>{t('labels.edgeServerLimitNote', { ns: 'tenants' })}</span></div>
                                                <input type="number" style={inputStyle} value={formData.max_edge_servers} onChange={e => setFormData({ ...formData, max_edge_servers: parseInt(e.target.value) })} disabled={saving} min="1" />
                                            </div>
                                            <div>
                                                <div style={labelStyle}>{t('labels.maxDataPoints', { ns: 'tenants' })}</div>
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
