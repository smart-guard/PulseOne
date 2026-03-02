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
        <div className="modal-overlay">
            <div className="modal-container user-modal">
                <div className="modal-header">
                    <div className="modal-title">
                        <h2>{role ? t('roleModal.editTitle', { ns: 'permissions' }) : t('roleModal.createTitle', { ns: 'permissions' })}</h2>
                    </div>
                    <button className="close-btn" onClick={onClose} disabled={loading}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="modal-body">
                    <form id="role-form" onSubmit={handleSubmit}>
                        <div className="modal-form-grid">
                            <div className="modal-form-section">
                                <h3><i className="fas fa-info-circle"></i> {t('roleModal.sectionBasic', { ns: 'permissions' })}</h3>
                                <div className="modal-form-group">
                                    <label className="required">{t('roleModal.name', { ns: 'permissions' })}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.name}
                                        onChange={(e) => handleInputChange('name', e.target.value)}
                                        placeholder={t('roleModal.namePlaceholder', { ns: 'permissions' })}
                                        required
                                        disabled={loading || (role?.is_system === 1)} // System role name usually locked? Let's lock it to be safe
                                    />
                                    {role?.is_system === 1 && <p className="modal-form-hint">{t('roleModal.systemNameLocked', { ns: 'permissions' })}</p>}
                                </div>
                                <div className="modal-form-group">
                                    <label>{t('roleModal.descLabel', { ns: 'permissions' })}</label>
                                    <textarea
                                        className="form-control"
                                        value={formData.description || ''}
                                        onChange={(e) => handleInputChange('description', e.target.value)}
                                        placeholder={t('roleModal.descPlaceholder', { ns: 'permissions' })}
                                        rows={3}
                                        disabled={loading}
                                    />
                                </div>
                            </div>

                            <div className="modal-form-section">
                                <h3><i className="fas fa-key"></i> {t('roleModal.sectionPermissions', { ns: 'permissions' })}</h3>
                                <div className="modal-form-group">
                                    <label>{t('roleModal.permCount', { ns: 'permissions', count: formData.permissions?.length || 0 })}</label>
                                    <div className="permissions-grid" style={{ maxHeight: '300px', overflowY: 'auto' }}>
                                        {availablePermissions.map(perm => (
                                            <label key={perm.id} className="permission-item">
                                                <input
                                                    type="checkbox"
                                                    checked={formData.permissions?.includes(perm.id)}
                                                    onChange={() => togglePermission(perm.id)}
                                                    disabled={loading}
                                                />
                                                <span style={{ marginLeft: '4px' }}>
                                                    <strong>{perm.name}</strong>
                                                    <span style={{ fontSize: '0.85em', color: '#666', display: 'block' }}>{perm.description}</span>
                                                </span>
                                            </label>
                                        ))}
                                    </div>
                                </div>
                            </div>
                        </div>
                    </form>
                </div>

                <div className="modal-footer">
                    <div className="footer-left">
                        {role && !role.is_system && (
                            <button
                                type="button"
                                className="btn btn-outline btn-error"
                                onClick={handleDelete}
                                disabled={loading}
                            >
                                <i className="fas fa-trash"></i> 역할 삭제
                            </button>
                        )}
                    </div>
                    <div className="footer-right">
                        <button type="button" className="btn btn-outline" onClick={onClose} disabled={loading}>
                            취소
                        </button>
                        <button type="submit" form="role-form" className="btn btn-primary" disabled={loading}>
                            {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-check"></i>}
                            {role ? '수정 완료' : '생성 완료'}
                        </button>
                    </div>
                </div>
            </div>
        </div>
    );
};
