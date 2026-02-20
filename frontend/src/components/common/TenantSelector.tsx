import React, { useState, useEffect } from 'react';
import { TenantApiService } from '../../api/services/tenantApi';
import { Tenant } from '../../types/common';
import { LogManager } from '../../utils/LogManager'; // Assuming this exists or using console

interface TenantSelectorProps {
    onTenantChange?: (tenantId: number | null) => void;
}

export const TenantSelector: React.FC<TenantSelectorProps> = ({ onTenantChange }) => {
    const [tenants, setTenants] = useState<Tenant[]>([]);
    const [selectedTenantId, setSelectedTenantId] = useState<number | null>(() => {
        const saved = localStorage.getItem('selected_tenant_id');
        return saved ? parseInt(saved, 10) : null;
    });
    const [isLoading, setIsLoading] = useState(false);
    const [isAdmin, setIsAdmin] = useState(false);

    useEffect(() => {
        // Check if user is sys_admin
        // For now, we'll check localStorage or a global flag. 
        // Ideally this should come from an AuthContext.
        const userRole = localStorage.getItem('user_role');
        if (userRole === 'system_admin') {
            setIsAdmin(true);
            fetchTenants();
        }
    }, []);

    const fetchTenants = async () => {
        setIsLoading(true);
        try {
            const response = await TenantApiService.getTenants();
            if (response.success && response.data) {
                setTenants(response.data.items);
            }
        } catch (error) {
            console.error('[TenantSelector] Failed to fetch tenants:', error);
        } finally {
            setIsLoading(false);
        }
    };

    const handleTenantChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
        const val = e.target.value;
        const newId = val ? parseInt(val, 10) : null;

        setSelectedTenantId(newId);

        if (newId) {
            localStorage.setItem('selected_tenant_id', newId.toString());
        } else {
            localStorage.removeItem('selected_tenant_id');
        }

        if (onTenantChange) {
            onTenantChange(newId);
        }

        // Refresh page to apply new tenant context across all API calls
        window.location.reload();
    };

    if (!isAdmin) return null;

    return (
        <div className="tenant-selector" style={{ display: 'flex', alignItems: 'center', gap: '8px', marginRight: '16px' }}>
            <i className="fas fa-building" style={{ color: '#6b7280', fontSize: '14px' }}></i>
            <select
                value={selectedTenantId || ''}
                onChange={handleTenantChange}
                style={{
                    padding: '4px 8px',
                    borderRadius: '6px',
                    border: '1px solid #d1d5db',
                    background: 'white',
                    fontSize: '13px',
                    color: '#374151',
                    outline: 'none',
                    cursor: 'pointer'
                }}
                disabled={isLoading}
            >
                <option value="">전체 테넌트 (기본)</option>
                {tenants.map(t => (
                    <option key={t.id} value={t.id}>
                        {t.company_name} ({t.company_code})
                    </option>
                ))}
            </select>
            {isLoading && <i className="fas fa-spinner fa-spin" style={{ fontSize: '12px', color: '#9ca3af' }}></i>}
        </div>
    );
};
