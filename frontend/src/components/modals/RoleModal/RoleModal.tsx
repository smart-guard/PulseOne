import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import RoleApiService, { Role, Permission, CreateRoleRequest, UpdateRoleRequest } from '../../../api/services/roleApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import '../UserModal/UserModal.css'; // Shared styles

// Initialize API Service if not imported
const roleApi = new RoleApiService();

interface RoleModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    role: Role | null;
}

export const RoleModal: React.FC<RoleModalProps> = ({
    isOpen,
    onClose,
    onSave,
    role
}) => {
    const { confirm } = useConfirmContext();
    const [loading, setLoading] = useState(false);
    const { t } = useTranslation(['permissions', 'common']);
    const [availablePermissions, setAvailablePermissions] = useState<Permission[]>([]);

    const [formData, setFormData] = useState<Partial<CreateRoleRequest>>({
        name: '',
        description: '',
        permissions: []
    });

    // Fetch all available permissions when modal opens
    useEffect(() => {
        if (isOpen) {
            const fetchPermissions = async () => {
                try {
                    const params = await roleApi.getPermissions();
                    setAvailablePermissions(params);
                } catch (e) {
                    console.error("Failed to fetch permissions", e);
                }
            };
            fetchPermissions();
        }
    }, [isOpen]);

    // Populate form data when editing
    useEffect(() => {
        if (role) {
            // If editing, we need to ensure we have the latest permission list for this role
            // But the role object passed usually has permissions as string[] if fetched from getRoles()
            // Wait, getRoles() returns permissions as string[]? Let's check RoleApi definition.
            // Yes, Role interface has permissions: string[]
            setFormData({
                name: role.name,
                description: role.description,
                permissions: role.permissions || []
            });
        } else {
            setFormData({
                name: '',
                description: '',
                permissions: []
            });
        }
    }, [role, isOpen]);

    const handleInputChange = (field: keyof CreateRoleRequest, value: any) => {
        setFormData(prev => ({ ...prev, [field]: value }));
    };

    const togglePermission = (permId: string) => {
        const currentPerms = formData.permissions || [];
        if (currentPerms.includes(permId)) {
            handleInputChange('permissions', currentPerms.filter(p => p !== permId));
        } else {
            handleInputChange('permissions', [...currentPerms, permId]);
        }
    };

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        try {
            setLoading(true);
            if (role) {
                await roleApi.updateRole(role.id, formData as UpdateRoleRequest);
            } else {
                await roleApi.createRole(formData as CreateRoleRequest);
            }
            onSave();
            onClose();
        } catch (error) {
            console.error('Failed to save role:', error);
            // alert('저장 실패: ' + error); // User requested no more popups, stick to console or toast if available
        } finally {
            setLoading(false);
        }
    };

    const handleDelete = async () => {
        if (!role) return;

        // System roles cannot be deleted usually, checked by API, but we should also warn in UI
        if (role.is_system) {
            await confirm({
                title: t('roleModal.cantDeleteSystemTitle', { ns: 'permissions' }),
                message: t('roleModal.cantDeleteSystemMsg', { ns: 'permissions' }),
                confirmText: t('roleModal.cantDeleteSystemBtn', { ns: 'permissions' }),
                confirmButtonType: 'primary'
            });
            return;
        }

        const confirmed = await confirm({
            title: t('roleModal.deleteTitle', { ns: 'permissions' }),
            message: t('roleModal.deleteMsg', { ns: 'permissions', name: role.name }),
            confirmText: t('roleModal.deleteBtn', { ns: 'permissions' }),
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setLoading(true);
                await roleApi.deleteRole(role.id);
                onSave();
                onClose();
            } catch (error) {
                console.error('Failed to delete role:', error);
            } finally {
                setLoading(false);
            }
        }
    };

    if (!isOpen) return null;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container user-modal" style={{ maxWidth: '760px', width: '90%' }}>
                {/* Header */}
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{role ? t('roleModal.editTitle', { ns: 'permissions' }) : t('roleModal.createTitle', { ns: 'permissions' })}</h2>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose} disabled={loading}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                {/* Body */}
                <div className="mgmt-modal-body">
                    <form id="role-form" onSubmit={handleSubmit}>

                        {/* 섹션 1: 기본 정보 (가로 2열) */}
                        <div className="mgmt-modal-form-section" style={{ marginBottom: '20px' }}>
                            <h3 style={{ marginBottom: '14px' }}><i className="fas fa-info-circle"></i> {t('roleModal.sectionBasic', { ns: 'permissions' })}</h3>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 2fr', gap: '16px', alignItems: 'start' }}>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('roleModal.name', { ns: 'permissions' })}</label>
                                    <input
                                        type="text"
                                        className="mgmt-input"
                                        value={formData.name}
                                        onChange={(e) => handleInputChange('name', e.target.value)}
                                        placeholder={t('roleModal.namePlaceholder', { ns: 'permissions' })}
                                        required
                                        disabled={loading || (role?.is_system === 1)}
                                    />
                                    {role?.is_system === 1 && (
                                        <p className="mgmt-modal-form-hint">{t('roleModal.systemNameLocked', { ns: 'permissions' })}</p>
                                    )}
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('roleModal.descLabel', { ns: 'permissions' })}</label>
                                    <input
                                        type="text"
                                        className="mgmt-input"
                                        value={formData.description || ''}
                                        onChange={(e) => handleInputChange('description', e.target.value)}
                                        placeholder={t('roleModal.descPlaceholder', { ns: 'permissions' })}
                                        disabled={loading}
                                    />
                                </div>
                            </div>
                        </div>

                        {/* 구분선 */}
                        <div style={{ borderTop: '1px solid var(--neutral-100)', margin: '0 0 20px 0' }} />

                        {/* 섹션 2: 권한 설정 (3열 그리드) */}
                        <div className="mgmt-modal-form-section">
                            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '14px' }}>
                                <h3 style={{ margin: 0 }}><i className="fas fa-key"></i> {t('roleModal.sectionPermissions', { ns: 'permissions' })}</h3>
                                <span style={{ fontSize: '13px', color: 'var(--neutral-500)', background: 'var(--neutral-100)', padding: '2px 10px', borderRadius: '12px' }}>
                                    {t('roleModal.permCount', { ns: 'permissions', count: formData.permissions?.length || 0 })}
                                </span>
                            </div>
                            <div style={{
                                display: 'grid',
                                gridTemplateColumns: 'repeat(3, 1fr)',
                                gap: '8px'
                            }}>
                                {availablePermissions.map(perm => (
                                    <label
                                        key={perm.id}
                                        className="permission-item"
                                        style={{
                                            display: 'flex',
                                            alignItems: 'flex-start',
                                            gap: '8px',
                                            padding: '10px 12px',
                                            borderRadius: '8px',
                                            border: '1px solid var(--neutral-200)',
                                            cursor: loading ? 'not-allowed' : 'pointer',
                                            background: formData.permissions?.includes(perm.id) ? 'var(--primary-50)' : 'var(--neutral-50)',
                                            borderColor: formData.permissions?.includes(perm.id) ? 'var(--primary-300)' : 'var(--neutral-200)',
                                            transition: 'all 0.15s'
                                        }}
                                    >
                                        <input
                                            type="checkbox"
                                            checked={formData.permissions?.includes(perm.id)}
                                            onChange={() => togglePermission(perm.id)}
                                            disabled={loading}
                                            style={{ marginTop: '2px', flexShrink: 0 }}
                                        />
                                        <span>
                                            <strong style={{ fontSize: '13px', display: 'block', color: 'var(--neutral-800)' }}>{perm.name}</strong>
                                            <span style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '2px', display: 'block', lineHeight: '1.4' }}>{perm.description}</span>
                                        </span>
                                    </label>
                                ))}
                            </div>
                        </div>

                    </form>
                </div>

                {/* Footer */}
                <div className="mgmt-modal-footer">
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%' }}>
                        <div>
                            {role && !role.is_system && (
                                <button
                                    type="button"
                                    className="mgmt-btn mgmt-btn-danger"
                                    onClick={handleDelete}
                                    disabled={loading}
                                >
                                    <i className="fas fa-trash"></i> {t('roleModal.deleteRoleBtn', { ns: 'permissions' })}
                                </button>
                            )}
                        </div>
                        <div style={{ display: 'flex', gap: '8px' }}>
                            <button type="button" className="mgmt-btn mgmt-btn-outline" onClick={onClose} disabled={loading}>
                                {t('roleModal.cancelBtn', { ns: 'permissions' })}
                            </button>
                            <button type="submit" form="role-form" className="mgmt-btn mgmt-btn-primary" disabled={loading}>
                                {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-check"></i>}
                                {' '}{role ? t('roleModal.editBtn', { ns: 'permissions' }) : t('roleModal.createBtn', { ns: 'permissions' })}
                            </button>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

