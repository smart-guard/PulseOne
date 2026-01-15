import React from 'react';
import { Role } from '../../../api/services/roleApi';

interface RoleDetailModalProps {
    isOpen: boolean;
    onClose: () => void;
    role: Role | null;
    onEdit: (role: Role) => void;
    onDelete?: (role: Role) => void;
}

export const RoleDetailModal: React.FC<RoleDetailModalProps> = ({
    isOpen,
    onClose,
    role,
    onEdit,
    onDelete
}) => {
    if (!isOpen || !role) return null;

    return (
        <div className="modal-overlay">
            {/* Ultra-wide for 3-column layout */}
            <div className="modal-container" style={{ maxWidth: '1100px' }}>
                <div className="modal-header">
                    <div className="modal-title">
                        <div className="title-row" style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                            <div style={{
                                width: '40px',
                                height: '40px',
                                borderRadius: 'var(--radius-lg)',
                                background: role.is_system ? 'var(--purple-50)' : 'var(--success-50)',
                                color: role.is_system ? 'var(--purple-600)' : 'var(--success-600)',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                                fontSize: '20px'
                            }}>
                                <i className={`fas ${role.is_system ? 'fa-lock' : 'fa-user-tag'}`}></i>
                            </div>
                            <div>
                                <h2 style={{ fontSize: '18px', fontWeight: 700, margin: 0 }}>{role.name}</h2>
                                <p style={{ fontSize: '13px', color: 'var(--neutral-500)', margin: 0 }}>@{role.id}</p>
                            </div>
                        </div>
                    </div>
                    <button className="close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="modal-body">
                    {/* 3-Column Grid Layout */}
                    <div className="modal-form-grid" style={{
                        display: 'grid',
                        gridTemplateColumns: 'minmax(250px, 3fr) minmax(350px, 5fr) minmax(200px, 2fr)',
                        gap: '20px',
                        alignItems: 'stretch'
                    }}>

                        {/* Col 1: Basic Info */}
                        <div className="modal-form-section" style={{ marginBottom: 0, display: 'flex', flexDirection: 'column' }}>
                            <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>

                            <div className="detail-item" style={{ marginBottom: '12px' }}>
                                <span className="detail-label">역할명</span>
                                <span className="detail-value highlight fs-15">{role.name}</span>
                            </div>

                            <div className="detail-item" style={{ marginBottom: '12px' }}>
                                <span className="detail-label">유형</span>
                                <div style={{ marginTop: '4px' }}>
                                    {role.is_system ? (
                                        <span className="status-badge status-purple">
                                            <i className="fas fa-lock"></i> 시스템 기본
                                        </span>
                                    ) : (
                                        <span className="status-badge status-success">
                                            <i className="fas fa-user-edit"></i> 사용자 정의
                                        </span>
                                    )}
                                </div>
                            </div>

                            <div className="detail-item" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
                                <span className="detail-label">설명</span>
                                <div style={{
                                    background: 'var(--neutral-50)',
                                    padding: '12px',
                                    borderRadius: '8px',
                                    fontSize: '13px',
                                    color: 'var(--neutral-700)',
                                    lineHeight: '1.5',
                                    flex: 1,
                                    marginTop: '4px'
                                }}>
                                    {role.description || '설명이 없습니다.'}
                                </div>
                            </div>
                        </div>

                        {/* Col 2: Permissions */}
                        <div className="modal-form-section" style={{ marginBottom: 0, display: 'flex', flexDirection: 'column' }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
                                <h3><i className="fas fa-key"></i> 포함된 권한</h3>
                                <span className="badge neutral" style={{ fontSize: '11px' }}>{role.permissions ? role.permissions.length : 0} items</span>
                            </div>

                            <div className="permissions-grid" style={{
                                display: 'grid',
                                gridTemplateColumns: 'repeat(auto-fill, minmax(130px, 1fr))',
                                gap: '8px',
                                background: 'var(--neutral-50)',
                                padding: '12px',
                                borderRadius: '8px',
                                border: '1px solid var(--neutral-200)',
                                flex: 1,
                                overflowY: 'auto',
                                alignContent: 'start',
                                maxHeight: '400px'
                            }}>
                                {role.permissions && role.permissions.length > 0 ? (
                                    role.permissions.map(p => (
                                        <div key={p} style={{
                                            background: 'white',
                                            border: '1px solid var(--neutral-200)',
                                            padding: '6px 10px',
                                            borderRadius: '6px',
                                            fontSize: '12px',
                                            display: 'flex',
                                            alignItems: 'center',
                                            gap: '6px',
                                            fontWeight: 500
                                        }}>
                                            <i className="fas fa-check-circle" style={{ color: 'var(--success-500)', fontSize: '12px' }}></i>
                                            {p}
                                        </div>
                                    ))
                                ) : (
                                    <div style={{ gridColumn: '1 / -1', textAlign: 'center', padding: '40px', color: '#999' }}>
                                        권한 없음
                                    </div>
                                )}
                            </div>
                        </div>

                        {/* Col 3: Meta Info */}
                        <div className="modal-form-section" style={{ marginBottom: 0, display: 'flex', flexDirection: 'column' }}>
                            <h3><i className="fas fa-clock"></i> 메타 정보</h3>

                            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', flex: 1 }}>
                                <div className="detail-item">
                                    <span className="detail-label">등록일시</span>
                                    <span className="detail-value" style={{ fontSize: '13px', fontFamily: 'monospace' }}>
                                        {role.created_at ? new Date(role.created_at).toLocaleDateString() : '-'}
                                        <br />
                                        <span style={{ color: '#999', fontSize: '11px' }}>
                                            {role.created_at ? new Date(role.created_at).toLocaleTimeString() : ''}
                                        </span>
                                    </span>
                                </div>

                                <div style={{ height: '1px', background: 'var(--neutral-100)' }}></div>

                                <div className="detail-item">
                                    <span className="detail-label">할당된 사용자</span>
                                    <span className="detail-value" style={{ fontSize: '18px', fontWeight: 700, color: 'var(--primary-600)' }}>
                                        {role.userCount || 0}
                                        <span style={{ fontSize: '12px', color: '#666', fontWeight: 400, marginLeft: '4px' }}>명</span>
                                    </span>
                                </div>

                                <div style={{ height: '1px', background: 'var(--neutral-100)' }}></div>

                                <div className="detail-item">
                                    <span className="detail-label">권한 정책 ID</span>
                                    <span className="detail-value" style={{ fontSize: '12px', color: '#999' }}>
                                        @{role.id}
                                    </span>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <div className="modal-footer">
                    <div className="footer-left">
                        {/* Delete Button for User-Defined Roles */}
                        {!role.is_system && onDelete && (
                            <button
                                className="btn btn-outline btn-error"
                                onClick={() => onDelete(role)}
                            >
                                <i className="fas fa-trash"></i> 역할 삭제
                            </button>
                        )}
                    </div>
                    <div className="footer-right" style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
                        <button className="btn btn-outline" onClick={onClose}>닫기</button>
                        {!role.is_system && (
                            <button className="btn btn-primary" onClick={() => onEdit(role)}>
                                <i className="fas fa-edit"></i> 역할 수정
                            </button>
                        )}
                        {!!role.is_system && (
                            <button className="btn btn-primary" disabled title="시스템 기본 역할은 수정할 수 없습니다">
                                <i className="fas fa-lock"></i> 수정 불가
                            </button>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};
