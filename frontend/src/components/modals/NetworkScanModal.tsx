import React, { useState, useEffect, useRef } from 'react';
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
    const [range, setRange] = useState('');
    const [timeoutValue, setTimeoutValue] = useState(5000);
    const [isScanning, setIsScanning] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [discoveredDevices, setDiscoveredDevices] = useState<Device[]>([]);
    const [scanStartTime, setScanStartTime] = useState<number | null>(null);
    const pollingTimer = useRef<NodeJS.Timeout | null>(null);

    // Reset state when modal opens/closes
    useEffect(() => {
        if (!isOpen) {
            if (pollingTimer.current) clearInterval(pollingTimer.current);
            setIsScanning(false);
            setDiscoveredDevices([]);
            setScanStartTime(null);
            setError(null);
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
        // Validation: range is mandatory for non-BACNET, non-MODBUS_RTU protocols.
        // For BACNET, empty range triggers broadcast/auto-detect.
        if (!range && protocol !== 'BACNET' && protocol !== 'MODBUS_RTU') {
            setError('IP 범위(CIDR)를 입력해주세요.');
            return;
        }

        try {
            setIsScanning(true);
            setError(null);
            setDiscoveredDevices([]);
            const startTime = Date.now();
            setScanStartTime(startTime);

            // Call Scan API
            const response = await DeviceApiService.scanNetwork({
                protocol,
                range,
                timeout: timeoutValue
            });

            if (response.success) {
                // Start polling for results
                startPolling(startTime);

                // Keep modal open to show results
                // We'll stop scanning state after a while or manually
                // Let's stop after timeoutValue + some buffer
                window.setTimeout(() => {
                    if (pollingTimer.current) {
                        clearInterval(pollingTimer.current);
                        pollingTimer.current = null;
                    }
                    setIsScanning(false);
                }, timeoutValue + 2000);

            } else {
                throw new Error(response.error || '스캔 시작 실패');
            }
        } catch (err: any) {
            setError(err.message || '스캔 중 오류가 발생했습니다.');
            setIsScanning(false);
        }
    };

    if (!isOpen) return null;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container network-scan-modal" style={{ width: '480px', maxWidth: '480px' }}>
                <div className="mgmt-modal-header" style={{ padding: '20px 24px', borderBottom: '1px solid #edf2f7' }}>
                    <div className="mgmt-modal-title">
                        <h3 style={{ margin: 0, color: '#1a202c', fontWeight: 800, fontSize: '1.2rem', letterSpacing: '-0.02em' }}>네트워크 장치 스캔</h3>
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
                                <div style={{ fontWeight: 700, marginBottom: '2px' }}>스캔 중 오류 발생</div>
                                <div style={{ opacity: 0.9 }}>{error}</div>
                            </div>
                        </div>
                    )}

                    {/* Section: Scan Configuration */}
                    <div style={{ marginBottom: '32px' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                            <div style={{ width: '4px', height: '16px', background: '#667eea', borderRadius: '2px' }}></div>
                            <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 700, color: '#2d3748' }}>스캔 설정</h4>
                        </div>

                        <div className="mgmt-modal-form-group" style={{ marginBottom: '20px' }}>
                            <label style={{ fontSize: '12px', fontWeight: 700, color: '#718096', marginBottom: '8px', textTransform: 'uppercase', letterSpacing: '0.02em' }}>프로토콜 선택</label>
                            <select
                                className="mgmt-select"
                                value={protocol}
                                onChange={(e) => setProtocol(e.target.value)}
                                disabled={isScanning}
                                style={{ height: '42px', borderRadius: '10px', fontSize: '14px' }}
                            >
                                <option value="BACNET">BACnet/IP (추천)</option>
                                <option value="MODBUS_TCP">Modbus TCP (테스트용)</option>
                                <option value="MODBUS_RTU">Modbus RTU (시리얼)</option>
                            </select>
                            {protocol.startsWith('MODBUS') && (
                                <div style={{ marginTop: '8px', color: '#dd6b20', fontSize: '11px', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '4px' }}>
                                    <i className="fas fa-flask"></i> <span>현재 실험적 기능입니다. 수동 등록을 권장합니다.</span>
                                </div>
                            )}
                        </div>

                        <div className="mgmt-modal-form-group">
                            <label style={{ fontSize: '12px', fontWeight: 700, color: '#718096', marginBottom: '8px', textTransform: 'uppercase', letterSpacing: '0.02em' }}>
                                {protocol === 'MODBUS_RTU' ? '시리얼 포트 경로' : '네트워크 대역 (CIDR)'}
                            </label>
                            <input
                                type="text"
                                className="mgmt-input"
                                placeholder={protocol === 'MODBUS_RTU' ? '예: /dev/ttyS0' : '예: 192.168.1.0/24'}
                                value={range}
                                onChange={(e) => setRange(e.target.value)}
                                disabled={isScanning}
                                style={{ height: '42px', borderRadius: '10px', fontSize: '14px' }}
                            />
                            {protocol !== 'MODBUS_RTU' && (
                                <div style={{ marginTop: '8px', color: '#a0aec0', fontSize: '11px' }}>
                                    대역을 비워두면 로컬 네트워크 브로드캐스트를 사용합니다.
                                </div>
                            )}
                        </div>
                    </div>

                    {/* Section: Scan Results */}
                    <div style={{ marginBottom: '8px' }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                <div style={{ width: '4px', height: '16px', background: '#667eea', borderRadius: '2px' }}></div>
                                <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 700, color: '#2d3748' }}>검색 결과</h4>
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
                                    <span>검색 중</span> <i className="fas fa-circle-notch fa-spin"></i>
                                </div>
                            )}
                        </div>

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
                                    <div style={{ fontWeight: 700, color: '#4a5568', fontSize: '14px', marginBottom: '4px' }}>발견된 장치 없음</div>
                                    <div style={{ color: '#a0aec0', fontSize: '12px' }}>상단의 설정을 확인하고 스캔을 시작하세요.</div>
                                </div>
                            ) : discoveredDevices.length === 0 && isScanning ? (
                                <div style={{ padding: '48px 24px', textAlign: 'center' }}>
                                    <div style={{ marginBottom: '16px' }}>
                                        <i className="fas fa-search-location fa-beat" style={{ fontSize: '32px', color: '#667eea', opacity: 0.6 }}></i>
                                    </div>
                                    <div style={{ fontWeight: 700, color: '#4a5568', fontSize: '14px', marginBottom: '4px' }}>네트워크 탐색 중...</div>
                                    <div style={{ color: '#a0aec0', fontSize: '12px' }}>장치를 찾는 동안 잠시만 기다려 주세요.</div>
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
                    {!isScanning && (
                        <button
                            className="mgmt-btn mgmt-btn-primary"
                            onClick={handleScan}
                            style={{
                                height: '42px',
                                padding: '0 28px',
                                fontSize: '14px',
                                borderRadius: '10px',
                                fontWeight: 700,
                                boxShadow: '0 4px 12px rgba(102, 126, 234, 0.4)',
                                background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
                                border: 'none'
                            }}
                        >
                            <i className="fas fa-search" style={{ marginRight: '8px' }}></i>
                            스캔 시작
                        </button>
                    )}
                </div>
            </div>
        </div>
    );
};

export default NetworkScanModal;
