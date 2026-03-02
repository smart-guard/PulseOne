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
            <div className="mgmt-modal-container user-modal" style={{ maxWidth: '860px' }}>
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
                        <div className="mgmt-modal-form-grid">
                            {/* Left: Basic Info */}
                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-info-circle"></i> {t('roleModal.sectionBasic', { ns: 'permissions' })}</h3>

                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('roleModal.name', { ns: 'permissions' })}</label>
                                    <input
                                        type="text"
                                        className="mgmt-modal-input"
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
                                    <textarea
                                        className="mgmt-modal-input"
                                        style={{ height: '100px', paddingTop: '10px', resize: 'vertical' }}
                                        value={formData.description || ''}
                                        onChange={(e) => handleInputChange('description', e.target.value)}
                                        placeholder={t('roleModal.descPlaceholder', { ns: 'permissions' })}
                                        disabled={loading}
                                    />
                                </div>
                            </div>

                            {/* Right: Permissions */}
                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-key"></i> {t('roleModal.sectionPermissions', { ns: 'permissions' })}</h3>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('roleModal.permCount', { ns: 'permissions', count: formData.permissions?.length || 0 })}</label>
                                    <div className="permissions-grid" style={{ maxHeight: '260px', overflowY: 'auto' }}>
                                        {availablePermissions.map(perm => (
                                            <label key={perm.id} className="permission-item">
                                                <input
                                                    type="checkbox"
                                                    checked={formData.permissions?.includes(perm.id)}
                                                    onChange={() => togglePermission(perm.id)}
                                                    disabled={loading}
                                                />
                                                <span style={{ marginLeft: '6px' }}>
                                                    <strong style={{ fontSize: '13px' }}>{perm.name}</strong>
                                                    <span style={{ fontSize: '12px', color: 'var(--neutral-500)', display: 'block', marginTop: '2px' }}>{perm.description}</span>
                                                </span>
                                            </label>
                                        ))}
                                    </div>
                                </div>
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

