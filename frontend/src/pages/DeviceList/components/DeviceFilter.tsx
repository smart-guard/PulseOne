import React from 'react';
import { useTranslation } from 'react-i18next';

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
    const { t } = useTranslation(['devices']);
    return (
        <div className="device-list-filters">
            <div className="device-list-search">
                <i className="fas fa-search"></i>
                <input
                    type="text"
                    placeholder={t('devices:basicInfo.deviceNamePlaceholder')}
                    value={searchTerm}
                    onChange={(e) => onSearchChange(e.target.value)}
                />
            </div>

            <select value={statusFilter} onChange={(e) => onStatusChange(e.target.value)}>
                <option value="all">{t('devices:filter.all')}</option>
                <option value="running">{t('devices:services.running', 'Running')}</option>
                <option value="stopped">{t('devices:services.stopped', 'Stopped')}</option>
                <option value="error">{t('devices:filter.error')}</option>
                <option value="disabled">Disabled</option>
            </select>

            <select value={protocolFilter} onChange={(e) => onProtocolChange(e.target.value)}>
                <option value="all">{t('devices:allProtocols')}</option>
                {availableProtocols.map(protocol => (
                    <option key={protocol} value={protocol}>{protocol}</option>
                ))}
            </select>

            <select value={connectionFilter} onChange={(e) => onConnectionChange(e.target.value)}>
                <option value="all">{t('devices:filter.all')}</option>
                <option value="connected">{t('devices:filter.connected')}</option>
                <option value="disconnected">{t('devices:filter.disconnected')}</option>
                <option value="error">{t('devices:filter.error')}</option>
            </select>
        </div>
    );
};

export default DeviceFilter;
