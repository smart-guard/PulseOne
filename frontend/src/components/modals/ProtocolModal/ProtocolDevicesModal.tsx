import React, { useState, useEffect, useCallback } from 'react';
import { createPortal } from 'react-dom';
import { DeviceApiService, Device } from '../../../api/services/deviceApi';
import { Protocol } from '../../../api/services/protocolApi';
import { Pagination } from '../../common/Pagination';
import '../../../styles/management.css';

interface ProtocolDevicesModalProps {
    isOpen: boolean;
    onClose: () => void;
    protocol: Protocol | null;
}

export const ProtocolDevicesModal: React.FC<ProtocolDevicesModalProps> = ({ isOpen, onClose, protocol }) => {
    const [devices, setDevices] = useState<Device[]>([]);
    const [loading, setLoading] = useState(false);
    const [currentPage, setCurrentPage] = useState(1);
    const [pageSize, setPageSize] = useState(8);
    const [totalCount, setTotalCount] = useState(0);

    const loadDevices = useCallback(async () => {
        if (!protocol) return;
        try {
            setLoading(true);
            const res = await DeviceApiService.getDevices({
                protocol_id: protocol.id,
                page: currentPage,
                limit: pageSize
            });
            if (res.success && res.data) {
                setDevices(res.data.items || []);
                // Use any-cast to bypass strict typing for dynamic pagination fields if they differ from interface
                const pagination = (res.data as any).pagination;
                setTotalCount(pagination?.total || pagination?.total_count || pagination?.totalItems || 0);
            }
        } catch (err) {
            console.error('Failed to load devices for protocol:', err);
        } finally {
            setLoading(false);
        }
    }, [protocol, currentPage, pageSize]);

    useEffect(() => {
        if (isOpen && protocol) {
            loadDevices();
        } else {
            setDevices([]);
            setCurrentPage(1);
        }
    }, [isOpen, protocol, loadDevices]);

    if (!isOpen || !protocol) return null;

    return createPortal(
        <div className="mgmt-modal-overlay" onClick={onClose}>
            <div
                className="mgmt-modal-container"
                style={{ maxWidth: '800px', width: '90%' }}
                onClick={e => e.stopPropagation()}
            >
                <div className="mgmt-modal-header">
                    <div className="mgmt-header-content">
                        <i className="fas fa-network-wired mgmt-header-icon"></i>
                        <h3>{protocol.display_name} 사용 디바이스 목록</h3>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose} aria-label="닫기">
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body" style={{ padding: '24px', minHeight: '400px', display: 'flex', flexDirection: 'column' }}>
                    {loading ? (
                        <div style={{ textAlign: 'center', padding: '60px 0', color: 'var(--neutral-500)', flex: 1 }}>
                            <i className="fas fa-spinner fa-spin fa-3x"></i>
                            <p style={{ marginTop: '16px', fontSize: '15px' }}>디바이스 목록을 불러오는 중...</p>
                        </div>
                    ) : devices.length === 0 ? (
                        <div style={{ textAlign: 'center', padding: '80px 0', color: 'var(--neutral-400)', flex: 1 }}>
                            <i className="fas fa-laptop-medical fa-4x" style={{ marginBottom: '20px', opacity: 0.3 }}></i>
                            <p style={{ fontSize: '16px' }}>이 프로토콜을 사용하는 디바이스가 없습니다.</p>
                        </div>
                    ) : (
                        <>
                            <div style={{ flex: 1 }}>
                                <table className="mgmt-table">
                                    <thead>
                                        <tr>
                                            <th>디바이스명</th>
                                            <th>제조사/모델</th>
                                            <th>설치 사이트</th>
                                            <th>앤드포인트</th>
                                            <th>상태</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {devices.map(device => (
                                            <tr key={device.id}>
                                                <td style={{ fontWeight: '600', color: 'var(--neutral-900)' }}>{device.name}</td>
                                                <td style={{ fontSize: '13px', color: 'var(--neutral-600)' }}>
                                                    {device.manufacturer || '-'} / {device.model || '-'}
                                                </td>
                                                <td>
                                                    <span className="mgmt-site-badge" style={{
                                                        background: 'var(--primary-50)',
                                                        color: 'var(--primary-700)',
                                                        padding: '2px 8px',
                                                        borderRadius: '4px',
                                                        fontSize: '12px',
                                                        fontWeight: '500'
                                                    }}>
                                                        {device.site_name || 'Global'}
                                                    </span>
                                                </td>
                                                <td style={{ fontFamily: 'monospace', fontSize: '12px', color: 'var(--neutral-500)' }}>
                                                    {device.endpoint}
                                                </td>
                                                <td>
                                                    <span className={`mgmt-badge ${device.is_enabled ? 'success' : 'neutral'}`}>
                                                        {device.is_enabled ? '활성' : '비활성'}
                                                    </span>
                                                </td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            </div>

                            <div className="modal-pagination" style={{ marginTop: '24px', paddingTop: '16px', borderTop: '1px solid var(--neutral-100)', display: 'flex', justifyContent: 'center' }}>
                                <Pagination
                                    current={currentPage}
                                    total={totalCount}
                                    pageSize={pageSize}
                                    onChange={(page) => setCurrentPage(page)}
                                    showSizeChanger={false}
                                />
                            </div>
                        </>
                    )}
                </div>

                <div className="mgmt-modal-footer" style={{ borderTop: '1px solid var(--neutral-100)', padding: '16px 24px', backgroundColor: 'var(--neutral-50)' }}>
                    <button className="mgmt-btn mgmt-btn-secondary" onClick={onClose} style={{ minWidth: '100px' }}>닫기</button>
                </div>
            </div>

            <style>{`
        .mgmt-header-content {
          display: flex;
          align-items: center;
          gap: 12px;
        }
        .mgmt-header-icon {
          font-size: 20px;
          color: var(--primary-500);
        }
        .mgmt-site-badge {
          display: inline-block;
        }
        .mgmt-modal-container {
          background: white;
          border-radius: 12px;
          box-shadow: var(--shadow-xl);
          overflow: hidden;
          animation: modal-fade-in 0.2s ease-out;
        }
        @keyframes modal-fade-in {
          from { opacity: 0; transform: translateY(20px); }
          to { opacity: 1; transform: translateY(0); }
        }
      `}</style>
        </div>,
        document.body
    );
};
