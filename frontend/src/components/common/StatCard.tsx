import React from 'react';
import '../../styles/management.css';

interface StatCardProps {
    title?: string;
    label?: string; // Compatibility
    value: string | number;
    icon?: string | React.ReactNode;
    type?: 'primary' | 'success' | 'warning' | 'error' | 'neutral';
    color?: string; // Manual color override
    className?: string;
}

export const StatCard: React.FC<StatCardProps> = ({
    title,
    label,
    value,
    icon,
    type = 'primary',
    color,
    className = ''
}) => {
    const displayLabel = title || label;

    return (
        <div
            className={`mgmt-stat-card ${type} ${className}`}
            style={color ? { borderTopColor: color } : {}}
        >
            {icon && (
                <div className="mgmt-stat-icon">
                    {typeof icon === 'string' ? <i className={icon}></i> : icon}
                </div>
            )}
            <div className="mgmt-stat-info">
                <div className="mgmt-stat-value">{value}</div>
                <div className="mgmt-stat-label">{displayLabel}</div>
            </div>
        </div>
    );
};

export default StatCard;
