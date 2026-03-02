
import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { useParams, useNavigate } from 'react-router-dom';
import { ProtocolApiService, Protocol } from '../../../api/services/protocolApi';
import { StatCard } from '../../common/StatCard';
import { useConfirmContext } from '../../common/ConfirmProvider';

interface MqttBrokerDashboardProps {
    protocol: Protocol;
    onRefresh: () => void;
}

const MqttBrokerDashboard: React.FC<MqttBrokerDashboardProps> = ({ protocol, onRefresh }) => {
    const { id: protocolId, tab: urlTab } = useParams();
    const navigate = useNavigate();

    // 탭 상태를 URL 파라미터와 동기화
    const activeTab = (urlTab as any) || 'monitoring';
    const [brokerStatus, setBrokerStatus] = useState<any>(null);
    const { t } = useTranslation(['protocols', 'common']);
    const [loading, setLoading] = useState(false);

    // 프로토콜 정보 수정을 위한 로컬 상태
    const [localProtocol, setLocalProtocol] = useState<Partial<Protocol>>(protocol);
    const [isEditingInfo, setIsEditingInfo] = useState(false);

    const { confirm } = useConfirmContext();

    console.log(`[MqttDashboard] Rendered for ID: ${protocolId}, Tab: ${activeTab}`);

    const fetchStatus = useCallback(async () => {
        try {
            const response = await ProtocolApiService.getBrokerStatus();
            if (response.success) {
                setBrokerStatus(response.data);
            }
        } catch (err) {
            console.error('Failed to fetch broker status:', err);
        }
    }, []);

    useEffect(() => {
        fetchStatus();
        const interval = setInterval(fetchStatus, 5000);
        return () => clearInterval(interval);
    }, [fetchStatus]);

    useEffect(() => {
        setLocalProtocol(protocol);
    }, [protocol]);

    const handleUpdateProtocol = async (updatedData: any) => {
        try {
            setLoading(true);
            const response = await ProtocolApiService.updateProtocol(protocol.id, updatedData);
            if (response.success) {
                onRefresh();
                await fetchStatus();
            }
        } catch (err) {
            console.error('Update failed:', err);
        } finally {
            setLoading(false);
        }
    };

    const handleToggleBroker = async (enabled: boolean) => {
        const message = enabled
            ? 'Enable MQTT broker service?'
            : 'Disabling the broker will interrupt communication for all connected sensors. Continue?';

        const ok = await confirm({
            title: enabled ? 'Enable Service' : 'Disable Service',
            message,
            confirmText: enabled ? 'Enable' : 'Stop Service',
            confirmButtonType: enabled ? 'primary' : 'danger'
        });

        if (ok) {
            const updatedParams: any = { ...protocol.connection_params, broker_enabled: enabled };

            // 만약 활성화 시 API Key가 없다면 자동 생성
            if (enabled && !updatedParams.broker_api_key) {
                updatedParams.broker_api_key = 'key_' + Math.random().toString(36).substring(2, 11) + '_' + Date.now().toString(36);
                updatedParams.broker_api_key_updated_at = new Date().toISOString();
            }

            await handleUpdateProtocol({
                connection_params: updatedParams
            });
        }
    };

    const handleReissueApiKey = async () => {
        const ok = await confirm({
            title: 'Reissue API Key',
            message: 'Reissuing the API Key will disconnect all sensors using the old key. Continue?',
            confirmText: 'Reissue',
            confirmButtonType: 'danger'
        });

        if (ok) {
            const newKey = 'key_' + Math.random().toString(36).substring(2, 11) + '_' + Date.now().toString(36);
            const updatedParams: any = {
                ...protocol.connection_params,
                broker_api_key: newKey,
                broker_api_key_updated_at: new Date().toISOString()
            };
            await handleUpdateProtocol({ connection_params: updatedParams });
        }
    };

    const isBrokerEnabled = protocol.connection_params?.broker_enabled === true;

    // 탭 이동 함수 (URL 기반)
    const switchTab = (tabId: string) => {
        navigate(`/protocols/dashboard/${protocol.id}/${tabId}`);
    };

    return (
        <div className="mqtt-dashboard" style={{
            height: '100%',
            display: 'flex',
            flexDirection: 'column',
            animation: 'fadeIn 0.5s ease-out',
            padding: '24px 32px' // Absolute alignment reference
        }}>
            {/* 1. Dashboard Header - Standard White Industrial Style */}
            <div className="dashboard-header" style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '20px',
                padding: '24px',
                background: 'white',
                color: 'var(--neutral-900)',
                borderRadius: '16px',
                boxShadow: '0 1px 3px rgba(0,0,0,0.1)',
                border: '1px solid var(--neutral-200)',
                width: '100%' // Ensure full width matching parent constraints
            }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '24px' }}>
                    <div style={{
                        width: '60px',
                        height: '60px',
                        background: 'var(--primary-50)',
                        color: 'var(--primary-600)',
                        borderRadius: '14px',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        fontSize: '28px',
                        border: '1px solid var(--primary-100)'
                    }}>
                        <i className="fas fa-server"></i>
                    </div>
                    <div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                            <h2 style={{ margin: 0, fontSize: '24px', fontWeight: 700, color: 'var(--neutral-900)' }}>{protocol.display_name} Management Center</h2>
                            <span className={`mgmt-status-pill ${isBrokerEnabled ? (brokerStatus?.is_healthy ? 'active' : 'warning') : 'inactive'}`} style={{ fontSize: '11px' }}>
                                {isBrokerEnabled ? (brokerStatus?.is_healthy ? 'Running' : 'Warning') : 'Inactive'}
                            </span>
                        </div>
                        <p style={{ margin: '4px 0 0 0', fontSize: '14px', color: 'var(--neutral-500)' }}>{protocol.description || 'MQTT broker service operation and security settings'}</p>
                    </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                    <div style={{ fontSize: '12px', color: 'var(--neutral-400)', marginBottom: '8px' }}>
                        ID: {protocol.id} | Ver: {protocol.min_firmware_version || '1.0.0'}
                    </div>
                    <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={onRefresh}>
                        <i className="fas fa-sync-alt" style={{ marginRight: '6px' }}></i> Refresh Status
                    </button>
                </div>
            </div>

            {/* 2. Custom Tabs Navigation - Pill Style */}
            <div className="dashboard-tabs" style={{
                display: 'inline-flex',
                background: 'var(--neutral-100)',
                padding: '4px',
                borderRadius: '12px',
                marginBottom: '20px',
                gap: '2px',
                alignSelf: 'flex-start'
            }}>
                {[
                    { id: 'monitoring', label: 'Live Monitor', icon: 'fa-chart-line' },
                    { id: 'clients', label: 'Connected Devices', icon: 'fa-microchip' },
                    { id: 'info', label: 'Technical Specs', icon: 'fa-code' },
                    { id: 'security', label: 'Broker Security', icon: 'fa-shield-halved' }
                ].map(tab => (
                    <button
                        key={tab.id}
                        onClick={() => switchTab(tab.id)}
                        className={`nav-tab ${activeTab === tab.id ? 'active' : ''}`}
                        style={{
                            padding: '8px 16px',
                            border: 'none',
                            borderRadius: '8px',
                            background: activeTab === tab.id ? 'white' : 'transparent',
                            color: activeTab === tab.id ? 'var(--primary-600)' : 'var(--neutral-500)',
                            fontSize: '13px',
                            fontWeight: 700,
                            cursor: 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                            transition: 'all 0.2s ease',
                            boxShadow: activeTab === tab.id ? '0 2px 4px rgba(0,0,0,0.05)' : 'none'
                        }}
                    >
                        <i className={`fas ${tab.icon}`} style={{ fontSize: '14px' }}></i>
                        {tab.label}
                    </button>
                ))}
            </div>

            {/* 3. Tab Content Area - Scrollable */}
            <div className="dashboard-content" style={{
                flex: 1,
                overflowY: 'auto',
                paddingBottom: '200px',
                animation: 'slideUp 0.4s ease-out',
                position: 'relative',
                display: 'flex',
                flexDirection: 'column',
                gap: '12px' // Reduced from 16px for vertical compression
            }}>

                {activeTab === 'monitoring' && (
                    <div className="monitoring-view">
                        <div className="mgmt-stats-panel" style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '24px', marginBottom: '32px' }}>
                            <StatCard label="Active Sessions" value={brokerStatus?.stats?.connections || 0} type="primary" />
                            <StatCard label="Msg Receive Rate" value={(brokerStatus?.stats?.message_rates?.publish || 0).toFixed(1) + ' msg/s'} type="success" />
                            <StatCard label="Msg Send Rate" value={(brokerStatus?.stats?.message_rates?.deliver || 0).toFixed(1) + ' msg/s'} type="success" />
                            <StatCard label="Engine Memory" value={brokerStatus?.health_details?.memory?.used || '0 MB'} type="warning" />
                        </div>

                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '24px' }}>
                            <div className="mgmt-card">
                                <h3 className="mgmt-card-title" style={{ fontSize: '15px' }}>
                                    <i className="fas fa-microchip" style={{ color: 'var(--primary-600)', marginRight: '8px' }}></i> Hardware Resource Metrics
                                </h3>
                                <div style={{ marginBottom: '20px', marginTop: '16px' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '13px', marginBottom: '8px', color: 'var(--neutral-600)', fontWeight: 600 }}>
                                        <span>{t('labels.memoryStabilityThreshold', {ns: 'protocols'})}</span>
                                        <span style={{ color: 'var(--neutral-900)' }}>{brokerStatus?.health_details?.memory?.used || '0MB'} / {brokerStatus?.health_details?.memory?.limit || '∞'}</span>
                                    </div>
                                    <div style={{ width: '100%', height: '8px', backgroundColor: 'var(--neutral-100)', borderRadius: '4px', overflow: 'hidden' }}>
                                        <div style={{
                                            width: `${(parseFloat(brokerStatus?.health_details?.memory?.used) / parseFloat(brokerStatus?.health_details?.memory?.limit) * 100) || 0}%`,
                                            height: '100%',
                                            background: 'var(--primary-500)',
                                            borderRadius: '4px'
                                        }}></div>
                                    </div>
                                </div>
                                <div style={{ fontSize: '12px', color: 'var(--neutral-500)', lineHeight: '1.5', backgroundColor: 'var(--neutral-50)', padding: '12px', borderRadius: '8px', borderLeft: '3px solid var(--primary-500)' }}>
                                    These metrics are based on real-time statistics from PulseOne's built-in RabbitMQ server.
                                </div>
                            </div>

                            <div className="mgmt-card">
                                <h3 className="mgmt-card-title" style={{ fontSize: '15px' }}>
                                    <i className="fas fa-database" style={{ color: 'var(--primary-600)', marginRight: '8px' }}></i> Messaging Queue Stats
                                </h3>
                                <div style={{ display: 'grid', gap: '12px', marginTop: '16px' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', padding: '12px', backgroundColor: 'var(--neutral-50)', borderRadius: '8px', alignItems: 'center' }}>
                                        <span style={{ color: 'var(--neutral-600)', fontSize: '13px', fontWeight: 600 }}>{t('labels.dedicatedQueuesCreated', {ns: 'protocols'})}</span>
                                        <span style={{ fontWeight: 800, fontSize: '18px', color: 'var(--neutral-900)' }}>{brokerStatus?.stats?.queues || 0}</span>
                                    </div>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', padding: '12px', backgroundColor: 'var(--neutral-50)', borderRadius: '8px', alignItems: 'center' }}>
                                        <span style={{ color: 'var(--neutral-600)', fontSize: '13px', fontWeight: 600 }}>{t('labels.activeConsumers', {ns: 'protocols'})}</span>
                                        <span style={{ fontWeight: 800, fontSize: '18px', color: 'var(--neutral-900)' }}>{brokerStatus?.stats?.consumers || 0}</span>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                )}

                {activeTab === 'clients' && (
                    <div className="clients-view">
                        <div className="mgmt-table-container" style={{ backgroundColor: 'white', borderRadius: '24px', border: '1px solid #e2e8f0', overflow: 'hidden', boxShadow: '0 10px 15px -3px rgba(0,0,0,0.05)' }}>
                            <table className="mgmt-table">
                                <thead style={{ backgroundColor: '#f8fafc' }}>
                                    <tr>
                                        <th style={{ padding: '20px 32px', color: '#475569', fontWeight: 700 }}>{t('labels.clientEndpoint', {ns: 'protocols'})}</th>
                                        <th style={{ padding: '20px 32px', color: '#475569', fontWeight: 700 }}>{t('labels.accountInfo', {ns: 'protocols'})}</th>
                                        <th style={{ padding: '20px 32px', color: '#475569', fontWeight: 700 }}>{t('labels.sessionStatus', {ns: 'protocols'})}</th>
                                        <th style={{ padding: '20px 32px', color: '#475569', fontWeight: 700 }}>{t('labels.protocol', {ns: 'protocols'})}</th>
                                        <th style={{ padding: '20px 32px', color: '#475569', fontWeight: 700 }}>{t('labels.connectedAt', {ns: 'protocols'})}</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {(brokerStatus?.connection_list || []).map((conn: any, idx: number) => (
                                        <tr key={idx} style={{ borderTop: '1px solid #f1f5f9' }}>
                                            <td style={{ padding: '20px 32px' }}>
                                                <div style={{ fontWeight: 800, color: '#1e293b' }}>{conn.client_address}:{conn.client_port}</div>
                                                <div style={{ fontSize: '11px', color: '#94a3b8', marginTop: '4px' }}>Client-ID: {conn.name || 'ANON_CLIENT'}</div>
                                            </td>
                                            <td style={{ padding: '20px 32px', fontWeight: 600, color: '#475569' }}>{conn.user}</td>
                                            <td style={{ padding: '20px 32px' }}>
                                                <span className={`mgmt-badge ${conn.state === 'running' ? 'success' : 'neutral'}`} style={{ padding: '6px 12px', borderRadius: '6px', fontWeight: 700 }}>{conn.state.toUpperCase()}</span>
                                            </td>
                                            <td style={{ padding: '20px 32px', fontWeight: 600, color: '#64748b' }}>MQTT v{conn.version || '3.1.1'}</td>
                                            <td style={{ padding: '20px 32px', fontSize: '13px', color: '#94a3b8' }}>{new Date(conn.connected_at).toLocaleString()}</td>
                                        </tr>
                                    ))}
                                    {(!brokerStatus?.connection_list || brokerStatus.connection_list.length === 0) && (
                                        <tr>
                                            <td colSpan={5} style={{ textAlign: 'center', padding: '120px', color: '#94a3b8' }}>
                                                <i className="fas fa-plug-circle-xmark" style={{ fontSize: '64px', marginBottom: '24px', display: 'block', opacity: 0.15 }}></i>
                                                <span style={{ fontSize: '18px', fontWeight: 600 }}>{t('labels.noConnectedClientSensors', {ns: 'protocols'})}</span>
                                            </td>
                                        </tr>
                                    )}
                                </tbody>
                            </table>
                        </div>
                    </div>
                )}

                {activeTab === 'info' && (
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
                        {/* View/Edit Toggle Header */}
                        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '12px', marginBottom: '4px' }}>
                            {isEditingInfo ? (
                                <>
                                    <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => {
                                        setLocalProtocol(protocol);
                                        setIsEditingInfo(false);
                                    }}>
                                        Cancel
                                    </button>
                                    <button className="mgmt-btn mgmt-btn-primary mgmt-btn-sm" onClick={async () => {
                                        await handleUpdateProtocol({
                                            display_name: localProtocol.display_name,
                                            description: localProtocol.description,
                                            default_polling_interval: localProtocol.default_polling_interval,
                                            default_timeout: localProtocol.default_timeout,
                                            max_concurrent_connections: localProtocol.max_concurrent_connections
                                        });
                                        setIsEditingInfo(false);
                                    }}>
                                        Save Complete
                                    </button>
                                </>
                            ) : (
                                <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => setIsEditingInfo(true)}>
                                    <i className="fas fa-edit" style={{ marginRight: '6px' }}></i> Edit Spec Mode
                                </button>
                            )}
                        </div>

                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '20px' }}>
                            <div className="mgmt-card">
                                <h3 className="mgmt-card-title" style={{ fontSize: '15px' }}>
                                    <i className="fas fa-info-circle" style={{ color: 'var(--primary-600)', marginRight: '8px' }}></i> Basic Info
                                </h3>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', marginTop: '16px' }}>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.displayName', {ns: 'protocols'})}</label>
                                        {isEditingInfo ? (
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                value={localProtocol.display_name || ''}
                                                onChange={(e) => setLocalProtocol({ ...localProtocol, display_name: e.target.value })}
                                            />
                                        ) : (
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', fontWeight: 600, padding: '8px 0' }}>{protocol.display_name}</div>
                                        )}
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.protocolDescription', {ns: 'protocols'})}</label>
                                        {isEditingInfo ? (
                                            <textarea
                                                className="mgmt-input"
                                                style={{ height: '80px', padding: '12px', resize: 'none' }}
                                                value={localProtocol.description || ''}
                                                onChange={(e) => setLocalProtocol({ ...localProtocol, description: e.target.value })}
                                            />
                                        ) : (
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-600)', lineHeight: '1.5', padding: '8px 0' }}>{protocol.description || 'No description'}</div>
                                        )}
                                    </div>
                                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.officialVendor', {ns: 'protocols'})}</label>
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', padding: '8px 0' }}>{protocol.vendor || 'Standard'}</div>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.standardPort', {ns: 'protocols'})}</label>
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', padding: '8px 0' }}>{protocol.default_port || 1883}</div>
                                        </div>
                                    </div>
                                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.techCategory', {ns: 'protocols'})}</label>
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', padding: '8px 0' }}>{protocol.category || 'IoT'}</div>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.minSupportedVersion', {ns: 'protocols'})}</label>
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', padding: '8px 0' }}>{protocol.min_firmware_version || '1.0.0'}</div>
                                        </div>
                                    </div>
                                </div>
                            </div>

                            <div className="mgmt-card">
                                <h3 className="mgmt-card-title" style={{ fontSize: '15px' }}>
                                    <i className="fas fa-sliders" style={{ color: 'var(--primary-600)', marginRight: '8px' }}></i> Communication Parameters
                                </h3>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', marginTop: '16px' }}>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.defaultPollingIntervalMs', {ns: 'protocols'})}</label>
                                        {isEditingInfo ? (
                                            <input
                                                type="number"
                                                className="mgmt-input"
                                                value={localProtocol.default_polling_interval || 1000}
                                                onChange={(e) => setLocalProtocol({ ...localProtocol, default_polling_interval: parseInt(e.target.value) })}
                                            />
                                        ) : (
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', padding: '8px 0' }}>{protocol.default_polling_interval} ms</div>
                                        )}
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.responseTimeoutMs', {ns: 'protocols'})}</label>
                                        {isEditingInfo ? (
                                            <input
                                                type="number"
                                                className="mgmt-input"
                                                value={localProtocol.default_timeout || 5000}
                                                onChange={(e) => setLocalProtocol({ ...localProtocol, default_timeout: parseInt(e.target.value) })}
                                            />
                                        ) : (
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', padding: '8px 0' }}>{protocol.default_timeout} ms</div>
                                        )}
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontSize: '13px', fontWeight: 600, color: 'var(--neutral-700)', marginBottom: '4px', display: 'block' }}>{t('labels.maxConcurrentSessions', {ns: 'protocols'})}</label>
                                        {isEditingInfo ? (
                                            <input
                                                type="number"
                                                className="mgmt-input"
                                                value={localProtocol.max_concurrent_connections || 100}
                                                onChange={(e) => setLocalProtocol({ ...localProtocol, max_concurrent_connections: parseInt(e.target.value) })}
                                            />
                                        ) : (
                                            <div style={{ fontSize: '14px', color: 'var(--neutral-900)', padding: '8px 0' }}>{protocol.max_concurrent_connections} Sessions</div>
                                        )}
                                    </div>

                                    <div style={{ border: '1px dashed var(--neutral-300)', padding: '16px', borderRadius: '12px', background: 'var(--neutral-50)', marginTop: '8px' }}>
                                        <h4 style={{ fontSize: '13px', fontWeight: 700, color: 'var(--neutral-700)', marginBottom: '12px' }}>{t('labels.supportedFeaturesCapabilities', {ns: 'protocols'})}</h4>
                                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                                            {protocol.supported_operations?.map(cap => (
                                                <div key={cap} style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '12px', color: 'var(--success-700)', background: 'white', padding: '4px 10px', borderRadius: '6px', border: '1px solid var(--success-100)' }}>
                                                    <i className="fas fa-check-circle"></i> {cap.toUpperCase()}
                                                </div>
                                            ))}
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                )}

                {activeTab === 'security' && (
                    <div className="service-security-view" style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                        {/* A. Activation Section - Explicit Left Alignment & Row Controls */}
                        <div className="mgmt-card" style={{
                            padding: '16px 28px',
                            display: 'flex',
                            flexDirection: 'row',
                            justifyContent: 'space-between',
                            alignItems: 'center',
                            background: '#fff',
                            borderLeft: `6px solid ${isBrokerEnabled ? 'var(--success-500)' : 'var(--neutral-300)'}`,
                            width: '100%',
                            textAlign: 'left'
                        }}>
                            <div style={{ flex: 1, textAlign: 'left' }}>
                                <h3 style={{ fontSize: '15px', fontWeight: 900, margin: 0, color: 'var(--neutral-900)' }}>{t('labels.mqttBrokerServerActivation', {ns: 'protocols'})}</h3>
                                <p style={{ fontSize: '12px', color: 'var(--neutral-500)', margin: '2px 0 0 0' }}>Allows real-time data connections from external devices and sensors.</p>
                            </div>

                            <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: '20px' }}>
                                <div style={{ textAlign: 'right', display: 'flex', flexDirection: 'column', alignItems: 'flex-end' }}>
                                    <div style={{ fontSize: '10px', fontWeight: 800, color: 'var(--neutral-400)', marginBottom: '0' }}>{t('labels.status', {ns: 'protocols'})}</div>
                                    <span style={{ fontSize: '14px', fontWeight: 900, color: isBrokerEnabled ? 'var(--success-600)' : 'var(--neutral-500)' }}>
                                        {isBrokerEnabled ? 'Running (RUNNING)' : 'Inactive (STOPPED)'}
                                    </span>
                                </div>
                                <label className="mgmt-switch" style={{ margin: 0 }}>
                                    <input type="checkbox" checked={isBrokerEnabled} onChange={(e) => handleToggleBroker(e.target.checked)} />
                                    <span className="mgmt-slider round"></span>
                                </label>
                            </div>
                        </div>

                        {/* 2. Main Content Grid (2-Column Layout) */}
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 480px', gap: '16px', alignItems: 'start' }}>
                            {/* Left: Administrative Controls */}
                            <div className="mgmt-card" style={{ padding: '24px', display: 'flex', flexDirection: 'column', gap: '20px' }}>
                                <div>
                                    <h3 style={{ fontSize: '15px', fontWeight: 900, margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                                        <i className="fas fa-shield-halved" style={{ color: 'var(--primary-600)' }}></i> Broker Access & Auth Management
                                    </h3>
                                </div>

                                {/* Connection Details Group */}
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '6px', display: 'block' }}>{t('labels.connectionUrlEndpoint', {ns: 'protocols'})}</label>
                                        <div style={{ position: 'relative' }}>
                                            <input type="text" className="mgmt-input" readOnly value={`mqtt://${window.location.hostname}:1883`} style={{ background: 'var(--neutral-50)', fontWeight: 800, paddingRight: '100px', fontSize: '13px', height: '38px', borderRadius: '8px' }} />
                                            <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" style={{ position: 'absolute', right: '4px', top: '4px', height: '30px', border: 'none', background: '#fff', fontSize: '11px', fontWeight: 800 }} onClick={() => navigator.clipboard.writeText(`mqtt://${window.location.hostname}:1883`)}>{t('labels.copyUrl', {ns: 'protocols'})}</button>
                                        </div>
                                    </div>

                                    <div style={{ display: 'grid', gridTemplateColumns: '0.9fr 0.9fr 1fr', gap: '12px' }}>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--primary-700)', marginBottom: '5px', display: 'block' }}>{t('labels.brokerId', {ns: 'protocols'})}</label>
                                            <div style={{ position: 'relative' }}>
                                                <input type="text" className="mgmt-input" placeholder="Enter ID" value={localProtocol.connection_params?.broker_username || ''} onChange={(e) => setLocalProtocol({ ...localProtocol, connection_params: { ...localProtocol.connection_params, broker_username: e.target.value } })} style={{ paddingRight: '36px', height: '36px', fontSize: '13px' }} />
                                                <button style={{ position: 'absolute', right: '8px', top: '9px', background: 'none', border: 'none', color: 'var(--primary-400)' }} onClick={() => localProtocol.connection_params?.broker_username && navigator.clipboard.writeText(localProtocol.connection_params.broker_username)}><i className="far fa-copy"></i></button>
                                            </div>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--primary-700)', marginBottom: '5px', display: 'block' }}>{t('labels.brokerPassword', {ns: 'protocols'})}</label>
                                            <div style={{ position: 'relative' }}>
                                                <input type="password" className="mgmt-input" placeholder="Enter Password" value={localProtocol.connection_params?.broker_password || ''} onChange={(e) => setLocalProtocol({ ...localProtocol, connection_params: { ...localProtocol.connection_params, broker_password: e.target.value } })} style={{ paddingRight: '36px', height: '36px', fontSize: '13px' }} />
                                                <button style={{ position: 'absolute', right: '8px', top: '9px', background: 'none', border: 'none', color: 'var(--primary-400)' }} onClick={() => localProtocol.connection_params?.broker_password && navigator.clipboard.writeText(localProtocol.connection_params.broker_password)}><i className="far fa-copy"></i></button>
                                            </div>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '5px', display: 'block' }}>vhost (default: /)</label>
                                            <input type="text" className="mgmt-input" placeholder="/" value={localProtocol.connection_params?.broker_vhost || ''} onChange={(e) => setLocalProtocol({ ...localProtocol, connection_params: { ...localProtocol.connection_params, broker_vhost: e.target.value } })} style={{ height: '36px', fontSize: '13px' }} />
                                        </div>
                                    </div>
                                </div>

                                {/* API Key (Sensor Token) */}
                                <div style={{ padding: '16px', background: 'linear-gradient(135deg, var(--primary-50) 0%, #fff 100%)', borderRadius: '12px', border: '1px solid var(--primary-200)', boxShadow: '0 2px 8px rgba(37, 99, 235, 0.05)' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <h4 style={{ fontSize: '12px', fontWeight: 700, color: 'var(--primary-800)', margin: 0 }}>{t('labels.sensorAuthTokenApiKey', {ns: 'protocols'})}</h4>
                                            <span style={{ fontSize: '10px', color: 'var(--neutral-400)', fontWeight: 500 }}>* For security, it is strongly recommended to use the above token instead of a password.</span>
                                        </div>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={handleReissueApiKey} disabled={loading} style={{ background: '#fff', height: '24px', fontSize: '10px', padding: '0 10px', fontWeight: 800 }}>{t('labels.reissueToken', {ns: 'protocols'})}</button>
                                    </div>
                                    <div style={{ position: 'relative' }}>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            readOnly
                                            value={protocol.connection_params?.broker_api_key || 'Auto-issued when broker starts'}
                                            style={{
                                                backgroundColor: '#ffffff',
                                                fontFamily: 'monospace',
                                                fontSize: '15px',
                                                fontWeight: 900,
                                                padding: '0 50px 0 12px',
                                                border: '2px solid var(--primary-500)',
                                                color: protocol.connection_params?.broker_api_key ? 'var(--neutral-900)' : 'var(--neutral-300)',
                                                textAlign: 'center',
                                                height: '40px',
                                                borderRadius: '8px'
                                            }}
                                        />
                                        <button
                                            style={{ position: 'absolute', right: '10px', top: '10px', background: 'none', border: 'none', color: 'var(--primary-500)', cursor: 'pointer' }}
                                            onClick={() => protocol.connection_params?.broker_api_key && navigator.clipboard.writeText(protocol.connection_params.broker_api_key)}
                                        >
                                            <i className="far fa-copy" style={{ fontSize: '18px' }}></i>
                                        </button>
                                    </div>
                                </div>

                                <div style={{ display: 'flex', justifyContent: 'center', borderTop: '1px solid var(--neutral-100)', paddingTop: '16px' }}>
                                    <button
                                        className="mgmt-btn mgmt-btn-primary"
                                        style={{ padding: '10px 100px', fontSize: '14px', fontWeight: 900, borderRadius: '8px', boxShadow: '0 4px 10px rgba(37,99,235,0.1)' }}
                                        onClick={() => handleUpdateProtocol({ connection_params: localProtocol.connection_params })}
                                        disabled={loading}
                                    >
                                        Save & Apply Security Settings
                                    </button>
                                </div>
                            </div>

                            {/* Right: Engineering Help Guide */}
                            <div className="mgmt-card" style={{ padding: '24px', background: 'var(--neutral-50)', border: '1px solid var(--neutral-200)', height: '100%', boxShadow: '0 2px 8px rgba(0,0,0,0.02)' }}>
                                <h3 style={{ fontSize: '15px', fontWeight: 900, color: 'var(--neutral-800)', marginBottom: '20px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <i className="fas fa-microchip" style={{ color: 'var(--neutral-500)' }}></i> Engineer Integration Guide
                                </h3>

                                <div style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
                                    <div style={{ padding: '14px', background: 'white', borderRadius: '10px', border: '1px solid var(--neutral-200)' }}>
                                        <div style={{ fontSize: '12px', fontWeight: 800, color: 'var(--primary-600)', marginBottom: '6px' }}>{t('labels.connectionSpecSpec', {ns: 'protocols'})}</div>
                                        <div style={{ fontSize: '13px', color: 'var(--neutral-600)', lineHeight: '1.5' }}>
                                            • Protocol: **MQTT v3.1.1 / v5.0**<br />
                                            • Port: **1883** (Default TCP)<br />
                                            • Keep-Alive: 60s (recommended)
                                        </div>
                                    </div>

                                    <div style={{ padding: '14px', background: 'white', borderRadius: '10px', border: '1px solid var(--neutral-200)' }}>
                                        <div style={{ fontSize: '12px', fontWeight: 800, color: 'var(--primary-600)', marginBottom: '6px' }}>{t('labels.authMethodAuth', {ns: 'protocols'})}</div>
                                        <div style={{ fontSize: '12px', color: 'var(--neutral-600)', lineHeight: '1.5' }}>
                                            When connecting external devices, enter the <strong>{t('labels.sensorTokenApiKey', {ns: 'protocols'})}</strong> on the left in the Password field. Use the configured Broker ID as-is for the ID.
                                        </div>
                                    </div>

                                    <div style={{ padding: '14px', background: 'white', borderRadius: '10px', border: '1px solid var(--neutral-200)' }}>
                                        <div style={{ fontSize: '12px', fontWeight: 800, color: 'var(--primary-600)', marginBottom: '6px' }}>{t('labels.topicStructureTopic', {ns: 'protocols'})}</div>
                                        <div style={{ fontSize: '12px', color: 'var(--neutral-600)', lineHeight: '1.5' }}>
                                            • Collect: `pulseone/devices/{"{id}"}/data`<br />
                                            • Control: `pulseone/devices/{"{id}"}/cmd`<br />
                                            <span style={{ fontSize: '11px', color: 'var(--neutral-400)', marginTop: '4px', display: 'block' }}>* {"{id}"} is the unique device identifier.</span>
                                        </div>
                                    </div>

                                </div>
                            </div>
                        </div>

                    </div>
                )}
            </div>

            <style>{`
            @keyframes fadeIn { from { opacity: 0; filter: blur(4px); } to { opacity: 1; filter: blur(0); } }
            @keyframes slideUp { from { transform: translateY(30px); opacity: 0; } to { transform: translateY(0); opacity: 1; } }
            .nav-tab:hover { background-color: white !important; color: var(--primary-600) !important; transform: translateY(-1px); }
            .nav-tab.active { box-shadow: 0 2px 4px rgba(0,0,0,0.05); }
        `}</style>
        </div >
    );
};

export default MqttBrokerDashboard;
