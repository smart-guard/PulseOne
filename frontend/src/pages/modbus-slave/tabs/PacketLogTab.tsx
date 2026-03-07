// =============================================================================
// frontend/src/pages/modbus-slave/tabs/PacketLogTab.tsx
// 패킷 로그 파일 뷰어 — logs/packets/날짜/modbus-slave_dN.log 파일 탐색 + 표시
// =============================================================================
import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { Select, Tag } from 'antd';
import { ModbusSlaveDevice } from '../../../api/services/modbusSlaveApi';
import { apiClient } from '../../../api/client';

const { Option } = Select;

// ─── 유틸 ─────────────────────────────────────────────────────────────────

const fmtSize = (bytes: number) =>
    bytes > 1024 * 1024 ? `${(bytes / 1024 / 1024).toFixed(1)} MB`
        : bytes > 1024 ? `${(bytes / 1024).toFixed(1)} KB`
            : `${bytes} B`;

// ─── 패킷 로그 파일 타입 ─────────────────────────────────────────────────

interface PacketLogFile {
    date: string;
    filename: string;
    path: string;
    size_bytes: number;
    modified_at: string | null;
}

interface PacketLogContent {
    path: string;
    total_lines: number;
    shown_lines: number;
    content: string;
}

// ─── 로그 라인 파서 ──────────────────────────────────────────────────────

interface ParsedLogLine {
    timestamp?: string;
    raw?: string;
    decoded?: string;
    full: string;
}

function parseLogLines(raw: string): ParsedLogLine[] {
    const result: ParsedLogLine[] = [];
    const lines = raw.split('\n').filter(l => l.trim());
    let i = 0;
    while (i < lines.length) {
        const line = lines[i];
        if (line.startsWith('[') && line.includes(']')) {
            const ts = line.match(/^\[([^\]]+)\]/)?.[1];
            const rawLine = lines[i + 1]?.startsWith('[RAW]') ? lines[i + 1] : undefined;
            const decodedLine = lines[i + 2]?.startsWith('[DECODED]') ? lines[i + 2] : undefined;
            result.push({
                timestamp: ts,
                raw: rawLine?.replace('[RAW] ', ''),
                decoded: decodedLine?.replace('[DECODED] ', ''),
                full: line,
            });
            i += rawLine ? (decodedLine ? 3 : 2) : 1;
        } else {
            result.push({ full: line });
            i++;
        }
    }
    return result;
}

// ─── 메인 탭 ─────────────────────────────────────────────────────────────

interface Props { devices: ModbusSlaveDevice[] }

const PacketLogTab: React.FC<Props> = ({ devices }) => {
    const { t } = useTranslation('settings');
    const [deviceId, setDeviceId] = useState<number | null>(null);
    const [files, setFiles] = useState<PacketLogFile[]>([]);
    const [selectedFile, setSelectedFile] = useState<PacketLogFile | null>(null);
    const [logContent, setLogContent] = useState<PacketLogContent | null>(null);
    const [loading, setLoading] = useState(false);
    const [autoScroll, setAutoScroll] = useState(true);
    const [autoRefresh, setAutoRefresh] = useState(false);
    const [countdown, setCountdown] = useState(10);
    // 실시간 스트리밍 모드
    const [streaming, setStreaming] = useState(false);
    const [streamLines, setStreamLines] = useState<string[]>([]);
    const [lastLineCount, setLastLineCount] = useState(0);
    const contentRef = useRef<HTMLPreElement>(null);
    const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);
    const streamTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
    const [searchQuery, setSearchQuery] = useState('');

    useEffect(() => {
        if (devices.length > 0 && deviceId === null) setDeviceId(devices[0].id);
    }, [devices]);

    const fetchFiles = useCallback(async () => {
        try {
            const res = await apiClient.get<any>(`/api/modbus-slave/packet-logs`, {
                params: { device_id: deviceId ?? undefined }
            });
            const list = res.data?.data ?? res.data ?? [];
            setFiles(Array.isArray(list) ? list : []);
        } catch { setFiles([]); }
    }, [deviceId]);

    const fetchContent = useCallback(async (file: PacketLogFile) => {
        try {
            setLoading(true);
            const res = await apiClient.get<any>(`/api/modbus-slave/packet-logs/read`, {
                params: { path: file.path, tail: 1000 }
            });
            setLogContent(res.data?.data ?? res.data ?? null);
        } finally { setLoading(false); }
    }, []);

    useEffect(() => { fetchFiles(); }, [fetchFiles]);
    useEffect(() => {
        if (files.length > 0 && !selectedFile) {
            setSelectedFile(files[0]);
        }
    }, [files]);
    useEffect(() => {
        if (selectedFile) fetchContent(selectedFile);
    }, [selectedFile, fetchContent]);
    useEffect(() => {
        if (autoScroll && contentRef.current) {
            contentRef.current.scrollTop = contentRef.current.scrollHeight;
        }
    }, [logContent, autoScroll]);

    // 실시간 스트리밍: 1초마다 tail API 호출 → 새 줄만 append
    const fetchIncremental = useCallback(async () => {
        if (!selectedFile) return;
        try {
            const res = await apiClient.get<any>(`/api/modbus-slave/packet-logs/read`, {
                params: { path: selectedFile.path, tail: 2000 }
            });
            const data: PacketLogContent = res.data?.data ?? res.data;
            if (!data) return;
            const allLines = data.content.split('\n').filter(l => l.trim());
            if (allLines.length > lastLineCount) {
                const newLines = allLines.slice(lastLineCount);
                setStreamLines(prev => [...prev, ...newLines]);
                setLastLineCount(allLines.length);
            }
        } catch { /* ignore */ }
    }, [selectedFile, lastLineCount]);

    useEffect(() => {
        if (!streaming || !selectedFile) {
            if (streamTimerRef.current) clearInterval(streamTimerRef.current);
            return;
        }
        // 시작 시 전체 로드
        setStreamLines([]);
        setLastLineCount(0);
        fetchIncremental();
        streamTimerRef.current = setInterval(fetchIncremental, 1000);
        return () => { if (streamTimerRef.current) clearInterval(streamTimerRef.current); };
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [streaming, selectedFile?.path]);

    useEffect(() => {
        if (timerRef.current) clearInterval(timerRef.current);
        if (!autoRefresh || !selectedFile) { setCountdown(10); return; }
        setCountdown(10);
        timerRef.current = setInterval(() => {
            setCountdown(prev => {
                if (prev <= 1) { fetchContent(selectedFile!); return 10; }
                return prev - 1;
            });
        }, 1000);
        return () => { if (timerRef.current) clearInterval(timerRef.current); };
    }, [autoRefresh, selectedFile, fetchContent]);

    const parsed = logContent ? parseLogLines(logContent.content) : [];
    // 검색 필터 적용
    const filteredParsed = searchQuery
        ? parsed.filter(item => item.full.toLowerCase().includes(searchQuery.toLowerCase()))
        : parsed;

    const handleDownload = () => {
        if (!selectedFile || !logContent) return;
        const blob = new Blob([logContent.content], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url; a.download = selectedFile.filename;
        a.click(); URL.revokeObjectURL(url);
    };

    return (
        <div style={{ padding: '16px 0 4px', display: 'flex', gap: 16, minHeight: 520 }}>
            {/* 왼쪽: 파일 트리 */}
            <div style={{ width: 280, flexShrink: 0, background: '#fff', border: '1px solid #e2e8f0', borderRadius: 12, overflow: 'visible', display: 'flex', flexDirection: 'column' }}>
                <div style={{ padding: '12px 16px', borderBottom: '1px solid #f1f5f9', background: '#f8fafc' }}>
                    <div style={{ fontWeight: 700, fontSize: 13, color: '#1e293b', marginBottom: 8 }}>
                        <i className="fas fa-file-alt" style={{ marginRight: 6, color: '#3b82f6' }} />{t('modbusSlave.packetLog.title')}
                    </div>
                    <Select value={deviceId} onChange={v => { setDeviceId(v); setSelectedFile(null); }}
                        style={{ width: '100%' }} size="small" placeholder={t('modbusSlave.device.name')}>
                        <Option value={null}>{t('modbusSlave.allTenants')}</Option>
                        {devices.map(d => <Option key={d.id} value={d.id}>{d.name}</Option>)}
                    </Select>
                </div>
                <div style={{ flex: 1, overflowY: 'auto' }}>
                    {files.length === 0 ? (
                        <div style={{ padding: 24, textAlign: 'center', color: '#94a3b8', fontSize: 13 }}>
                            <i className="fas fa-inbox" style={{ fontSize: 24, display: 'block', marginBottom: 8 }} />
                            {t('modbusSlave.packetLog.noLogs')}<br />
                            <small>{t('modbusSlave.packetLog.enableHint')}</small>
                        </div>
                    ) : files.map((f, i) => (
                        <div key={i} onClick={() => setSelectedFile(f)}
                            style={{
                                padding: '10px 14px', cursor: 'pointer', borderBottom: '1px solid #f1f5f9',
                                background: selectedFile?.path === f.path ? '#eff6ff' : 'transparent',
                                borderLeft: selectedFile?.path === f.path ? '3px solid #3b82f6' : '3px solid transparent',
                            }}>
                            <div style={{ fontSize: 12, fontWeight: 600, color: '#1e293b', wordBreak: 'break-all' }}>{f.filename}</div>
                            <div style={{ marginTop: 3, display: 'flex', gap: 6 }}>
                                <Tag style={{ fontSize: 10, margin: 0 }}>{f.date}</Tag>
                                <span style={{ fontSize: 10, color: '#94a3b8' }}>{fmtSize(f.size_bytes)}</span>
                            </div>
                        </div>
                    ))}
                </div>
                <div style={{ padding: '8px 14px', borderTop: '1px solid #f1f5f9', background: '#f8fafc' }}>
                    <button onClick={fetchFiles}
                        style={{ width: '100%', padding: '6px 0', background: '#3b82f6', color: '#fff', border: 'none', borderRadius: 6, cursor: 'pointer', fontSize: 12 }}>
                        <i className="fas fa-sync-alt" style={{ marginRight: 4 }} />{t('modbusSlave.packetLog.refreshFiles')}

                    </button>
                </div>
            </div>

            {/* 오른쪽: 로그 내용 */}
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 10 }}>
                {/* 툴바 */}
                <div style={{ background: '#fff', border: '1px solid #e2e8f0', borderRadius: 10, padding: '10px 14px', display: 'flex', alignItems: 'center', gap: 10 }}>
                    <span style={{ fontSize: 13, fontWeight: 600, color: '#1e293b' }}>
                        {selectedFile ? selectedFile.filename : t('modbusSlave.packetLog.selectFile')}
                    </span>
                    {logContent && (
                        <span style={{ fontSize: 11, color: '#94a3b8' }}>
                            ({logContent.shown_lines.toLocaleString()}/{logContent.total_lines.toLocaleString()})
                        </span>
                    )}
                    {loading && <i className="fas fa-sync fa-spin" style={{ fontSize: 12, color: '#94a3b8' }} />}
                    <div style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center', gap: 8 }}>
                        {/* 검색 입력 */}
                        <input
                            type="text"
                            value={searchQuery}
                            onChange={e => setSearchQuery(e.target.value)}
                            placeholder="FC03, 192.168..."
                            style={{ border: '1px solid #e2e8f0', background: '#f8fafc', color: '#334155', borderRadius: 6, padding: '3px 10px', fontSize: 12, width: 160 }}
                        />
                        {/* 다운로드 버튼 */}
                        {selectedFile && logContent && (
                            <button onClick={handleDownload}
                                style={{ background: '#16a34a', color: '#fff', border: 'none', borderRadius: 6, padding: '4px 10px', cursor: 'pointer', fontSize: 12 }}>
                                <i className="fas fa-download" style={{ marginRight: 4 }} />{t('modbusSlave.download')}
                            </button>
                        )}
                        {/* 실시간 스트리밍 버튼 */}
                        <button
                            onClick={() => { setStreaming(v => !v); if (autoRefresh) setAutoRefresh(false); }}
                            style={{
                                background: streaming ? '#fef3c7' : '#f1f5f9', border: 'none', borderRadius: 20,
                                padding: '4px 12px', cursor: 'pointer', fontSize: 12,
                                color: streaming ? '#92400e' : '#64748b', fontWeight: streaming ? 700 : 400,
                            }}
                        >
                            {streaming
                                ? <><i className="fas fa-satellite-dish fa-beat" style={{ marginRight: 5, color: '#f59e0b' }} />{t('modbusSlave.tailStream.streaming')}&nbsp;&nbsp;<i className="fas fa-stop" />{t('modbusSlave.tailStream.stop')}</>
                                : <><i className="fas fa-satellite-dish" style={{ marginRight: 5 }} />{t('modbusSlave.tailStream.start')}</>}
                        </button>
                        <label style={{ fontSize: 12, color: '#64748b', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: 4 }}>
                            <input type="checkbox" checked={autoScroll} onChange={e => setAutoScroll(e.target.checked)} />
                            {t('modbusSlave.packetLog.autoScroll')}
                        </label>
                        <button onClick={() => setAutoRefresh(v => !v)}
                            style={{ background: autoRefresh ? '#dbeafe' : '#f1f5f9', border: 'none', borderRadius: 20, padding: '4px 10px', cursor: 'pointer', fontSize: 12, color: autoRefresh ? '#2563eb' : '#64748b' }}>
                            {autoRefresh ? <><i className="fas fa-circle-notch fa-spin" style={{ marginRight: 4, fontSize: 10 }} />{countdown}s {t('modbusSlave.packetLog.autoRefresh')}</> : `⏸ ${t('modbusSlave.packetLog.autoRefresh')}`}
                        </button>
                        {selectedFile && (
                            <button onClick={() => fetchContent(selectedFile)}
                                style={{ background: '#3b82f6', color: '#fff', border: 'none', borderRadius: 6, padding: '4px 10px', cursor: 'pointer', fontSize: 12 }}>
                                <i className="fas fa-sync-alt" style={{ marginRight: 4 }} />{t('modbusSlave.packetLog.refresh')}
                            </button>
                        )}
                    </div>
                </div>

                {/* 로그 본문 */}
                <div style={{ flex: 1, background: '#0f172a', borderRadius: 10, overflow: 'hidden', display: 'flex', flexDirection: 'column', minHeight: 440 }}>
                    {!selectedFile ? (
                        <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#475569' }}>
                            <div style={{ textAlign: 'center' }}>
                                <i className="fas fa-file-code" style={{ fontSize: 48, display: 'block', marginBottom: 12 }} />
                                {t('modbusSlave.packetLog.selectFilePrompt')}
                            </div>
                        </div>
                    ) : streaming ? (
                        /* 실시간 스트리밍 뷰 */
                        <pre ref={contentRef} style={{ flex: 1, overflowY: 'auto', margin: 0, padding: '12px 16px', fontFamily: 'JetBrains Mono, Consolas, monospace', fontSize: 12, lineHeight: 1.7, color: '#e2e8f0' }}>
                            {streamLines.length === 0
                                ? <span style={{ color: '#475569' }}>{t('modbusSlave.tailStream.waiting')}</span>
                                : streamLines.map((line, i) => {
                                    const m = line.match(/^\[([^\]]+)\] (CLIENT=\S+) (.+) (REQ\[.+)$/);
                                    return (
                                        <div key={i}>
                                            {m ? (
                                                <>
                                                    <span style={{ color: '#94a3b8' }}>[{m[1]}] </span>
                                                    <span style={{ color: '#fbbf24' }}>{m[2]} </span>
                                                    <span style={{ color: '#86efac' }}>{m[3]} </span>
                                                    <span style={{ color: '#93c5fd' }}>{m[4]}</span>
                                                </>
                                            ) : <span style={{ color: '#64748b' }}>{line}</span>}
                                        </div>
                                    );
                                })
                            }
                        </pre>
                    ) : (
                        <pre ref={contentRef} style={{ flex: 1, overflowY: 'auto', margin: 0, padding: '12px 16px', fontFamily: 'JetBrains Mono, Consolas, monospace', fontSize: 12, color: '#e2e8f0', lineHeight: 1.7 }}>
                            {filteredParsed.length === 0 ? (
                                <span style={{ color: '#475569' }}>{searchQuery ? `"${searchQuery}" 일치 없음` : '내용 없음'}</span>
                            ) : filteredParsed.map((item, i) => (
                                <div key={i} style={{ marginBottom: item.raw ? 12 : 0 }}>
                                    {item.timestamp && (
                                        <span style={{ color: '#94a3b8' }}>[{item.timestamp}]{' '}{'\n'}</span>
                                    )}
                                    {item.raw && (
                                        <span>
                                            <span style={{ color: '#fbbf24' }}>{'[RAW]    '}</span>
                                            <span style={{ color: '#93c5fd' }}>{item.raw}</span>{'\n'}
                                        </span>
                                    )}
                                    {item.decoded && (
                                        <span>
                                            <span style={{ color: '#86efac' }}>{'[DECODED]'}</span>
                                            <span style={{ color: '#f0fdf4' }}> {item.decoded}</span>{'\n'}
                                        </span>
                                    )}
                                    {!item.timestamp && !item.raw && !item.decoded && (
                                        <span style={{ color: '#64748b' }}>{item.full}{'\n'}</span>
                                    )}
                                </div>
                            ))}
                        </pre>
                    )}
                </div>
            </div>
        </div>
    );
};

export default PacketLogTab;
