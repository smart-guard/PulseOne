import React, { useState, useEffect } from 'react';
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
            setError('고객사 정보를 불러오는 중 오류가 발생했습니다.');
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
            title: '변경 사항 저장',
            message: '입력하신 변경 내용을 저장하시겠습니까?',
            confirmText: '저장',
            confirmButtonType: 'primary'
        });

        if (!isConfirmed) return;

        try {
            setSaving(true);
            setError(null);
            const res = await TenantApiService.updateTenant(tenantId, formData);
            if (res.success) {
                await confirm({
                    title: '저장 완료',
                    message: '고객사 정보가 성공적으로 수정되었습니다.',
                    confirmText: '확인',
                    showCancelButton: false,
                    confirmButtonType: 'primary'
                });
                await loadTenant(tenantId);
                onSave();
                onClose();
            } else {
                setError(res.message || '업데이트에 실패했습니다.');
            }
        } catch (err: any) {
            setError(err.message || '서버 통신 중 오류가 발생했습니다.');
        } finally {
            setSaving(false);
        }
    };

    const handleDelete = async () => {
        if (!tenant) return;

        const confirmed = await confirm({
            title: '고객사 삭제 확인',
            message: `'${tenant.company_name}' 고객사를 삭제하시겠습니까?`,
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setSaving(true);
                const res = await TenantApiService.deleteTenant(tenant.id);
                if (res.success) {
                    await confirm({
                        title: '삭제 완료',
                        message: '고객사가 성공적으로 삭제되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || '삭제에 실패했습니다.');
                }
            } catch (err: any) {
                setError(err.message || '삭제 중 오류가 발생했습니다.');
            } finally {
                setSaving(false);
            }
        }
    };

    const renderContent = () => {
        if (loading) {
            return (
                <div className="loading-spinner">
                    <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                </div>
            );
        }

        if (!tenant) {
            return <div className="alert alert-error">고객사 정보를 불러오지 못했습니다.</div>;
        }

        if (isEditing) {
            return (
                <form id="tenant-detail-form" onSubmit={handleSubmit}>
                    <div className="modal-form-grid">
                        <div className="modal-form-section">
                            <h3><i className="fas fa-building"></i> 기본 정보</h3>
                            <div className="modal-form-group">
                                <label className="required">회사명</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.company_name}
                                    onChange={e => setFormData({ ...formData, company_name: e.target.value })}
                                    required
                                />
                            </div>
                            <div className="modal-form-group">
                                <label>회사 코드</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.company_code}
                                    disabled
                                    title="회사 코드는 변경할 수 없습니다."
                                />
                            </div>
                            <div className="modal-form-group">
                                <label>도메인</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.domain || ''}
                                    onChange={e => setFormData({ ...formData, domain: e.target.value })}
                                />
                            </div>
                        </div>

                        <div className="modal-form-section">
                            <h3><i className="fas fa-user-tie"></i> 담당자 정보</h3>
                            <div className="modal-form-group">
                                <label>이름</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.contact_name || ''}
                                    onChange={e => setFormData({ ...formData, contact_name: e.target.value })}
                                />
                            </div>
                            <div className="modal-form-group">
                                <label>이메일</label>
                                <input
                                    type="email"
                                    className="form-control"
                                    value={formData.contact_email || ''}
                                    onChange={e => setFormData({ ...formData, contact_email: e.target.value })}
                                />
                            </div>
                            <div className="modal-form-group">
                                <label>연락처</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.contact_phone || ''}
                                    onChange={e => setFormData({ ...formData, contact_phone: e.target.value })}
                                />
                            </div>
                        </div>

                        <div className="modal-form-section span-full">
                            <h3><i className="fas fa-cog"></i> 설정 및 상태</h3>
                            <div className="modal-form-row">
                                <div className="modal-form-group">
                                    <label>구독 플랜</label>
                                    <select
                                        className="form-control"
                                        value={formData.subscription_plan}
                                        onChange={e => setFormData({ ...formData, subscription_plan: e.target.value as any })}
                                    >
                                        <option value="starter">스타터 (Starter)</option>
                                        <option value="professional">프로페셔널 (Professional)</option>
                                        <option value="enterprise">엔터프라이즈 (Enterprise)</option>
                                    </select>
                                </div>
                                <div className="modal-form-group">
                                    <label>구독 상태</label>
                                    <select
                                        className="form-control"
                                        value={formData.subscription_status}
                                        onChange={e => setFormData({ ...formData, subscription_status: e.target.value as any })}
                                    >
                                        <option value="active">활성 (Active)</option>
                                        <option value="trial">트라이얼 (Trial)</option>
                                        <option value="suspended">정지 (Suspended)</option>
                                        <option value="cancelled">취소 (Cancelled)</option>
                                    </select>
                                </div>
                                <div className="modal-form-group">
                                    <label>계정 상태</label>
                                    <div className="checkbox-group" style={{ height: '38px', display: 'flex', alignItems: 'center' }}>
                                        <label className="checkbox-label" style={{ margin: 0 }}>
                                            <input
                                                type="checkbox"
                                                checked={formData.is_active}
                                                onChange={e => setFormData({ ...formData, is_active: e.target.checked })}
                                            />
                                            계정 활성화
                                        </label>
                                    </div>
                                </div>
                            </div>

                            <div className="modal-form-row" style={{ marginTop: '12px' }}>
                                <div className="modal-form-group">
                                    <label>최대 에지 서버</label>
                                    <input
                                        type="number"
                                        className="form-control"
                                        value={formData.max_edge_servers || ''}
                                        onChange={e => setFormData({ ...formData, max_edge_servers: parseInt(e.target.value) || 0 })}
                                        min="1"
                                        placeholder="1"
                                    />
                                </div>
                                <div className="modal-form-group">
                                    <label>최대 데이터 포인트</label>
                                    <input
                                        type="number"
                                        className="form-control"
                                        value={formData.max_data_points || ''}
                                        onChange={e => setFormData({ ...formData, max_data_points: parseInt(e.target.value) || 0 })}
                                        min="100"
                                        placeholder="100"
                                    />
                                </div>
                            </div>
                        </div>
                    </div>
                </form>
            );
        }

        return (
            <div className="modal-form-grid">
                <div className="modal-form-section">
                    <h3><i className="fas fa-building"></i> 기본 정보</h3>
                    <div className="detail-item">
                        <div className="detail-label">회사명</div>
                        <div className="detail-value highlight">{tenant.company_name}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">회사 코드</div>
                        <div className="detail-value"><code>{tenant.company_code}</code></div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">도메인</div>
                        <div className="detail-value">{tenant.domain || '-'}</div>
                    </div>
                </div>

                <div className="modal-form-section">
                    <h3><i className="fas fa-user-tie"></i> 담당자 정보</h3>
                    <div className="detail-item">
                        <div className="detail-label">이름</div>
                        <div className="detail-value">{tenant.contact_name || '-'}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">연락처</div>
                        <div className="detail-value">
                            {tenant.contact_email && <div><i className="fas fa-envelope"></i> {tenant.contact_email}</div>}
                            {tenant.contact_phone && <div><i className="fas fa-phone"></i> {tenant.contact_phone}</div>}
                            {!tenant.contact_email && !tenant.contact_phone && '-'}
                        </div>
                    </div>
                </div>

                <div className="modal-form-section span-full">
                    <h3><i className="fas fa-chart-pie"></i> 구독 및 사용량</h3>
                    <div className="modal-form-grid-inner">
                        <div className="detail-item">
                            <div className="detail-label">플랜</div>
                            <div className="detail-value">
                                <span className={`badge ${tenant.subscription_plan === 'enterprise' ? 'primary' : 'neutral'}`}>
                                    {tenant.subscription_plan ? tenant.subscription_plan.toUpperCase() : '-'}
                                </span>
                            </div>
                        </div>
                        <div className="detail-item">
                            <div className="detail-label">상태</div>
                            <div className="detail-value">
                                <span className={`badge ${tenant.subscription_status === 'active' ? 'success' : 'warning'}`}>
                                    {tenant.subscription_status ? tenant.subscription_status.toUpperCase() : '-'}
                                </span>
                            </div>
                        </div>
                        <div className="detail-item">
                            <div className="detail-label">Edge 서버</div>
                            <div className="detail-value">
                                <span className="usage-indicator">
                                    <strong>{(tenant as any).edge_servers_count || 0}</strong> / {tenant.max_edge_servers} 대
                                </span>
                            </div>
                        </div>
                        <div className="detail-item">
                            <div className="detail-label">데이터 포인트</div>
                            <div className="detail-value">
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
        <div className="modal-overlay">
            <div className="modal-container tenant-modal">
                <div className="modal-header">
                    <div className="modal-title">
                        <h2>{isEditing ? '고객사 정보 수정' : '고객사 상세 정보'}</h2>
                    </div>
                    <button className="close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="modal-body">
                    {error && <div className="alert alert-danger">{error}</div>}
                    {renderContent()}
                </div>

                <div className="modal-footer">
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
