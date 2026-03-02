import React, { useState, useEffect, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { DeviceApiService, Device } from '../../api/services/deviceApi';
import { useConfirmContext } from '../common/ConfirmProvider';

interface NetworkScanModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSuccess?: () => void;
}

const NetworkScanModal: React.FC<NetworkScanModalProps> = ({ isOpen, onClose, onSuccess }) => {
    const { confirm } = useConfirmContext();
    const [protocol, setProtocol] = useState('BACNET');
    const { t } = useTranslation(['network', 'common']);
    const [range, setRange] = useState('');
    const [timeoutValue, setTimeoutValue] = useState(5000);
    const [isScanning, setIsScanning] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [discoveredDevices, setDiscoveredDevices] = useState<Device[]>([]);
    const [scanStartTime, setScanStartTime] = useState<number | null>(null);
    const pollingTimer = useRef<NodeJS.Timeout | null>(null);
    const [scanDuration, setScanDuration] = useState(0);
    const [progress, setProgress] = useState(0);

    // Reset state when modal opens/closes
    useEffect(() => {
        if (!isOpen) {
            if (pollingTimer.current) clearInterval(pollingTimer.current);
            setIsScanning(false);
            setDiscoveredDevices([]);
            setScanStartTime(null);
            setError(null);
            setProgress(0);
        }
    }, [isOpen]);

    const startPolling = (startTime: number) => {
        if (pollingTimer.current) clearInterval(pollingTimer.current);

        pollingTimer.current = setInterval(async () => {
            try {
                const results = await DeviceApiService.getScanResults({
                    since: startTime.toString(),
                    protocol: protocol
                });

                if (results.success && results.data) {
                    setDiscoveredDevices(results.data);
                }
            } catch (err) {
                console.error("Polling error:", err);
            }
        }, 2000); // Poll every 2 seconds
    };

    const handleScan = async () => {
        if (!range && protocol !== 'BACNET' && protocol !== 'MODBUS_RTU' && protocol !== 'MQTT') {
            setError(protocol === 'MQTT' ? t('labels.enterBrokerUrl', { ns: 'protocols', defaultValue: '브로커 URL을 입력해주세요.' }) : t('labels.enterIpRange', { ns: 'protocols', defaultValue: 'IP 범위(CIDR)를 입력해주세요.' }));
            return;
        }

        try {
            setIsScanning(true);
            setError(null);
            setDiscoveredDevices([]);
            setProgress(0);
            const startTime = Date.now();
            setScanStartTime(startTime);
            setScanDuration(timeoutValue + 2000);
            // Start progress animation
            const progressInterval = 100;
            const totalDuration = timeoutValue + 2000;
            let currentProgress = 0;

            const progressTimer = setInterval(() => {
                currentProgress += progressInterval;
                const newProgress = Math.min((currentProgress / totalDuration) * 100, 95);
                setProgress(newProgress);
                if (currentProgress >= totalDuration) clearInterval(progressTimer);
            }, progressInterval);

            const response = await DeviceApiService.scanNetwork({
                protocol,
                range,
                timeout: timeoutValue
            });

            if (response.success) {
                startPolling(startTime);

                window.setTimeout(async () => {
                    if (pollingTimer.current) {
                        clearInterval(pollingTimer.current);
                        pollingTimer.current = null;
                    }
                    setIsScanning(false);
                    setProgress(100);
                    clearInterval(progressTimer);

                    // Fetch final results to get the count
                    const finalResults = await DeviceApiService.getScanResults({
                        since: startTime.toString(),
                        protocol: protocol
                    });

                    const foundCount = finalResults.success && finalResults.data ? finalResults.data.length : 0;

                    if (foundCount > 0) {
                        await confirm({
                            title: t('labels.scanComplete', { ns: 'protocols', defaultValue: '스캔 완료' }),
                            message: t('labels.scanFoundDevices', { ns: 'protocols', count: foundCount, defaultValue: '{{count}}개 신규 디바이스를 발견하여 자동 등록했습니다.' }),
                            confirmText: 'OK',
                            showCancelButton: false,
                            confirmButtonType: 'success'
                        });
                    } else {
                        await confirm({
                            title: t('labels.scanComplete', { ns: 'protocols', defaultValue: '스캔 완료' }),
                            message: t('labels.scanNoDevices', { ns: 'protocols', defaultValue: '신규 디바이스를 발견하지 못했습니다. 네트워크 설정을 확인해주세요.' }),
                            confirmText: 'OK',
                            showCancelButton: false,
                            confirmButtonType: 'warning'
                        });
                    }
                }, totalDuration);

            } else {
                clearInterval(progressTimer);
                throw new Error(response.error || t('labels.scanStartFailed', { ns: 'protocols', defaultValue: '스캔 시작에 실패했습니다.' }));
            }
        } catch (err: any) {
            setError(err.message || t('labels.scanError', { ns: 'protocols', defaultValue: '스캔 중 오류가 발생했습니다.' }));
            setIsScanning(false);
            setProgress(0);
        }
    };

    if (!isOpen) return null;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container network-scan-modal" style={{ width: '780px', maxWidth: '780px' }}>
                <div className="mgmt-modal-header" style={{ padding: '20px 24px', borderBottom: '1px solid #edf2f7' }}>
                    <div className="mgmt-modal-title">
                        <h3 style={{ margin: 0, color: '#1a202c', fontWeight: 800, fontSize: '1.2rem', letterSpacing: '-0.02em' }}>{t('labels.networkDeviceScan', { ns: 'protocols' })}</h3>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose} disabled={isScanning}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body" style={{ padding: '0', maxHeight: '75vh', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                    {/* Error Alert */}
                    {error && (
                        <div className="alert alert-danger" style={{
                            margin: '16px 24px 0',
                            padding: '12px 16px',
                            borderRadius: '10px',
                            fontSize: '0.88rem',
                            display: 'flex',
                            alignItems: 'flex-start',
                            gap: '10px',
                            background: '#fff5f5',
                            border: '1px solid #feb2b2',
                            color: '#c53030',
                        }}>
                            <i className="fas fa-exclamation-circle" style={{ marginTop: '3px' }}></i>
                            <div style={{ lineHeight: '1.5' }}>
                                <div style={{ fontWeight: 700, marginBottom: '2px' }}>{t('labels.scanErrorOccurred', { ns: 'protocols' })}</div>
                                <div style={{ opacity: 0.9 }}>{error}</div>
                            </div>
                        </div>
                    )}

                    {/* 2-Column Layout */}
                    <div style={{ display: 'grid', gridTemplateColumns: '280px 1fr', gap: '0', flex: 1, overflow: 'hidden' }}>

                        {/* LEFT: Scan Settings */}
                        <div style={{ padding: '20px 20px 20px 24px', borderRight: '1px solid #edf2f7', background: '#f8fafc', overflowY: 'auto' }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                                <div style={{ width: '4px', height: '16px', background: '#667eea', borderRadius: '2px' }}></div>
                                <h4 style={{ margin: 0, fontSize: '13px', fontWeight: 700, color: '#2d3748' }}>{t('labels.scanSettings', { ns: 'protocols' })}</h4>
                            </div>

                            <div className="mgmt-modal-form-group" style={{ marginBottom: '16px' }}>
                                <label style={{ fontSize: '11px', fontWeight: 700, color: '#718096', marginBottom: '6px', display: 'block', textTransform: 'uppercase', letterSpacing: '0.04em' }}>{t('labels.selectProtocol', { ns: 'protocols' })}</label>
                                <select
                                    className="mgmt-select"
                                    value={protocol}
                                    onChange={(e) => setProtocol(e.target.value)}
                                    disabled={isScanning}
                                    style={{ height: '52px', borderRadius: '10px', fontSize: '14px', width: '100%' }}
                                >
                                    <option value="BACNET">{t('labels.bacnetipRecommended', { ns: 'protocols' })}</option>
                                    <option value="MQTT">{t('labels.mqttDiscoverySmartSearch', { ns: 'protocols' })}</option>
                                    <option value="MODBUS_TCP">{t('labels.modbusTcpTest', { ns: 'protocols' })}</option>
                                    <option value="MODBUS_RTU">{t('labels.modbusRtuSerial', { ns: 'protocols' })}</option>
                                </select>
                                {protocol.startsWith('MODBUS') && (
                                    <div style={{ marginTop: '6px', color: '#dd6b20', fontSize: '11px', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '4px' }}>
                                        <i className="fas fa-flask"></i> <span>{t('labels.experimentalFeatureHint', { ns: 'protocols' })}</span>
                                    </div>
                                )}
                            </div>

                            <div className="mgmt-modal-form-group">
                                <label style={{ fontSize: '11px', fontWeight: 700, color: '#718096', marginBottom: '6px', display: 'block', textTransform: 'uppercase', letterSpacing: '0.04em' }}>
                                    {protocol === 'MQTT' ? t('labels.brokerUrl', { ns: 'protocols' }) : (protocol === 'MODBUS_RTU' ? t('labels.serialPortPath', { ns: 'protocols' }) : t('labels.networkRangeCidr', { ns: 'protocols' }))}
                                </label>
                                <input
                                    type="text"
                                    className="mgmt-input"
                                    placeholder={protocol === 'MQTT' ? 'e.g. mqtt://rabbitmq:1883' : (protocol === 'MODBUS_RTU' ? 'e.g. /dev/ttyS0' : 'e.g. 192.168.1.0/24')}
                                    value={range}
                                    onChange={(e) => setRange(e.target.value)}
                                    disabled={isScanning}
                                    style={{ height: '52px', borderRadius: '10px', fontSize: '14px', width: '100%' }}
                                />
                                {protocol !== 'MODBUS_RTU' && protocol !== 'MQTT' && (
                                    <div style={{ marginTop: '6px', color: '#a0aec0', fontSize: '11px', lineHeight: 1.4 }}>
                                        {t('labels.broadcastHint', { ns: 'protocols' })}
                                    </div>
                                )}
                                {protocol === 'MQTT' && (
                                    <div style={{ marginTop: '6px', color: '#a0aec0', fontSize: '11px', lineHeight: 1.4 }}>
                                        {t('labels.mqttBrokerHint', { ns: 'protocols' })}
                                    </div>
                                )}
                            </div>
                        </div>

                        {/* RIGHT: Scan Results */}
                        <div style={{ padding: '20px 24px 20px 20px', overflowY: 'auto', display: 'flex', flexDirection: 'column' }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
                                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <div style={{ width: '4px', height: '16px', background: '#667eea', borderRadius: '2px' }}></div>
                                    <h4 style={{ margin: 0, fontSize: '13px', fontWeight: 700, color: '#2d3748' }}>{t('labels.searchResults', { ns: 'protocols' })}</h4>
                                    {discoveredDevices.length > 0 && (
                                        <span style={{
                                            padding: '2px 8px',
                                            borderRadius: '20px',
                                            background: '#ebf4ff',
                                            color: '#5a67d8',
                                            fontSize: '11px',
                                            fontWeight: 800
                                        }}>{discoveredDevices.length}</span>
                                    )}
                                </div>
                                {isScanning && (
                                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: '#667eea', fontSize: '12px', fontWeight: 600 }}>
                                        <span>{t('labels.searching', { ns: 'protocols' })}</span> <i className="fas fa-circle-notch fa-spin"></i>
                                    </div>
                                )}
                            </div>

                            {/* Progress Bar */}
                            {isScanning && (
                                <div style={{ marginBottom: '12px' }}>
                                    <div style={{
                                        height: '5px',
                                        background: '#e2e8f0',
                                        borderRadius: '3px',
                                        overflow: 'hidden',
                                        marginBottom: '6px'
                                    }}>
                                        <div style={{
                                            width: `${progress}%`,
                                            height: '100%',
                                            background: 'linear-gradient(90deg, #667eea, #764ba2)',
                                            transition: 'width 0.3s ease-out'
                                        }}></div>
                                    </div>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '11px', color: '#718096', fontWeight: 500 }}>
                                        <span>{t('labels.scanningNetwork', { ns: 'protocols' })}</span>
                                        <span>{Math.round(progress)}%</span>
                                    </div>
                                </div>
                            )}

                            <div style={{
                                border: '1px solid #e2e8f0',
                                borderRadius: '12px',
                                overflow: 'hidden',
                                background: '#f8fafc',
                                flex: 1,
                                minHeight: '180px'
                            }}>
                                {discoveredDevices.length === 0 && !isScanning ? (
                                    <div style={{ padding: '28px 24px', textAlign: 'center' }}>
                                        <div style={{
                                            width: '48px',
                                            height: '48px',
                                            background: 'white',
                                            borderRadius: '16px',
                                            display: 'flex',
                                            alignItems: 'center',
                                            justifyContent: 'center',
                                            margin: '0 auto 12px',
                                            boxShadow: '0 4px 6px rgba(0,0,0,0.05)',
                                            color: '#cbd5e0'
                                        }}>
                                            <i className="fas fa-satellite-dish" style={{ fontSize: '20px' }}></i>
                                        </div>
                                        <div style={{ fontWeight: 700, color: '#4a5568', fontSize: '13px', marginBottom: '4px' }}>{t('labels.noDevicesFound', { ns: 'protocols' })}</div>
                                        <div style={{ color: '#a0aec0', fontSize: '11px' }}>{t('labels.checkTheSettingsAboveAndStartScanning', { ns: 'protocols' })}</div>
                                    </div>
                                ) : discoveredDevices.length === 0 && isScanning ? (
                                    <div style={{ padding: '28px 24px', textAlign: 'center' }}>
                                        <div style={{ marginBottom: '12px', position: 'relative', display: 'inline-block' }}>
                                            <i className="fas fa-search-location" style={{ fontSize: '28px', color: '#667eea', opacity: 0.6 }}></i>
                                            <div style={{
                                                position: 'absolute',
                                                top: '-8px',
                                                right: '-8px',
                                                width: '18px',
                                                height: '18px',
                                                background: '#ebf4ff',
                                                borderRadius: '50%',
                                                display: 'flex',
                                                alignItems: 'center',
                                                justifyContent: 'center',
                                                boxShadow: '0 2px 4px rgba(0,0,0,0.1)'
                                            }}>
                                                <i className="fas fa-circle-notch fa-spin" style={{ fontSize: '9px', color: '#667eea' }}></i>
                                            </div>
                                        </div>
                                        <div style={{ fontWeight: 700, color: '#4a5568', fontSize: '13px', marginBottom: '4px' }}>{t('labels.scanningNetwork', { ns: 'protocols' })}</div>
                                        <div style={{ color: '#a0aec0', fontSize: '11px' }}>{t('labels.pleaseWaitWhileSearchingForDevices', { ns: 'protocols' })}</div>
                                    </div>
                                ) : (
                                    <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '13px' }}>
                                        <thead style={{ background: '#f1f5f9', borderBottom: '1px solid #e2e8f0' }}>
                                            <tr>
                                                <th style={{ padding: '10px 14px', textAlign: 'left', fontWeight: 700, color: '#64748b', fontSize: '11px', textTransform: 'uppercase' }}>{t('labels.deviceInfo', { ns: 'protocols' })}</th>
                                                <th style={{ padding: '10px 14px', textAlign: 'left', fontWeight: 700, color: '#64748b', fontSize: '11px', textTransform: 'uppercase' }}>{t('labels.endpoint', { ns: 'protocols' })}</th>
                                                <th style={{ padding: '10px 14px', textAlign: 'right', fontWeight: 700, color: '#64748b', fontSize: '11px', textTransform: 'uppercase' }}>{t('labels.statusCol', { ns: 'protocols' })}</th>
                                            </tr>
                                        </thead>
                                        <tbody style={{ background: 'white' }}>
                                            {discoveredDevices.map((dev, idx) => (
                                                <tr key={idx} style={{
                                                    borderBottom: idx === discoveredDevices.length - 1 ? 'none' : '1px solid #f1f5f9'
                                                }}>
                                                    <td style={{ padding: '10px 14px' }}>
                                                        <div style={{ fontWeight: 700, color: '#1a202c' }}>{dev.name}</div>
                                                        <div style={{ fontSize: '11px', color: '#718096' }}>{dev.protocol_name || protocol}</div>
                                                    </td>
                                                    <td style={{ padding: '10px 14px', color: '#4a5568', fontFamily: 'monospace', fontSize: '12px' }}>{dev.endpoint}</td>
                                                    <td style={{ padding: '10px 14px', textAlign: 'right' }}>
                                                        <span style={{
                                                            padding: '3px 8px',
                                                            borderRadius: '6px',
                                                            background: '#f0fff4',
                                                            color: '#2f855a',
                                                            fontSize: '11px',
                                                            fontWeight: 800,
                                                            border: '1px solid #c6f6d5'
                                                        }}>{t('labels.autoRegistered', { ns: 'protocols' })}</span>
                                                    </td>
                                                </tr>
                                            ))}
                                        </tbody>
                                    </table>
                                )}
                            </div>
                            {discoveredDevices.length > 0 && (
                                <div style={{ marginTop: '10px', padding: '8px 12px', background: '#f7fafc', borderRadius: '8px', fontSize: '11px', color: '#718096', border: '1px dashed #e2e8f0' }}>
                                    <i className="fas fa-check-circle" style={{ color: '#38a169', marginRight: '6px' }}></i>
                                    {t('labels.foundDevicesHint', { ns: 'protocols' })}
                                </div>
                            )}
                        </div>

                    </div>
                </div>

                <div className="mgmt-modal-footer" style={{ padding: '20px 24px', borderTop: '1px solid #edf2f7', gap: '12px', background: '#f8fafc' }}>
                    <button
                        className="mgmt-btn mgmt-btn-outline"
                        onClick={onClose}
                        disabled={isScanning}
                        style={{ height: '42px', padding: '0 24px', fontSize: '14px', borderRadius: '10px', background: 'white' }}
                    >
                        {discoveredDevices.length > 0 ? t('labels.doneBtn', { ns: 'protocols' }) : t('labels.closeBtn', { ns: 'protocols' })}
                    </button>
                    <button
                        className="mgmt-btn mgmt-btn-primary"
                        onClick={handleScan}
                        disabled={isScanning}
                        style={{
                            height: '42px',
                            minWidth: '140px',
                            padding: '0 28px',
                            fontSize: '14px',
                            borderRadius: '10px',
                            fontWeight: 700,
                            boxShadow: isScanning ? 'none' : '0 4px 12px rgba(102, 126, 234, 0.4)',
                            background: isScanning ? '#cbd5e0' : 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
                            border: 'none',
                            color: 'white',
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                            gap: '8px'
                        }}
                    >
                        {isScanning ? (
                            <>
                                <i className="fas fa-circle-notch fa-spin"></i>
                                <span>{t('labels.scanningBtn', { ns: 'protocols' })}</span>
                            </>
                        ) : (
                            <>
                                <i className="fas fa-search"></i>
                                <span>{t('labels.startScanBtn', { ns: 'protocols' })}</span>
                            </>
                        )}
                    </button>
                </div>
            </div>
        </div>
    );
};

export default NetworkScanModal;
