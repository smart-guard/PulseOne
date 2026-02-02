
import React, { useState, useEffect, useCallback } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { ProtocolApiService, Protocol, ProtocolInstance } from '../../api/services/protocolApi';
import { StatCard } from '../common/StatCard';
import { useConfirmContext } from '../common/ConfirmProvider';
import MqttSecuritySlot from './mqtt/MqttSecuritySlot';
import ProtocolInstanceManager from './ProtocolInstanceManager';

interface ProtocolDashboardProps {
    protocol: Protocol;
    onRefresh: () => void;
}

const ProtocolDashboard: React.FC<ProtocolDashboardProps> = ({ protocol, onRefresh }) => {
    const { type: protocolType, id: protocolId, tab: urlTab } = useParams();
    const navigate = useNavigate();
    const { confirm } = useConfirmContext();

    // 탭 상태 (URL과 동기화)
    const activeTab = urlTab || 'info';
    const [loading, setLoading] = useState(false);
    const [instances, setInstances] = useState<ProtocolInstance[]>([]);
    const [selectedInstanceId, setSelectedInstanceId] = useState<number | null>(null);
    const [devices, setDevices] = useState<any[]>([]);

    // 편집 모드 상태
    const [isEditing, setIsEditing] = useState(false);
    const [editData, setEditData] = useState<Partial<Protocol>>({});

    // 인스턴스 목록 조회
    const fetchInstances = useCallback(async () => {
        try {
            setLoading(true);
            const response = await ProtocolApiService.getProtocolInstances(protocol.id);
            if (response.success && response.data) {
                // Paginated Response handles { items, pagination }
                const responseData = response.data as any;
                const list = responseData.items || (Array.isArray(responseData) ? responseData : []);
                setInstances(list);

                // 선택된 인스턴스가 없거나 목록에 없으면 첫 번째 인스턴스 선택
                if (list.length > 0 && (!selectedInstanceId || !list.find((i: any) => i.id === selectedInstanceId))) {
                    setSelectedInstanceId(list[0].id);
                }
            }
        } catch (err) {
            console.error('Failed to fetch instances:', err);
        } finally {
            setLoading(false);
        }
    }, [protocol.id, selectedInstanceId]);

    // 디바이스 목록 조회
    const fetchDevices = useCallback(async () => {
        try {
            setLoading(true);
            const response = await ProtocolApiService.getProtocolDevices(protocol.id);
            if (response.success) {
                setDevices(response.data || []);
            }
        } catch (err) {
            console.error('Failed to fetch devices:', err);
        } finally {
            setLoading(false);
        }
    }, [protocol.id]);

    useEffect(() => {
        if (activeTab === 'security' || activeTab === 'instances') {
            fetchInstances();
        }
        if (activeTab === 'devices' || activeTab === 'monitoring' || activeTab === 'info') {
            fetchDevices();
        }
    }, [activeTab, fetchInstances, fetchDevices]);

    const handleTabChange = (tab: string) => {
        navigate(`/protocols/${protocolType}/${protocolId}/${tab}`);
    };

    // 공통: 프로토콜 업데이트 로직
    const handleUpdateProtocol = async (updatedData: any) => {
        try {
            setLoading(true);
            const response = await ProtocolApiService.updateProtocol(protocol.id, updatedData);
            if (response.success) {
                onRefresh();
            }
        } catch (err) {
            console.error('Protocol update failed:', err);
        } finally {
            setLoading(false);
        }
    };

    const selectedInstance = instances.find(i => i.id === selectedInstanceId);

    // 변경된 필드 추출
    const getChangedFields = () => {
        const changes: string[] = [];
        if (editData.display_name !== protocol.display_name) changes.push(`프로토콜 명칭: ${protocol.display_name} -> ${editData.display_name}`);
        if (editData.default_port !== protocol.default_port) changes.push(`기본 포트: ${protocol.default_port} -> ${editData.default_port}`);
        if (editData.uses_serial !== protocol.uses_serial) changes.push(`시리얼 지원: ${protocol.uses_serial ? '지원' : '미지원'} -> ${editData.uses_serial ? '지원' : '미지원'}`);
        if (editData.requires_broker !== protocol.requires_broker) changes.push(`브로커 필수: ${protocol.requires_broker ? '필수' : '불필요'} -> ${editData.requires_broker ? '필수' : '불필요'}`);
        if (editData.default_polling_interval !== protocol.default_polling_interval) changes.push(`폴링 주기: ${protocol.default_polling_interval}ms -> ${editData.default_polling_interval}ms`);
        if (editData.category !== protocol.category) changes.push(`카테고리: ${protocol.category} -> ${editData.category}`);
        if (JSON.stringify(editData.supported_data_types) !== JSON.stringify(protocol.supported_data_types)) changes.push('지원 데이터 타입 변경');
        if (JSON.stringify(editData.supported_operations) !== JSON.stringify(protocol.supported_operations)) changes.push('프로토콜 기능 변경');
        if (editData.description !== protocol.description) changes.push('상세 설명 내용 변경');
        return changes;
    };

    return (
        <div className="protocol-dashboard" style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '24px',
            animation: 'fadeIn 0.5s ease-out',
            padding: '24px', // 🔥 상하좌우 여백을 주어 보더가 잘리지 않도록 함
            paddingBottom: '60px',
            boxSizing: 'border-box'
        }}>
            {/* 1. 상단 요약 영역 (공통) */}
            <div className="dashboard-summary" style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '16px' }}>
                <StatCard
                    label="연결 상태"
                    value={protocol.is_enabled ? '정상' : '비활성'}
                    type={protocol.is_enabled ? 'success' : 'neutral'}
                    icon={protocol.is_enabled ? 'fas fa-check-circle' : 'fas fa-pause-circle'}
                />
                {protocol.requires_broker && (
                    <StatCard
                        label="연결된 인스턴스"
                        value={instances.length}
                        type="primary"
                        icon="fas fa-server"
                    />
                )}
                <StatCard
                    label="연결된 장치"
                    value={protocol.device_count || 0}
                    type="primary"
                    icon="fas fa-microchip"
                />
                <StatCard
                    label="오류로그(24h)"
                    value="0"
                    type="error"
                    icon="fas fa-exclamation-circle"
                />
            </div>

            {/* 2. 메인 탭 영역 */}
            <div className="dashboard-main-card" style={{
                background: '#fff',
                borderRadius: '16px',
                border: '1px solid var(--neutral-200)',
                boxShadow: '0 4px 20px rgba(0,0,0,0.05)',
                display: 'flex',
                flexDirection: 'column',
                minHeight: '500px',
                marginBottom: '60px', // 🔥 하단 보더라인 및 그림자 공간 충분히 확보
                overflow: 'visible'    // 🔥 그림자가 잘리지 않도록 허용
            }}>
                {/* 탭 헤더 */}
                <div className="dashboard-tabs" style={{
                    display: 'flex',
                    padding: '0 20px',
                    borderBottom: '1px solid var(--neutral-100)',
                    background: 'var(--neutral-50)',
                    gap: '8px'
                }}>
                    {[
                        { id: 'info', label: '프로토콜 정보', icon: 'fas fa-info-circle' },
                        { id: 'monitoring', label: '실시간 상태', icon: 'fas fa-desktop' },
                        // 브로커(vhost) 격리가 필요한 경우에만 인스턴스 및 보안 탭 노출
                        ...(protocol.requires_broker ? [
                            { id: 'instances', label: '인스턴스', icon: 'fas fa-server' },
                            { id: 'security', label: '보안/인증', icon: 'fas fa-shield-alt' }
                        ] : []),
                        { id: 'devices', label: '장치 목록', icon: 'fas fa-list' }
                    ].map(tab => (
                        <button
                            key={tab.id}
                            onClick={() => handleTabChange(tab.id)}
                            style={{
                                padding: '16px 20px',
                                border: 'none',
                                background: 'none',
                                fontSize: '14px',
                                fontWeight: activeTab === tab.id ? 800 : 500,
                                color: activeTab === tab.id ? 'var(--primary-600)' : 'var(--neutral-500)',
                                borderBottom: activeTab === tab.id ? '3px solid var(--primary-500)' : '3px solid transparent',
                                cursor: 'pointer',
                                transition: 'all 0.2s ease',
                                display: 'flex',
                                alignItems: 'center',
                                gap: '8px'
                            }}
                        >
                            <i className={tab.icon} style={{ fontSize: '13px', opacity: activeTab === tab.id ? 1 : 0.6 }}></i>
                            {tab.label}
                        </button>
                    ))}
                </div>

                {/* 탭 컨텐츠 */}
                <div className="dashboard-content" style={{ flex: 1, padding: '24px', paddingBottom: '40px', overflowY: 'auto', boxSizing: 'border-box' }}>
                    {activeTab === 'monitoring' && (
                        <div className="monitoring-content" style={{ animation: 'fadeIn 0.3s ease-in' }}>
                            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(300px, 1fr))', gap: '20px' }}>
                                {devices.length > 0 ? (
                                    devices.map(device => (
                                        <div key={device.id} style={{
                                            background: '#fff',
                                            borderRadius: '12px',
                                            padding: '20px',
                                            border: '1px solid var(--neutral-200)',
                                            display: 'flex',
                                            flexDirection: 'column',
                                            gap: '12px'
                                        }}>
                                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                                                <div>
                                                    <h4 style={{ margin: 0, fontSize: '15px', fontWeight: 800 }}>{device.name}</h4>
                                                    <span style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>{device.endpoint}</span>
                                                </div>
                                                <span style={{
                                                    padding: '4px 8px',
                                                    borderRadius: '6px',
                                                    fontSize: '10px',
                                                    fontWeight: 800,
                                                    background: device.is_enabled ? 'var(--success-50)' : 'var(--neutral-100)',
                                                    color: device.is_enabled ? 'var(--success-600)' : 'var(--neutral-500)'
                                                }}>
                                                    {device.is_enabled ? 'ACTIVE' : 'DISABLED'}
                                                </span>
                                            </div>
                                            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                                                <span style={{ fontSize: '11px', padding: '2px 6px', background: 'var(--neutral-50)', borderRadius: '4px', border: '1px solid var(--neutral-100)' }}>
                                                    Type: {device.device_type}
                                                </span>
                                                {device.last_communication && (
                                                    <span style={{ fontSize: '11px', padding: '2px 6px', background: 'var(--primary-50)', color: 'var(--primary-600)', borderRadius: '4px' }}>
                                                        <i className="fas fa-clock" style={{ marginRight: '4px' }}></i>
                                                        Last: {new Date(device.last_communication).toLocaleTimeString()}
                                                    </span>
                                                )}
                                            </div>
                                        </div>
                                    ))
                                ) : (
                                    <div style={{ gridColumn: '1/-1', textAlign: 'center', padding: '100px', color: 'var(--neutral-400)' }}>
                                        <i className="fas fa-chart-area" style={{ fontSize: '48px', marginBottom: '16px', opacity: 0.2 }}></i>
                                        <p>실시간 모니터링할 장치가 없습니다. 프로토콜을 사용하는 장치를 먼저 등록해주세요.</p>
                                    </div>
                                )}
                            </div>
                        </div>
                    )}

                    {activeTab === 'devices' && (
                        <div className="devices-content" style={{ animation: 'fadeIn 0.3s ease-in' }}>
                            <div style={{ marginBottom: '20px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                <h3 style={{ fontSize: '16px', fontWeight: 800, margin: 0 }}>연결된 장치 목록 ({devices.length})</h3>
                                <button
                                    className="mgmt-btn mgmt-btn-outline primary"
                                    style={{ padding: '4px 12px', fontSize: '12px', height: '32px' }}
                                    onClick={() => navigate(`/devices?protocolId=${protocol.id}${selectedInstanceId ? `&instanceId=${selectedInstanceId}` : ''}`)}
                                >
                                    <i className="fas fa-external-link-alt" style={{ marginRight: '6px' }}></i>
                                    장치 관리에서 보기
                                </button>
                            </div>

                            <div style={{ background: '#fff', borderRadius: '12px', border: '1px solid var(--neutral-200)', overflow: 'hidden' }}>
                                <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '13px' }}>
                                    <thead>
                                        <tr style={{ background: 'var(--neutral-50)', borderBottom: '1px solid var(--neutral-100)' }}>
                                            <th style={{ padding: '12px 20px', textAlign: 'left', fontWeight: 800, color: 'var(--neutral-600)', width: '60px' }}>ID</th>
                                            <th style={{ padding: '12px 20px', textAlign: 'left', fontWeight: 800, color: 'var(--neutral-600)' }}>장치명</th>
                                            <th style={{ padding: '12px 20px', textAlign: 'left', fontWeight: 800, color: 'var(--neutral-600)' }}>엔드포인트</th>
                                            <th style={{ padding: '12px 20px', textAlign: 'left', fontWeight: 800, color: 'var(--neutral-600)' }}>타입</th>
                                            <th style={{ padding: '12px 20px', textAlign: 'center', fontWeight: 800, color: 'var(--neutral-600)', width: '100px' }}>상태</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {devices.length > 0 ? (
                                            devices.map(device => (
                                                <tr key={device.id} style={{ borderBottom: '1px solid var(--neutral-50)' }}>
                                                    <td style={{ padding: '12px 20px', color: 'var(--neutral-400)' }}>{device.id}</td>
                                                    <td style={{ padding: '12px 20px', fontWeight: 700, color: 'var(--neutral-800)' }}>{device.name}</td>
                                                    <td style={{ padding: '12px 20px', color: 'var(--neutral-600)', fontFamily: 'monospace' }}>{device.endpoint}</td>
                                                    <td style={{ padding: '12px 20px' }}>
                                                        <span style={{ padding: '2px 8px', background: 'var(--neutral-100)', borderRadius: '4px', fontSize: '11px', fontWeight: 600 }}>
                                                            {device.device_type}
                                                        </span>
                                                    </td>
                                                    <td style={{ padding: '12px 20px', textAlign: 'center' }}>
                                                        <span style={{
                                                            display: 'inline-block',
                                                            width: '8px',
                                                            height: '8px',
                                                            borderRadius: '50%',
                                                            background: device.is_enabled ? 'var(--success-500)' : 'var(--neutral-300)',
                                                            marginRight: '8px'
                                                        }}></span>
                                                        <span style={{ fontSize: '12px', fontWeight: 700, color: device.is_enabled ? 'var(--success-700)' : 'var(--neutral-500)' }}>
                                                            {device.is_enabled ? 'Active' : 'Disabled'}
                                                        </span>
                                                    </td>
                                                </tr>
                                            ))
                                        ) : (
                                            <tr>
                                                <td colSpan={5} style={{ padding: '60px', textAlign: 'center', color: 'var(--neutral-400)' }}>
                                                    연결된 장치가 없습니다.
                                                </td>
                                            </tr>
                                        )}
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    )}

                    {activeTab === 'instances' && (
                        <ProtocolInstanceManager protocolId={protocol.id} />
                    )}

                    {activeTab === 'info' && (
                        <div className="protocol-info-tab" style={{ animation: 'fadeIn 0.3s ease-in' }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '24px', borderBottom: '2px solid var(--neutral-100)', paddingBottom: '12px' }}>
                                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                    {isEditing ? (
                                        <input
                                            type="text"
                                            value={editData.display_name || ''}
                                            onChange={(e) => setEditData({ ...editData, display_name: e.target.value })}
                                            className="inline-edit-input"
                                            style={{
                                                fontSize: '18px',
                                                fontWeight: 800,
                                                minWidth: '350px',
                                                background: 'var(--primary-50)',
                                                borderColor: 'var(--primary-300)'
                                            }}
                                            autoFocus
                                        />
                                    ) : (
                                        <h3 style={{ fontSize: '18px', fontWeight: 800, margin: 0 }}>{protocol.display_name} 상세 스펙</h3>
                                    )}
                                </div>
                                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                    <span style={{ fontSize: '12px', color: 'var(--neutral-400)', fontWeight: 600 }}>Protocol ID: {protocol.id}</span>
                                    {isEditing ? (
                                        <div style={{ display: 'flex', gap: '8px' }}>
                                            <button
                                                className="mgmt-btn mgmt-btn-sm mgmt-btn-outline"
                                                onClick={() => setIsEditing(false)}
                                                disabled={loading}
                                            >
                                                취소
                                            </button>
                                            <button
                                                className="mgmt-btn mgmt-btn-sm mgmt-btn-primary"
                                                onClick={async () => {
                                                    const changes = getChangedFields();

                                                    if (changes.length === 0) {
                                                        await confirm({
                                                            title: '변경사항 없음',
                                                            message: '수정된 내용이 없습니다.',
                                                            confirmText: '확인',
                                                            confirmButtonType: 'primary'
                                                        });
                                                        setIsEditing(false);
                                                        return;
                                                    }

                                                    const ok = await confirm({
                                                        title: '설정 저장',
                                                        message: (
                                                            <div>
                                                                <p style={{ marginBottom: '12px' }}>다음 내역을 저장하시겠습니까?</p>
                                                                <ul style={{
                                                                    fontSize: '13px',
                                                                    color: 'var(--neutral-600)',
                                                                    background: 'var(--neutral-50)',
                                                                    padding: '12px 12px 12px 28px',
                                                                    borderRadius: '8px',
                                                                    border: '1px solid var(--neutral-100)'
                                                                }}>
                                                                    {changes.map((c, i) => <li key={i} style={{ marginBottom: '4px' }}>{c}</li>)}
                                                                </ul>
                                                            </div>
                                                        ) as any,
                                                        confirmText: '저장',
                                                        confirmButtonType: 'primary'
                                                    });
                                                    if (ok) {
                                                        await handleUpdateProtocol(editData);
                                                        setIsEditing(false);
                                                    }
                                                }}
                                                disabled={loading}
                                            >
                                                <i className="fas fa-save" style={{ marginRight: '6px' }}></i>
                                                저장
                                            </button>
                                        </div>
                                    ) : (
                                        <button
                                            className="mgmt-btn mgmt-btn-sm mgmt-btn-outline"
                                            onClick={() => {
                                                setEditData({ ...protocol });
                                                setIsEditing(true);
                                            }}
                                        >
                                            <i className="fas fa-edit" style={{ marginRight: '6px' }}></i>
                                            편집
                                        </button>
                                    )}
                                </div>
                            </div>

                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '32px' }}>
                                {/* Left: Basic Specs */}
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
                                    <section>
                                        <h4 style={{ fontSize: '14px', fontWeight: 800, color: 'var(--neutral-800)', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <i className="fas fa-info-circle" style={{ color: 'var(--primary-500)' }}></i> 기본 사양
                                        </h4>
                                        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '13px' }}>
                                            <tbody>
                                                {[
                                                    { label: '내부 식별자', value: protocol.protocol_type, isReadOnly: true },
                                                    {
                                                        label: '기본 포트',
                                                        value: protocol.default_port || 'N/A',
                                                        field: 'default_port',
                                                        type: 'number'
                                                    },
                                                    {
                                                        label: '시리얼 지원',
                                                        value: protocol.uses_serial ? '지원' : '미지원',
                                                        field: 'uses_serial',
                                                        type: 'checkbox'
                                                    },
                                                    {
                                                        label: '브로커(vhost)',
                                                        value: protocol.requires_broker ? '필수' : '불필요',
                                                        field: 'requires_broker',
                                                        type: 'checkbox'
                                                    },
                                                    {
                                                        label: '기본 Polling',
                                                        value: `${protocol.default_polling_interval || 1000} ms`,
                                                        field: 'default_polling_interval',
                                                        type: 'number',
                                                        unit: 'ms'
                                                    }
                                                ].map((row, idx) => (
                                                    <tr key={idx} style={{ borderBottom: '1px solid var(--neutral-50)' }}>
                                                        <td style={{ padding: '10px 0', color: 'var(--neutral-500)', width: '120px', fontWeight: 600 }}>{row.label}</td>
                                                        <td style={{ padding: '10px 0', color: 'var(--neutral-900)', fontWeight: 700 }}>
                                                            {isEditing && !row.isReadOnly ? (
                                                                row.type === 'checkbox' ? (
                                                                    <label className="mgmt-switch" style={{ margin: 0 }}>
                                                                        <input
                                                                            type="checkbox"
                                                                            checked={!!(editData as any)[row.field!]}
                                                                            onChange={(e) => setEditData({ ...editData, [row.field!]: e.target.checked })}
                                                                        />
                                                                        <span className="mgmt-slider round"></span>
                                                                    </label>
                                                                ) : (
                                                                    <div className="inline-edit-container">
                                                                        <input
                                                                            type={row.type}
                                                                            value={(editData as any)[row.field!] || ''}
                                                                            onChange={(e) => setEditData({ ...editData, [row.field!]: row.type === 'number' ? parseInt(e.target.value) : e.target.value })}
                                                                            className="inline-edit-input"
                                                                            style={{ width: '120px' }}
                                                                        />
                                                                        {row.unit && <span style={{ color: 'var(--neutral-400)', fontSize: '12px' }}>{row.unit}</span>}
                                                                    </div>
                                                                )
                                                            ) : (
                                                                row.value
                                                            )}
                                                        </td>
                                                    </tr>
                                                ))}
                                                {isEditing && (
                                                    <tr style={{ borderBottom: '1px solid var(--neutral-50)' }}>
                                                        <td style={{ padding: '10px 0', color: 'var(--neutral-500)', width: '120px', fontWeight: 600 }}>카테고리</td>
                                                        <td style={{ padding: '10px 0', color: 'var(--neutral-900)', fontWeight: 700 }}>
                                                            <select
                                                                value={editData.category || ''}
                                                                onChange={(e) => setEditData({ ...editData, category: e.target.value })}
                                                                className="inline-edit-select"
                                                                style={{ width: '200px' }}
                                                            >
                                                                <option value="industrial">산업용</option>
                                                                <option value="iot">IoT</option>
                                                                <option value="building_automation">빌딩 자동화</option>
                                                                <option value="network">네트워크</option>
                                                                <option value="web">웹</option>
                                                            </select>
                                                        </td>
                                                    </tr>
                                                )}
                                            </tbody>
                                        </table>
                                    </section>

                                    <section>
                                        <h4 style={{ fontSize: '14px', fontWeight: 800, color: 'var(--neutral-800)', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <i className="fas fa-database" style={{ color: 'var(--success-500)' }}></i> 지원 데이터 타입
                                        </h4>
                                        <div className="inline-edit-chip-container">
                                            {isEditing ? (
                                                <div className="inline-edit-chip-input-wrapper">
                                                    <input
                                                        type="text"
                                                        value={editData.supported_data_types?.join(', ') || ''}
                                                        onChange={(e) => setEditData({ ...editData, supported_data_types: e.target.value.split(',').map(s => s.trim()).filter(s => s) })}
                                                        className="inline-edit-chip-input"
                                                        placeholder="쉼표로 구분 예: BOOL, INT16, FLOAT32"
                                                    />
                                                    <i className="fas fa-tags"></i>
                                                </div>
                                            ) : (
                                                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
                                                    {protocol.supported_data_types?.map(type => (
                                                        <span key={type} style={{ padding: '4px 10px', background: 'var(--success-50)', color: 'var(--success-700)', borderRadius: '6px', fontSize: '12px', fontWeight: 800, border: '1px solid var(--success-100)' }}>
                                                            {type.toUpperCase()}
                                                        </span>
                                                    ))}
                                                </div>
                                            )}
                                        </div>
                                    </section>
                                </div>

                                {/* Right: Operations & Description */}
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
                                    <section>
                                        <h4 style={{ fontSize: '14px', fontWeight: 800, color: 'var(--neutral-800)', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <i className="fas fa-cogs" style={{ color: 'var(--warning-500)' }}></i> 프로토콜 기능 (Operations)
                                        </h4>
                                        <div className="inline-edit-chip-container">
                                            {isEditing ? (
                                                <div className="inline-edit-chip-input-wrapper">
                                                    <input
                                                        type="text"
                                                        value={editData.supported_operations?.join(', ') || ''}
                                                        onChange={(e) => setEditData({ ...editData, supported_operations: e.target.value.split(',').map(s => s.trim()).filter(s => s) })}
                                                        className="inline-edit-chip-input"
                                                        placeholder="쉼표로 구분 예: read, write, scan"
                                                    />
                                                    <i className="fas fa-microchip"></i>
                                                </div>
                                            ) : (
                                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                                                    {protocol.supported_operations?.map(op => (
                                                        <div key={op} style={{ padding: '8px 12px', background: 'var(--neutral-50)', color: 'var(--neutral-700)', borderRadius: '8px', fontSize: '12px', fontWeight: 600, border: '1px solid var(--neutral-200)', display: 'flex', alignItems: 'center', gap: '6px' }}>
                                                            <i className="fas fa-check-circle" style={{ color: 'var(--success-500)', fontSize: '10px' }}></i>
                                                            {op.replace(/_/g, ' ')}
                                                        </div>
                                                    ))}
                                                </div>
                                            )}
                                        </div>
                                    </section>

                                    <section style={{ flex: 1 }}>
                                        <h4 style={{ fontSize: '14px', fontWeight: 800, color: 'var(--neutral-800)', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <i className="fas fa-align-left" style={{ color: 'var(--neutral-400)' }}></i> 프로토콜 설명
                                        </h4>
                                        {isEditing ? (
                                            <textarea
                                                value={editData.description || ''}
                                                onChange={(e) => setEditData({ ...editData, description: e.target.value })}
                                                className="inline-edit-textarea"
                                                placeholder="프로토콜에 대한 상세 설명을 입력하세요..."
                                            />
                                        ) : (
                                            <div style={{ padding: '16px', background: 'var(--neutral-50)', borderRadius: '12px', color: 'var(--neutral-600)', fontSize: '13px', lineHeight: '1.6', border: '1px solid var(--neutral-100)' }}>
                                                {protocol.description || '이 프로토콜에 대한 추가 설명이 없습니다.'}
                                            </div>
                                        )}
                                    </section>
                                </div>
                            </div>

                            {/* Raw Data (Foldable/Optional - Keep for super technical needs but hide by default) */}
                            <details style={{ marginTop: '32px', borderTop: '1px dashed var(--neutral-200)', paddingTop: '16px' }}>
                                <summary style={{ fontSize: '12px', color: 'var(--neutral-400)', cursor: 'pointer', fontWeight: 700 }}>Raw Protocol Meta-JSON</summary>
                                <pre style={{ background: 'var(--neutral-50)', padding: '16px', borderRadius: '12px', fontSize: '11px', marginTop: '12px', overflowX: 'auto' }}>
                                    {JSON.stringify(protocol, null, 2)}
                                </pre>
                            </details>
                        </div>
                    )}

                    {activeTab === 'security' && (
                        <div className="security-content" style={{ animation: 'fadeIn 0.3s ease-in' }}>
                            {instances.length > 0 ? (
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                                    {/* Consolidated Header Row */}
                                    <div style={{
                                        display: 'flex',
                                        alignItems: 'center',
                                        justifyContent: 'space-between',
                                        background: 'var(--neutral-50)',
                                        padding: '12px 20px',
                                        borderRadius: '12px',
                                        border: '1px solid var(--neutral-200)',
                                        flexWrap: 'wrap',
                                        gap: '12px'
                                    }}>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                            <label style={{ fontSize: '13px', fontWeight: 800, color: 'var(--neutral-600)' }}>설정할 인스턴스 선택:</label>
                                            <select
                                                value={selectedInstanceId || ''}
                                                onChange={(e) => setSelectedInstanceId(parseInt(e.target.value))}
                                                style={{
                                                    padding: '6px 12px',
                                                    borderRadius: '8px',
                                                    border: '1px solid var(--neutral-300)',
                                                    minWidth: '160px',
                                                    fontSize: '13px',
                                                    fontWeight: 600
                                                }}
                                            >
                                                {instances.map(inst => (
                                                    <option key={inst.id} value={inst.id}>{inst.instance_name}</option>
                                                ))}
                                            </select>
                                        </div>

                                        {selectedInstance && (
                                            <div style={{ display: 'flex', alignItems: 'center', gap: '20px' }}>
                                                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px' }}>
                                                    <span style={{ color: 'var(--neutral-500)', fontWeight: 600 }}>접속 상태:</span>
                                                    <code style={{ background: 'var(--neutral-200)', padding: '2px 6px', borderRadius: '4px', color: 'var(--neutral-700)', fontWeight: 700 }}>vhost: {selectedInstance.vhost}</code>
                                                    <span style={{
                                                        fontWeight: 800,
                                                        color: selectedInstance.is_enabled ? 'var(--success-600)' : 'var(--neutral-500)',
                                                        marginLeft: '4px'
                                                    }}>
                                                        {selectedInstance.is_enabled ? 'RUNNING' : 'STOPPED'}
                                                    </span>
                                                </div>

                                                <label className="mgmt-switch" style={{ margin: 0 }}>
                                                    <input
                                                        type="checkbox"
                                                        checked={selectedInstance.is_enabled}
                                                        onChange={async (e) => {
                                                            const enabled = e.target.checked;
                                                            const ok = await confirm({
                                                                title: enabled ? '인스턴스 활성화' : '인스턴스 비활성화',
                                                                message: `[${selectedInstance.instance_name}] 인스턴스를 ${enabled ? '활성화' : '비활성화'}하시겠습니까?`,
                                                                confirmText: enabled ? '활성화' : '중지',
                                                                confirmButtonType: enabled ? 'primary' : 'danger'
                                                            });
                                                            if (ok) {
                                                                await ProtocolApiService.updateProtocolInstance(selectedInstance.id, { is_enabled: enabled });
                                                                fetchInstances();
                                                            }
                                                        }}
                                                    />
                                                    <span className="mgmt-slider round"></span>
                                                </label>
                                            </div>
                                        )}
                                    </div>

                                    {selectedInstance && (
                                        <MqttSecuritySlot
                                            instance={selectedInstance}
                                            onUpdate={fetchInstances}
                                        />
                                    )}
                                </div>
                            ) : (
                                <div style={{ textAlign: 'center', padding: '100px', color: 'var(--neutral-400)' }}>
                                    <i className="fas fa-server" style={{ fontSize: '48px', marginBottom: '16px', opacity: 0.2 }}></i>
                                    <p>보안 설정을 관리할 인스턴스가 없습니다. '인스턴스' 탭에서 먼저 생성해주세요.</p>
                                </div>
                            )}
                        </div>
                    )}
                </div>
            </div>

            {/* 🔥 Layout Safe Guard Spacer */}
            <div style={{ minHeight: '100px', width: '100%', flexShrink: 0 }}></div>
        </div>
    );
};

export default ProtocolDashboard;
