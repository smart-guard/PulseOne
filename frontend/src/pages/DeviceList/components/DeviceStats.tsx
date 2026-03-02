import React from 'react';
import { useTranslation } from 'react-i18next';
import { DeviceStats as DeviceStatsType } from '../../../api/services/deviceApi';
import StatCard from '../../../components/common/StatCard';

interface DeviceStatsProps {
    stats: DeviceStatsType | null;
}

const DeviceStats: React.FC<DeviceStatsProps> = ({ stats }) => {
    const { t } = useTranslation(['devices']);
    if (!stats) return null;

    return (
        <div className="device-list-stats">
            <StatCard
                title={t('devices:totalDevices')}
                value={stats.total_devices || 0}
                icon={<i className="fas fa-network-wired"></i>}
                color="#3b82f6"
            />
            <StatCard
                title={t('devices:filter.connected')}
                value={stats.connected_devices || 0}
                icon={<i className="fas fa-check-circle"></i>}
                color="#10b981"
            />
            <StatCard
                title={t('devices:filter.disconnected')}
                value={stats.disconnected_devices || 0}
                icon={<i className="fas fa-times-circle"></i>}
                color="#ef4444"
            />
            <StatCard
                title={t('devices:filter.error')}
                value={stats.error_devices || 0}
                icon={<i className="fas fa-exclamation-triangle"></i>}
                color="#f59e0b"
            />
        </div>
    );
};

export default DeviceStats;
