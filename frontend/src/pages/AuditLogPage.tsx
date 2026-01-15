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
            setError('감사 로그를 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    }, [searchTerm, entityTypeFilter, currentPage, pageSize]);

    useEffect(() => {
        loadLogs();
    }, [loadLogs]);

    const getActionBadge = (action: string) => {
        switch (action) {
            case 'CREATE': return <span className="badge success">생성</span>;
            case 'UPDATE': return <span className="badge warning">수정</span>;
            case 'DELETE': return <span className="badge danger">삭제</span>;
            case 'INSTANTIATE': return <span className="badge primary">인스턴스화</span>;
            case 'ACTION': return <span className="badge info">동작</span>;
            default: return <span className="badge neutral">{action}</span>;
        }
    };

    return (
        <ManagementLayout>
            <PageHeader
                title="시스템 감사 로그"
                description="시스템 구성 변경 및 사용자 작업 이력을 확인합니다."
                icon="fas fa-clipboard-check"
            />

            <div className="mgmt-stats-panel">
                <StatCard label="오늘의 로그" value={totalCount} type="primary" />
                <StatCard label="위험 탐지" value="0" type="error" />
                <StatCard label="마지막 기록" value="1분 전" type="neutral" />
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
                        label: '엔티티 타입',
                        value: entityTypeFilter,
                        onChange: setEntityTypeFilter,
                        options: [
                            { value: 'all', label: '전체' },
                            { value: 'DEVICE', label: '디바이스' },
                            { value: 'TEMPLATE', label: '템플릿' },
                            { value: 'USER', label: '사용자' },
                            { value: 'SYSTEM', label: '시스템' }
                        ]
                    }
                ]}
            />

            <div className="mgmt-card" style={{ marginTop: '20px' }}>
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>일시</th>
                            <th>작업자</th>
                            <th>작업</th>
                            <th>대상</th>
                            <th>대상 이름</th>
                            <th>상세 내용</th>
                        </tr>
                    </thead>
                    <tbody>
                        {loading ? (
                            <tr><td colSpan={6} style={{ textAlign: 'center', padding: '40px' }}>불러오는 중...</td></tr>
                        ) : logs.length === 0 ? (
                            <tr><td colSpan={6} style={{ textAlign: 'center', padding: '40px' }}>기록된 로그가 없습니다.</td></tr>
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
