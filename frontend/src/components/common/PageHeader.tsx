import React from 'react';
import '../../styles/management.css';

interface PageHeaderProps {
    title: string;
    description?: string;
    actions?: React.ReactNode;
    icon?: string;
    className?: string;
}

export const PageHeader: React.FC<PageHeaderProps> = ({
    title,
    description,
    actions,
    icon,
    className = ''
}) => {
    return (
        <div className={`mgmt-header ${className}`}>
            <div className="mgmt-header-info">
                <h1 className="mgmt-title">
                    {icon && <i className={icon}></i>}
                    {title}
                </h1>
                {description && <p className="mgmt-subtitle">{description}</p>}
            </div>
            {actions && (
                <div className="mgmt-header-actions">
                    {actions}
                </div>
            )}
        </div>
    );
};

export default PageHeader;
