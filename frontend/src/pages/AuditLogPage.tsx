import React, { useState, useEffect, useCallback } from 'react';
import { AuditLogApiService } from '../api/services/auditLogApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import { AuditLog } from '../types/manufacturing';
import '../styles/management.css';

const AuditLogPage: React.FC = () => {
    const [logs, setLogs] = useState<AuditLog[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [searchTerm, setSearchTerm] = useState('');
    const [currentPage, setCurrentPage] = useState(1);
    const [pageSize, setPageSize] = useState(20);
    const [totalCount, setTotalCount] = useState(0);

    const [entityTypeFilter, setEntityTypeFilter] = useState('all');

    const loadLogs = useCallback(async () => {
        try {
            setLoading(true);
            const response = await AuditLogApiService.getAuditLogs({
                search: searchTerm || undefined,
                entity_type: entityTypeFilter !== 'all' ? entityTypeFilter : undefined,
                page: currentPage,
                limit: pageSize
            });
            if (response.success && response.data) {
                setLogs(response.data.items || []);
                setTotalCount(response.data.pagination?.total_count || 0);
            }
        } catch (err) {
            setError('Error loading audit logs.');
        } finally {
            setLoading(false);
        }
    }, [searchTerm, entityTypeFilter, currentPage, pageSize]);

    useEffect(() => {
        loadLogs();
    }, [loadLogs]);

    const getActionBadge = (action: string) => {
        switch (action) {
            case 'CREATE': return <span className="badge success">Created</span>;
            case 'UPDATE': return <span className="badge warning">Modified</span>;
            case 'DELETE': return <span className="badge danger">Deleted</span>;
            case 'INSTANTIATE': return <span className="badge primary">Instantiate</span>;
            case 'ACTION': return <span className="badge info">Action</span>;
            default: return <span className="badge neutral">{action}</span>;
        }
    };

    return (
        <ManagementLayout>
            <PageHeader
                title="System Audit Log"
                description="View system configuration changes and user activity history."
                icon="fas fa-clipboard-check"
            />

            <div className="mgmt-stats-panel">
                <StatCard label="Today's Logs" value={totalCount} type="primary" />
                <StatCard label="Threats Detected" value="0" type="error" />
                <StatCard label="Last Entry" value="1 min ago" type="neutral" />
            </div>

            <FilterBar
                searchTerm={searchTerm}
                onSearchChange={setSearchTerm}
                onReset={() => {
                    setSearchTerm('');
                    setEntityTypeFilter('all');
                }}
                activeFilterCount={(searchTerm ? 1 : 0) + (entityTypeFilter !== 'all' ? 1 : 0)}
                filters={[
                    {
                        label: 'Entity Type',
                        value: entityTypeFilter,
                        onChange: setEntityTypeFilter,
                        options: [
                            { value: 'all', label: 'All' },
                            { value: 'DEVICE', label: 'Device' },
                            { value: 'TEMPLATE', label: 'Template' },
                            { value: 'USER', label: 'User' },
                            { value: 'SYSTEM', label: 'System' }
                        ]
                    }
                ]}
            />

            <div className="mgmt-card" style={{ marginTop: '20px' }}>
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>Timestamp</th>
                            <th>Operator</th>
                            <th>Action</th>
                            <th>Target</th>
                            <th>Target Name</th>
                            <th>Details</th>
                        </tr>
                    </thead>
                    <tbody>
                        {loading ? (
                            <tr><td colSpan={6} style={{ textAlign: 'center', padding: '40px' }}>Loading...</td></tr>
                        ) : logs.length === 0 ? (
                            <tr><td colSpan={6} style={{ textAlign: 'center', padding: '40px' }}>No logs recorded.</td></tr>
                        ) : (
                            logs.map(log => (
                                <tr key={log.id}>
                                    <td style={{ whiteSpace: 'nowrap' }}>{new Date(log.created_at).toLocaleString()}</td>
                                    <td>{log.user_name || `User ID: ${log.user_id}`}</td>
                                    <td>{getActionBadge(log.action)}</td>
                                    <td><span className="badge outline">{log.entity_type}</span></td>
                                    <td style={{ fontWeight: '500' }}>{log.entity_name || log.entity_id}</td>
                                    <td style={{ fontSize: '12px', color: '#64748b' }}>{log.change_summary || '-'}</td>
                                </tr>
                            ))
                        )}
                    </tbody>
                </table>
            </div>

            <Pagination
                current={currentPage}
                total={totalCount}
                pageSize={pageSize}
                onChange={(page) => setCurrentPage(page)}
            />
        </ManagementLayout>
    );
};

export default AuditLogPage;
