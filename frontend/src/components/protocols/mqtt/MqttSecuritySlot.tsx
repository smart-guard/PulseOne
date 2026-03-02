
import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { ProtocolInstance, ProtocolApiService } from '../../../api/services/protocolApi';
import { useConfirmContext } from '../../common/ConfirmProvider';

interface MqttSecuritySlotProps {
    instance: ProtocolInstance;
    onUpdate: () => void; // Refresh parent
}

const MqttSecuritySlot: React.FC<MqttSecuritySlotProps> = ({ instance, onUpdate }) => {
    const { confirm } = useConfirmContext();
    const { t } = useTranslation(['protocols', 'common']);
    const [loading, setLoading] = useState(false);
    const [localParams, setLocalParams] = useState<any>(instance.connection_params || {});
    const [originalParams, setOriginalParams] = useState<any>(instance.connection_params || {});

    useEffect(() => {
        const params = instance.connection_params || {};
        setLocalParams(params);
        setOriginalParams(params);
    }, [instance]);

    const isEnabled = instance.is_enabled;

    const handleToggleStatus = async (enabled: boolean) => {
        const message = enabled
            ? `Activate instance [${instance.instance_name}]?`
            : `Deactivating [${instance.instance_name}] will stop all sensors using vhost (${instance.vhost}).`;

        const ok = await confirm({
            title: enabled ? 'Activate Instance' : 'Deactivate Instance',
            message,
            confirmText: enabled ? 'Activate' : 'Stop Service',
            confirmButtonType: enabled ? 'primary' : 'danger'
        });

        if (ok) {
            setLoading(true);
            try {
                await ProtocolApiService.updateProtocolInstance(instance.id, { is_enabled: enabled });
                onUpdate();
            } catch (err: any) {
                alert(err.message);
            } finally {
                setLoading(false);
            }
        }
    };

    const handleReissueApiKey = async () => {
        const ok = await confirm({
            title: 'Reissue API Key',
            message: 'Reissuing the API Key will immediately block all sensors using the old key. Continue?',
            confirmText: 'Reissue',
            confirmButtonType: 'danger'
        });

        if (ok) {
            setLoading(true);
            try {
                // 백엔드에서 자동 생성하도록 빈 값 또는 특정 플래그 전송 (여기서는 클라이언트 생성 테스트용)
                const newKey = 'key_' + Math.random().toString(36).substring(2, 11) + '_' + Date.now().toString(36);
                await ProtocolApiService.updateProtocolInstance(instance.id, {
                    api_key: newKey,
                    api_key_updated_at: new Date().toISOString()
                });
                onUpdate();
            } catch (err: any) {
                alert(err.message);
            } finally {
                setLoading(false);
            }
        }
    };

    const handleSaveParams = async () => {
        const changes: string[] = [];

        // Host change detection
        const oldHost = originalParams.host || window.location.hostname;
        const newHost = localParams.host || window.location.hostname;
        if (oldHost !== newHost) {
            changes.push(`Host: ${oldHost} ➔ ${newHost}`);
        }

        // Port change detection
        const oldPort = originalParams.port || 1883;
        const newPort = localParams.port || 1883;
        if (oldPort !== newPort) {
            changes.push(`Port: ${oldPort} ➔ ${newPort}`);
        }

        // Credential changes (EXTERNAL ONLY)
        if (instance.broker_type === 'EXTERNAL') {
            if (localParams.username !== originalParams.username) changes.push(`Username: ${originalParams.username || 'N/A'} ➔ ${localParams.username}`);
            if (localParams.password !== originalParams.password) changes.push(`Password: modified`);
        }

        if (changes.length === 0) {
            await confirm({
                title: 'Notice',
                message: 'No changes to save.',
                confirmText: 'OK',
                confirmButtonType: 'primary',
                showCancelButton: false
            });
            return;
        }

        const ok = await confirm({
            title: 'Save Changes',
            message: `Save the following changes?\n\n${changes.map(c => `• ${c}`).join('\n')}`,
            confirmText: 'Save',
            confirmButtonType: 'primary'
        });

        if (ok) {
            setLoading(true);
            try {
                await ProtocolApiService.updateProtocolInstance(instance.id, { connection_params: localParams });
                setOriginalParams({ ...localParams }); // Sync original after success
                onUpdate();

                await confirm({
                    title: 'Success',
                    message: 'Settings saved successfully.',
                    confirmText: 'OK',
                    confirmButtonType: 'primary',
                    showCancelButton: false
                });
            } catch (err: any) {
                alert(err.message);
            } finally {
                setLoading(false);
            }
        }
    };

    return (
        <div className="instance-security-view" style={{ display: 'flex', flexDirection: 'column', gap: '12px', animation: 'fadeIn 0.3s ease-in' }}>
            {/* Status Section Removed (Moved to Dashboard Header) */}

            {/* B. Credentials Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '20px' }}>
                <div style={{ padding: '24px', background: '#fff', borderRadius: '12px', border: '1px solid var(--neutral-200)', display: 'flex', flexDirection: 'column', gap: '20px' }}>
                    <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 800 }}>{t('labels.connectionAuthManagement', {ns: 'protocols'})}</h4>

                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                        <div className="form-group">
                            <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>
                                {instance.broker_type === 'EXTERNAL' ? 'External Broker Host' : 'Broker Host'}
                            </label>
                            <input
                                type="text"
                                value={localParams.host || (instance.broker_type === 'EXTERNAL' ? '' : window.location.hostname)}
                                onChange={(e) => setLocalParams({ ...localParams, host: e.target.value })}
                                placeholder={instance.broker_type === 'EXTERNAL' ? "e.g. mqtt.example.com" : "e.g. 192.168.0.10"}
                                style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)', fontSize: '13px' }}
                            />
                        </div>
                        <div className="form-group">
                            <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>{t('labels.brokerPort', {ns: 'protocols'})}</label>
                            <input
                                type="number"
                                value={localParams.port || 1883}
                                onChange={(e) => setLocalParams({ ...localParams, port: parseInt(e.target.value) || 1883 })}
                                style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)', fontSize: '13px' }}
                            />
                        </div>
                    </div>

                    {instance.broker_type === 'EXTERNAL' && (
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginTop: '-8px' }}>
                            <div className="form-group">
                                <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>{t('labels.mqttUsername', {ns: 'protocols'})}</label>
                                <input
                                    type="text"
                                    value={localParams.username || ''}
                                    onChange={(e) => setLocalParams({ ...localParams, username: e.target.value })}
                                    placeholder="Username"
                                    style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)', fontSize: '13px' }}
                                />
                            </div>
                            <div className="form-group">
                                <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>{t('labels.mqttPassword', {ns: 'protocols'})}</label>
                                <input
                                    type="password"
                                    value={localParams.password || ''}
                                    onChange={(e) => setLocalParams({ ...localParams, password: e.target.value })}
                                    placeholder="Password"
                                    style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)', fontSize: '13px' }}
                                />
                            </div>
                        </div>
                    )}

                    <div className="form-group">
                        <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>{t('labels.calculatedEndpointUrlMqtt', {ns: 'protocols'})}</label>
                        <div style={{ position: 'relative' }}>
                            <input
                                type="text"
                                readOnly
                                value={instance.broker_type === 'EXTERNAL'
                                    ? `mqtt://${localParams.host || 'BROKER_ADDRESS'}:${localParams.port || 1883}`
                                    : `mqtt://${localParams.host || window.location.hostname}:${localParams.port || 1883}${instance.vhost}`}
                                style={{ width: '100%', background: 'var(--neutral-100)', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-200)', fontSize: '13px', fontWeight: 700, color: 'var(--neutral-600)' }}
                            />
                            <div style={{ fontSize: '10px', color: 'var(--neutral-400)', marginTop: '4px' }}>
                                {instance.broker_type === 'EXTERNAL'
                                    ? '* Enter the external broker address accurately. Standard MQTT connection without Vhost path will be used.'
                                    : '* Use this URL when connecting from external sensors or devices.'}
                            </div>
                        </div>
                    </div>

                    <div style={{
                        display: 'flex',
                        alignItems: 'center', // Align items center for cleaner horizontal line
                        gap: '12px'
                    }}>
                        {/* 1. API Key Section (Decorative Blue Box) */}
                        {instance.broker_type === 'INTERNAL' && (
                            <div style={{
                                flex: 1,
                                padding: '12px',
                                background: 'var(--primary-50)',
                                borderRadius: '12px',
                                border: '1px solid var(--primary-200)'
                            }}>
                                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                                    <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--primary-800)' }}>{t('labels.authenticationTokenApiKey', {ns: 'protocols'})}</label>
                                    <button onClick={handleReissueApiKey} disabled={loading} style={{ padding: '2px 6px', fontSize: '10px', background: '#fff', border: '1px solid var(--primary-300)', borderRadius: '4px', cursor: 'pointer' }}>{t('labels.reissue', {ns: 'protocols'})}</button>
                                </div>
                                <input
                                    type="text"
                                    readOnly
                                    value={instance.api_key || 'N/A'}
                                    style={{ width: '100%', padding: '8px', borderRadius: '6px', border: 'none', background: '#fff', fontFamily: 'monospace', fontWeight: 800, fontSize: '13px', textAlign: 'center' }}
                                />
                            </div>
                        )}

                        {/* 2. Global Action Section (Outside the box) */}
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                            <button
                                className="btn-primary"
                                onClick={handleSaveParams}
                                disabled={loading}
                                style={{
                                    padding: '12px 20px',
                                    background: 'var(--primary-600)',
                                    color: '#fff',
                                    border: 'none',
                                    borderRadius: '8px',
                                    fontWeight: 700,
                                    cursor: 'pointer',
                                    fontSize: '13px',
                                    height: 'auto', // Auto height to fit text + icon
                                    display: 'flex',
                                    alignItems: 'center',
                                    gap: '8px',
                                    boxShadow: '0 4px 12px rgba(var(--primary-rgb), 0.3)',
                                    minWidth: 'fit-content'
                                }}
                            >
                                <i className="fas fa-save"></i>
                                Save Changes
                            </button>
                            <span style={{ fontSize: '10px', color: 'var(--neutral-400)', fontWeight: 600 }}>
                                * 설정 저장 시 해당 {instance.broker_type === 'EXTERNAL' ? 'connecting to broker' : 'host/port info will be updated'}됩니다.
                            </span>
                        </div>
                    </div>
                </div>

                <div style={{ padding: '24px', background: 'var(--neutral-50)', borderRadius: '12px', border: '1px solid var(--neutral-200)', flex: 1 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                        <i className="fas fa-info-circle" style={{ color: 'var(--primary-600)' }}></i>
                        <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 800 }}>{t('labels.mqttFieldDeviceIntegrationGuide', {ns: 'protocols'})}</h4>
                    </div>

                    <div style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
                        <div style={{ paddingBottom: '12px', borderBottom: '1px solid var(--neutral-200)' }}>
                            <p style={{ margin: '0 0 4px 0', fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>{t('labels.brokerAddressHostport', {ns: 'protocols'})}</p>
                            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                                <code style={{ fontSize: '13px', fontWeight: 800 }}>{localParams.host || window.location.hostname}</code>
                                <span style={{ color: 'var(--neutral-300)' }}>:</span>
                                <code style={{ fontSize: '13px', fontWeight: 800 }}>{localParams.port || 1883}</code>
                            </div>
                            <span style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>Use internal network or domain address for 연결하되, 포트 개방 여부를 확인하십시오.</span>
                        </div>

                        <div style={{ paddingBottom: '12px', borderBottom: '1px solid var(--neutral-200)' }}>
                            <p style={{ margin: '0 0 4px 0', fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>{t('labels.credentialsUserpass', {ns: 'protocols'})}</p>
                            <div style={{ display: 'grid', gridTemplateColumns: '80px 1fr', gap: '8px', fontSize: '12px' }}>
                                <span style={{ color: 'var(--neutral-600)' }}>{t('labels.username', {ns: 'protocols'})}</span>
                                <code style={{ fontWeight: 800 }}>pulseone</code>
                                <span style={{ color: 'var(--neutral-600)' }}>{t('labels.password', {ns: 'protocols'})}</span>
                                <code style={{ fontWeight: 800 }}>[위 발급된 API Key]</code>
                            </div>
                            <p style={{ fontSize: '11px', color: 'var(--neutral-400)', margin: '4px 0 0 0' }}>API Key는 인스턴스 전용 비밀번호 역할을 병행합니다.</p>
                        </div>

                        {instance.broker_type === 'INTERNAL' && (
                            <div>
                                <p style={{ margin: '0 0 4px 0', fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>{t('labels.isolationVhostpath', {ns: 'protocols'})}</p>
                                <div style={{ display: 'flex', gap: '12px', alignItems: 'center', marginBottom: '6px' }}>
                                    <div style={{ fontSize: '12px' }}>
                                        <span style={{ color: 'var(--neutral-600)' }}>{t('labels.vhostpath', {ns: 'protocols'})}</span>
                                        <code style={{ fontWeight: 800, marginLeft: '8px' }}>{instance.vhost}</code>
                                    </div>
                                </div>
                                <div style={{
                                    background: 'white',
                                    padding: '10px',
                                    borderRadius: '8px',
                                    border: '1px solid var(--neutral-100)',
                                    fontSize: '11px',
                                    color: 'var(--neutral-600)',
                                    lineHeight: '1.5'
                                }}>
                                    <i className="fas fa-shield-alt" style={{ marginRight: '6px', color: 'var(--success-500)' }}></i>
                                    <strong>Vhost 격리 기술 적용:</strong> 별도의 Topic Prefix(접두어) 설정이 필요 없습니다. 해당 Vhost 경로 자체로 논리적인 독립망을 보장하므로, 장치는 루트(/) 토픽을 자유롭게 사용해도 다른 서비스와 혼선되지 않습니다.
                                </div>
                            </div>
                        )}

                        {instance.broker_type === 'EXTERNAL' && (
                            <div style={{
                                background: 'var(--warning-50)',
                                padding: '16px',
                                borderRadius: '12px',
                                border: '1px solid var(--warning-200)',
                                fontSize: '12px',
                                color: 'var(--warning-900)',
                                lineHeight: '1.6'
                            }}>
                                <div style={{ fontWeight: 800, marginBottom: '8px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                                    <i className="fas fa-exclamation-triangle"></i> 외부 브로커 연결 주의사항
                                </div>
                                <ul style={{ margin: 0, paddingLeft: '20px' }}>
                                    <li>입력한 Host와 Port로 PulseOne 서버가 직접 접속을 수행합니다.</li>
                                    <li>방화벽 및 네트워크 접근 권한이 확보되어 있는지 확인하십시오.</li>
                                    <li>외부 브로커의 토픽 구조는 수집기 드라이버 설정에 따라 결정됩니다.</li>
                                    <li>SSL/TLS 연결 정보는 현재 기본 설정(Non-SSL)을 따릅니다.</li>
                                </ul>
                            </div>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default MqttSecuritySlot;
