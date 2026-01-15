import React from 'react';
import { DeviceStats as DeviceStatsType } from '../../../api/services/deviceApi';
import StatCard from '../../../components/common/StatCard';

interface DeviceStatsProps {
    stats: DeviceStatsType | null;
}

const DeviceStats: React.FC<DeviceStatsProps> = ({ stats }) => {
    if (!stats) return null;

    return (
        <div className="device-list-stats">
            <StatCard
                title="전체 디바이스"
                value={stats.total_devices || 0}
                icon={<i className="fas fa-network-wired"></i>}
                color="#3b82f6"
            />
            <StatCard
                title="연결됨"
                value={stats.connected_devices || 0}
                icon={<i className="fas fa-check-circle"></i>}
                color="#10b981"
            />
            <StatCard
                title="연결 끊김"
                value={stats.disconnected_devices || 0}
                icon={<i className="fas fa-times-circle"></i>}
                color="#ef4444"
            />
            <StatCard
                title="오류"
                value={stats.error_devices || 0}
                icon={<i className="fas fa-exclamation-triangle"></i>}
                color="#f59e0b"
            />
        </div>
    );
};

export default DeviceStats;
