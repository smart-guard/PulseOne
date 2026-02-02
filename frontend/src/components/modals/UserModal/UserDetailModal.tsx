import React, { useEffect, useState } from 'react';
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
            'system_admin': '시스템 관리자',
            'company_admin': '기업 관리자',
            'site_admin': '사이트 관리자',
            'manager': '매니저',
            'engineer': '엔지니어',
            'operator': '운영자',
            'viewer': '조회자'
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
                                <h2 style={{ fontSize: '18px', fontWeight: 700, margin: 0 }}>사용자 상세 정보</h2>
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
                                <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                                    <div className="detail-item">
                                        <span className="detail-label">성함</span>
                                        <span className="detail-value highlight fs-15">{user.full_name}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">이메일</span>
                                        <span className="detail-value">{user.email}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">연락처</span>
                                        <span className="detail-value">{user.phone || '-'}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">부서 / 조직</span>
                                        <span className="detail-value">{user.department || '-'}</span>
                                    </div>
                                    <div className="detail-item">
                                        <span className="detail-label">계정 상태</span>
                                        <span className="detail-value">
                                            <StatusBadge
                                                status={user.is_deleted ? '삭제됨(비활성)' : (user.is_active ? '정상 활성' : '일시 중지')}
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
                                <h3><i className="fas fa-shield-alt"></i> 보안 및 권한</h3>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                                    <div className="detail-item">
                                        <span className="detail-label">배정 역할</span>
                                        <span className="detail-value" style={{ marginTop: '4px' }}>
                                            <span className={`status-badge ${isSystemRole(user.role) ? 'status-purple' : 'status-success'}`} style={{ display: 'inline-flex', gap: '6px', alignItems: 'center' }}>
                                                <i className={`fas ${isSystemRole(user.role) ? 'fa-lock' : 'fa-user-tag'}`}></i>
                                                {getRoleLabel(user.role)}
                                            </span>
                                        </span>
                                    </div>

                                    <div className="detail-item">
                                        <span className="detail-label">활성화된 권한 정책</span>
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
                                        <span className="detail-label">사이트 접근 권한</span>
                                        <div className="detail-value" style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginTop: '6px' }}>
                                            {/* Typically sites would be looked up by ID, presenting IDs for now or if passed context */}
                                            {user.site_access && user.site_access.length > 0 ? (
                                                <span className="badge neutral">총 {user.site_access.length}개 사이트</span>
                                            ) : <span className="text-neutral-400" style={{ fontSize: '13px' }}>전체 접근 가능 (System Admin)</span>}
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
