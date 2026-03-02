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
            setError(protocol === 'MQTT' ? 'Please enter the broker URL.' : 'Please enter the IP range (CIDR).');
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
                            title: 'Scan Complete',
                            message: `${foundCount} new device(s) found and auto-registered.`,
                            confirmText: 'OK',
                            showCancelButton: false,
                            confirmButtonType: 'success'
                        });
                    } else {
                        await confirm({
                            title: 'Scan Complete',
                            message: 'No new devices found. Please check your network settings.',
                            confirmText: 'OK',
                            showCancelButton: false,
                            confirmButtonType: 'warning'
                        });
                    }
                }, totalDuration);

            } else {
                clearInterval(progressTimer);
                throw new Error(response.error || 'Scan start failed');
            }
        } catch (err: any) {
            setError(err.message || 'An error occurred during scanning.');
            setIsScanning(false);
            setProgress(0);
        }
    };

    if (!isOpen) return null;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container network-scan-modal" style={{ width: '480px', maxWidth: '480px' }}>
                <div className="mgmt-modal-header" style={{ padding: '20px 24px', borderBottom: '1px solid #edf2f7' }}>
                    <div className="mgmt-modal-title">
                        <h3 style={{ margin: 0, color: '#1a202c', fontWeight: 800, fontSize: '1.2rem', letterSpacing: '-0.02em' }}>{t('labels.networkDeviceScan', {ns: 'protocols'})}</h3>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose} disabled={isScanning}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body" style={{ padding: '24px', maxHeight: '72vh' }}>
                    {/* Error Alert */}
                    {error && (
                        <div className="alert alert-danger" style={{
                            marginBottom: '24px',
                            padding: '14px 18px',
                            borderRadius: '12px',
                            fontSize: '0.88rem',
                            display: 'flex',
                            alignItems: 'flex-start',
                            gap: '12px',
                            background: '#fff5f5',
                            border: '1px solid #feb2b2',
                            color: '#c53030',
                            boxShadow: '0 2px 4px rgba(245, 101, 101, 0.1)'
                        }}>
                            <i className="fas fa-exclamation-circle" style={{ marginTop: '3px' }}></i>
                            <div style={{ lineHeight: '1.5' }}>
                                <div style={{ fontWeight: 700, marginBottom: '2px' }}>{t('labels.scanErrorOccurred', {ns: 'protocols'})}</div>
                                <div style={{ opacity: 0.9 }}>{error}</div>
                            </div>
                        </div>
                    )}

                    {/* Section: Scan Configuration */}
                    <div style={{ marginBottom: '32px' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                            <div style={{ width: '4px', height: '16px', background: '#667eea', borderRadius: '2px' }}></div>
                            <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 700, color: '#2d3748' }}>{t('labels.scanSettings', {ns: 'protocols'})}</h4>
                        </div>

                        <div className="mgmt-modal-form-group" style={{ marginBottom: '20px' }}>
                            <label style={{ fontSize: '12px', fontWeight: 700, color: '#718096', marginBottom: '8px', textTransform: 'uppercase', letterSpacing: '0.02em' }}>{t('labels.selectProtocol', {ns: 'protocols'})}</label>
                            <select
                                className="mgmt-select"
                                value={protocol}
                                onChange={(e) => setProtocol(e.target.value)}
                                disabled={isScanning}
                                style={{ height: '42px', borderRadius: '10px', fontSize: '14px' }}
                            >
                                <option value="BACNET">{t('labels.bacnetipRecommended', {ns: 'protocols'})}</option>
                                <option value="MQTT">{t('labels.mqttDiscoverySmartSearch', {ns: 'protocols'})}</option>
                                <option value="MODBUS_TCP">{t('labels.modbusTcpTest', {ns: 'protocols'})}</option>
                                <option value="MODBUS_RTU">{t('labels.modbusRtuSerial', {ns: 'protocols'})}</option>
                            </select>
                            {protocol.startsWith('MODBUS') && (
                                <div style={{ marginTop: '8px', color: '#dd6b20', fontSize: '11px', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '4px' }}>
                                    <i className="fas fa-flask"></i> <span>This is an experimental feature. Manual registration is recommended.</span>
                                </div>
                            )}
                        </div>

                        <div className="mgmt-modal-form-group">
                            <label style={{ fontSize: '12px', fontWeight: 700, color: '#718096', marginBottom: '8px', textTransform: 'uppercase', letterSpacing: '0.02em' }}>
                                {protocol === 'MQTT' ? 'Broker URL' : (protocol === 'MODBUS_RTU' ? 'Serial Port Path' : 'Network Range (CIDR)')}
                            </label>
                            <input
                                type="text"
                                className="mgmt-input"
                                placeholder={protocol === 'MQTT' ? 'e.g. mqtt://rabbitmq:1883' : (protocol === 'MODBUS_RTU' ? 'e.g. /dev/ttyS0' : 'e.g. 192.168.1.0/24')}
                                value={range}
                                onChange={(e) => setRange(e.target.value)}
                                disabled={isScanning}
                                style={{ height: '42px', borderRadius: '10px', fontSize: '14px' }}
                            />
                            {protocol !== 'MODBUS_RTU' && protocol !== 'MQTT' && (
                                <div style={{ marginTop: '8px', color: '#a0aec0', fontSize: '11px' }}>
                                    If left empty, local network broadcast is used.
                                </div>
                            )}
                            {protocol === 'MQTT' && (
                                <div style={{ marginTop: '8px', color: '#a0aec0', fontSize: '11px' }}>
                                    Connects to the broker and auto-discovers devices based on active topics.
                                </div>
                            )}
                        </div>
                    </div>

                    {/* Section: Scan Results */}
                    <div style={{ marginBottom: '8px' }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                <div style={{ width: '4px', height: '16px', background: '#667eea', borderRadius: '2px' }}></div>
                                <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 700, color: '#2d3748' }}>{t('labels.searchResults', {ns: 'protocols'})}</h4>
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
                                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', color: '#667eea', fontSize: '12px', fontWeight: 600 }}>
                                    <span>{t('labels.searching', {ns: 'protocols'})}</span> <i className="fas fa-circle-notch fa-spin"></i>
                                </div>
                            )}
                        </div>

                        {/* Progress Bar Container */}
                        {isScanning && (
                            <div style={{ marginBottom: '20px' }}>
                                <div style={{
                                    height: '6px',
                                    background: '#e2e8f0',
                                    borderRadius: '3px',
                                    overflow: 'hidden',
                                    marginBottom: '8px'
                                }}>
                                    <div style={{
                                        width: `${progress}%`,
                                        height: '100%',
                                        background: 'linear-gradient(90deg, #667eea, #764ba2)',
                                        transition: 'width 0.3s ease-out'
                                    }}></div>
                                </div>
                                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '11px', color: '#718096', fontWeight: 500 }}>
                                    <span>{t('labels.scanningNetwork', {ns: 'protocols'})}</span>
                                    <span>{Math.round(progress)}%</span>
                                </div>
                            </div>
                        )}

                        <div style={{
                            border: '1px solid #e2e8f0',
                            borderRadius: '16px',
                            overflow: 'hidden',
                            background: '#f8fafc',
                            boxShadow: 'inset 0 2px 4px rgba(0,0,0,0.02)'
                        }}>
                            {discoveredDevices.length === 0 && !isScanning ? (
                                <div style={{ padding: '48px 24px', textAlign: 'center' }}>
                                    <div style={{
                                        width: '64px',
                                        height: '64px',
                                        background: 'white',
                                        borderRadius: '20px',
                                        display: 'flex',
                                        alignItems: 'center',
                                        justifyContent: 'center',
                                        margin: '0 auto 16px',
                                        boxShadow: '0 4px 6px rgba(0,0,0,0.05)',
                                        color: '#cbd5e0'
                                    }}>
                                        <i className="fas fa-satellite-dish" style={{ fontSize: '24px' }}></i>
                                    </div>
                                    <div style={{ fontWeight: 700, color: '#4a5568', fontSize: '14px', marginBottom: '4px' }}>{t('labels.noDevicesFound', {ns: 'protocols'})}</div>
                                    <div style={{ color: '#a0aec0', fontSize: '12px' }}>{t('labels.checkTheSettingsAboveAndStartScanning', {ns: 'protocols'})}</div>
                                </div>
                            ) : discoveredDevices.length === 0 && isScanning ? (
                                <div style={{ padding: '48px 24px', textAlign: 'center' }}>
                                    <div style={{ marginBottom: '16px', position: 'relative', display: 'inline-block' }}>
                                        <i className="fas fa-search-location" style={{ fontSize: '32px', color: '#667eea', opacity: 0.6 }}></i>
                                        <div style={{
                                            position: 'absolute',
                                            top: '-10px',
                                            right: '-10px',
                                            width: '20px',
                                            height: '20px',
                                            background: '#ebf4ff',
                                            borderRadius: '50%',
                                            display: 'flex',
                                            alignItems: 'center',
                                            justifyContent: 'center',
                                            boxShadow: '0 2px 4px rgba(0,0,0,0.1)'
                                        }}>
                                            <i className="fas fa-circle-notch fa-spin" style={{ fontSize: '10px', color: '#667eea' }}></i>
                                        </div>
                                    </div>
                                    <div style={{ fontWeight: 700, color: '#4a5568', fontSize: '14px', marginBottom: '4px' }}>{t('labels.scanningNetwork', {ns: 'protocols'})}</div>
                                    <div style={{ color: '#a0aec0', fontSize: '12px' }}>{t('labels.pleaseWaitWhileSearchingForDevices', {ns: 'protocols'})}</div>
                                </div>
                            ) : (
                                <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '13px' }}>
                                    <thead style={{ background: '#f1f5f9', borderBottom: '1px solid #e2e8f0' }}>
                                        <tr>
                                            <th style={{ padding: '12px 16px', textAlign: 'left', fontWeight: 700, color: '#64748b', fontSize: '11px', textTransform: 'uppercase' }}>장치 정보</th>
                                            <th style={{ padding: '12px 16px', textAlign: 'left', fontWeight: 700, color: '#64748b', fontSize: '11px', textTransform: 'uppercase' }}>엔드포인트</th>
                                            <th style={{ padding: '12px 16px', textAlign: 'right', fontWeight: 700, color: '#64748b', fontSize: '11px', textTransform: 'uppercase' }}>상태</th>
                                        </tr>
                                    </thead>
                                    <tbody style={{ background: 'white' }}>
                                        {discoveredDevices.map((dev, idx) => (
                                            <tr key={idx} style={{
                                                borderBottom: idx === discoveredDevices.length - 1 ? 'none' : '1px solid #f1f5f9'
                                            }}>
                                                <td style={{ padding: '12px 16px' }}>
                                                    <div style={{ fontWeight: 700, color: '#1a202c' }}>{dev.name}</div>
                                                    <div style={{ fontSize: '11px', color: '#718096' }}>{dev.protocol_name || protocol}</div>
                                                </td>
                                                <td style={{ padding: '12px 16px', color: '#4a5568', fontFamily: 'monospace', fontSize: '12px' }}>{dev.endpoint}</td>
                                                <td style={{ padding: '12px 16px', textAlign: 'right' }}>
                                                    <span style={{
                                                        padding: '4px 8px',
                                                        borderRadius: '6px',
                                                        background: '#f0fff4',
                                                        color: '#2f855a',
                                                        fontSize: '11px',
                                                        fontWeight: 800,
                                                        border: '1px solid #c6f6d5'
                                                    }}>자동 등록됨</span>
                                                </td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            )}
                        </div>
                        {discoveredDevices.length > 0 && (
                            <div style={{ marginTop: '12px', padding: '10px 14px', background: '#f7fafc', borderRadius: '10px', fontSize: '11px', color: '#718096', border: '1px dashed #e2e8f0' }}>
                                <i className="fas fa-check-circle" style={{ color: '#38a169', marginRight: '6px' }}></i>
                                발견된 장치는 시스템에 자동으로 등록되었습니다. 모달을 닫고 목록에서 확인하세요.
                            </div>
                        )}
                    </div>
                </div>

                <div className="mgmt-modal-footer" style={{ padding: '20px 24px', borderTop: '1px solid #edf2f7', gap: '12px', background: '#f8fafc' }}>
                    <button
                        className="mgmt-btn mgmt-btn-outline"
                        onClick={onClose}
                        disabled={isScanning}
                        style={{ height: '42px', padding: '0 24px', fontSize: '14px', borderRadius: '10px', background: 'white' }}
                    >
                        {discoveredDevices.length > 0 ? '완료' : '닫기'}
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
                                <span>스캔 중...</span>
                            </>
                        ) : (
                            <>
                                <i className="fas fa-search"></i>
                                <span>스캔 시작</span>
                            </>
                        )}
                    </button>
                </div>
            </div>
        </div>
    );
};

export default NetworkScanModal;
