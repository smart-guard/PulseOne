import React, { useState } from 'react';
import { DeviceApiService } from '../../api/services/deviceApi';
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
    const [timeout, setTimeout] = useState(5000);
    const [isScanning, setIsScanning] = useState(false);
    const [error, setError] = useState<string | null>(null);

    if (!isOpen) return null;

    const handleScan = async () => {
        if (!range) {
            setError('IP 범위(CIDR) 또는 포트 경로를 입력해주세요.');
            return;
        }

        try {
            setIsScanning(true);
            setError(null);

            // Call Scan API
            const response = await DeviceApiService.scanNetwork({
                protocol,
                range,
                timeout
            });

            if (response.success) {
                await confirm({
                    title: '스캔 시작됨',
                    message: `${protocol} 네트워크 스캔이 시작되었습니다. 결과는 자동으로 장치 목록에 추가됩니다.`,
                    confirmText: '확인',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                onClose();
                if (onSuccess) onSuccess();
            } else {
                throw new Error(response.error || '스캔 시작 실패');
            }
        } catch (err: any) {
            setError(err.message || '스캔 중 오류가 발생했습니다.');
        } finally {
            setIsScanning(false);
        }
    };

    return (
        <div className="modal-overlay">
            <div className="modal-content network-scan-modal" style={{ width: '550px', borderRadius: '12px', overflow: 'hidden' }}>
                <div className="modal-header" style={{ padding: '20px 24px', background: '#f8f9fa', borderBottom: '1px solid #eee' }}>
                    <h3 style={{ margin: 0, fontSize: '1.25rem', color: '#1a202c', fontWeight: 600 }}>네트워크 장치 스캔</h3>
                    <button className="btn-close" onClick={onClose} disabled={isScanning} style={{ padding: '8px', borderRadius: '50%', background: 'transparent', border: 'none', cursor: 'pointer', color: '#718096' }}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="modal-body" style={{ padding: '24px' }}>
                    {error && (
                        <div className="alert alert-danger" style={{ marginBottom: '20px', padding: '12px 16px', borderRadius: '8px', fontSize: '0.9rem', display: 'flex', alignItems: 'center', gap: '10px' }}>
                            <i className="fas fa-exclamation-circle"></i> {error}
                        </div>
                    )}

                    <div className="form-group" style={{ marginBottom: '20px' }}>
                        <label style={{ display: 'block', marginBottom: '8px', fontSize: '0.9rem', fontWeight: 500, color: '#4a5568' }}>스캔 프로토콜</label>
                        <select
                            className="form-control"
                            value={protocol}
                            onChange={(e) => setProtocol(e.target.value)}
                            disabled={isScanning}
                            style={{ width: '100%', padding: '10px 12px', borderRadius: '8px', border: '1px solid #e2e8f0', background: 'white', color: '#2d3748', fontSize: '1rem' }}
                        >
                            <option value="BACNET">BACnet/IP</option>
                            <option value="MODBUS_TCP">Modbus TCP</option>
                            <option value="MODBUS_RTU">Modbus RTU (Serial)</option>
                        </select>
                        <small style={{ display: 'block', marginTop: '6px', fontSize: '0.8rem', color: '#718096' }}>사용 가능한 산업용 통신 프로토콜을 선택하세요.</small>
                    </div>

                    <div className="form-group" style={{ marginBottom: '20px' }}>
                        <label style={{ display: 'block', marginBottom: '8px', fontSize: '0.9rem', fontWeight: 500, color: '#4a5568' }}>
                            {protocol === 'MODBUS_RTU' ? '시리얼 포트 경로' : '네트워크 대역 (CIDR)'}
                        </label>
                        <input
                            type="text"
                            className="form-control"
                            placeholder={protocol === 'MODBUS_RTU' ? '예: /dev/ttyS0' : '예: 192.168.1.0/24'}
                            value={range}
                            onChange={(e) => setRange(e.target.value)}
                            disabled={isScanning}
                            style={{ width: '100%', padding: '10px 12px', borderRadius: '8px', border: '1px solid #e2e8f0', background: 'white', color: '#2d3748', fontSize: '1rem' }}
                        />
                        <small style={{ display: 'block', marginTop: '6px', fontSize: '0.8rem', color: '#718096' }}>
                            {protocol === 'MODBUS_RTU'
                                ? '스캔할 장비의 시리얼 포트 주소 (/dev/...)를 입력하세요.'
                                : '스캔 범위를 CIDR 표기법으로 입력하세요 (예: /24).'}
                        </small>
                    </div>

                    <div className="form-group">
                        <label style={{ display: 'block', marginBottom: '8px', fontSize: '0.9rem', fontWeight: 500, color: '#4a5568' }}>스캔 타임아웃 (ms)</label>
                        <input
                            type="number"
                            className="form-control"
                            value={timeout}
                            onChange={(e) => setTimeout(Number(e.target.value))}
                            disabled={isScanning}
                            min={1000}
                            step={1000}
                            style={{ width: '100%', padding: '10px 12px', borderRadius: '8px', border: '1px solid #e2e8f0', background: 'white', color: '#2d3748', fontSize: '1rem' }}
                        />
                        <small style={{ display: 'block', marginTop: '6px', fontSize: '0.8rem', color: '#718096' }}>장치 응답을 대기할 최대 시간입니다.</small>
                    </div>
                </div>

                <div className="modal-footer" style={{ padding: '20px 24px', background: '#f8f9fa', borderTop: '1px solid #eee', display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                    <button
                        className="btn-link"
                        onClick={onClose}
                        disabled={isScanning}
                        style={{ padding: '10px 16px', borderRadius: '8px', border: 'none', background: 'transparent', cursor: 'pointer', color: '#718096', fontWeight: 500 }}
                    >
                        취소
                    </button>
                    <button
                        className="btn-primary"
                        onClick={handleScan}
                        disabled={isScanning}
                        style={{
                            padding: '10px 24px',
                            borderRadius: '8px',
                            border: 'none',
                            background: isScanning ? '#cbd5e0' : 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
                            cursor: isScanning ? 'not-allowed' : 'pointer',
                            color: 'white',
                            fontWeight: 600,
                            boxShadow: '0 4px 6px rgba(0,0,0,0.1)',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '8px'
                        }}
                    >
                        {isScanning ? (
                            <>
                                <i className="fas fa-spinner fa-spin"></i> 스캔 수행 중...
                            </>
                        ) : (
                            <>
                                <i className="fas fa-search"></i> 스캔 시작
                            </>
                        )}
                    </button>
                </div>
            </div>
        </div>
    );
};

export default NetworkScanModal;
