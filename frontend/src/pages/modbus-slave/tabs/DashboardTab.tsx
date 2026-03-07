// =============================================================================
// frontend/src/pages/modbus-slave/tabs/DashboardTab.tsx
// Modbus Slave 전체 대시보드 — 모든 디바이스 실시간 서머리
// 3초 폴링으로 각 디바이스 stats 수집 → 카드+테이블+파이 차트
// =============================================================================
import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { Tooltip } from 'antd';
import modbusSlaveApi, {
    ModbusSlaveDevice,
    DeviceStats,
} from '../../../api/services/modbusSlaveApi';

// ─── 유틸 ─────────────────────────────────────────────────────────────────────
const fmtUs = (us: number) =>
    us < 1000 ? `${us.toFixed(0)} µs` : `${(us / 1000).toFixed(2)} ms`;
const fmtRate = (r: number) => `${(r * 100).toFixed(1)}%`;

// ─── 요약 KPI 카드 ────────────────────────────────────────────────────────────
const KpiCard: React.FC<{
    icon: string; label: string; value: string | number;
    sub?: string; bg: string; fg: string;
}> = ({ icon, label, value, sub, bg, fg }) => (
    <div style={{
        background: '#fff', border: '1px solid #e2e8f0', borderRadius: 14,
        padding: '16px 20px', display: 'flex', alignItems: 'center', gap: 14,
        flex: 1, minWidth: 160, height: 88,
        boxShadow: '0 1px 4px rgba(0,0,0,0.04)',
    }}>
        <div style={{ width: 44, height: 44, background: bg, borderRadius: 12, display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0 }}>
            <i className={`fas ${icon}`} style={{ color: fg, fontSize: 18 }} />
        </div>
        <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontSize: 24, fontWeight: 800, color: '#1e293b', letterSpacing: '-0.5px', lineHeight: 1.1 }}>{value}</div>
            {sub && (
                <Tooltip title={sub} placement="top">
                    <div style={{ fontSize: 11, color: '#94a3b8', marginTop: 2, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', cursor: 'default' }}>{sub}</div>
                </Tooltip>
            )}
            <Tooltip title={label} placement="top">
                <div style={{ fontSize: 12, color: '#64748b', marginTop: 3, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', cursor: 'default' }}>{label}</div>
            </Tooltip>
        </div>
    </div>
);

// ─── 미니 스파크라인 (SVG) ────────────────────────────────────────────────────
const Sparkline: React.FC<{ data: number[]; color: string; height?: number }> = ({ data, color, height = 32 }) => {
    if (data.length < 2) return null;
    const max = Math.max(...data, 1);
    const w = 80, h = height;
    const pts = data.map((v, i) => `${(i / (data.length - 1)) * w},${h - (v / max) * h}`).join(' ');
    return (
        <svg width={w} height={h} style={{ display: 'block' }}>
            <polyline points={pts} fill="none" stroke={color} strokeWidth={1.8} strokeLinejoin="round" />
            <circle cx={(data.length - 1) / (data.length - 1) * w} cy={h - (data[data.length - 1] / max) * h} r={3} fill={color} />
        </svg>
    );
};

// ─── 디바이스 상태 행 ─────────────────────────────────────────────────────────
interface DeviceRow {
    device: ModbusSlaveDevice;
    stats: DeviceStats | null;
    reqHistory: number[];
}

const tdS: React.CSSProperties = { padding: '12px 14px', borderBottom: '1px solid #f1f5f9', fontSize: 13, color: '#334155' };
const thS: React.CSSProperties = { padding: '10px 14px', fontSize: 12, fontWeight: 700, color: '#64748b', textAlign: 'left', borderBottom: '2px solid #e2e8f0', background: '#f8fafc' };

// ─── 메인 탭 ─────────────────────────────────────────────────────────────────
interface Props { devices: ModbusSlaveDevice[] }

const DashboardTab: React.FC<Props> = ({ devices }) => {
    const { t } = useTranslation('settings');
    const [rows, setRows] = useState<DeviceRow[]>([]);
    const [lastUpdate, setLastUpdate] = useState<Date | null>(null);
    const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);

    const fetchAll = useCallback(async () => {
        if (devices.length === 0) return;
        const results = await Promise.allSettled(
            devices.map(d => modbusSlaveApi.getDeviceStats(d.id))
        );
        setRows(prev => {
            const next: DeviceRow[] = devices.map((dev, i) => {
                const result = results[i];
                const stats = result.status === 'fulfilled' && result.value?.success
                    ? result.value.data ?? null
                    : null;
                const prevRow = prev.find(r => r.device.id === dev.id);
                const reqPerMin = stats?.slave_stats?.window_5min?.req_per_min ?? 0;
                const hist = prevRow ? [...prevRow.reqHistory.slice(-19), reqPerMin] : [reqPerMin];
                return { device: dev, stats, reqHistory: hist };
            });
            return next;
        });
        setLastUpdate(new Date());
    }, [devices]);

    useEffect(() => {
        fetchAll();
        timerRef.current = setInterval(fetchAll, 3000);
        return () => { if (timerRef.current) clearInterval(timerRef.current); };
    }, [fetchAll]);

    // ── 전체 집계 ─────────────────────────────────────────────────────────────
    const runningCount = rows.filter(r => r.stats?.process_status === 'running').length;
    const stoppedCount = devices.length - runningCount;
    const totalMappings = devices.reduce((s, d) => s + (d.mapping_count || 0), 0);
    const totalClients = rows.reduce((s, r) => s + (r.stats?.clients?.current_clients ?? 0), 0);
    const totalReqsPerMin = rows.reduce((s, r) => s + (r.stats?.slave_stats?.window_5min?.req_per_min ?? 0), 0);
    const avgResponse = (() => {
        const active = rows.filter(r => r.stats?.slave_stats);
        if (!active.length) return 0;
        return active.reduce((s, r) => s + (r.stats!.slave_stats!.avg_response_us), 0) / active.length;
    })();

    if (devices.length === 0) return (
        <div style={{ padding: '60px 0', textAlign: 'center', color: '#94a3b8' }}>
            <i className="fas fa-tachometer-alt fa-3x" style={{ display: 'block', marginBottom: 16, opacity: 0.3 }} />
            <p>{t('modbusSlave.noDevices')}</p>
        </div>
    );

    return (
        <div style={{ padding: '16px 0', display: 'flex', flexDirection: 'column', gap: 20 }}>

            {/* KPI */}
            <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap', alignItems: 'stretch' }}>
                <KpiCard icon="fa-server" label={t('modbusSlave.dashboard.runningDevices')}
                    value={`${runningCount} / ${devices.length}`}
                    bg="#eff6ff" fg="#3b82f6" />
                <KpiCard icon="fa-stop-circle" label={t('modbusSlave.stopped')}
                    value={stoppedCount}
                    bg="#f8fafc" fg="#94a3b8" />
                <KpiCard icon="fa-th" label={t('modbusSlave.mappingPoints')}
                    value={totalMappings}
                    bg="#fdf4ff" fg="#9333ea" />
                <KpiCard icon="fa-users" label={t('modbusSlave.dashboard.connectedClients')}
                    value={totalClients}
                    sub="SCADA/HMI"
                    bg="#f0fdf4" fg="#16a34a" />
                <KpiCard icon="fa-bolt" label={t('modbusSlave.monitor.reqPerMin')}
                    value={totalReqsPerMin.toFixed(1)}
                    bg="#fff7ed" fg="#f97316" />
                <KpiCard icon="fa-tachometer-alt" label={t('modbusSlave.dashboard.avgResponse')}
                    value={avgResponse > 0 ? fmtUs(avgResponse) : '-'}
                    bg="#e0f2fe" fg="#0369a1" />
                {lastUpdate && (
                    <div style={{ marginLeft: 'auto', fontSize: 11, color: '#94a3b8', alignSelf: 'flex-end', paddingBottom: 4 }}>
                        <i className="fas fa-sync-alt fa-spin" style={{ marginRight: 4, fontSize: 9 }} />
                        {lastUpdate.toLocaleTimeString()}
                    </div>
                )}
            </div>

            {/* ── 디바이스별 상태 테이블 ─────────────────────────────── */}
            <div style={{ background: '#fff', border: '1px solid #e2e8f0', borderRadius: 14, overflow: 'hidden' }}>
                <div style={{ padding: '16px 20px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', gap: 10 }}>
                    <i className="fas fa-table" style={{ color: '#6366f1' }} />
                    <span style={{ fontWeight: 700, fontSize: 15, color: '#1e293b' }}>{t('modbusSlave.dashboard.allDevices')}</span>
                </div>
                <div style={{ overflowX: 'auto' }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                        <thead>
                            <tr>
                                <th style={thS}>{t('modbusSlave.device.name')}</th>
                                <th style={thS}>{t('modbusSlave.device.port')}</th>
                                <th style={thS}>{t('modbusSlave.device.status')}</th>
                                <th style={{ ...thS, textAlign: 'right' }}>{t('modbusSlave.currentClients')}</th>
                                <th style={{ ...thS, textAlign: 'right' }}>{t('modbusSlave.log.totalRequests')}</th>
                                <th style={{ ...thS, textAlign: 'right' }}>{t('modbusSlave.log.successRate')}</th>
                                <th style={{ ...thS, textAlign: 'right' }}>{t('modbusSlave.log.avgResponse')}</th>
                                <th style={{ ...thS, textAlign: 'right' }}>{t('modbusSlave.monitor.reqPerMin')}</th>
                                <th style={{ ...thS, textAlign: 'center' }}>{t('modbusSlave.responseTrend')}</th>
                                <th style={{ ...thS, textAlign: 'right' }}>PID</th>
                            </tr>
                        </thead>
                        <tbody>
                            {rows.map(({ device, stats, reqHistory }, i) => {
                                const isRunning = stats?.process_status === 'running';
                                const s = stats?.slave_stats;
                                const c = stats?.clients;
                                const clients = c?.current_clients ?? 0;
                                const rate = s?.overall_success_rate ?? c?.success_rate ?? 0;
                                const rateColor = rate >= 0.99 ? '#16a34a' : rate >= 0.9 ? '#f59e0b' : '#ef4444';
                                return (
                                    <tr key={device.id} style={{ background: i % 2 === 0 ? '#fafafa' : '#fff' }}>
                                        <td style={tdS}>
                                            <div style={{ fontWeight: 600 }}>{device.name}</div>
                                            {device.description && <div style={{ fontSize: 11, color: '#94a3b8' }}>{device.description}</div>}
                                        </td>
                                        <td style={{ ...tdS, fontFamily: 'monospace', color: '#6366f1', fontWeight: 700 }}>:{device.tcp_port}</td>
                                        <td style={tdS}>
                                            <span style={{
                                                display: 'inline-flex', alignItems: 'center', gap: 5,
                                                padding: '3px 10px', borderRadius: 20, fontSize: 12, fontWeight: 700,
                                                background: isRunning ? '#dcfce7' : '#f1f5f9',
                                                color: isRunning ? '#16a34a' : '#94a3b8',
                                            }}>
                                                <i className="fas fa-circle" style={{ fontSize: 7 }} />
                                                {isRunning ? t('modbusSlave.running') : t('modbusSlave.stopped')}
                                            </span>
                                        </td>
                                        <td style={{ ...tdS, textAlign: 'right' }}>
                                            {isRunning ? (
                                                <span style={{ fontWeight: 700, color: clients > 0 ? '#3b82f6' : '#94a3b8', fontSize: 16 }}>
                                                    {clients}
                                                    <span style={{ fontSize: 11, color: '#94a3b8', fontWeight: 400, marginLeft: 3 }}>/ {device.max_clients}</span>
                                                </span>
                                            ) : <span style={{ color: '#cbd5e1' }}>—</span>}
                                        </td>
                                        <td style={{ ...tdS, textAlign: 'right' }}>{s ? s.total_requests.toLocaleString() : <span style={{ color: '#cbd5e1' }}>—</span>}</td>
                                        <td style={{ ...tdS, textAlign: 'right' }}>
                                            {s ? <span style={{ color: rateColor, fontWeight: 700 }}>{fmtRate(rate)}</span> : <span style={{ color: '#cbd5e1' }}>—</span>}
                                        </td>
                                        <td style={{ ...tdS, textAlign: 'right' }}>
                                            {s ? fmtUs(s.avg_response_us) : <span style={{ color: '#cbd5e1' }}>—</span>}
                                        </td>
                                        <td style={{ ...tdS, textAlign: 'right' }}>
                                            {s ? s.window_5min.req_per_min.toFixed(1) : <span style={{ color: '#cbd5e1' }}>—</span>}
                                        </td>
                                        <td style={{ ...tdS, textAlign: 'center' }}>
                                            <Tooltip title={t('modbusSlave.dashboard.sparklineTooltip')}>
                                                <Sparkline data={reqHistory} color={isRunning ? '#6366f1' : '#cbd5e1'} />
                                            </Tooltip>
                                        </td>
                                        <td style={{ ...tdS, textAlign: 'right', fontFamily: 'monospace', fontSize: 11, color: '#94a3b8' }}>
                                            {stats?.pid ?? '—'}
                                        </td>
                                    </tr>
                                );
                            })}
                        </tbody>
                    </table>
                </div>
            </div>

            {/* ── FC 분포 종합 ───────────────────────────────────────── */}
            {rows.some(r => r.stats?.slave_stats?.fc_counters) && (
                <div style={{ background: '#fff', border: '1px solid #e2e8f0', borderRadius: 14, padding: '20px 24px' }}>
                    <div style={{ fontWeight: 700, fontSize: 15, color: '#1e293b', marginBottom: 16 }}>
                        <i className="fas fa-layer-group" style={{ color: '#6366f1', marginRight: 8 }} />
                        {t('modbusSlave.dashboard.fcDistribution')}
                    </div>
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: 10 }}>
                        {[
                            { key: 'fc01', label: 'FC01 Read Coils', color: '#6366f1' },
                            { key: 'fc02', label: 'FC02 Read Discrete', color: '#8b5cf6' },
                            { key: 'fc03', label: 'FC03 Read HR', color: '#3b82f6' },
                            { key: 'fc04', label: 'FC04 Read IR', color: '#06b6d4' },
                            { key: 'fc05', label: 'FC05 Write Coil', color: '#f59e0b' },
                            { key: 'fc06', label: 'FC06 Write HR', color: '#f97316' },
                            { key: 'fc15', label: 'FC15 Write Multi Coil', color: '#ef4444' },
                            { key: 'fc16', label: 'FC16 Write Multi HR', color: '#ec4899' },
                        ].map(({ key, label, color }) => {
                            const total = rows.reduce((s, r) =>
                                s + ((r.stats?.slave_stats?.fc_counters as any)?.[key] ?? 0), 0);
                            if (!total) return null;
                            const max = Math.max(...[
                                ...rows.flatMap(r => Object.values(r.stats?.slave_stats?.fc_counters ?? {}))
                            ], 1) * rows.length;
                            return (
                                <div key={key}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 12, color: '#64748b', marginBottom: 4 }}>
                                        <span>{label}</span>
                                        <span style={{ fontWeight: 600, color: '#1e293b' }}>{total.toLocaleString()}</span>
                                    </div>
                                    <div style={{ height: 6, background: '#f1f5f9', borderRadius: 3, overflow: 'hidden' }}>
                                        <div style={{ width: `${Math.min((total / max) * 100, 100)}%`, height: '100%', background: color, borderRadius: 3, transition: 'width 0.4s' }} />
                                    </div>
                                </div>
                            );
                        })}
                    </div>
                </div>
            )}
        </div>
    );
};

export default DashboardTab;
