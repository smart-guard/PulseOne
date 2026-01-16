import React from 'react';

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
                            {connectionStatus === 'connected' && 'API 연결됨'}
                            {connectionStatus === 'connecting' && 'API 연결중'}
                            {connectionStatus === 'disconnected' && 'API 연결 끊김'}
                        </span>
                        <span>
                            ({statistics.total_devices || 0}개 디바이스, {activeDevicesCount}개 활성)
                        </span>
                    </div>
                    <div>
                        마지막 업데이트: {lastRefresh.toLocaleTimeString()}
                    </div>
                    {!!statistics.rtu_masters && (
                        <div style={{ fontSize: '12px', color: '#6b7280' }}>
                            RTU: 마스터 {statistics.rtu_masters}개, 슬레이브 {statistics.rtu_slaves}개
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
                        자동 새로고침
                    </label>
                    {autoRefresh && (
                        <select
                            value={refreshInterval}
                            onChange={(e) => setRefreshInterval(Number(e.target.value))}
                            className="refresh-interval"
                        >
                            <option value={5}>5초</option>
                            <option value={10}>10초</option>
                            <option value={30}>30초</option>
                            <option value={60}>1분</option>
                        </select>
                    )}
                </div>

                <button
                    onClick={handleRefresh}
                    disabled={isLoading}
                    className="btn btn-outline"
                >
                    <span style={{ transform: isLoading ? 'rotate(360deg)' : 'none', transition: 'transform 1s linear' }}>🔄</span>
                    새로고침
                </button>

                <button
                    onClick={handleExportData}
                    disabled={exportDisabled}
                    className="btn btn-primary"
                >
                    📥 데이터 내보내기
                </button>
            </div>
        </div>
    );
};

export default ExplorerHeader;
