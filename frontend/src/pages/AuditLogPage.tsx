import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { AuditLogApiService } from '../api/services/auditLogApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import { AuditLog } from '../types/manufacturing';
import '../styles/management.css';

const AuditLogPage: React.FC = () => {
    const { t } = useTranslation(['auditLog', 'common']);
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
            setError(t('errorLoading', { defaultValue: '감사 로그를 불러오는 중 오류가 발생했습니다.' }));
        } finally {
            setLoading(false);
        }
    }, [searchTerm, entityTypeFilter, currentPage, pageSize]);

    useEffect(() => {
        loadLogs();
    }, [loadLogs]);

    const getActionBadge = (action: string) => {
        switch (action) {
            case 'CREATE': return <span className="badge success">{t('action.created', { defaultValue: '생성' })}</span>;
            case 'UPDATE': return <span className="badge warning">{t('action.modified', { defaultValue: '수정' })}</span>;
            case 'DELETE': return <span className="badge danger">{t('action.deleted', { defaultValue: '삭제' })}</span>;
            case 'INSTANTIATE': return <span className="badge primary">{t('action.instantiate', { defaultValue: '인스턴스화' })}</span>;
            case 'ACTION': return <span className="badge info">{t('action.action', { defaultValue: '액션' })}</span>;
            default: return <span className="badge neutral">{action}</span>;
        }
    };

    return (
        <ManagementLayout>
            <PageHeader
                title={t('pageTitle', { defaultValue: '시스템 감사 로그' })}
                description={t('pageDesc', { defaultValue: '시스템 구성 변경 및 사용자 활동 이력을 확인합니다.' })}
                icon="fas fa-clipboard-check"
            />

            <div className="mgmt-stats-panel">
                <StatCard label={t('stats.todayLogs', { defaultValue: '오늘 로그' })} value={totalCount} type="primary" />
                <StatCard label={t('stats.threatsDetected', { defaultValue: '위협 감지' })} value="0" type="error" />
                <StatCard label={t('stats.lastEntry', { defaultValue: '마지막 항목' })} value={t('stats.lastEntryValue', { defaultValue: '1분 전' })} type="neutral" />
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
                        label: t('filter.entityType', { defaultValue: '엔티티 유형' }),
                        value: entityTypeFilter,
                        onChange: setEntityTypeFilter,
                        options: [
                            { value: 'all', label: t('filter.all', { defaultValue: '전체' }) },
                            { value: 'DEVICE', label: t('filter.device', { defaultValue: '디바이스' }) },
                            { value: 'TEMPLATE', label: t('filter.template', { defaultValue: '템플릿' }) },
                            { value: 'USER', label: t('filter.user', { defaultValue: '사용자' }) },
                            { value: 'SYSTEM', label: t('filter.system', { defaultValue: '시스템' }) }
                        ]
                    }
                ]}
            />

            <div className="mgmt-card" style={{ marginTop: '20px' }}>
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>{t('col.timestamp', { defaultValue: '시각' })}</th>
                            <th>{t('col.operator', { defaultValue: '운영자' })}</th>
                            <th>{t('col.action', { defaultValue: '액션' })}</th>
                            <th>{t('col.target', { defaultValue: '대상' })}</th>
                            <th>{t('col.targetName', { defaultValue: '대상명' })}</th>
                            <th>{t('col.details', { defaultValue: '상세' })}</th>
                        </tr>
                    </thead>
                    <tbody>
                        {loading ? (
                            <tr><td colSpan={6} style={{ textAlign: 'center', padding: '40px' }}>{t('common:loading', { defaultValue: '로딩 중...' })}</td></tr>
                        ) : logs.length === 0 ? (
                            <tr><td colSpan={6} style={{ textAlign: 'center', padding: '40px' }}>{t('noLogs', { defaultValue: '기록된 로그가 없습니다.' })}</td></tr>
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
