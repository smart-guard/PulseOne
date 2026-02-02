
import React, { useState, useEffect } from 'react';
import { ProtocolInstance, ProtocolApiService } from '../../../api/services/protocolApi';
import { useConfirmContext } from '../../common/ConfirmProvider';

interface MqttSecuritySlotProps {
    instance: ProtocolInstance;
    onUpdate: () => void; // Refresh parent
}

const MqttSecuritySlot: React.FC<MqttSecuritySlotProps> = ({ instance, onUpdate }) => {
    const { confirm } = useConfirmContext();
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
            ? `[${instance.instance_name}] 인스턴스를 활성화하시겠습니까?`
            : `[${instance.instance_name}] 인스턴스를 비활성화하면 해당 vhost(${instance.vhost})를 사용하는 모든 센서의 접속이 중단됩니다.`;

        const ok = await confirm({
            title: enabled ? '인스턴스 활성화' : '인스턴스 비활성화',
            message,
            confirmText: enabled ? '활성화' : '서비스 중단',
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
            title: 'API Key 재발급',
            message: 'API Key를 재발급하면 기존 Key를 사용하는 모든 센서의 접속이 즉시 차단됩니다. 계속하시겠습니까?',
            confirmText: '재발급 실행',
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
            if (localParams.password !== originalParams.password) changes.push(`Password: 수정됨`);
        }

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
            title: '변경사항 저장',
            message: `다음 변경사항을 저장하시겠습니까?\n\n${changes.map(c => `• ${c}`).join('\n')}`,
            confirmText: '저장하기',
            confirmButtonType: 'primary'
        });

        if (ok) {
            setLoading(true);
            try {
                await ProtocolApiService.updateProtocolInstance(instance.id, { connection_params: localParams });
                setOriginalParams({ ...localParams }); // Sync original after success
                onUpdate();

                await confirm({
                    title: '성공',
                    message: '설정이 성공적으로 저장되었습니다.',
                    confirmText: '확인',
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
                    <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 800 }}>접속 인증 관리</h4>

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
                            <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>Broker Port</label>
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
                                <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>MQTT Username</label>
                                <input
                                    type="text"
                                    value={localParams.username || ''}
                                    onChange={(e) => setLocalParams({ ...localParams, username: e.target.value })}
                                    placeholder="Username"
                                    style={{ width: '100%', padding: '10px', borderRadius: '8px', border: '1px solid var(--neutral-300)', fontSize: '13px' }}
                                />
                            </div>
                            <div className="form-group">
                                <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>MQTT Password</label>
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
                        <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--neutral-500)', marginBottom: '4px', display: 'block' }}>Calculated Endpoint URL (MQTT)</label>
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
                                    ? '* 외부 브로커 주소를 정확히 입력하십시오. Vhost 경로가 없는 표준 MQTT 연결을 사용합니다.'
                                    : '* 외부 센서나 디바이스에서 접속할 때는 이 URL을 사용하세요.'}
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
                                    <label style={{ fontSize: '11px', fontWeight: 800, color: 'var(--primary-800)' }}>Authentication Token (API Key)</label>
                                    <button onClick={handleReissueApiKey} disabled={loading} style={{ padding: '2px 6px', fontSize: '10px', background: '#fff', border: '1px solid var(--primary-300)', borderRadius: '4px', cursor: 'pointer' }}>재발급</button>
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
                                변경사항 저장
                            </button>
                            <span style={{ fontSize: '10px', color: 'var(--neutral-400)', fontWeight: 600 }}>
                                * 설정 저장 시 해당 {instance.broker_type === 'EXTERNAL' ? '브로커로 연결을 시도' : '호스트/포트 정보가 업데이트'}됩니다.
                            </span>
                        </div>
                    </div>
                </div>

                <div style={{ padding: '24px', background: 'var(--neutral-50)', borderRadius: '12px', border: '1px solid var(--neutral-200)', flex: 1 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                        <i className="fas fa-info-circle" style={{ color: 'var(--primary-600)' }}></i>
                        <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 800 }}>MQTT 현장 장치 연동 가이드</h4>
                    </div>

                    <div style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
                        <div style={{ paddingBottom: '12px', borderBottom: '1px solid var(--neutral-200)' }}>
                            <p style={{ margin: '0 0 4px 0', fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>BROKER ADDRESS (HOST/PORT)</p>
                            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                                <code style={{ fontSize: '13px', fontWeight: 800 }}>{localParams.host || window.location.hostname}</code>
                                <span style={{ color: 'var(--neutral-300)' }}>:</span>
                                <code style={{ fontSize: '13px', fontWeight: 800 }}>{localParams.port || 1883}</code>
                            </div>
                            <span style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>내부망 또는 도메인 주소로 연결하되, 포트 개방 여부를 확인하십시오.</span>
                        </div>

                        <div style={{ paddingBottom: '12px', borderBottom: '1px solid var(--neutral-200)' }}>
                            <p style={{ margin: '0 0 4px 0', fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>CREDENTIALS (USER/PASS)</p>
                            <div style={{ display: 'grid', gridTemplateColumns: '80px 1fr', gap: '8px', fontSize: '12px' }}>
                                <span style={{ color: 'var(--neutral-600)' }}>Username:</span>
                                <code style={{ fontWeight: 800 }}>pulseone</code>
                                <span style={{ color: 'var(--neutral-600)' }}>Password:</span>
                                <code style={{ fontWeight: 800 }}>[위 발급된 API Key]</code>
                            </div>
                            <p style={{ fontSize: '11px', color: 'var(--neutral-400)', margin: '4px 0 0 0' }}>API Key는 인스턴스 전용 비밀번호 역할을 병행합니다.</p>
                        </div>

                        {instance.broker_type === 'INTERNAL' && (
                            <div>
                                <p style={{ margin: '0 0 4px 0', fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>ISOLATION (VHOST/PATH)</p>
                                <div style={{ display: 'flex', gap: '12px', alignItems: 'center', marginBottom: '6px' }}>
                                    <div style={{ fontSize: '12px' }}>
                                        <span style={{ color: 'var(--neutral-600)' }}>Vhost/Path:</span>
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
