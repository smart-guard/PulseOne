import React from 'react';
import { Device } from '../../../api/services/deviceApi';
import DeviceRow from './DeviceRow';

interface DeviceTableProps {
    devices: Device[];
    selectedDevices: number[];
    onDeviceSelect: (deviceId: number, selected: boolean) => void;
    onSelectAll: (selected: boolean) => void;
    onDeviceClick: (device: Device) => void;
    onEditDevice: (device: Device) => void;
    onStartWorker: (deviceId: number) => void;
    onStopWorker: (deviceId: number) => void;
    onRestartWorker: (deviceId: number) => void;
    onRestoreDevice: (device: Device) => void;
    onMoveDevice: (device: Device) => void;
    sortField: string;
    sortOrder: 'ASC' | 'DESC';
    onSort: (field: string) => void;
    getSortIcon: (field: string) => React.ReactNode;
    getSortableHeaderStyle: (field: string) => React.CSSProperties;
    tableBodyRef: React.RefObject<HTMLDivElement>;
    renderKey: number;
}

const DeviceTable: React.FC<DeviceTableProps> = ({
    devices,
    selectedDevices,
    onDeviceSelect,
    onSelectAll,
    onDeviceClick,
    onEditDevice,
    onStartWorker,
    onStopWorker,
    onRestartWorker,
    onRestoreDevice,
    onMoveDevice,
    sortField,
    sortOrder,
    onSort,
    getSortIcon,
    getSortableHeaderStyle,
    tableBodyRef,
    renderKey,
}) => {
    return (
        <div className="device-list-table-wrapper">
            <div
                key={`table-${renderKey}`}
                style={{
                    display: 'flex',
                    flexDirection: 'column',
                    height: '100%',
                    overflow: 'hidden'
                }}
            >
                {/* 정렬 기능이 있는 헤더 */}
                <div className="device-list-table-header">
                    <div className="device-list-checkbox-cell">
                        <input
                            type="checkbox"
                            checked={selectedDevices.length === devices.length && devices.length > 0}
                            onChange={(e) => onSelectAll(e.target.checked)}
                        />
                    </div>

                    <div
                        className="device-list-device-cell"
                        style={getSortableHeaderStyle('name')}
                        onClick={() => onSort('name')}
                    >
                        디바이스 명
                        {getSortIcon('name')}
                    </div>

                    <div className="device-list-template-cell">
                        마스터 모델 / 제조사
                    </div>

                    <div className="device-list-endpoint-cell">
                        접속 정보
                    </div>

                    <div className="device-list-status-cell">
                        상태
                    </div>

                    <div
                        className="device-list-data-cell"
                        style={getSortableHeaderStyle('data_point_count')}
                        onClick={() => onSort('data_point_count')}
                    >
                        포인트
                        {getSortIcon('data_point_count')}
                    </div>

                    <div className="device-list-actions-cell">작업</div>
                </div>

                {/* 바디 */}
                <div className="device-list-table-body" ref={tableBodyRef}>
                    {devices.map((device, index) => (
                        <DeviceRow
                            key={`device-${device.id}-${renderKey}`}
                            device={device}
                            isSelected={selectedDevices.includes(device.id)}
                            onSelect={(selected) => onDeviceSelect(device.id, selected)}
                            onClick={() => onDeviceClick(device)}
                            onEdit={() => onEditDevice(device)}
                            onStartWorker={() => onStartWorker(device.id)}
                            onStopWorker={() => onStopWorker(device.id)}
                            onRestartWorker={() => onRestartWorker(device.id)}
                            onRestore={() => onRestoreDevice(device)}
                            onMove={() => onMoveDevice(device)}
                        />
                    ))}
                </div>
            </div>
        </div>
    );
};

export default DeviceTable;
