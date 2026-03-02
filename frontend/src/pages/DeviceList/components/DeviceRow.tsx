import React from 'react';
import { useTranslation } from 'react-i18next';
import { Device } from '../../../api/services/deviceApi';

interface DeviceRowProps {
    device: Device;
    isSelected: boolean;
    onSelect: (selected: boolean) => void;
    onClick: () => void;
    onEdit: () => void;
    onStartWorker: () => void;
    onStopWorker: () => void;
    onRestartWorker: () => void;
    onRestore: () => void;
    onMove: () => void;
}

const DeviceRow: React.FC<DeviceRowProps> = ({
    device,
    isSelected,
    onSelect,
    onClick,
    onEdit,
    onStartWorker,
    onStopWorker,
    onRestartWorker,
    onRestore,
    onMove,
}) => {
    const { t } = useTranslation(['devices']);
    const getProtocolBadgeClass = (protocolType: string) => {
        const protocol = protocolType?.toUpperCase() || 'UNKNOWN';
        switch (protocol) {
            case 'MODBUS_TCP':
            case 'MODBUS_RTU':
                return 'device-list-protocol-badge device-list-protocol-modbus';
            case 'BACNET':
                return 'device-list-protocol-badge device-list-protocol-bacnet';
            default:
                return 'device-list-protocol-badge device-list-protocol-unknown';
        }
    };

    const getProtocolDisplayName = (protocolType: string) => {
        const protocol = protocolType?.toUpperCase() || 'UNKNOWN';
        switch (protocol) {
            case 'MODBUS_TCP': return 'MODBUS TCP';
            case 'MODBUS_RTU': return 'MODBUS RTU';
            case 'BACNET': return 'BACnet/IP';
            default: return protocol || 'Unknown';
        }
    };

    const getStatusBadgeClass = (status: string) => {
        switch (status) {
            case 'connected': return 'device-list-status-badge device-list-status-connected';
            case 'disconnected':
            case 'error': return 'device-list-status-badge device-list-status-disconnected';
            default: return 'device-list-status-badge device-list-status-unknown';
        }
    };

    // Collector Worker 상태 감지
    const getWorkerStatusInfo = () => {
        const status = (device.collector_status?.status || 'unknown').toLowerCase();

        let label = status;
        if (status === 'running') label = 'Running';
        else if (status === 'paused') label = 'Paused';
        else if (status === 'stopped') label = 'Stopped';
        else if (status === 'error') label = t('devices:filter.error');
        else if (status === 'initializing') label = 'Initializing';
        else if (status === 'unknown') label = 'Unknown';

        return { status, label };
    };

    const { status: workerStatus, label: workerLabel } = getWorkerStatusInfo();

    const isDeleted = device.is_deleted === 1;

    return (
        <div className={`device-list-table-row ${isDeleted ? 'device-deleted-row' : ''}`} style={isDeleted ? { opacity: 0.6, backgroundColor: 'var(--neutral-50)' } : undefined}>
            <div className="device-list-checkbox-cell">
                <input
                    type="checkbox"
                    checked={isSelected}
                    onChange={(e) => onSelect(e.target.checked)}
                />
            </div>

            <div className="device-list-device-cell" onClick={onClick} style={{ cursor: 'pointer' }}>
                <div style={{ display: 'flex', alignItems: 'center' }}>
                    <div className="device-list-device-icon" style={{ marginRight: '10px' }}>
                        <i className="fas fa-microchip"></i>
                    </div>
                    <div className="device-list-device-name clickable-name" style={{ fontWeight: 600 }}>{device.name}</div>
                </div>
            </div>

            <div className="device-list-template-cell">
                <div className="device-list-cell-text">
                    <div style={{ fontWeight: 600, fontSize: '13px', color: device.template_name ? 'var(--neutral-900)' : 'var(--neutral-400)' }}>
                        {device.template_name || t('devices:manualRegister')}
                    </div>
                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '2px' }}>
                        {device.manufacturer || '-'} / {device.model || '-'}
                    </div>
                </div>
            </div>

            <div className="device-list-endpoint-cell">
                <div className="device-list-cell-text" style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
                    <div style={{ fontFamily: 'monospace', fontSize: '12px', fontWeight: 600, color: 'var(--neutral-800)' }}>
                        {device.endpoint}
                    </div>
                    <div>
                        <span className={getProtocolBadgeClass(device.protocol_type)} style={{ fontSize: '10px', padding: '1px 6px' }}>
                            {getProtocolDisplayName(device.protocol_type)}
                        </span>
                    </div>
                </div>
            </div>

            <div className="device-list-status-cell">
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                    <div style={{ display: 'flex', alignItems: 'center' }}>
                        <span className={`device-list-status-indicator status-${workerStatus} ${workerStatus === 'running' ? 'status-pulse-success' : ''}`}></span>
                        <span style={{ fontSize: '12px', fontWeight: 500 }}>{workerLabel}</span>
                    </div>
                    <div>
                        <span className={getStatusBadgeClass(device.connection_status)}>
                            {device.connection_status === 'connected' ? t('devices:filter.connected') : t('devices:filter.disconnected')}
                        </span>
                    </div>
                </div>
            </div>

            <div className="device-list-data-cell">
                <div className="device-list-point-count">
                    <strong>{device.data_point_count || 0}</strong> Pts
                </div>
            </div>

            <div className="device-list-actions-cell">
                <div className="device-list-row-actions">
                    {isDeleted ? (
                        <button className="mgmt-btn-icon success" onClick={onRestore} title={t('devices:restore.confirmTitle')}>
                            <i className="fas fa-undo"></i>
                        </button>
                    ) : (
                        <>
                            <button className="mgmt-btn-icon" onClick={onMove} title={t('devices:moveGroup')}>
                                <i className="fas fa-exchange-alt"></i>
                            </button>

                            {(device.collector_status?.status === 'running' || device.collector_status?.status === 'paused') ? (
                                <>
                                    <button className="mgmt-btn-icon error" onClick={onStopWorker} title={t('devices:worker.stop')}>
                                        <i className="fas fa-stop"></i>
                                    </button>
                                    <button className="mgmt-btn-icon warning" onClick={onRestartWorker} title={t('devices:worker.restart')}>
                                        <i className="fas fa-sync-alt"></i>
                                    </button>
                                </>
                            ) : (
                                <button className="mgmt-btn-icon success" onClick={onStartWorker} title={t('devices:worker.start')}>
                                    <i className="fas fa-play"></i>
                                </button>
                            )}
                        </>
                    )}
                </div>
            </div>
        </div>
    );
};

export default DeviceRow;
