import React from 'react';
import { useTranslation } from 'react-i18next';

interface ExplorerHeaderProps {
    connectionStatus: 'connected' | 'connecting' | 'disconnected';
    statistics: {
        total_devices: number;
        rtu_masters?: number;
        rtu_slaves?: number;
    };
    activeDevicesCount: number;
    lastRefresh: Date;
    autoRefresh: boolean;
    setAutoRefresh: (val: boolean) => void;
    refreshInterval: number;
    setRefreshInterval: (val: number) => void;
    handleRefresh: () => void;
    handleExportData: () => void;
    isLoading: boolean;
    exportDisabled: boolean;
}

const ExplorerHeader: React.FC<ExplorerHeaderProps> = ({
    connectionStatus,
    statistics,
    activeDevicesCount,
    lastRefresh,
    autoRefresh,
    setAutoRefresh,
    refreshInterval,
    setRefreshInterval,
    handleRefresh,
    handleExportData,
    isLoading,
    exportDisabled,
}) => {
    const { t } = useTranslation(['dataExplorer', 'common']);

    return (
        <div className="page-header">
            <div className="header-left">
                <h1 className="page-title">
                    📊 PulseOne Data Explorer
                </h1>
                <div className="header-meta">
                    <div className={`connection-status status-${connectionStatus}`}>
                        <span>
                            {connectionStatus === 'connected' && '✅'}
                            {connectionStatus === 'connecting' && '🔄'}
                            {connectionStatus === 'disconnected' && '❌'}
                        </span>
                        <span>
                            {connectionStatus === 'connected' && t('status.connected', { ns: 'dataExplorer' })}
                            {connectionStatus === 'connecting' && t('status.connecting', { ns: 'dataExplorer' })}
                            {connectionStatus === 'disconnected' && t('status.disconnected', { ns: 'dataExplorer' })}
                        </span>
                        <span>
                            ({statistics.total_devices || 0} {t('status.devices', { ns: 'dataExplorer' })}, {activeDevicesCount} {t('status.active', { ns: 'dataExplorer' })})
                        </span>
                    </div>
                    <div>
                        {t('lastUpdated', { ns: 'dataExplorer' })}: {lastRefresh.toLocaleTimeString()}
                    </div>
                    {!!statistics.rtu_masters && (
                        <div style={{ fontSize: '12px', color: '#6b7280' }}>
                            RTU: {t('rtuMaster', { ns: 'dataExplorer' })} {statistics.rtu_masters}, {t('rtuSlave', { ns: 'dataExplorer' })} {statistics.rtu_slaves}
                        </div>
                    )}
                </div>
            </div>

            <div className="page-actions">
                <div className="auto-refresh-control">
                    <label className="refresh-toggle">
                        <input
                            type="checkbox"
                            checked={autoRefresh}
                            onChange={(e) => setAutoRefresh(e.target.checked)}
                        />
                        {t('autoRefresh', { ns: 'dataExplorer' })}
                    </label>
                    {autoRefresh && (
                        <select
                            value={refreshInterval}
                            onChange={(e) => setRefreshInterval(Number(e.target.value))}
                            className="refresh-interval"
                        >
                            <option value={5}>5{t('sec', { ns: 'dataExplorer' })}</option>
                            <option value={10}>10{t('sec', { ns: 'dataExplorer' })}</option>
                            <option value={30}>30{t('sec', { ns: 'dataExplorer' })}</option>
                            <option value={60}>1{t('min', { ns: 'dataExplorer' })}</option>
                        </select>
                    )}
                </div>

                <button
                    onClick={handleRefresh}
                    disabled={isLoading}
                    className="btn btn-outline"
                >
                    <span style={{ transform: isLoading ? 'rotate(360deg)' : 'none', transition: 'transform 1s linear' }}>🔄</span>
                    {t('refresh', { ns: 'common' })}
                </button>

                <button
                    onClick={handleExportData}
                    disabled={exportDisabled}
                    className="btn btn-primary"
                >
                    📥 {t('exportData', { ns: 'dataExplorer' })}
                </button>
            </div>
        </div>
    );
};

export default ExplorerHeader;
