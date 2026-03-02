import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { Tenant } from '../../../types/common';
import { TenantApiService } from '../../../api/services/tenantApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import './TenantModal.css';

interface TenantDetailModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    tenantId: number | null;
}

export const TenantDetailModal: React.FC<TenantDetailModalProps> = ({
    isOpen,
    onClose,
    onSave,
    tenantId
}) => {
    const { confirm } = useConfirmContext();
    const [tenant, setTenant] = useState<Tenant | null>(null);
    const { t } = useTranslation(['tenants', 'common']);
    const [isEditing, setIsEditing] = useState(false);
    const [formData, setFormData] = useState<Partial<Tenant>>({});
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        if (isOpen && tenantId) {
            loadTenant(tenantId);
            setIsEditing(false);
        }
    }, [isOpen, tenantId]);

    const loadTenant = async (id: number) => {
        try {
            setLoading(true);
            const res = await TenantApiService.getTenant(id);
            if (res.success && res.data) {
                setTenant(res.data);
                setFormData({
                    ...res.data,
                    contact_name: res.data.contact_name || '',
                    contact_email: res.data.contact_email || '',
                    contact_phone: res.data.contact_phone || '',
                    max_edge_servers: res.data.max_edge_servers || 1,
                    max_data_points: res.data.max_data_points || 100
                });
                setError(null);
            }
        } catch (err: any) {
            setError('Error loading tenant information.');
        } finally {
            setLoading(false);
        }
    };

    const handleEditToggle = () => {
        setIsEditing(!isEditing);
        setError(null);
    };

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!tenantId) return;

        const isConfirmed = await confirm({
            title: 'Save Changes',
            message: 'Save the entered changes?',
            confirmText: 'Save',
            confirmButtonType: 'primary'
        });

        if (!isConfirmed) return;

        try {
            setSaving(true);
            setError(null);
            const res = await TenantApiService.updateTenant(tenantId, formData);
            if (res.success) {
                await confirm({
                    title: 'Save Complete',
                    message: 'Tenant information updated successfully.',
                    confirmText: 'OK',
                    showCancelButton: false,
                    confirmButtonType: 'primary'
                });
                await loadTenant(tenantId);
                onSave();
                onClose();
            } else {
                setError(res.message || 'Update failed.');
            }
        } catch (err: any) {
            setError(err.message || 'Server communication error.');
        } finally {
            setSaving(false);
        }
    };

    const handleDelete = async () => {
        if (!tenant) return;

        const confirmed = await confirm({
            title: 'Confirm Tenant Delete',
            message: `Delete tenant '${tenant.company_name}'?`,
            confirmText: 'Delete',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setSaving(true);
                const res = await TenantApiService.deleteTenant(tenant.id);
                if (res.success) {
                    await confirm({
                        title: 'Delete Complete',
                        message: 'Tenant deleted successfully.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || 'Delete failed.');
                }
            } catch (err: any) {
                setError(err.message || 'Error during delete.');
            } finally {
                setSaving(false);
            }
        }
    };

    const sectionStyle: React.CSSProperties = { border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '12px 14px' };
    const sectionTitleStyle: React.CSSProperties = { fontSize: '13px', fontWeight: 700, color: 'var(--neutral-700)', marginBottom: '10px', display: 'flex', alignItems: 'center', gap: '6px' };
    const fieldLabel: React.CSSProperties = { fontSize: '11px', fontWeight: 600, color: 'var(--neutral-400)', textTransform: 'uppercase', letterSpacing: '0.03em', marginBottom: '2px' };
    const fieldValue: React.CSSProperties = { fontSize: '13px', fontWeight: 500, color: 'var(--neutral-800)' };
    const inputStyle: React.CSSProperties = { width: '100%', padding: '6px 10px', border: '1px solid var(--neutral-200)', borderRadius: '6px', fontSize: '13px', outline: 'none' };

    const renderContent = () => {
        if (loading) return <div style={{ textAlign: 'center', padding: '40px 0', color: 'var(--neutral-400)', fontSize: '13px' }}><i className="fas fa-spinner fa-spin" style={{ marginRight: '6px' }}></i>{t('loading', {ns: 'common'})}</div>;
        if (!tenant) return <div className="mgmt-alert mgmt-alert-danger">{t('labels.failedToLoadTenantInformation', {ns: 'tenants'})}</div>;

        if (isEditing) {
            return (
                <form id="tenant-detail-form" onSubmit={handleSubmit}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                        {/* Basic Info */}
                        <div style={sectionStyle}>
                            <div style={sectionTitleStyle}><i className="fas fa-building"></i> Basic Info</div>
                            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                <div>
                                    <div style={fieldLabel}>Company Name <span style={{ color: 'var(--error-500)' }}>*</span></div>
                                    <input type="text" style={inputStyle} value={formData.company_name} onChange={e => setFormData({ ...formData, company_name: e.target.value })} required />
                                </div>
                                <div>
                                    <div style={fieldLabel}>{t('table.companyCode', {ns: 'tenants'})}</div>
                                    <input type="text" style={{ ...inputStyle, background: 'var(--neutral-50)', color: 'var(--neutral-500)' }} value={formData.company_code} disabled />
                                </div>
                                <div>
                                    <div style={fieldLabel}>{t('table.domain', {ns: 'tenants'})}</div>
                                    <input type="text" style={inputStyle} value={formData.domain || ''} onChange={e => setFormData({ ...formData, domain: e.target.value })} />
                                </div>
                            </div>
                        </div>

                        {/* Manager Info */}
                        <div style={sectionStyle}>
                            <div style={sectionTitleStyle}><i className="fas fa-user-tie"></i> Manager Info</div>
                            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                <div>
                                    <div style={fieldLabel}>{t('labels.name', {ns: 'tenants'})}</div>
                                    <input type="text" style={inputStyle} value={formData.contact_name || ''} onChange={e => setFormData({ ...formData, contact_name: e.target.value })} />
                                </div>
                                <div>
                                    <div style={fieldLabel}>{t('labels.email', {ns: 'tenants'})}</div>
                                    <input type="email" style={inputStyle} value={formData.contact_email || ''} onChange={e => setFormData({ ...formData, contact_email: e.target.value })} />
                                </div>
                                <div>
                                    <div style={fieldLabel}>{t('labels.contact', {ns: 'tenants'})}</div>
                                    <input type="text" style={inputStyle} value={formData.contact_phone || ''} onChange={e => setFormData({ ...formData, contact_phone: e.target.value })} />
                                </div>
                            </div>
                        </div>

                        {/* Settings & Status */}
                        <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                            <div style={sectionTitleStyle}><i className="fas fa-cog"></i> Settings & Status</div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
                                <div>
                                    <div style={fieldLabel}>{t('table.subscriptionPlan', {ns: 'tenants'})}</div>
                                    <select style={inputStyle} value={formData.subscription_plan} onChange={e => setFormData({ ...formData, subscription_plan: e.target.value as any })}>
                                        <option value="starter">{t('labels.starter', {ns: 'tenants'})}</option>
                                        <option value="professional">{t('labels.professional', {ns: 'tenants'})}</option>
                                        <option value="enterprise">{t('labels.enterprise', {ns: 'tenants'})}</option>
                                    </select>
                                </div>
                                <div>
                                    <div style={fieldLabel}>{t('labels.subscriptionStatus', {ns: 'tenants'})}</div>
                                    <select style={inputStyle} value={formData.subscription_status} onChange={e => setFormData({ ...formData, subscription_status: e.target.value as any })}>
                                        <option value="active">Active</option>
                                        <option value="trial">{t('stats.trial', {ns: 'tenants'})}</option>
                                        <option value="suspended">{t('labels.suspended', {ns: 'tenants'})}</option>
                                        <option value="cancelled">{t('labels.cancelled', {ns: 'tenants'})}</option>
                                    </select>
                                </div>
                                <div>
                                    <div style={fieldLabel}>{t('labels.accountStatus', {ns: 'tenants'})}</div>
                                    <div style={{ padding: '6px 10px', background: 'var(--neutral-50)', borderRadius: '6px', border: '1px solid var(--neutral-200)', height: '32px', display: 'flex', alignItems: 'center' }}>
                                        <label style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '13px', cursor: 'pointer', margin: 0 }}>
                                            <input type="checkbox" checked={formData.is_active} onChange={e => setFormData({ ...formData, is_active: e.target.checked })} />
                                            Account Active
                                        </label>
                                    </div>
                                </div>
                            </div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginTop: '10px' }}>
                                <div>
                                    <div style={fieldLabel}>Edge Server Quota <span style={{ fontSize: '10px', color: 'var(--neutral-400)', fontWeight: 'normal' }}>(1 used per site registered)</span></div>
                                    <input type="number" style={inputStyle} value={formData.max_edge_servers || ''} onChange={e => setFormData({ ...formData, max_edge_servers: parseInt(e.target.value) || 0 })} min="1" />
                                </div>
                                <div>
                                    <div style={fieldLabel}>{t('labels.maxDataPoints', {ns: 'tenants'})}</div>
                                    <input type="number" style={inputStyle} value={formData.max_data_points || ''} onChange={e => setFormData({ ...formData, max_data_points: parseInt(e.target.value) || 0 })} min="100" />
                                </div>
                            </div>
                        </div>
                    </div>
                </form>
            );
        }

        // 상세 보기 모드
        return (
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                {/* Basic Info */}
                <div style={sectionStyle}>
                    <div style={sectionTitleStyle}><i className="fas fa-building"></i> Basic Info</div>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                        <div>
                            <div style={fieldLabel}>{t('table.companyName', {ns: 'tenants'})}</div>
                            <div style={{ ...fieldValue, fontSize: '15px', fontWeight: 700 }}>{tenant.company_name}</div>
                        </div>
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                            <div>
                                <div style={fieldLabel}>{t('table.companyCode', {ns: 'tenants'})}</div>
                                <div style={fieldValue}><code style={{ background: 'var(--neutral-100)', padding: '1px 5px', borderRadius: '4px', fontSize: '12px' }}>{tenant.company_code}</code></div>
                            </div>
                            <div>
                                <div style={fieldLabel}>{t('table.domain', {ns: 'tenants'})}</div>
                                <div style={fieldValue}>{tenant.domain || '-'}</div>
                            </div>
                        </div>
                    </div>
                </div>

                {/* Manager Info */}
                <div style={sectionStyle}>
                    <div style={sectionTitleStyle}><i className="fas fa-user-tie"></i> Manager Info</div>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                        <div>
                            <div style={fieldLabel}>{t('labels.name', {ns: 'tenants'})}</div>
                            <div style={fieldValue}>{tenant.contact_name || '-'}</div>
                        </div>
                        <div>
                            <div style={fieldLabel}>{t('labels.contact', {ns: 'tenants'})}</div>
                            <div style={fieldValue}>
                                {tenant.contact_email && <div style={{ fontSize: '12px' }}><i className="fas fa-envelope" style={{ marginRight: '4px', color: 'var(--neutral-400)' }}></i>{tenant.contact_email}</div>}
                                {tenant.contact_phone && <div style={{ fontSize: '12px', marginTop: '2px' }}><i className="fas fa-phone" style={{ marginRight: '4px', color: 'var(--neutral-400)' }}></i>{tenant.contact_phone}</div>}
                                {!tenant.contact_email && !tenant.contact_phone && '-'}
                            </div>
                        </div>
                    </div>
                </div>

                {/* Subscription & Usage */}
                <div style={{ ...sectionStyle, gridColumn: '1 / -1' }}>
                    <div style={sectionTitleStyle}><i className="fas fa-chart-pie"></i> Subscription & Usage</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '12px' }}>
                        <div>
                            <div style={fieldLabel}>{t('labels.plan', {ns: 'tenants'})}</div>
                            <div style={fieldValue}>
                                <span style={{ display: 'inline-block', padding: '1px 8px', borderRadius: '10px', fontSize: '11px', fontWeight: 600, background: tenant.subscription_plan === 'enterprise' ? '#dbeafe' : 'var(--neutral-100)', color: tenant.subscription_plan === 'enterprise' ? '#1e40af' : 'var(--neutral-600)' }}>
                                    {tenant.subscription_plan ? tenant.subscription_plan.toUpperCase() : '-'}
                                </span>
                            </div>
                        </div>
                        <div>
                            <div style={fieldLabel}>{t('table.status', {ns: 'tenants'})}</div>
                            <div style={fieldValue}>
                                <span style={{ display: 'inline-block', padding: '1px 8px', borderRadius: '10px', fontSize: '11px', fontWeight: 600, background: tenant.subscription_status === 'active' ? '#dcfce7' : '#fef3c7', color: tenant.subscription_status === 'active' ? '#15803d' : '#92400e' }}>
                                    {tenant.subscription_status ? tenant.subscription_status.toUpperCase() : '-'}
                                </span>
                            </div>
                        </div>
                        <div>
                            <div style={fieldLabel}>{t('labels.edgeServerStatus', {ns: 'tenants'})}</div>
                            <div style={fieldValue}>
                                <span className="usage-indicator">
                                    <i className="fas fa-server" style={{ marginRight: '4px', color: 'var(--primary-500)' }}></i>
                                    <strong>{(tenant as any).edge_servers_count || 0}</strong> / {tenant.max_edge_servers} 대
                                </span>
                            </div>
                        </div>
                        <div>
                            <div style={fieldLabel}>데이터 포인트</div>
                            <div style={fieldValue}>
                                <span className="usage-indicator">
                                    <strong>{(tenant as any).data_points_count || 0}</strong> / {(tenant.max_data_points || 0).toLocaleString()} 개
                                </span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        );
    };

    if (!isOpen) return null;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container tenant-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{isEditing ? '고객사 정보 수정' : '고객사 상세 정보'}</h2>
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
                            <button className="btn btn-primary" onClick={handleEditToggle}>
                                <i className="fas fa-edit"></i> 수정
                            </button>
                        </div>
                    ) : (
                        <div style={{ width: '100%', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <div className="footer-left">
                                <button
                                    type="button"
                                    className="btn btn-error"
                                    onClick={handleDelete}
                                    disabled={saving}
                                >
                                    <i className="fas fa-trash"></i> 삭제
                                </button>
                            </div>
                            <div className="footer-right" style={{ display: 'flex', gap: '12px' }}>
                                <button className="btn btn-outline" onClick={handleEditToggle} disabled={saving}>취소</button>
                                <button type="submit" form="tenant-detail-form" className="btn btn-primary" disabled={saving}>
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
