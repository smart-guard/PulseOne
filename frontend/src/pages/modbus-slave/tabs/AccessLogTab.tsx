// =============================================================================
// frontend/src/pages/modbus-slave/tabs/AccessLogTab.tsx
// Modbus Slave 통신 이력 탭 — Export Gateway의 ExportHistory.tsx와 동일 패턴
// =============================================================================
import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { Select, Tag } from 'antd';
import modbusSlaveApi, {
    ModbusSlaveDevice,
    AccessLog,
    AccessLogStats,
} from '../../../api/services/modbusSlaveApi';

const { Option } = Select;

// ─── 유틸 ─────────────────────────────────────────────────────────────────

const fmtUs = (us: number) => us < 1000 ? `${us.toFixed(0)} µs` : `${(us / 1000).toFixed(2)} ms`;
const fmtRate = (r: number) => `${(r * 100).toFixed(1)}%`;
const fmtDt = (iso: string) => iso ? new Date(iso).toLocaleString('ko-KR', {
    year: 'numeric', month: '2-digit', day: '2-digit',
    hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false,
}) : '-';

const th: React.CSSProperties = {
    padding: '10px 12px', fontSize: 12, fontWeight: 700,
    color: '#64748b', background: '#f8fafc', borderBottom: '2px solid #e2e8f0', textAlign: 'left',
};
const td: React.CSSProperties = {
    padding: '10px 12px', fontSize: 13, color: '#334155', borderBottom: '1px solid #f1f5f9',
};

// ─── 통계 카드 ────────────────────────────────────────────────────────────

const StatBadge: React.FC<{ label: string; value: string | number; fg: string; bg: string }> = ({ label, value, fg, bg }) => (
    <div style={{ background: bg, border: `1px solid ${fg}22`, borderRadius: 8, padding: '5px 12px', display: 'flex', alignItems: 'center', gap: 6, whiteSpace: 'nowrap' }}>
        <span style={{ fontSize: 13, fontWeight: 800, color: fg }}>{value}</span>
        <span style={{ fontSize: 11, color: '#64748b' }}>{label}</span>
    </div>
);

// ─── FC 요약 셀 ──────────────────────────────────────────────────────────

const FcSummary: React.FC<{ log: AccessLog }> = ({ log }) => {
    const items = [
        { fc: 'FC01', v: log.fc01_count }, { fc: 'FC02', v: log.fc02_count },
        { fc: 'FC03', v: log.fc03_count }, { fc: 'FC04', v: log.fc04_count },
        { fc: 'FC05', v: log.fc05_count }, { fc: 'FC06', v: log.fc06_count },
        { fc: 'FC15', v: log.fc15_count }, { fc: 'FC16', v: log.fc16_count },
    ].filter(i => i.v > 0);

    if (items.length === 0) return <span style={{ color: '#94a3b8', fontSize: 12 }}>-</span>;
    return (
        <span style={{ fontFamily: 'monospace', fontSize: 12 }}>
            {items.map(i => `${i.fc}×${i.v}`).join('  ')}
        </span>
    );
};

// ─── 메인 탭 ─────────────────────────────────────────────────────────────

interface Props { devices?: ModbusSlaveDevice[] }

// ─── 에러 바운더리 ────────────────────────────────────────────────────────
class LogErrorBoundary extends React.Component<{ children: React.ReactNode }, { hasError: boolean; error: string }> {
    constructor(props: { children: React.ReactNode }) {
        super(props);
        this.state = { hasError: false, error: '' };
    }
    static getDerivedStateFromError(error: Error) {
        return { hasError: true, error: error?.message || String(error) };
    }
    render() {
        if (this.state.hasError) {
            return (
                <div style={{ padding: 32, textAlign: 'center', color: '#ef4444' }}>
                    <i className="fas fa-exclamation-triangle" style={{ fontSize: 32, display: 'block', marginBottom: 12 }} />
                    <strong>통신 이력 탭 오류</strong>
                    <pre style={{ marginTop: 8, fontSize: 12, color: '#64748b', whiteSpace: 'pre-wrap', textAlign: 'left', background: '#f8fafc', padding: 12, borderRadius: 8 }}>
                        {this.state.error}
                    </pre>
                    <button onClick={() => this.setState({ hasError: false, error: '' })}
                        style={{ marginTop: 12, padding: '6px 16px', background: '#3b82f6', color: '#fff', border: 'none', borderRadius: 8, cursor: 'pointer' }}>
                        다시 시도
                    </button>
                </div>
            );
        }
        return this.props.children;
    }
}

const AUTO_REFRESH_SEC = 15;

const AccessLogTabInner: React.FC<{ devices: ModbusSlaveDevice[] }> = ({ devices }) => {
    const { t } = useTranslation('settings');
    const [deviceId, setDeviceId] = useState<number | null>(null);
    const [logs, setLogs] = useState<AccessLog[]>([]);
    const [logStats, setLogStats] = useState<AccessLogStats | null>(null);
    const [page, setPage] = useState(1);
    const [total, setTotal] = useState(0);
    const [loading, setLoading] = useState(false);
    const [countdown, setCountdown] = useState(AUTO_REFRESH_SEC);
    const [autoRefresh, setAutoRefresh] = useState(true);
    const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);

    const limit = 30;
    const [ipFilter, setIpFilter] = useState<string>('');

    // 날짜 범위 기본값: 최근 24시간
    const [dateFrom, setDateFrom] = useState(() =>
        new Date(Date.now() - 24 * 3600 * 1000).toISOString().slice(0, 16));
    const [dateTo, setDateTo] = useState(() =>
        new Date(Date.now() + 3600 * 1000).toISOString().slice(0, 16));

    // 디바이스 목록 준비되면 첫 번째 선택
    useEffect(() => {
        if (devices.length > 0 && deviceId === null) setDeviceId(devices[0].id);
    }, [devices]);

    const fetchData = useCallback(async () => {
        try {
            setLoading(true);
            const params = {
                device_id: deviceId ?? undefined,
                date_from: new Date(dateFrom).toISOString(),
                date_to: new Date(dateTo).toISOString(),
                page, limit,
                ...(ipFilter ? { client_ip: ipFilter } : {}),
            };
            const [logsRes, statsRes] = await Promise.all([
                modbusSlaveApi.getAccessLogs(params),
                modbusSlaveApi.getAccessLogStats({
                    device_id: deviceId ?? undefined,
                    date_from: params.date_from, date_to: params.date_to
                }),
            ]);
            // null-safe: data가 없거나 중첩 에러여도 크래시 방지
            if (logsRes?.success && logsRes?.data) {
                setLogs(logsRes.data?.items ?? []);
                setTotal(logsRes.data?.pagination?.total ?? 0);
            } else {
                setLogs([]);
                setTotal(0);
            }
            if (statsRes?.success && statsRes?.data) setLogStats(statsRes.data);
        } catch (e) {
            console.error('[AccessLogTab] fetchData 에러:', e);
            setLogs([]);
        } finally {
            setLoading(false);
        }
    }, [deviceId, dateFrom, dateTo, page, ipFilter]);

    // 자동 갱신
    useEffect(() => {
        fetchData();
    }, [fetchData]);

    useEffect(() => {
        if (timerRef.current) clearInterval(timerRef.current);
        if (!autoRefresh) { setCountdown(AUTO_REFRESH_SEC); return; }
        setCountdown(AUTO_REFRESH_SEC);
        timerRef.current = setInterval(() => {
            setCountdown(prev => {
                if (prev <= 1) { fetchData(); return AUTO_REFRESH_SEC; }
                return prev - 1;
            });
        }, 1000);
        return () => { if (timerRef.current) clearInterval(timerRef.current); };
    }, [autoRefresh, fetchData]);

    const totalPages = Math.ceil(total / limit);

    return (
        <div style={{ padding: '16px 0', display: 'flex', flexDirection: 'column', gap: 10 }}>

            {/* 필터 바 */}
            <div style={{
                display: 'flex', alignItems: 'center', gap: 10, flexWrap: 'wrap',
                background: '#fff', borderRadius: 12, border: '1px solid #e2e8f0', padding: '12px 16px'
            }}>
                <Select value={deviceId} onChange={v => { setDeviceId(v); setPage(1); }} style={{ minWidth: 200 }} placeholder="전체 디바이스">
                    <Option value={null}>전체 디바이스</Option>
                    {devices.map(d => (
                        <Option key={d.id} value={d.id}>{d.name} <span style={{ color: '#94a3b8', fontSize: 12 }}>:{d.tcp_port}</span></Option>
                    ))}
                </Select>
                <span style={{ fontSize: 12, color: '#64748b' }}>기간</span>
                <input type="datetime-local" value={dateFrom} onChange={e => { setDateFrom(e.target.value); setPage(1); }}
                    style={{ border: '1px solid #e2e8f0', borderRadius: 8, padding: '6px 10px', fontSize: 13 }} />
                <span style={{ color: '#94a3b8' }}>~</span>
                <input type="datetime-local" value={dateTo} onChange={e => { setDateTo(e.target.value); setPage(1); }}
                    style={{ border: '1px solid #e2e8f0', borderRadius: 8, padding: '6px 10px', fontSize: 13 }} />
                <button onClick={() => { fetchData(); setCountdown(AUTO_REFRESH_SEC); }}
                    style={{ marginLeft: 4, padding: '6px 14px', background: '#3b82f6', color: '#fff', border: 'none', borderRadius: 8, cursor: 'pointer', fontSize: 13 }}>
                    <i className="fas fa-search" style={{ marginRight: 6 }} />{t('modbusSlave.log.search')}

                </button>
                {/* 자동 갱신 토글 */}
                <div style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center', gap: 8 }}>
                    <span style={{ fontSize: 12, color: autoRefresh ? '#3b82f6' : '#94a3b8' }}>
                        {autoRefresh ? <><i className="fas fa-circle-notch fa-spin" style={{ marginRight: 4, fontSize: 10 }} />{countdown}s</> : '일시정지'}
                    </span>
                    <button onClick={() => setAutoRefresh(v => !v)}
                        style={{ background: autoRefresh ? '#dbeafe' : '#f1f5f9', border: 'none', borderRadius: 20, padding: '4px 10px', cursor: 'pointer', fontSize: 12, color: autoRefresh ? '#2563eb' : '#64748b' }}>
                        <i className={`fas ${autoRefresh ? 'fa-pause' : 'fa-play'}`} />
                    </button>
                </div>
            </div>

            {/* IP 필터 활성 표시 */}
            {ipFilter && (
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <span style={{ fontSize: 12, color: '#64748b' }}>IP 필터:</span>
                    <Tag closable color="blue" onClose={() => { setIpFilter(''); setPage(1); }}>
                        {ipFilter}
                    </Tag>
                </div>
            )}

            {/* 통계 카드 */}
            {logStats && (
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', background: '#fff', border: '1px solid #e2e8f0', borderRadius: 10, padding: '8px 14px', alignItems: 'center' }}>
                    <StatBadge label={t('modbusSlave.log.totalRequests')} value={(logStats.total_requests ?? 0).toLocaleString()} fg="#3b82f6" bg="#eff6ff" />
                    <StatBadge label={t('modbusSlave.log.uniqueClients')} value={logStats.unique_clients ?? 0} fg="#16a34a" bg="#f0fdf4" />
                    <StatBadge label={t('modbusSlave.log.avgSuccessRate')} value={fmtRate(logStats.avg_success_rate ?? 1)} fg="#9333ea" bg="#fdf4ff" />
                    <StatBadge label={t('modbusSlave.log.avgResponse')} value={fmtUs(logStats.avg_response_us ?? 0)} fg="#f97316" bg="#fff7ed" />
                    <StatBadge label={t('modbusSlave.log.totalFailures')} value={(logStats.total_failures ?? 0).toLocaleString()} fg="#ef4444" bg="#fef2f2" />
                </div>
            )}

            {/* 이력 테이블 */}
            <div style={{ background: '#fff', border: '1px solid #e2e8f0', borderRadius: 12, overflow: 'visible' }}>
                <div style={{ padding: '14px 20px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', gap: 8 }}>
                    <i className="fas fa-history" style={{ color: '#3b82f6' }} />
                    <span style={{ fontWeight: 700, fontSize: 14, color: '#1e293b' }}>{t('modbusSlave.tabs.logs')}</span>
                    <span style={{ background: '#dbeafe', color: '#1d4ed8', fontSize: 12, fontWeight: 700, padding: '2px 8px', borderRadius: 12 }}>{total.toLocaleString()}</span>
                    {loading && <i className="fas fa-sync fa-spin" style={{ fontSize: 12, color: '#94a3b8', marginLeft: 4 }} />}
                </div>
                {logs.length === 0 ? (
                    <div style={{ padding: 48, textAlign: 'center', color: '#94a3b8' }}>
                        <i className="fas fa-inbox" style={{ fontSize: 32, display: 'block', marginBottom: 8 }} />
                        {t('modbusSlave.log.noData')}
                    </div>
                ) : (
                    <div style={{ overflowX: 'auto' }}>
                        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                            <thead>
                                <tr>
                                    <th style={th}>{t('modbusSlave.log.recordedAt')}</th>
                                    <th style={th}>{t('modbusSlave.log.device')}</th>
                                    <th style={th}>{t('modbusSlave.log.clientIp')}</th>
                                    <th style={th}>{t('modbusSlave.log.port')}</th>
                                    <th style={{ ...th, textAlign: 'right' }}>{t('modbusSlave.log.requests')}</th>
                                    <th style={{ ...th, textAlign: 'right' }}>{t('modbusSlave.log.successRate')}</th>
                                    <th style={{ ...th, textAlign: 'right' }}>{t('modbusSlave.log.avgResponse')}</th>
                                    <th style={th}>{t('modbusSlave.log.fcDistribution')}</th>
                                    <th style={{ ...th, textAlign: 'center' }}>{t('modbusSlave.log.status')}</th>
                                </tr>
                            </thead>
                            <tbody>
                                {logs.map((log, i) => {
                                    const rateColor = log.success_rate >= 0.99 ? '#16a34a' : log.success_rate >= 0.9 ? '#f59e0b' : '#ef4444';
                                    return (
                                        <tr key={log.id} style={{ background: i % 2 === 0 ? '#f8fafc' : '#fff' }}>
                                            <td style={td}><span style={{ fontSize: 12 }}>{fmtDt(log.recorded_at)}</span></td>
                                            <td style={td}><span style={{ fontWeight: 600, color: '#1e293b' }}>{log.device_name || `#${log.device_id}`}</span></td>
                                            <td style={td}><span style={{ fontFamily: 'monospace', fontWeight: 600, color: '#3b82f6', cursor: 'pointer', textDecoration: 'underline' }}
                                                onClick={() => { setIpFilter(log.client_ip); setPage(1); }}
                                                title="클릭하여 이 IP만 필터">{log.client_ip}</span></td>
                                            <td style={td}>{log.client_port || '-'}</td>
                                            <td style={{ ...td, textAlign: 'right' }}>{(log.total_requests ?? 0).toLocaleString()}</td>
                                            <td style={{ ...td, textAlign: 'right' }}><span style={{ color: rateColor, fontWeight: 700 }}>{fmtRate(log.success_rate ?? 1)}</span></td>
                                            <td style={{ ...td, textAlign: 'right' }}>{fmtUs(log.avg_response_us ?? 0)}</td>
                                            <td style={td}><FcSummary log={log} /></td>
                                            <td style={{ ...td, textAlign: 'center' }}>
                                                <Tag color={log.is_active ? 'green' : 'default'}>
                                                    {log.is_active ? '접속 중' : '해제'}
                                                </Tag>
                                            </td>
                                        </tr>
                                    );
                                })}
                            </tbody>
                        </table>
                    </div>
                )}
                {/* 페이지네이션 */}
                {totalPages > 1 && (
                    <div style={{ padding: '12px 20px', borderTop: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 8 }}>
                        <button onClick={() => setPage(p => Math.max(1, p - 1))} disabled={page <= 1}
                            style={{ padding: '4px 12px', borderRadius: 6, border: '1px solid #e2e8f0', background: '#fff', cursor: page <= 1 ? 'not-allowed' : 'pointer', color: '#64748b' }}>
                            {t('modbusSlave.prev')}
                        </button>
                        <span style={{ fontSize: 13, color: '#64748b' }}>{page} / {totalPages}</span>
                        <button onClick={() => setPage(p => Math.min(totalPages, p + 1))} disabled={page >= totalPages}
                            style={{ padding: '4px 12px', borderRadius: 6, border: '1px solid #e2e8f0', background: '#fff', cursor: page >= totalPages ? 'not-allowed' : 'pointer', color: '#64748b' }}>
                            {t('modbusSlave.next')}
                        </button>
                    </div>
                )}
            </div>
        </div>
    );
};

const AccessLogTab: React.FC<Props> = ({ devices = [] }) => (
    <LogErrorBoundary>
        <AccessLogTabInner devices={devices} />
    </LogErrorBoundary>
);

export default AccessLogTab;
