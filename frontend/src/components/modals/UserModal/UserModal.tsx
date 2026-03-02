import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import UserApiService, { User, CreateUserRequest, UpdateUserRequest } from '../../../api/services/userApi';
import { roleApi, Role } from '../../../api/services/roleApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';
import './UserModal.css';

interface UserModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    user: User | null;
}

export const UserModal: React.FC<UserModalProps> = ({
    isOpen,
    onClose,
    onSave,
    user
}) => {
    const { confirm } = useConfirmContext();
    const [loading, setLoading] = useState(false);
    const { t } = useTranslation(['permissions', 'common']);
    const [formData, setFormData] = useState<Partial<User>>({
        username: '',
        email: '',
        full_name: '',
        role: 'viewer',
        department: '',
        permissions: [],
        is_active: true
    });
    const [tenants, setTenants] = useState<any[]>([]);
    const [sites, setSites] = useState<any[]>([]);
    const [roles, setRoles] = useState<Role[]>([]);
    const [availablePermissions, setAvailablePermissions] = useState<any[]>([]);

    useEffect(() => {
        const fetchMetadata = async () => {
            try {
                // Fetch Tenants (for System Admin context)
                const tenantRes = await import('../../../api/services/tenantApi').then(m => m.TenantApiService.getTenants());
                if (tenantRes.success) {
                    const tenantData = Array.isArray(tenantRes.data) ? tenantRes.data : (tenantRes.data as any).items;
                    if (Array.isArray(tenantData)) {
                        setTenants(tenantData);
                    }
                }

                // Fetch Sites (for Site Access)
                const siteRes = await import('../../../api/services/siteApi').then(m => m.SiteApiService.getSites({ limit: 100 }));
                if (siteRes.success) {
                    const siteData = Array.isArray(siteRes.data) ? siteRes.data : (siteRes.data as any).items;
                    if (Array.isArray(siteData)) {
                        setSites(siteData);
                    }
                }

                // Fetch Dynamic Roles
                const rolesData = await roleApi.getRoles();
                setRoles(rolesData);

                // Fetch Dynamic Permissions
                const permsData = await roleApi.getPermissions();
                setAvailablePermissions(permsData);
            } catch (error) {
                console.error('Failed to fetch metadata:', error);
            }
        };

        if (isOpen) {
            fetchMetadata();
        }
    }, [isOpen]);

    useEffect(() => {
        if (user) {
            setFormData({
                ...user,
                is_active: !!user.is_active,
                site_access: user.site_access || []
            });
        } else {
            setFormData({
                username: '',
                email: '',
                full_name: '',
                role: 'viewer',
                department: '',
                permissions: [],
                is_active: true,
                site_access: [],
                tenant_id: tenants.length === 1 ? tenants[0].id : undefined // Auto-select if only 1
            });
        }
    }, [user, isOpen, tenants]);

    if (!isOpen) return null;

    const handleInputChange = (field: keyof User | 'tenant_id', value: any) => {
        setFormData(prev => ({ ...prev, [field]: value }));
    };

    const togglePermission = (permissionId: string) => {
        const permissions = formData.permissions || [];
        if (permissions.includes(permissionId)) {
            handleInputChange('permissions', permissions.filter(p => p !== permissionId));
        } else {
            handleInputChange('permissions', [...permissions, permissionId]);
        }
    };

    const toggleSiteAccess = (siteId: number) => {
        const currentAccess = formData.site_access || [];
        if (currentAccess.includes(siteId)) {
            handleInputChange('site_access', currentAccess.filter(id => id !== siteId));
        } else {
            handleInputChange('site_access', [...currentAccess, siteId]);
        }
    };

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        try {
            setLoading(true);
            if (user) {
                await UserApiService.updateUser(user.id, formData as UpdateUserRequest);
            } else {
                await UserApiService.createUser(formData as CreateUserRequest);
            }
            onSave();
            onClose();
        } catch (error) {
            console.error('Failed to save user:', error);
        } finally {
            setLoading(false);
        }
    };

    const handleDelete = async () => {
        if (!user) return;

        const confirmed = await confirm({
            title: 'Delete User',
            message: `'${user.full_name}' user will be deactivated. Deleted users will be in the 'Deleted users' section and can be restored later.`,
            confirmText: 'Delete',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setLoading(true);
                await UserApiService.deleteUser(user.id);
                onSave();
                onClose();
            } catch (error) {
                console.error('Failed to delete user:', error);
            } finally {
                setLoading(false);
            }
        }
    };

    // Filter sites based on selected tenant
    const filteredSites = formData.tenant_id
        ? sites.filter(s => s.tenant_id === Number(formData.tenant_id))
        : sites;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container user-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{user ? 'Edit User Info' : 'Register New User'}</h2>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose} disabled={loading}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body">
                    <form id="user-form" onSubmit={handleSubmit}>
                        <div className="mgmt-modal-form-grid">
                            {/* Left Column: Basic Info */}
                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-info-circle"></i> Basic Info</h3>

                                {/* Tenant Selection (Visible if multiple tenants available) */}
                                {tenants.length > 1 && (
                                    <div className="mgmt-modal-form-group">
                                        <label className="required">{t('labels.companyTenant', {ns: 'permissions'})}</label>
                                        <select
                                            className="form-control"
                                            value={formData.tenant_id || ''}
                                            onChange={(e) => handleInputChange('tenant_id', Number(e.target.value))}
                                            disabled={!!user || loading}
                                            required
                                        >
                                            <option value="">{t('labels.selectACompany', {ns: 'permissions'})}</option>
                                            {tenants.map(t => (
                                                <option key={t.id} value={t.id}>{t.company_name} ({t.company_code})</option>
                                            ))}
                                        </select>
                                    </div>
                                )}

                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('labels.username', {ns: 'permissions'})}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.username}
                                        onChange={(e) => handleInputChange('username', e.target.value)}
                                        placeholder="Username (e.g. admin01)"
                                        required
                                        disabled={!!user || loading}
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('labels.fullName', {ns: 'permissions'})}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.full_name}
                                        onChange={(e) => handleInputChange('full_name', e.target.value)}
                                        placeholder="Full name"
                                        required
                                        disabled={loading}
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('labels.phoneNumber', {ns: 'permissions'})}</label>
                                    <input
                                        type="tel"
                                        className="form-control"
                                        value={formData.phone || ''}
                                        onChange={(e) => handleInputChange('phone', e.target.value)}
                                        placeholder="010-1234-5678"
                                        disabled={loading}
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('labels.email', {ns: 'permissions'})}</label>
                                    <input
                                        type="email"
                                        className="form-control"
                                        value={formData.email}
                                        onChange={(e) => handleInputChange('email', e.target.value)}
                                        placeholder="user@example.com"
                                        required
                                        disabled={loading}
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('labels.department', {ns: 'permissions'})}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.department || ''}
                                        onChange={(e) => handleInputChange('department', e.target.value)}
                                        placeholder="Operations / Manufacturing Dept."
                                        disabled={loading}
                                    />
                                </div>
                            </div>

                            {/* Right Column: Roles & Permissions */}
                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-shield-alt"></i> Access & Account Settings</h3>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('labels.systemRole', {ns: 'permissions'})}</label>
                                    <select
                                        className="form-control"
                                        value={formData.role}
                                        onChange={(e) => handleInputChange('role', e.target.value)}
                                        disabled={loading}
                                    >
                                        <option value="">{t('labels.selectARole', {ns: 'permissions'})}</option>
                                        {roles.map(role => (
                                            <option key={role.id} value={role.id}>
                                                {role.name} {role.is_system ? '(System)' : ''}
                                            </option>
                                        ))}
                                    </select>
                                </div>

                                <div className="mgmt-modal-form-group" style={{ marginTop: '12px' }}>
                                    <label>{t('labels.detailedPermissions', {ns: 'permissions'})}</label>
                                    <div className="permissions-grid" style={{ maxHeight: '150px', overflowY: 'auto' }}>
                                        {availablePermissions.length > 0 ? (
                                            availablePermissions.map(perm => (
                                                <label key={perm.id} className="permission-item">
                                                    <input
                                                        type="checkbox"
                                                        checked={formData.permissions?.includes(perm.id)}
                                                        onChange={() => togglePermission(perm.id)}
                                                        disabled={loading}
                                                    />
                                                    {perm.id}
                                                </label>
                                            ))
                                        ) : (
                                            <div style={{ gridColumn: '1 / -1', color: '#999', fontSize: '13px', textAlign: 'center', padding: '10px' }}>
                                                No permissions defined.
                                            </div>
                                        )}
                                    </div>
                                    <p className="mgmt-modal-form-hint">Use this to grant additional permissions not included in the role.</p>
                                </div>

                                {/* Site Access Selection */}
                                <div className="mgmt-modal-form-group" style={{ marginTop: '12px' }}>
                                    <label>Site Access ({filteredSites.length})</label>
                                    <div className="permissions-grid" style={{ maxHeight: '100px', overflowY: 'auto' }}>
                                        {filteredSites.length === 0 ? (
                                            <span style={{ color: '#999', fontSize: '13px' }}>{t('labels.noSitesRegistered', {ns: 'permissions'})}</span>
                                        ) : (
                                            filteredSites.map(site => (
                                                <label key={site.id} className="permission-item" title={site.name}>
                                                    <input
                                                        type="checkbox"
                                                        checked={formData.site_access?.includes(site.id)}
                                                        onChange={() => toggleSiteAccess(site.id)}
                                                        disabled={loading}
                                                    />
                                                    {site.name}
                                                </label>
                                            ))
                                        )}
                                    </div>
                                </div>

                                <div className="mgmt-modal-form-group" style={{ marginTop: 'auto' }}>
                                    <div className="checkbox-group">
                                        <label className="checkbox-label">
                                            <input
                                                type="checkbox"
                                                checked={!!formData.is_active}
                                                onChange={(e) => handleInputChange('is_active', e.target.checked)}
                                                disabled={loading}
                                            />
                                            계정 활성화 상태
                                        </label>
                                        <p className="mgmt-modal-form-hint">
                                            비활성화 시 해당 사용자의 시스템 로그인이 즉시 차단됩니다.
                                        </p>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </form>
                </div>

                <div className="mgmt-modal-footer">
                    <div className="footer-left">
                        {user && !user.is_deleted && (
                            <button
                                type="button"
                                className="btn btn-outline btn-error"
                                onClick={handleDelete}
                                disabled={loading}
                            >
                                <i className="fas fa-trash"></i> 사용자 삭제
                            </button>
                        )}
                    </div>
                    <div className="footer-right">
                        <button type="button" className="btn btn-outline" onClick={onClose} disabled={loading}>
                            취소
                        </button>
                        <button type="submit" form="user-form" className="btn btn-primary" disabled={loading}>
                            {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-check"></i>}
                            {user ? '수정 완료' : '등록 완료'}
                        </button>
                    </div>
                </div>
            </div>
        </div>
    );
};
