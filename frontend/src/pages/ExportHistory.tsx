// ============================================================================
// frontend/src/pages/ExportHistory.tsx
// 내보내기 이력 (Export History) 페이지
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo, useRef } from 'react';
import exportGatewayApi, { ExportLog, ExportLogStatistics, ExportTarget, Gateway } from '../api/services/exportGatewayApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import LogDetailModal from '../components/modals/LogDetailModal';
import { usePagination } from '../hooks/usePagination';
import '../styles/management.css';
import '../styles/pagination.css';

// -----------------------------------------------------------------------------
// Type Definitions
// -----------------------------------------------------------------------------

interface FilterOptions {
    dateRange: {
        start: Date;
        end: Date;
    };
    status: string;
    target_type: string;
    target_id: string;
    gateway_id: string;
}

// -----------------------------------------------------------------------------
// Component
// -----------------------------------------------------------------------------

const ExportHistory: React.FC = () => {
    // 1. State Management
    const [logs, setLogs] = useState<ExportLog[]>([]);
    const [statistics, setStatistics] = useState<ExportLogStatistics | null>(null);
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [gateways, setGateways] = useState<Gateway[]>([]);

    const [isInitialLoading, setIsInitialLoading] = useState(true);
    const [isRefreshing, setIsRefreshing] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [activeRowId, setActiveRowId] = useState<number | null>(null);
    const [selectedLog, setSelectedLog] = useState<ExportLog | null>(null);

    // Filters
    const [filters, setFilters] = useState<FilterOptions>({
        dateRange: {
            start: new Date(Date.now() - 24 * 60 * 60 * 1000), // Last 24 hours
            end: new Date()
        },
        status: 'all',
        target_type: 'all',
        target_id: 'all',
        gateway_id: 'all'
    });

    // Pagination Hook
    const pagination = usePagination({
        initialPage: 1,
        initialPageSize: 20,
        totalCount: 0
    });

    // Refs
    const containerRef = useRef<HTMLDivElement>(null);
    const [scrollPosition, setScrollPosition] = useState(0);

    // ---------------------------------------------------------------------------
    // Data Fetching
    // ---------------------------------------------------------------------------

    // Fetch Targets & Gateways (for filter dropdown)
    useEffect(() => {
        const fetchFilters = async () => {
            try {
                const [targetRes, gatewayRes] = await Promise.all([
                    exportGatewayApi.getTargets(),
                    exportGatewayApi.getGateways({ limit: 100 }) // Fetch reasonable amount
                ]); // Parallel fetch

                if (targetRes.success && targetRes.data) {
                    setTargets(targetRes.data);
                }
                if (gatewayRes.success && gatewayRes.data) {
                    setGateways(gatewayRes.data.items || []);
                }
            } catch (err) {
                console.error('Failed to fetch filter options:', err);
            }
        };
        fetchFilters();
    }, []);

    // Fetch Logs
    const fetchLogs = useCallback(async (isBackground = false) => {
        try {
            if (isBackground && containerRef.current) {
                setScrollPosition(containerRef.current.scrollTop);
            }

            if (!isBackground) {
                setIsInitialLoading(true);
                setError(null);
            } else {
                setIsRefreshing(true);
            }

            const params = {
                page: pagination.currentPage,
                limit: pagination.pageSize,
                status: filters.status !== 'all' ? filters.status : undefined,
                target_type: filters.target_type !== 'all' ? filters.target_type : undefined,
                target_id: filters.target_id !== 'all' ? parseInt(filters.target_id) : undefined,
                gateway_id: filters.gateway_id !== 'all' ? parseInt(filters.gateway_id) : undefined,
                date_from: filters.dateRange.start.toISOString(),
                date_to: filters.dateRange.end.toISOString()
            };

            const response = await exportGatewayApi.getExportLogs(params);

            if (response.success && response.data) {
                const items = response.data.items || [];
                setLogs(items);

                if (response.data.pagination) {
                    pagination.updateTotalCount(response.data.pagination.total_count || 0);
                } else {
                    // Fallback
                    pagination.updateTotalCount(items.length);
                }
            } else {
                throw new Error(response.message || '로그 조회 실패');
            }
        } catch (err) {
            console.error('Export Log Fetch Error:', err);
            setError(err instanceof Error ? err.message : '로그를 불러오는데 실패했습니다.');
            if (!Array.isArray(logs)) setLogs([]);
        } finally {
            setIsInitialLoading(false);
            setIsRefreshing(false);

            if (isBackground && containerRef.current) {
                setTimeout(() => {
                    containerRef.current?.scrollTo(0, scrollPosition);
                }, 100);
            }
        }
    }, [pagination.currentPage, pagination.pageSize, filters]);

    // Fetch Statistics
    const fetchStatistics = useCallback(async () => {
        try {
            const params = {
                status: filters.status !== 'all' ? filters.status : undefined,
                target_type: filters.target_type !== 'all' ? filters.target_type : undefined,
                target_id: filters.target_id !== 'all' ? parseInt(filters.target_id) : undefined,
                gateway_id: filters.gateway_id !== 'all' ? parseInt(filters.gateway_id) : undefined,
                date_from: filters.dateRange.start.toISOString(),
                date_to: filters.dateRange.end.toISOString()
            };

            const response = await exportGatewayApi.getExportLogStatistics(params);
            if (response.success && response.data) {
                setStatistics(response.data);
            }
        } catch (err) {
            console.error('Statistics Fetch Error:', err);
        }
    }, [filters]);

    // Initial Load & Refresh
    useEffect(() => {
        fetchLogs();
        fetchStatistics();
    }, [fetchLogs, fetchStatistics]);

    const handleRefresh = () => {
        fetchLogs(true);
        fetchStatistics();
    };

    // ---------------------------------------------------------------------------
    // Event Handlers
    // ---------------------------------------------------------------------------

    const handleFilterChange = (newFilters: Partial<FilterOptions>) => {
        setFilters(prev => ({ ...prev, ...newFilters }));
        pagination.goToPage(1);
    };

    const handleDateChange = (type: 'start' | 'end', value: string) => {
        if (!value) return;
        handleFilterChange({
            dateRange: {
                ...filters.dateRange,
                [type]: new Date(value)
            }
        });
    };

    // ---------------------------------------------------------------------------
    // Utilities
    // ---------------------------------------------------------------------------

    const formatDateForInput = (date: Date): string => {
        return new Date(date.getTime() - (date.getTimezoneOffset() * 60000)).toISOString().slice(0, 16);
    };

    const formatDateTime = (isoString?: string) => {
        if (!isoString) return '-';
        return new Date(isoString).toLocaleString('ko-KR', {
            year: 'numeric', month: '2-digit', day: '2-digit',
            hour: '2-digit', minute: '2-digit', second: '2-digit'
        });
    };

    const getStatusColor = (status: string) => {
        switch (status?.toLowerCase()) {
            case 'success': return 'var(--success-500)';
            case 'failure': return 'var(--error-500)';
            default: return 'var(--neutral-500)';
        }
    };

    const getStatusBadgeClass = (status: string) => {
        const s = (status || '').toLowerCase();
        if (s === 'success') return 'mgmt-status-pill active';
        if (s === 'failure') return 'mgmt-status-pill error';
        return 'mgmt-status-pill inactive';
    };

    const computedStats = useMemo(() => {
        if (statistics) return statistics;
        // Fallback if generic stats not loaded yet
        return { total: 0, success: 0, failure: 0 };
    }, [statistics]);

    // ---------------------------------------------------------------------------
    // Render
    // ---------------------------------------------------------------------------

    return (
        <ManagementLayout>
            <div className="export-history" style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
                <PageHeader
                    title="내보내기 이력"
                    description="외부 시스템으로 전송된 데이터의 성공/실패 이력을 조회합니다."
                    icon="fas fa-history"
                    actions={
                        <button
                            className="mgmt-btn mgmt-btn-outline"
                            onClick={handleRefresh}
                            disabled={isRefreshing || isInitialLoading}
                        >
                            <i className={`fas fa-sync-alt ${isRefreshing ? 'fa-spin' : ''}`}></i>
                            새로고침
                        </button>
                    }
                />

                {/* Summary Cards */}
                <div className="mgmt-stats-panel" style={{ gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px' }}>
                    <StatCard
                        title="총 시도"
                        value={computedStats.total}
                        icon="fas fa-list"
                        type="primary"
                    />
                    <StatCard
                        title="성공"
                        value={computedStats.success}
                        icon="fas fa-check-circle"
                        type="success"
                    />
                    <StatCard
                        title="실패"
                        value={computedStats.failure}
                        icon="fas fa-times-circle"
                        type="error"
                    />
                </div>

                {/* Filter Bar */}
                <FilterBar
                    activeFilterCount={
                        (filters.status !== 'all' ? 1 : 0) +
                        (filters.target_type !== 'all' ? 1 : 0) +
                        (filters.target_id !== 'all' ? 1 : 0) +
                        (filters.gateway_id !== 'all' ? 1 : 0)
                    }
                    onReset={() => setFilters({
                        dateRange: { start: new Date(Date.now() - 24 * 60 * 60 * 1000), end: new Date() },
                        status: 'all',
                        target_type: 'all',
                        target_id: 'all',
                        gateway_id: 'all'
                    })}
                    filters={[
                        {
                            label: '게이트웨이',
                            value: filters.gateway_id,
                            onChange: (v) => handleFilterChange({ gateway_id: v }),
                            options: [
                                { label: '전체', value: 'all' },
                                ...gateways.map(g => ({ label: g.name, value: g.id.toString() }))
                            ]
                        },
                        {
                            label: '상태',
                            value: filters.status,
                            onChange: (v) => handleFilterChange({ status: v }),
                            options: [
                                { label: '전체', value: 'all' },
                                { label: '성공', value: 'success' },
                                { label: '실패', value: 'failure' }
                            ]
                        },
                        {
                            label: '타겟 유형',
                            value: filters.target_type,
                            onChange: (v) => handleFilterChange({ target_type: v }),
                            options: [
                                { label: '전체', value: 'all' },
                                { label: 'HTTP', value: 'http' },
                                { label: 'MQTT', value: 'mqtt' },
                                { label: 'S3', value: 's3' },
                                { label: 'SQL', value: 'sql' }
                            ]
                        },
                        {
                            label: '타겟 명',
                            value: filters.target_id,
                            onChange: (v) => handleFilterChange({ target_id: v }),
                            options: [
                                { label: '전체', value: 'all' },
                                ...targets.map(t => ({ label: t.name, value: t.id.toString() }))
                            ]
                        }
                    ]}
                    leftActions={
                        <div className="mgmt-filter-group">
                            <label>기간</label>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                <input
                                    type="datetime-local"
                                    className="mgmt-input"
                                    style={{ width: '210px' }}
                                    value={formatDateForInput(filters.dateRange.start)}
                                    onChange={(e) => handleDateChange('start', e.target.value)}
                                />
                                <span style={{ color: 'var(--neutral-400)' }}>~</span>
                                <input
                                    type="datetime-local"
                                    className="mgmt-input"
                                    style={{ width: '210px' }}
                                    value={formatDateForInput(filters.dateRange.end)}
                                    onChange={(e) => handleDateChange('end', e.target.value)}
                                />
                            </div>
                        </div>
                    }
                    rightActions={
                        <button
                            className="mgmt-btn mgmt-btn-primary"
                            onClick={() => { pagination.goToPage(1); fetchLogs(); fetchStatistics(); }}
                            style={{ height: '36px' }}
                        >
                            조회
                        </button>
                    }
                />

                {error && (
                    <div className="mgmt-card" style={{ padding: '16px', background: '#fef2f2', border: '1px solid #fecaca', color: '#dc2626', marginBottom: '24px' }}>
                        <i className="fas fa-exclamation-circle" style={{ marginRight: '8px' }}></i>
                        {error}
                    </div>
                )}

                {/* Main Table */}
                <div className="mgmt-card table-container" style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: '500px' }}>
                    <div className="mgmt-header" style={{ border: 'none', background: 'transparent', paddingBottom: '0' }}>
                        <div className="mgmt-header-info">
                            <h3 className="mgmt-title" style={{ fontSize: '18px' }}>
                                <i className="fas fa-list-ul" style={{ color: 'var(--primary-500)' }}></i>
                                전송 이력 ({computedStats.total.toLocaleString()})
                            </h3>
                        </div>
                    </div>

                    {isInitialLoading ? (
                        <div className="mgmt-loading">
                            <i className="fas fa-spinner fa-spin"></i>
                            <p>데이터를 불러오는 중입니다...</p>
                        </div>
                    ) : (
                        <>
                            <div className="mgmt-table-wrapper" ref={containerRef} style={{ flex: 1 }}>
                                <table className="mgmt-table">
                                    <thead>
                                        <tr>
                                            <th style={{ width: '80px' }}>ID</th>
                                            <th style={{ width: '100px' }}>상태</th>
                                            <th style={{ width: '180px' }}>발생 시간</th>
                                            <th>타겟 시스템</th>
                                            <th>설정 정보 (Profile / Gateway)</th>
                                            <th>메시지 (Source)</th>
                                            <th style={{ width: '100px' }}>응답 코드</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {logs.length === 0 ? (
                                            <tr>
                                                <td colSpan={7} className="text-center" style={{ padding: '80px 0' }}>
                                                    <div className="text-muted">
                                                        <i className="fas fa-inbox" style={{ fontSize: '48px', marginBottom: '16px', display: 'block', opacity: 0.3 }}></i>
                                                        데이터가 없습니다.
                                                    </div>
                                                </td>
                                            </tr>
                                        ) : (
                                            logs.map(log => {
                                                const sourceOverview = log.source_value.length > 50
                                                    ? log.source_value.substring(0, 50) + '...'
                                                    : log.source_value;

                                                return (
                                                    <tr
                                                        key={log.id}
                                                        className={activeRowId === log.id ? 'active' : ''}
                                                        onClick={() => {
                                                            setActiveRowId(log.id);
                                                            setSelectedLog(log);
                                                        }}
                                                        style={{ cursor: 'pointer' }}
                                                    >
                                                        <td className="font-bold text-neutral-600">#{log.id}</td>
                                                        <td>
                                                            <span className={getStatusBadgeClass(log.status)}>
                                                                {(log.status || 'UNKNOWN').toUpperCase()}
                                                            </span>
                                                        </td>
                                                        <td className="text-neutral-700">
                                                            {formatDateTime(log.timestamp)}
                                                        </td>
                                                        <td>
                                                            <div className="font-semibold text-neutral-900">{log.target_name || '-'}</div>
                                                            <div className="text-xs text-neutral-500">{log.target_id ? `Target #${log.target_id}` : ''}</div>
                                                        </td>
                                                        <td>
                                                            {log.profile_name ? (
                                                                <div className="flex flex-col">
                                                                    <span className="text-sm font-medium text-primary-700">
                                                                        <i className="fas fa-file-alt mr-1"></i> {log.profile_name}
                                                                    </span>
                                                                    {log.gateway_name && (
                                                                        <span className="text-xs text-neutral-500 mt-1">
                                                                            <i className="fas fa-server mr-1"></i> {log.gateway_name}
                                                                        </span>
                                                                    )}
                                                                </div>
                                                            ) : (
                                                                <span className="text-neutral-400">-</span>
                                                            )}
                                                        </td>
                                                        <td>
                                                            <div className="text-neutral-800" title={log.source_value}>{sourceOverview}</div>
                                                            {log.error_message && (
                                                                <div className="text-xs text-error-600 mt-1">
                                                                    <i className="fas fa-exclamation-triangle mr-1"></i>
                                                                    {log.error_message}
                                                                </div>
                                                            )}
                                                        </td>
                                                        <td>
                                                            {log.http_status_code > 0 ? (
                                                                <span className={`font-mono text-sm ${log.http_status_code >= 200 && log.http_status_code < 300 ? 'text-success-600' : 'text-error-600'}`}>
                                                                    {log.http_status_code}
                                                                </span>
                                                            ) : (
                                                                <span className="text-neutral-400">-</span>
                                                            )}
                                                        </td>
                                                    </tr>
                                                );
                                            })
                                        )}
                                    </tbody>
                                </table>
                            </div>

                            {/* Pagination */}
                            <div className="mgmt-footer" style={{ padding: '16px 24px', borderTop: '1px solid var(--neutral-200)', background: 'white' }}>
                                <Pagination
                                    current={pagination.currentPage}
                                    total={pagination.totalCount}
                                    pageSize={pagination.pageSize}
                                    onChange={(page, size) => {
                                        pagination.goToPage(page);
                                        if (size && size !== pagination.pageSize) {
                                            pagination.changePageSize(size);
                                        }
                                    }}
                                    showSizeChanger={true}
                                />
                            </div>
                        </>
                    )}
                </div>
                {selectedLog && (
                    <LogDetailModal
                        log={selectedLog}
                        onClose={() => setSelectedLog(null)}
                    />
                )}
            </div>
        </ManagementLayout>
    );
};

export default ExportHistory;
