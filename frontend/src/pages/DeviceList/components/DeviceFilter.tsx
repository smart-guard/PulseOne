import React from 'react';

interface DeviceFilterProps {
    searchTerm: string;
    onSearchChange: (value: string) => void;
    statusFilter: string;
    onStatusChange: (value: string) => void;
    protocolFilter: string;
    onProtocolChange: (value: string) => void;
    connectionFilter: string;
    onConnectionChange: (value: string) => void;
    availableProtocols: string[];
}

const DeviceFilter: React.FC<DeviceFilterProps> = ({
    searchTerm,
    onSearchChange,
    statusFilter,
    onStatusChange,
    protocolFilter,
    onProtocolChange,
    connectionFilter,
    onConnectionChange,
    availableProtocols,
}) => {
    return (
        <div className="device-list-filters">
            <div className="device-list-search">
                <i className="fas fa-search"></i>
                <input
                    type="text"
                    placeholder="디바이스 이름, 설명, 제조사 검색..."
                    value={searchTerm}
                    onChange={(e) => onSearchChange(e.target.value)}
                />
            </div>

            <select value={statusFilter} onChange={(e) => onStatusChange(e.target.value)}>
                <option value="all">모든 상태</option>
                <option value="running">실행 중</option>
                <option value="stopped">중지됨</option>
                <option value="error">오류</option>
                <option value="disabled">비활성화</option>
            </select>

            <select value={protocolFilter} onChange={(e) => onProtocolChange(e.target.value)}>
                <option value="all">모든 프로토콜</option>
                {availableProtocols.map(protocol => (
                    <option key={protocol} value={protocol}>{protocol}</option>
                ))}
            </select>

            <select value={connectionFilter} onChange={(e) => onConnectionChange(e.target.value)}>
                <option value="all">모든 연결상태</option>
                <option value="connected">연결됨</option>
                <option value="disconnected">연결 끊김</option>
                <option value="error">연결 오류</option>
            </select>
        </div>
    );
};

export default DeviceFilter;
