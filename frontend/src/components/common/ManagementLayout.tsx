import React from 'react';
import '../../styles/management.css';

interface ManagementLayoutProps {
    children: React.ReactNode;
    className?: string;
}

export const ManagementLayout: React.FC<ManagementLayoutProps> = ({
    children,
    className = ''
}) => {
    return (
        <div className={`management-container ${className}`}>
            {children}
        </div>
    );
};

export default ManagementLayout;
