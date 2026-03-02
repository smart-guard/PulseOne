import React, { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { User } from '../../../api/services/userApi';
import { roleApi, Role } from '../../../api/services/roleApi';
import StatusBadge from '../../common/StatusBadge';

interface UserDetailModalProps {
    isOpen: boolean;
    onClose: () => void;
    user: User | null;
    onEdit: (user: User) => void;
    onRestore: (user: User) => void;
}

export const UserDetailModal: React.FC<UserDetailModalProps> = ({
    isOpen,
    onClose,
    user,
    onEdit,
    onRestore
}) => {
    const [roles, setRoles] = useState<Role[]>([]);
    const { t } = useTranslation(['permissions', 'common']);

    useEffect(() => {
        if (isOpen) {
            loadRoles();
        }
    }, [isOpen]);

    const loadRoles = async () => {
        try {
            const fetchedRoles = await roleApi.getRoles();
            setRoles(fetchedRoles);
        } catch (err) {
            console.error('Failed to load roles for detail view', err);
        }
    };

    if (!isOpen || !user) return null;

    // Helper to find role name from dynamic list, falling back to ID or hardcoded map if needed
    const getRoleLabel = (roleId: string) => {
        const found = roles.find(r => r.id === roleId);
        if (found) return found.name;

        // Legacy Fallback
        const labels: Record<string, string> = {
            'system_admin': 'System Admin',
            'company_admin': 'Company Admin',
            'site_admin': 'Site Admin',
            'manager': 'Manager',
            'engineer': 'Engineer',
            'operator': 'Operator',
            'viewer': 'Viewer'
        };
        return labels[roleId] || roleId;
    };

    const isSystemRole = (roleId: string) => {
        // Check if it's a known system role ID or if found in roles list as system
        const found = roles.find(r => r.id === roleId);
        if (found) return found.is_system;
        return ['system_admin', 'company_admin', 'site_admin', 'manager', 'engineer', 'operator', 'viewer'].includes(roleId);
    };

    return (
        <div className="mgmt-modal-overlay">
            {/* Matches UserModal width (800px) */}
            <div className="mgmt-modal-container user-modal" style={{ maxWidth: '800px' }}>
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <div className="title-row" style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                            <div style={{
                                width: '40px',
                                height: '40px',
                                borderRadius: 'var(--radius-lg)',
                                background: 'var(--primary-50)',
                                color: 'var(--primary-600)',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                                fontSize: '20px'
                            }}>
                                <i className="fas fa-user"></i>
                            </div>
                            <div>
                                <h2 style={{ fontSize: '18px', fontWeight: 700, margin: 0 }}>{t('labels.userDetails', {ns: 'permissions'})}</h2>
                                <p style={{ fontSize: '13px', color: 'var(--neutral-500)', margin: 0 }}>@{user.username} (ID: {user.id})</p>
                            </div>
                        </div>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body">
                    {/* Balanced 2-Column Grid */}
                    <div className="mgmt-modal-form-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '24px' }}>

                        {/* LEFT COLUMN: Basic Info */}
                        <div className="left-col" style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
                            <div className="mgmt-modal-form-section" style={{ border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '16px', height: '100%' }}>
                                <h3><i className="fas fa-info-circle"></i> Basic Info</h3>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.fullName', {ns: 'permissions'})}</span>
                                        <span className="detail-value highlight fs-15">{user.full_name}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.email', {ns: 'permissions'})}</span>
                                        <span className="detail-value">{user.email}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.contact', {ns: 'permissions'})}</span>
                                        <span className="detail-value">{user.phone || '-'}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.department', {ns: 'permissions'})}</span>
                                        <span className="detail-value">{user.department || '-'}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.accountStatus', {ns: 'permissions'})}</span>
                                        <span className="detail-value">
                                            <StatusBadge
                                                status={user.is_deleted ? 'Deleted (Inactive)' : (user.is_active ? 'Active' : 'Suspended')}
                                                type={user.is_deleted ? 'error' : (user.is_active ? 'active' : 'inactive')}
                                            />
                                        </span>
                                    </div>
                                </div>
                            </div>
                        </div>

                        {/* RIGHT COLUMN: Permissions & System Records */}
                        <div className="right-col" style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
                            {/* Security & Permissions */}
                            <div className="mgmt-modal-form-section" style={{ border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '16px' }}>
                                <h3><i className="fas fa-shield-alt"></i> Security & Permissions</h3>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.assignedRole', {ns: 'permissions'})}</span>
                                        <span className="detail-value" style={{ marginTop: '4px' }}>
                                            <span className={`status-badge ${isSystemRole(user.role) ? 'status-purple' : 'status-success'}`} style={{ display: 'inline-flex', gap: '6px', alignItems: 'center' }}>
                                                <i className={`fas ${isSystemRole(user.role) ? 'fa-lock' : 'fa-user-tag'}`}></i>
                                                {getRoleLabel(user.role)}
                                            </span>
                                        </span>
                                    </div>

                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.activePermissionPolicy', {ns: 'permissions'})}</span>
                                        <div className="detail-value" style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginTop: '6px' }}>
                                            {user.permissions && user.permissions.length > 0 ? (
                                                user.permissions.map(p => (
                                                    <span key={p} className="badge neutral" style={{ fontSize: '11px', padding: '3px 8px', background: 'var(--neutral-100)' }}>
                                                        {p}
                                                    </span>
                                                ))
                                            ) : <span className="text-neutral-400" style={{ fontSize: '13px' }}>-</span>}
                                        </div>
                                    </div>

                                    <div className="detail-item">
                                        <span className="detail-label">{t('labels.siteAccess', {ns: 'permissions'})}</span>
                                        <div className="detail-value" style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginTop: '6px' }}>
                                            {/* Typically sites would be looked up by ID, presenting IDs for now or if passed context */}
                                            {user.site_access && user.site_access.length > 0 ? (
                                                <span className="badge neutral">{user.site_access.length} site(s) total</span>
                                            ) : <span className="text-neutral-400" style={{ fontSize: '13px' }}>{t('labels.fullAccessSystemAdmin', {ns: 'permissions'})}</span>}
                                        </div>
                                    </div>
                                </div>
                            </div>

                            {/* System Records - Moved to Right Column */}
                            <div className="mgmt-modal-form-section" style={{ border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '16px', flex: 1 }}>
                                <h3><i className="fas fa-history"></i> 시스템 기록</h3>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                    <div className="detail-item" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                        <span className="detail-label" style={{ marginBottom: 0 }}>계정 생성</span>
                                        <span className="detail-value" style={{ fontSize: '13px', fontFamily: 'monospace' }}>
                                            {user.created_at ? new Date(user.created_at).toLocaleString() : '-'}
                                        </span>
                                    </div>
                                    <div className="detail-item" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                        <span className="detail-label" style={{ marginBottom: 0 }}>최종 로그인</span>
                                        <span className="detail-value" style={{ fontSize: '13px', color: 'var(--primary-600)', fontWeight: 600 }}>
                                            {user.last_login ? new Date(user.last_login).toLocaleString() : '기록 없음'}
                                        </span>
                                    </div>
                                    <div className="detail-item" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                        <span className="detail-label" style={{ marginBottom: 0 }}>최근 수정</span>
                                        <span className="detail-value" style={{ fontSize: '13px', fontFamily: 'monospace' }}>
                                            {user.updated_at ? new Date(user.updated_at).toLocaleString() : '-'}
                                        </span>
                                    </div>
                                </div>
                            </div>
                        </div>

                    </div>
                </div>

                <div className="mgmt-modal-footer">
                    <div className="footer-left">
                        {!!user.is_deleted && (
                            <button className="btn btn-outline success" onClick={() => onRestore(user)}>
                                <i className="fas fa-undo"></i> 계정 복구하기
                            </button>
                        )}
                    </div>
                    <div className="footer-right">
                        <button className="btn btn-outline" onClick={onClose}>닫기</button>
                        {!user.is_deleted && (
                            <button className="btn btn-primary" onClick={() => onEdit(user)}>
                                <i className="fas fa-edit"></i> 정보 수정
                            </button>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default UserDetailModal;
