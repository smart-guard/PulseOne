import React from 'react';

interface StatusBadgeProps {
    status: string;
    type?: 'active' | 'inactive' | 'online' | 'offline' | 'error' | 'warning' | 'neutral';
    label?: string;
    className?: string;
    showDot?: boolean;
    pulse?: boolean;
}

export const StatusBadge: React.FC<StatusBadgeProps> = ({
    status,
    type,
    label,
    className = '',
    showDot = true,
    pulse = false
}) => {
    // 상태별 타입 자동 매핑
    const getMappedType = () => {
        if (type) return type;
        const s = status.toLowerCase();
        if (s.includes('active') || s.includes('running') || s === '사용중') return 'active';
        if (s.includes('inactive') || s.includes('stopped') || s === '정지') return 'inactive';
        if (s.includes('online')) return 'online';
        if (s.includes('offline')) return 'offline';
        if (s.includes('error') || s.includes('fail')) return 'error';
        if (s.includes('warning') || s.includes('paused')) return 'warning';
        return 'neutral';
    };

    const badgeType = getMappedType();
    const displayLabel = label || status;

    return (
        <div className={`status-indicator ${badgeType} ${className}`}>
            {showDot && (
                <span className={`status-dot ${badgeType} ${pulse || badgeType === 'online' ? 'pulse' : ''}`}></span>
            )}
            <span className={`status-text ${badgeType}`}>
                {displayLabel}
            </span>
        </div>
    );
};

export default StatusBadge;
