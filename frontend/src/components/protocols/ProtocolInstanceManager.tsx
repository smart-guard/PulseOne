import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { ProtocolApiService, ProtocolInstance, ApiResponse } from '../../api/services/protocolApi';
import { useConfirmContext } from '../common/ConfirmProvider';
import { TenantApiService } from '../../api/services/tenantApi';

// 간단한 JWT 파싱 유틸리티
const getRoleFromToken = (token: string | null): string | null => {
    try {
        const token = localStorage.getItem('auth_token');
        if (!token) return null;

        // 🔥 개발 환경 더미 토큰 처리
        if (token === 'dev-dummy-token') {
            console.log('Dev Token Detected: Assuming system_admin');
            return 'system_admin';
        }

        const parts = token.split('.');
        if (parts.length !== 3) return null;

        const base64Url = parts[1];
        const base64 = base64Url.replace(/-/g, '+').replace(/_/g, '/');
        const jsonPayload = decodeURIComponent(window.atob(base64).split('').map(function (c) {
            return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
        }).join(''));
        const decoded = JSON.parse(jsonPayload);
        // role이 없을 경우 대비, 혹은 대소문자 이슈 처리
        const role = decoded.role || decoded.ROLE || null;
        return role ? role.toLowerCase() : null;
    } catch (e) {
        console.error('Token parsing failed:', e);
        return null;
    }
};

interface ProtocolInstanceManagerProps {
    protocolId: number;
}

const ProtocolInstanceManager: React.FC<ProtocolInstanceManagerProps> = ({ protocolId }) => {
    const navigate = useNavigate();
    const [instances, setInstances] = useState<ProtocolInstance[]>([]);
    const [pagination, setPagination] = useState({
        current: 1,
        pageSize: 10,
        total: 0
    });
    const [loading, setLoading] = useState(false); // Changed initial state to false
    const [error, setError] = useState<string | null>(null);
    const [isAdding, setIsAdding] = useState(false);
    const [editingInstance, setEditingInstance] = useState<ProtocolInstance | null>(null);

    // 🔥 Multi-Tenant Support
    const [userRole, setUserRole] = useState<string | null>(null);
    const [tenants, setTenants] = useState<any[]>([]);
    const { confirm } = useConfirmContext();

    const [formData, setFormData] = useState({
        instance_name: '',
        description: '',
        vhost: '/',
        is_enabled: true,
        tenant_id: null as number | null,
        broker_type: 'INTERNAL' as 'INTERNAL' | 'EXTERNAL'
    });
    const [originalFormData, setOriginalFormData] = useState<any>(null);

    const fetchInstances = async (page = 1, pageSize = 10) => { // Modified signature
        if (!protocolId) return;
        setLoading(true);
        try {
            const response = await ProtocolApiService.getProtocolInstances(protocolId, page, pageSize); // Added page, pageSize
            if (response.success && response.data) {
                // PaginatedApiResponse uses { items, pagination }
                const list = response.data.items || [];
                const pag = response.data.pagination;

                setInstances(list);
                if (pag) {
                    setPagination({
                        current: pag.current_page,
                        pageSize: pag.limit,
                        total: pag.total_count
                    });
                }
            } else {
                setError(response.message || 'Failed to load instances');
                setInstances([]); // Clear instances on error
            }
        } catch (err: any) {
            setError(err.message);
            console.error('Failed to fetch instances:', err); // Added console.error
            // message.error('인스턴스 목록을 불러오는데 실패했습니다.'); // Removed Ant Design specific message
        } finally {
            setLoading(false);
        }
    };

    // 초기 로딩 시 사용자 역할 확인 및 테넌트 목록 로드
    useEffect(() => {
        const token = localStorage.getItem('auth_token'); // Using 'auth_token' as per original code
        if (token) {
            const role = getRoleFromToken(token); // Pass token to getRoleFromToken
            setUserRole(role);
            if (role === 'system_admin') {
                loadTenants(); // Call loadTenants
            }
        }
        // Initial fetch instances is now handled by a separate useEffect
    }, []); // Empty dependency array for role/tenant loading

    useEffect(() => {
        if (protocolId) {
            fetchInstances(pagination.current, pagination.pageSize);
        }
    }, [protocolId, pagination.current, pagination.pageSize]); // Added pagination dependencies to trigger re-fetch on page/size change

    const loadTenants = async () => {
        try {
            const response = await TenantApiService.getTenants({ limit: 100 });
            if (response.success && response.data) {
                // PaginatedResponse uses 'items'
                setTenants((response.data as any).items || (response.data as any).data || []);
            }
        } catch (e) {
            console.error('Failed to load tenants:', e);
        }
    };

    const handleOpenAdd = () => {
        console.log('User Role:', userRole); // Debugging
        setFormData({
            instance_name: '',
            description: '',
            vhost: '/',
            is_enabled: true,
            tenant_id: null,
            broker_type: 'INTERNAL'
        });
        setIsAdding(true);
        setEditingInstance(null);
    };

    const handleOpenEdit = (instance: ProtocolInstance) => {
        const data = {
            instance_name: instance.instance_name,
            description: instance.description || '',
            vhost: instance.vhost || '/',
            is_enabled: instance.is_enabled,
            tenant_id: (instance as any).tenant_id || null,
            broker_type: instance.broker_type || 'INTERNAL'
        };
        setFormData(data);
        setOriginalFormData(data); // Store original for change detection
        setEditingInstance(instance);
        setIsAdding(false);
    };

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();

        const changes: string[] = [];
        if (editingInstance && originalFormData) {
            if (formData.instance_name !== originalFormData.instance_name) changes.push(`이름: ${originalFormData.instance_name} ➔ ${formData.instance_name}`);
            if (formData.vhost !== originalFormData.vhost) changes.push(`Vhost: ${originalFormData.vhost} ➔ ${formData.vhost}`);
            if (formData.description !== originalFormData.description) changes.push(`설명 수정됨`);
            if (formData.is_enabled !== originalFormData.is_enabled) changes.push(`활성화 상태: ${originalFormData.is_enabled ? 'ON' : 'OFF'} ➔ ${formData.is_enabled ? 'ON' : 'OFF'}`);
            if (formData.tenant_id !== originalFormData.tenant_id) {
                const oldTenant = tenants.find(t => t.id === originalFormData.tenant_id)?.name || 'Shared';
                const newTenant = tenants.find(t => t.id === formData.tenant_id)?.name || 'Shared';
                changes.push(`소유 테넌트: ${oldTenant} ➔ ${newTenant}`);
            }
            if (formData.broker_type !== originalFormData.broker_type) changes.push(`제어 모드: ${originalFormData.broker_type} ➔ ${formData.broker_type}`);

            if (changes.length === 0) {
                await confirm({
                    title: '알림',
                    message: '변경사항이 없습니다.',
                    confirmText: '확인',
                    confirmButtonType: 'primary',
                    showCancelButton: false
                });
                return;
            }

            const ok = await confirm({
                title: '설정 수정 확인',
                message: `다음 변경사항을 저장하시겠습니까?\n\n${changes.map(c => `• ${c}`).join('\n')}`,
                confirmText: '수정 완료',
                confirmButtonType: 'primary'
            });
            if (!ok) return;
        } else {
            // New creation confirmation
            const ok = await confirm({
                title: '인스턴스 추가 확인',
                message: `새로운 인스턴스 [${formData.instance_name}]을(를) 추가하시겠습니까?`,
                confirmText: '추가하기',
                confirmButtonType: 'primary'
            });
            if (!ok) return;
        }

        setLoading(true);
        try {
            let response: ApiResponse<any>;
            if (editingInstance) {
                response = await ProtocolApiService.updateProtocolInstance(editingInstance.id, formData);
            } else {
                response = await ProtocolApiService.createProtocolInstance(protocolId, formData);
            }

            if (response.success) {
                await confirm({
                    title: '완료',
                    message: editingInstance ? '인스턴스 설정이 수정되었습니다.' : '새로운 인스턴스가 추가되었습니다.',
                    confirmText: '확인',
                    confirmButtonType: 'primary',
                    showCancelButton: false
                });
                await fetchInstances(pagination.current, pagination.pageSize);
                setIsAdding(false);
                setEditingInstance(null);
                setOriginalFormData(null);
            } else {
                alert(response.message);
            }
        } catch (err: any) {
            alert(err.message);
        } finally {
            setLoading(false);
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '인스턴스 삭제',
            message: '정말로 이 인스턴스를 삭제하시겠습니까? 관련 연결이 실시간으로 끊어집니다.',
            confirmText: '삭제',
            cancelText: '취소',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            setLoading(true);
            try {
                const response = await ProtocolApiService.deleteProtocolInstance(id);
                if (response.success) {
                    fetchInstances(pagination.current, pagination.pageSize); // Refresh list
                }
            } catch (err: any) {
                alert(err.message);
            } finally {
                setLoading(false);
            }
        }
    };

    // Helper for API Key rendering (from instruction, adapted to current style)
    const renderApiKey = (apiKey: string | undefined) => {
        return (
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <code style={{ background: '#f0f0f0', padding: '2px 6px', borderRadius: '4px' }}>
                    {apiKey ? '••••••••' : 'N/A'}
                </code>
                {apiKey && (
                    <button className="btn-icon" title="Copy Key" style={{ border: 'none', background: 'none', cursor: 'pointer', color: 'var(--neutral-400)' }}>
                        <i className="fas fa-copy"></i>
                    </button>
                )}
            </div>
        );
    };

    // Simple Pagination component (mimicking Ant Design's structure for the instruction)
    const SimplePagination: React.FC<{
        current: number;
        pageSize: number;
        total: number;
        onChange: (page: number, pageSize: number) => void;
        showSizeChanger?: boolean;
        showTotal?: (total: number) => string;
    }> = ({ current, pageSize, total, onChange, showSizeChanger, showTotal }) => {
        const totalPages = Math.ceil(total / pageSize);
        const startItem = (current - 1) * pageSize + 1;
        const endItem = Math.min(current * pageSize, total);

        const handlePageChange = (page: number) => {
            if (page >= 1 && page <= totalPages) {
                onChange(page, pageSize);
            }
        };

        const handlePageSizeChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
            const newSize = parseInt(e.target.value, 10);
            onChange(1, newSize); // Reset to first page when page size changes
        };

        return (
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px', fontSize: '13px' }}>
                {showTotal && <span style={{ color: 'var(--neutral-600)' }}>{showTotal(total)}</span>}
                <button
                    onClick={() => handlePageChange(current - 1)}
                    disabled={current === 1}
                    style={{ padding: '6px 10px', border: '1px solid var(--neutral-300)', borderRadius: '4px', background: 'white', cursor: 'pointer' }}
                >
                    이전
                </button>
                <span style={{ padding: '6px 10px', border: '1px solid var(--neutral-300)', borderRadius: '4px', background: 'var(--primary-50)', color: 'var(--primary-700)', fontWeight: 600 }}>
                    {current} / {totalPages}
                </span>
                <button
                    onClick={() => handlePageChange(current + 1)}
                    disabled={current === totalPages}
                    style={{ padding: '6px 10px', border: '1px solid var(--neutral-300)', borderRadius: '4px', background: 'white', cursor: 'pointer' }}
                >
                    다음
                </button>
                {showSizeChanger && (
                    <select
                        value={pageSize}
                        onChange={handlePageSizeChange}
                        style={{ padding: '6px 10px', border: '1px solid var(--neutral-300)', borderRadius: '4px', background: 'white', cursor: 'pointer' }}
                    >
                        <option value={10}>10 / 페이지</option>
                        <option value={20}>20 / 페이지</option>
                        <option value={50}>50 / 페이지</option>
                    </select>
                )}
            </div>
        );
    };


    return (
        <div className="instance-manager">
            {/* Header Section */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '24px' }}>
                <div>
                    <h3 style={{ fontSize: '18px', fontWeight: 800, margin: 0 }}>브로커 인스턴스 관리</h3>
                    <p style={{ color: 'var(--neutral-500)', fontSize: '13px', margin: '4px 0 0 0' }}>
                        멀티 테넌트 및 격리된 보안 환경을 위한 가상 호스트(vhost)별 연결을 관리합니다.
                    </p>
                </div>
                {!isAdding && !editingInstance && (
                    <button
                        onClick={handleOpenAdd}
                        className="btn-primary"
                        style={{
                            padding: '10px 16px',
                            background: 'var(--primary-600)',
                            color: '#fff',
                            border: 'none',
                            borderRadius: '8px',
                            fontWeight: 600,
                            cursor: 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '8px'
                        }}
                    >
                        <i className="fas fa-plus"></i> 인스턴스 추가
                    </button>
                )}
            </div>

            {/* Editor Section */}
            {(isAdding || editingInstance) ? (
                <div style={{ background: 'var(--neutral-50)', padding: '24px', borderRadius: '12px', marginBottom: '24px', border: '1px solid var(--neutral-200)' }}>
                    {/* Editor Header with Tenant Selector */}
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px' }}>
                        <h4 style={{ margin: 0, fontSize: '16px' }}>
                            {editingInstance ? '인스턴스 설정 수정' : '새 인스턴스 추가'}
                        </h4>

                        {/* 🔥 System Admin Only: Tenant Selector in Header */}
                        {userRole === 'system_admin' && (
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                <label style={{ fontSize: '12px', fontWeight: 700, color: 'var(--primary-700)', display: 'flex', alignItems: 'center', gap: '4px' }}>
                                    <i className="fas fa-building"></i> 소유 테넌트
                                </label>
                                <select
                                    value={formData.tenant_id || ''}
                                    onChange={e => setFormData({ ...formData, tenant_id: e.target.value ? parseInt(e.target.value) : null })}
                                    style={{
                                        padding: '6px 12px',
                                        borderRadius: '6px',
                                        border: '1px solid var(--primary-200)',
                                        fontWeight: 600,
                                        fontSize: '13px',
                                        minWidth: '200px',
                                        background: '#fff'
                                    }}
                                >
                                    <option value="">공용 (Global Shared)</option>
                                    {tenants.map(t => (
                                        <option key={t.id} value={t.id}>{t.name}</option>
                                    ))}
                                </select>
                            </div>
                        )}
                    </div>

                    <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>

                        <div className="form-group">
                            <label style={{ display: 'block', marginBottom: '6px', fontSize: '12px', fontWeight: 700 }}>브로커 연결 모드</label>
                            <div style={{ display: 'flex', gap: '20px', padding: '10px 0' }}>
                                <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', fontSize: '14px' }}>
                                    <input
                                        type="radio"
                                        name="broker_type"
                                        checked={formData.broker_type === 'INTERNAL'}
                                        onChange={() => setFormData({ ...formData, broker_type: 'INTERNAL' })}
                                    />
                                    <strong>Internal</strong> (PulseOne 관리형)
                                </label>
                                <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', fontSize: '14px' }}>
                                    <input
                                        type="radio"
                                        name="broker_type"
                                        checked={formData.broker_type === 'EXTERNAL'}
                                        onChange={() => setFormData({ ...formData, broker_type: 'EXTERNAL' })}
                                    />
                                    <strong>External</strong> (외부 표준 브로커)
                                </label>
                            </div>
                            <div style={{ fontSize: '12px', color: 'var(--neutral-500)', marginTop: '-8px' }}>
                                {formData.broker_type === 'INTERNAL'
                                    ? 'PulseOne 내부 RabbitMQ 브로커를 사용하며 Vhost와 API Key로 격리됩니다.'
                                    : 'AWS IoT, Azure EventGrid 또는 타사 MQTT 브로커와 직접 연동합니다.'}
                            </div>
                        </div>

                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                            <div className="form-group">
                                <label style={{ display: 'block', marginBottom: '6px', fontSize: '12px', fontWeight: 700 }}>인스턴스 이름</label>
                                <input
                                    type="text"
                                    value={formData.instance_name}
                                    onChange={e => setFormData({ ...formData, instance_name: e.target.value })}
                                    placeholder="예: 고객사-A-전용"
                                    required
                                    style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)' }}
                                />
                            </div>

                            {formData.broker_type === 'INTERNAL' && (
                                <div className="form-group">
                                    <label style={{ display: 'block', marginBottom: '6px', fontSize: '12px', fontWeight: 700 }}>Vhost (Virtual Host)</label>
                                    <input
                                        type="text"
                                        value={formData.vhost}
                                        onChange={e => setFormData({ ...formData, vhost: e.target.value })}
                                        placeholder="예: /client-a"
                                        required
                                        style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)' }}
                                    />
                                </div>
                            )}
                        </div>

                        <div className="form-group">
                            <label style={{ display: 'block', marginBottom: '6px', fontSize: '12px', fontWeight: 700 }}>설명</label>
                            <textarea
                                value={formData.description}
                                onChange={e => setFormData({ ...formData, description: e.target.value })}
                                placeholder="인스턴스 용도 및 테넌트 식별 정보"
                                style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)', minHeight: '80px', resize: 'vertical' }}
                            />
                        </div>

                        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                            <input
                                type="checkbox"
                                id="is_enabled"
                                checked={formData.is_enabled}
                                onChange={e => setFormData({ ...formData, is_enabled: e.target.checked })}
                            />
                            <label htmlFor="is_enabled" style={{ fontSize: '14px' }}>인스턴스 활성화 (즉시 연결 시도)</label>
                        </div>
                        <div style={{ display: 'flex', gap: '12px', marginTop: '8px' }}>
                            <button
                                type="submit"
                                className="btn-save"
                                disabled={loading}
                                style={{ padding: '10px 24px', background: 'var(--primary-600)', color: '#fff', border: 'none', borderRadius: '8px', fontWeight: 700, cursor: 'pointer' }}
                            >
                                {loading ? '저장 중...' : '설정 저장'}
                            </button>
                            <button
                                type="button"
                                onClick={() => { setIsAdding(false); setEditingInstance(null); }}
                                style={{ padding: '10px 24px', background: 'transparent', color: 'var(--neutral-600)', border: '1px solid var(--neutral-300)', borderRadius: '8px', fontWeight: 600, cursor: 'pointer' }}
                            >
                                취소
                            </button>
                        </div>
                    </form>
                </div>
            ) : (
                loading && instances.length === 0 ? (
                    <div style={{ textAlign: 'center', padding: '40px' }}>
                        <i className="fas fa-spinner fa-spin" style={{ marginRight: '8px' }}></i> 로딩 중...
                    </div>
                ) : instances.length === 0 ? (
                    <div style={{ padding: '40px', textAlign: 'center', color: 'var(--neutral-400)', background: 'var(--neutral-50)', borderRadius: '12px', border: '1px dashed var(--neutral-300)' }}>
                        <i className="fas fa-info-circle" style={{ display: 'block', fontSize: '24px', marginBottom: '12px' }}></i>
                        등록된 인스턴스가 없습니다. '인스턴스 추가' 버튼을 눌러 새 연결을 만드세요.
                    </div>
                ) : (
                    <>
                        <div className="instance-list-table" style={{ border: '1px solid var(--neutral-200)', borderRadius: '12px', overflow: 'hidden', marginBottom: '20px' }}>
                            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '14px' }}>
                                <thead style={{ background: 'var(--neutral-50)', borderBottom: '1px solid var(--neutral-200)' }}>
                                    <tr>
                                        <th style={{ padding: '12px 16px', textAlign: 'left', fontWeight: 700 }}>상태</th>
                                        <th style={{ padding: '12px 16px', textAlign: 'left', fontWeight: 700 }}>모드</th>
                                        <th style={{ padding: '12px 16px', textAlign: 'left', fontWeight: 700 }}>인스턴스명</th>
                                        <th style={{ padding: '12px 16px', textAlign: 'left', fontWeight: 700 }}>소유 테넌트</th>
                                        <th style={{ padding: '12px 16px', textAlign: 'left', fontWeight: 700 }}>API Key</th>
                                        <th style={{ padding: '12px 16px', textAlign: 'right', fontWeight: 700 }}>작업</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {instances.map((instance) => (
                                        <tr key={instance.id} style={{ borderBottom: '1px solid var(--neutral-100)' }}>
                                            <td style={{ padding: '12px 16px' }}>
                                                <span style={{
                                                    display: 'inline-flex',
                                                    alignItems: 'center',
                                                    gap: '6px',
                                                    padding: '4px 8px',
                                                    borderRadius: '20px',
                                                    fontSize: '12px',
                                                    fontWeight: 600,
                                                    background: instance.is_enabled ? 'var(--success-50)' : 'var(--neutral-100)',
                                                    color: instance.is_enabled ? 'var(--success-700)' : 'var(--neutral-600)'
                                                }}>
                                                    <span style={{ width: '6px', height: '6px', borderRadius: '50%', background: instance.is_enabled ? 'var(--success-500)' : 'var(--neutral-400)' }}></span>
                                                    {instance.is_enabled ? 'Active' : 'Disabled'}
                                                </span>
                                            </td>
                                            <td style={{ padding: '12px 16px' }}>
                                                <span style={{
                                                    fontSize: '11px',
                                                    padding: '2px 8px',
                                                    borderRadius: '12px',
                                                    background: instance.broker_type === 'EXTERNAL' ? 'var(--warning-50)' : 'var(--neutral-100)',
                                                    color: instance.broker_type === 'EXTERNAL' ? 'var(--warning-700)' : 'var(--neutral-500)',
                                                    fontWeight: 800
                                                }}>
                                                    {instance.broker_type || 'INTERNAL'}
                                                </span>
                                            </td>
                                            <td style={{ padding: '12px 16px' }}>
                                                <div style={{ fontWeight: 600 }}>{instance.instance_name}</div>
                                                {instance.broker_type !== 'EXTERNAL' ? (
                                                    <div style={{ fontSize: '11px', color: 'var(--neutral-400)', marginTop: '2px' }}>
                                                        <code>{instance.vhost}</code>
                                                    </div>
                                                ) : (
                                                    <div style={{ fontSize: '11px', color: 'var(--warning-600)', marginTop: '2px' }}>
                                                        <i className="fas fa-globe" style={{ fontSize: '10px' }}></i> External Broker
                                                    </div>
                                                )}
                                            </td>
                                            <td style={{ padding: '12px 16px' }}>
                                                <span style={{
                                                    fontSize: '11px',
                                                    padding: '2px 8px',
                                                    borderRadius: '12px',
                                                    background: (instance as any).tenant_name ? 'var(--primary-50)' : 'var(--neutral-100)',
                                                    color: (instance as any).tenant_name ? 'var(--primary-700)' : 'var(--neutral-500)',
                                                    fontWeight: 600
                                                }}>
                                                    {(instance as any).tenant_name || 'Shared (Global)'}
                                                </span>
                                            </td>
                                            <td style={{ padding: '12px 16px' }}>
                                                {renderApiKey(instance.api_key)}
                                            </td>
                                            <td style={{ padding: '12px 16px', textAlign: 'right' }}>
                                                <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '8px' }}>
                                                    <button
                                                        onClick={() => navigate(`/devices?protocolId=${protocolId}&instanceId=${instance.id}`)}
                                                        style={{ border: 'none', background: 'none', cursor: 'pointer', color: 'var(--primary-600)', padding: '4px' }}
                                                        title="이 인스턴스의 장치 보기"
                                                    >
                                                        <i className="fas fa-external-link-alt"></i>
                                                    </button>
                                                    <button onClick={() => handleOpenEdit(instance)} style={{ border: 'none', background: 'none', cursor: 'pointer', color: 'var(--primary-600)', padding: '4px' }}>
                                                        <i className="fas fa-edit"></i>
                                                    </button>
                                                    <button onClick={() => handleDelete(instance.id)} style={{ border: 'none', background: 'none', cursor: 'pointer', color: 'var(--error-600)', padding: '4px' }}>
                                                        <i className="fas fa-trash-alt"></i>
                                                    </button>
                                                </div>
                                            </td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>
                        {/* Pagination Section */}
                        <div style={{ display: 'flex', justifyContent: 'flex-end', paddingBottom: '40px' }}>
                            <SimplePagination
                                current={pagination.current}
                                pageSize={pagination.pageSize}
                                total={pagination.total}
                                onChange={(page, pageSize) => fetchInstances(page, pageSize)}
                                showSizeChanger
                                showTotal={(total) => `Total ${total} items`}
                            />
                        </div>
                    </>
                )
            )}
        </div>
    );

};

export default ProtocolInstanceManager;
