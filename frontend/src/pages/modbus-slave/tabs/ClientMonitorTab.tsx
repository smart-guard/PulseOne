// =============================================================================
// frontend/src/pages/modbus-slave/tabs/ClientMonitorTab.tsx
// 접속 클라이언트 실시간 모니터링 탭
// - 디바이스 선택 → 3초 폴링으로 getDeviceStats API 읽기
// - 요약 카드: 현재 접속 수, 총 요청, 성공률, 평균 응답시간
// - 클라이언트 개별 테이블: IP, 포트, 접속 시간, 요청수, 성공률, 응답시간
// - FC 분포 미니 바차트
// - 5분 / 60분 윈도우 통계
// =============================================================================
import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { Select, Tag, Tooltip } from 'antd';
import modbusSlaveApi, {
    ModbusSlaveDevice,
    DeviceStats,
    ClientSession,
} from '../../../api/services/modbusSlaveApi';
import { apiClient } from '../../../api/client';

const { Option } = Select;

// ─── 유틸 ──────────────────────────────────────────────────────────────────

const fmtUs = (us: number) =>
    us < 1000 ? `${us.toFixed(0)} µs` : `${(us / 1000).toFixed(2)} ms`;

const fmtRate = (r: number) => `${(r * 100).toFixed(1)}%`;

const fmtDuration = (sec: number) => {
    if (sec < 60) return `${sec}초`;
    if (sec < 3600) return `${Math.floor(sec / 60)}분 ${sec % 60}초`;
    return `${Math.floor(sec / 3600)}시간 ${Math.floor((sec % 3600) / 60)}분`;
};

const fmtUptime = (sec: number) => {
    const d = Math.floor(sec / 86400);
    const h = Math.floor((sec % 86400) / 3600);
    const m = Math.floor((sec % 3600) / 60);
    const s = sec % 60;
    if (d > 0) return `${d}일 ${h}시간 ${m}분`;
    if (h > 0) return `${h}시간 ${m}분 ${s}초`;
    return `${m}분 ${s}초`;
};

// ─── 미니 FC 바차트 ───────────────────────────────────────────────────────

const FcBar: React.FC<{ fc_counters: NonNullable<DeviceStats['slave_stats']>['fc_counters'] }> = ({ fc_counters }) => {
    const items = [
        { label: 'FC01 Read Coils', val: fc_counters?.fc01 ?? 0, color: '#6366f1' },
        { label: 'FC02 Read Discrete', val: fc_counters?.fc02 ?? 0, color: '#8b5cf6' },
        { label: 'FC03 Read HR', val: fc_counters?.fc03 ?? 0, color: '#3b82f6' },
        { label: 'FC04 Read IR', val: fc_counters?.fc04 ?? 0, color: '#06b6d4' },
        { label: 'FC05 Write Coil', val: fc_counters?.fc05 ?? 0, color: '#f59e0b' },
        { label: 'FC06 Write HR', val: fc_counters?.fc06 ?? 0, color: '#f97316' },
        { label: 'FC15 Write Multi Coil', val: fc_counters?.fc15 ?? 0, color: '#ef4444' },
        { label: 'FC16 Write Multi HR', val: fc_counters?.fc16 ?? 0, color: '#ec4899' },
    ].filter(i => i.val > 0);

    const max = Math.max(...items.map(i => i.val), 1);

    if (items.length === 0) return <span style={{ color: '#94a3b8', fontSize: 13 }}>요청 없음</span>;

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
            {items.map(it => (
                <Tooltip key={it.label} title={`${it.label}: ${it.val.toLocaleString()}회`}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                        <span style={{ width: 120, fontSize: 11, color: '#64748b', flexShrink: 0 }}>{it.label}</span>
                        <div style={{ flex: 1, height: 16, background: '#f1f5f9', borderRadius: 4, overflow: 'hidden' }}>
                            <div style={{
                                width: `${(it.val / max) * 100}%`,
                                height: '100%',
                                background: it.color,
                                borderRadius: 4,
                                minWidth: 4,
                                transition: 'width 0.4s',
                            }} />
                        </div>
                        <span style={{ width: 60, fontSize: 11, color: '#334155', textAlign: 'right', flexShrink: 0 }}>
                            {it.val.toLocaleString()}
                        </span>
                    </div>
                </Tooltip>
            ))}
        </div>
    );
};

// ─── 요약 카드 ────────────────────────────────────────────────────────────

const SummaryCard: React.FC<{
    label: string; value: string | number; sub?: string;
    icon: string; bg: string; fg: string;
}> = ({ label, value, sub, icon, bg, fg }) => (
    <div style={{
        background: '#fff', border: '1px solid #e2e8f0', borderRadius: 12, padding: '16px 20px',
        display: 'flex', alignItems: 'center', gap: 14, flex: 1, minWidth: 160,
    }}>
        <div style={{ width: 40, height: 40, background: bg, borderRadius: 10, display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0 }}>
            <i className={`fas ${icon}`} style={{ color: fg, fontSize: 17 }} />
        </div>
        <div>
            <div style={{ fontSize: 22, fontWeight: 800, color: '#1e293b', lineHeight: 1 }}>{value}</div>
            {sub && <div style={{ fontSize: 11, color: '#94a3b8', marginTop: 2 }}>{sub}</div>}
            <div style={{ fontSize: 12, color: '#64748b', marginTop: 2 }}>{label}</div>
        </div>
    </div>
);

// ─── 응답시간 트렌드 SVG ──────────────────────────────────────────────────────
const ResponseTrend: React.FC<{ history: number[] }> = ({ history }) => {
    if (history.length < 2) return <span style={{ color: '#94a3b8', fontSize: 11 }}>수집 중...</span>;
    const max = Math.max(...history, 1);
    const w = 120, h = 32;
    const pts = history.map((v, i) => `${(i / (history.length - 1)) * w},${h - (v / max) * (h - 4)}`).join(' ');
    const last = history[history.length - 1];
    const prev = history[history.length - 2];
    const color = last < prev * 0.95 ? '#16a34a' : last > prev * 1.05 ? '#ef4444' : '#6366f1';
    return (
        <Tooltip title={`최근 ${history.length}회 평균 응답시간 추이`}>
            <svg width={w} height={h} style={{ display: 'block', cursor: 'default' }}>
                <polyline points={pts} fill="none" stroke={color} strokeWidth={1.6} strokeLinejoin="round" />
                <circle cx={(history.length - 1) / (history.length - 1) * w}
                    cy={h - (last / max) * (h - 4)}
                    r={3} fill={color} />
            </svg>
        </Tooltip>
    );
};

// ─── 클라이언트 행 ─────────────────────────────────────────────────────────

const ClientRow: React.FC<{ sess: ClientSession; idx: number; deviceId: number | null; onBlock: (ip: string) => void; blocked: boolean }> = ({ sess, idx, onBlock, blocked }) => {
    const { t } = useTranslation('settings');
    const rate = sess.success_rate;
    const rateColor = rate >= 0.99 ? '#16a34a' : rate >= 0.9 ? '#f59e0b' : '#ef4444';
    return (
        <tr style={{ background: idx % 2 === 0 ? '#f8fafc' : '#fff', transition: 'background 0.15s' }}>
            <td style={td}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <span style={{ fontFamily: 'monospace', fontWeight: 600, color: blocked ? '#ef4444' : '#3b82f6' }}>{sess.ip}</span>
                    {blocked && <span style={{ fontSize: 10, background: '#fef2f2', color: '#ef4444', padding: '1px 6px', borderRadius: 10, fontWeight: 700 }}>{t('modbusSlave.blockIp.blocked')}</span>}
                </div>
            </td>
            <td style={td}>{sess.port}</td>
            <td style={td}><span style={{ fontSize: 12 }}>{sess.connected_at ? sess.connected_at.replace('T', ' ') : '-'}</span></td>
            <td style={td}>{fmtDuration(sess.duration_sec)}</td>
            <td style={{ ...td, textAlign: 'right' }}>{sess.requests.toLocaleString()}</td>
            <td style={{ ...td, textAlign: 'right' }}>
                <span style={{ color: rateColor, fontWeight: 700 }}>{fmtRate(rate)}</span>
            </td>
            <td style={{ ...td, textAlign: 'right' }}>{fmtUs(sess.avg_response_us)}</td>
            <td style={{ ...td, textAlign: 'right' }}>{sess.fc03_count.toLocaleString()}</td>
            <td style={{ ...td, textAlign: 'center' }}>
                <button
                    onClick={() => onBlock(sess.ip)}
                    disabled={blocked}
                    style={{
                        background: blocked ? '#f1f5f9' : '#fef2f2', border: `1px solid ${blocked ? '#e2e8f0' : '#fecaca'}`,
                        color: blocked ? '#94a3b8' : '#ef4444', borderRadius: 6,
                        padding: '3px 10px', cursor: blocked ? 'not-allowed' : 'pointer', fontSize: 11, fontWeight: 600,
                    }}
                >
                    {blocked ? t('modbusSlave.blockIp.blocked') : <><i className="fas fa-ban" style={{ marginRight: 3 }} />{t('modbusSlave.blockIp.button')}</>}
                </button>
            </td>
        </tr>
    );
};
const td: React.CSSProperties = { padding: '10px 12px', borderBottom: '1px solid #f1f5f9', fontSize: 13, color: '#334155' };
const th: React.CSSProperties = { padding: '10px 12px', fontSize: 12, fontWeight: 700, color: '#64748b', textAlign: 'left', borderBottom: '2px solid #e2e8f0', background: '#f8fafc' };

// ─── 메인 탭 ──────────────────────────────────────────────────────────────

interface Props {
    devices: ModbusSlaveDevice[];
}

const ClientMonitorTab: React.FC<Props> = ({ devices }) => {
    const { t } = useTranslation('settings');
    const [selectedId, setSelectedId] = useState<number | null>(null);
    const [stats, setStats] = useState<DeviceStats | null>(null);
    const [loading, setLoading] = useState(false);
    const [lastRefresh, setLastRefresh] = useState<Date | null>(null);
    const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);
    // 응답시간 히스토리 (최근 30개 폴링 값)
    const [respHistory, setRespHistory] = useState<number[]>([]);
    // 차단된 IP 목록
    const [blockedIps, setBlockedIps] = useState<Set<string>>(new Set());

    // 디바이스 목록 생기면 첫 번째 선택
    useEffect(() => {
        if (devices.length > 0 && selectedId === null) {
            setSelectedId(devices[0].id);
        }
    }, [devices]);

    const fetchStats = useCallback(async () => {
        if (!selectedId) return;
        try {
            setLoading(true);
            const res = await modbusSlaveApi.getDeviceStats(selectedId);
            if (res.success && res.data) {
                setStats(res.data);
                setLastRefresh(new Date());
                // 응답시간 히스토리 누적
                const us = res.data.slave_stats?.avg_response_us ?? 0;
                if (us > 0) setRespHistory(prev => [...prev.slice(-29), us]);
            }
        } finally {
            setLoading(false);
        }
    }, [selectedId]);

    // 디바이스 변경 시 히스토리 초기화
    useEffect(() => { setRespHistory([]); setBlockedIps(new Set()); }, [selectedId]);

    const handleBlockIp = async (ip: string) => {
        if (!selectedId) return;
        try {
            await apiClient.post(`/api/modbus-slave/${selectedId}/block-ip`, { ip });
            setBlockedIps(prev => new Set([...prev, ip]));
        } catch {
            // fallback: 로컬 상태만 추가
            setBlockedIps(prev => new Set([...prev, ip]));
        }
    };

    useEffect(() => {
        fetchStats();
        timerRef.current = setInterval(fetchStats, 3000);
        return () => { if (timerRef.current) clearInterval(timerRef.current); };
    }, [fetchStats]);

    const s = stats?.slave_stats;
    const c = stats?.clients;
    const sessions: ClientSession[] = c?.sessions ?? [];
    const isRunning = stats?.process_status === 'running';

    return (
        <div style={{ padding: '16px 0', display: 'flex', flexDirection: 'column', gap: 16 }}>

            {/* 디바이스 선택 + 상태 */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, flexWrap: 'wrap' }}>
                <Select
                    value={selectedId}
                    onChange={setSelectedId}
                    style={{ minWidth: 240 }}
                    placeholder="디바이스 선택"
                >
                    {devices.map(d => (
                        <Option key={d.id} value={d.id}>
                            <span style={{ marginRight: 6 }}>
                                <i className="fas fa-network-wired" style={{ color: d.process_status === 'running' ? '#16a34a' : '#94a3b8', marginRight: 6 }} />
                            </span>
                            {d.name} <span style={{ color: '#94a3b8', fontSize: 12 }}>(:{d.tcp_port})</span>
                        </Option>
                    ))}
                </Select>
                <Tag color={isRunning ? 'green' : 'default'}>
                    {isRunning ? `● ${t('modbusSlave.running')}` : `○ ${t('modbusSlave.stopped')}`}
                </Tag>
                {s && <span style={{ fontSize: 12, color: '#94a3b8' }}>{fmtUptime(s.uptime_sec)}</span>}
                {lastRefresh && (
                    <span style={{ fontSize: 12, color: '#b0bec5', marginLeft: 'auto' }}>
                        {t('modbusSlave.lastSeen')}: {lastRefresh.toLocaleTimeString()} {loading && <i className="fas fa-sync fa-spin" style={{ fontSize: 10, marginLeft: 4 }} />}
                    </span>
                )}
            </div>

            {/* Worker 미연결 안내 */}
            {!isRunning && (
                <div style={{ padding: '20px', textAlign: 'center', color: '#94a3b8', background: '#f8fafc', borderRadius: 10, border: '1px dashed #e2e8f0' }}>
                    <i className="fas fa-plug" style={{ fontSize: 28, display: 'block', marginBottom: 8 }} />
                    {t('modbusSlave.workerStopped')}
                </div>
            )}

            {isRunning && (
                <>
                    {/* 요약 카드 */}
                    <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap' }}>
                        <SummaryCard label={t('modbusSlave.monitor.clientIp')} value={c?.current_clients ?? 0}
                            sub={`${c?.total_connections ?? 0}`}
                            icon="fa-users" bg="#eff6ff" fg="#3b82f6" />
                        <SummaryCard label={t('modbusSlave.log.successRate')} value={c ? fmtRate(c.success_rate) : '-'}
                            sub={s ? `${s.total_requests.toLocaleString()}` : undefined}
                            icon="fa-check-circle" bg="#f0fdf4" fg="#16a34a" />
                        <SummaryCard label={t('modbusSlave.log.avgResponse')} value={s ? fmtUs(s.avg_response_us) : '-'}
                            sub={s ? `Max ${fmtUs(s.max_response_us)}` : undefined}
                            icon="fa-tachometer-alt" bg="#fdf4ff" fg="#9333ea" />
                        <SummaryCard label="5min req/min" value={s ? s.window_5min.req_per_min.toFixed(1) : '-'}
                            sub={s ? fmtRate(s.window_5min.success_rate) : undefined}
                            icon="fa-chart-line" bg="#fff7ed" fg="#f97316" />
                    </div>

                    {/* 클라이언트 테이블 */}
                    <div style={{ background: '#fff', border: '1px solid #e2e8f0', borderRadius: 12, overflow: 'hidden' }}>
                        <div style={{ padding: '14px 20px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', gap: 8 }}>
                            <i className="fas fa-list" style={{ color: '#3b82f6' }} />
                            <span style={{ fontWeight: 700, fontSize: 14, color: '#1e293b' }}>접속 클라이언트 상세</span>
                            <span style={{ background: '#dbeafe', color: '#1d4ed8', fontSize: 12, fontWeight: 700, padding: '2px 8px', borderRadius: 12, marginLeft: 4 }}>
                                {sessions.length}개
                            </span>
                            {/* 응답시간 트렌드 */}
                            {respHistory.length > 1 && (
                                <div style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center', gap: 8 }}>
                                    <span style={{ fontSize: 11, color: '#64748b' }}>{t('modbusSlave.responseTrend')}</span>
                                    <ResponseTrend history={respHistory} />
                                </div>
                            )}
                        </div>
                        {sessions.length === 0 ? (
                            <div style={{ padding: '32px', textAlign: 'center', color: '#94a3b8' }}>
                                <i className="fas fa-satellite-dish" style={{ fontSize: 28, display: 'block', marginBottom: 8 }} />
                                현재 연결된 SCADA/HMI 클라이언트가 없습니다
                            </div>
                        ) : (
                            <div style={{ overflowX: 'auto' }}>
                                <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                                    <thead>
                                        <tr>
                                            <th style={th}>IP</th>
                                            <th style={th}>{t('modbusSlave.log.port')}</th>
                                            <th style={th}>{t('modbusSlave.monitor.connected')}</th>
                                            <th style={th}>Duration</th>
                                            <th style={{ ...th, textAlign: 'right' }}>{t('modbusSlave.log.requests')}</th>
                                            <th style={{ ...th, textAlign: 'right' }}>{t('modbusSlave.log.successRate')}</th>
                                            <th style={{ ...th, textAlign: 'right' }}>{t('modbusSlave.log.avgResponse')}</th>
                                            <th style={{ ...th, textAlign: 'right' }}>FC03</th>
                                            <th style={{ ...th, textAlign: 'center' }}>{t('modbusSlave.blockIp.button')} IP</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {sessions.map((sess, i) => (
                                            <ClientRow key={`${sess.ip}-${sess.port}-${i}`}
                                                sess={sess} idx={i}
                                                deviceId={selectedId}
                                                onBlock={handleBlockIp}
                                                blocked={blockedIps.has(sess.ip)} />
                                        ))}
                                    </tbody>
                                </table>
                            </div>
                        )}
                    </div>

                    {/* FC 분포 + 윈도우 통계 */}
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
                        {/* FC 요청 분포 */}
                        <div style={{ background: '#fff', border: '1px solid #e2e8f0', borderRadius: 12, padding: '16px 20px' }}>
                            <div style={{ fontWeight: 700, fontSize: 14, color: '#1e293b', marginBottom: 12 }}>
                                <i className="fas fa-layer-group" style={{ color: '#6366f1', marginRight: 8 }} />
                                Function Code 요청 분포
                            </div>
                            {s?.fc_counters ? (
                                <FcBar fc_counters={s.fc_counters} />
                            ) : (
                                <span style={{ color: '#94a3b8', fontSize: 13 }}>데이터 없음</span>
                            )}
                        </div>

                        {/* 윈도우 통계 */}
                        <div style={{ background: '#fff', border: '1px solid #e2e8f0', borderRadius: 12, padding: '16px 20px' }}>
                            <div style={{ fontWeight: 700, fontSize: 14, color: '#1e293b', marginBottom: 12 }}>
                                <i className="fas fa-clock" style={{ color: '#f97316', marginRight: 8 }} />
                                시간 윈도우 통계
                            </div>
                            {s ? (
                                <table style={{ width: '100%', fontSize: 13, borderCollapse: 'collapse' }}>
                                    <thead>
                                        <tr>
                                            <th style={{ ...th, padding: '6px 10px' }}>구간</th>
                                            <th style={{ ...th, padding: '6px 10px', textAlign: 'right' }}>요청</th>
                                            <th style={{ ...th, padding: '6px 10px', textAlign: 'right' }}>성공률</th>
                                            <th style={{ ...th, padding: '6px 10px', textAlign: 'right' }}>요청/분</th>
                                            <th style={{ ...th, padding: '6px 10px', textAlign: 'right' }}>평균 응답</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {[
                                            { label: '최근 5분', w: s.window_5min },
                                            { label: '최근 60분', w: s.window_60min },
                                        ].map(({ label, w }) => (
                                            <tr key={label}>
                                                <td style={{ ...td, padding: '8px 10px', fontWeight: 600 }}>{label}</td>
                                                <td style={{ ...td, padding: '8px 10px', textAlign: 'right' }}>{w.requests.toLocaleString()}</td>
                                                <td style={{ ...td, padding: '8px 10px', textAlign: 'right' }}>
                                                    <span style={{ color: w.success_rate >= 0.99 ? '#16a34a' : '#f59e0b', fontWeight: 700 }}>
                                                        {fmtRate(w.success_rate)}
                                                    </span>
                                                </td>
                                                <td style={{ ...td, padding: '8px 10px', textAlign: 'right' }}>{w.req_per_min.toFixed(1)}</td>
                                                <td style={{ ...td, padding: '8px 10px', textAlign: 'right' }}>
                                                    {w.avg_resp_us ? fmtUs(w.avg_resp_us) : '-'}
                                                </td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            ) : (
                                <span style={{ color: '#94a3b8', fontSize: 13 }}>Worker 통계 미수신 (30초 후 갱신)</span>
                            )}
                        </div>
                    </div>
                </>
            )}
        </div>
    );
};

export default ClientMonitorTab;
