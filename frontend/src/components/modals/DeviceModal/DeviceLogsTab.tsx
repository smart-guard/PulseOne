// ============================================================================
// DeviceLogsTab.tsx - 실제 API 연동 버전
// GET /api/devices/:id/packet-logs  → C++ logPacket 파일 파싱
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import apiClient from '../../../api/client';

// ── 타입 ────────────────────────────────────────────────────────────────────
interface PacketEntry {
  timestamp: string;
  protocol: string;
  raw: string;
  decoded: string;
}


// ── Modbus raw 텍스트 파서 (point_id=XX raw=YY) ──────────────────────────
const parseModbusDecoded = (decoded: string) => {
  const kv: Record<string, string> = {};
  decoded.split(' ').forEach(part => {
    const [k, v] = part.split('=');
    if (k && v !== undefined) kv[k] = v;
  });
  return kv; // { value, name, ... }
};

const parseModbusRaw = (raw: string) => {
  const kv: Record<string, string> = {};
  raw.split(' ').forEach(part => {
    const [k, v] = part.split('=');
    if (k && v !== undefined) kv[k] = v;
  });
  return kv; // { point_id, raw }
};

// ── 프로토콜별 해석 패널 ─────────────────────────────────────────────────
const AnalysisPanel: React.FC<{ entry: PacketEntry }> = ({ entry }) => {
  const p = entry.protocol?.toUpperCase();

  if (p === 'MODBUS') {
    const r = parseModbusRaw(entry.raw);
    const d = parseModbusDecoded(entry.decoded);
    return (
      <div className="analysis-grid">
        <div className="a-item"><span className="a-label">🔢 Point ID</span>{r.point_id ?? '-'}</div>
        <div className="a-item"><span className="a-label">📡 Raw Value</span>{r.raw ?? '-'}</div>
        <div className="a-item"><span className="a-label">✅ Scaled Value</span>{d.value ?? '-'}</div>
        <div className="a-item"><span className="a-label">🏷️ Point Name</span>{d.name ?? '-'}</div>
        <div className="a-item a-full">
          <span className="a-label">💡 해석</span>
          {d.name ? (
            <span>포인트 <strong>{d.name}</strong> (ID:{r.point_id}) 폴링 완료 — 원시값 {r.raw} → 스케일값 {d.value}</span>
          ) : (
            <span>Modbus 폴링 데이터 (ID:{r.point_id})</span>
          )}
        </div>
      </div>
    );
  }

  if (p === 'MQTT') {
    return (
      <div className="analysis-grid">
        <div className="a-item"><span className="a-label">📨 Topic</span>{entry.raw}</div>
        <div className="a-item a-full"><span className="a-label">📦 Payload</span>
          <pre className="payload-pre">{(() => {
            try { return JSON.stringify(JSON.parse(entry.decoded), null, 2); }
            catch { return entry.decoded; }
          })()}</pre>
        </div>
        <div className="a-item a-full">
          <span className="a-label">💡 해석</span>
          토픽 <strong>{entry.raw}</strong>으로 메시지 수신 — 페이로드 {entry.decoded.length}자
        </div>
      </div>
    );
  }

  if (p === 'BACNET') {
    // raw = HEX 덤프
    const bytes = entry.raw.trim().split(' ').filter(Boolean);
    const byteCount = bytes.length;
    const pduType = bytes[0] ? parseInt(bytes[0], 16) : null;
    return (
      <div className="analysis-grid">
        <div className="a-item"><span className="a-label">📏 길이</span>{byteCount} bytes</div>
        <div className="a-item"><span className="a-label">🔖 PDU Type (byte0)</span>0x{bytes[0] ?? '--'}</div>
        <div className="a-item a-full"><span className="a-label">🔢 HEX Dump</span>
          <pre className="hex-dump">{entry.raw}</pre>
        </div>
        <div className="a-item a-full">
          <span className="a-label">💡 해석</span>
          BACnet 패킷 수신 ({byteCount} bytes)
          {pduType !== null && `, PDU 타입 0x${bytes[0]}`}
        </div>
      </div>
    );
  }

  return (
    <div className="analysis-grid">
      <div className="a-item a-full"><span className="a-label">RAW</span>{entry.raw}</div>
      <div className="a-item a-full"><span className="a-label">DECODED</span>{entry.decoded}</div>
    </div>
  );
};

// ── 메인 컴포넌트 ─────────────────────────────────────────────────────────
interface Props { deviceId: number; }

const DeviceLogsTab: React.FC<Props> = ({ deviceId }) => {
  const [entries, setEntries] = useState<PacketEntry[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [expanded, setExpanded] = useState<number[]>([]);
  const [search, setSearch] = useState('');
  const [dateStr, setDateStr] = useState(new Date().toISOString().slice(0, 10));
  const [logDir, setLogDir] = useState('');
  const [isRealTime, setIsRealTime] = useState(false);

  const load = useCallback(async () => {
    setIsLoading(true);
    setError(null);
    try {
      const result = await apiClient.get<{
        device_id: number;
        device_name: string;
        date: string;
        log_dir: string;
        entries: PacketEntry[];
      }>(`/api/devices/${deviceId}/packet-logs`, { date: dateStr, limit: 200 });
      if (result.success && result.data) {
        setEntries(result.data.entries);
        setLogDir(result.data.log_dir);
      } else {
        setError(result.message || '로그 조회 실패');
      }
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : '네트워크 오류');
    } finally {
      setIsLoading(false);
    }
  }, [deviceId, dateStr]);

  useEffect(() => { load(); }, [load]);

  // 실시간 모드: 10초마다 갱신
  useEffect(() => {
    if (!isRealTime) return;
    const timer = setInterval(load, 10000);
    return () => clearInterval(timer);
  }, [isRealTime, load]);

  const filtered = entries.filter(e => {
    if (!search) return true;
    return (
      e.raw.toLowerCase().includes(search.toLowerCase()) ||
      e.decoded.toLowerCase().includes(search.toLowerCase()) ||
      e.protocol.toLowerCase().includes(search.toLowerCase())
    );
  });

  const toggle = (i: number) =>
    setExpanded(prev => prev.includes(i) ? prev.filter(x => x !== i) : [...prev, i]);

  const protocolColor = (p: string) => {
    if (p === 'Modbus') return '#3b82f6';
    if (p === 'MQTT') return '#8b5cf6';
    if (p === 'BACnet') return '#f59e0b';
    return '#64748b';
  };

  return (
    <div className="plt-wrapper">
      {/* 컨트롤 바 */}
      <div className="plt-controls">
        <div className="plt-left">
          <span className="plt-title">통신 패킷 로그</span>
          <span className="plt-subtitle">
            {logDir ? <code>{logDir}</code> : '로그 파일 조회'}
          </span>
        </div>
        <div className="plt-right">
          <input
            type="date"
            value={dateStr}
            onChange={e => setDateStr(e.target.value)}
            className="plt-datepicker"
          />
          <input
            type="text"
            placeholder="검색..."
            value={search}
            onChange={e => setSearch(e.target.value)}
            className="plt-search"
          />
          <label className="plt-realtime">
            <input type="checkbox" checked={isRealTime} onChange={e => setIsRealTime(e.target.checked)} />
            실시간
          </label>
          <button className="plt-btn" onClick={load} disabled={isLoading} title="새로고침">
            <span className={isLoading ? 'spin' : ''}>⟳</span>
          </button>
        </div>
      </div>

      {/* 상태 메시지 */}
      {error && (
        <div className="plt-error">
          ⚠️ {error}
          {error.includes('ENOENT') || error.includes('없') ? (
            <span> — packet_logging_enabled=true 설정 후 재시작하면 로그가 생성됩니다.</span>
          ) : null}
        </div>
      )}

      {!error && !isLoading && entries.length === 0 && (
        <div className="plt-empty">
          <div>📭 패킷 로그 없음</div>
          <div className="plt-empty-hint">
            시스템 설정에서 <strong>packet_logging_enabled = true</strong>로 설정하면
            <br />Modbus / MQTT / BACnet 통신 데이터가 <code>logs/packets/</code>에 기록됩니다.
          </div>
        </div>
      )}

      {/* 테이블 */}
      {filtered.length > 0 && (
        <div className="plt-table-wrap">
          <table className="plt-table">
            <thead>
              <tr>
                <th style={{ width: 180 }}>시간</th>
                <th style={{ width: 90 }}>프로토콜</th>
                <th>RAW</th>
                <th>DECODED</th>
              </tr>
            </thead>
            <tbody>
              {filtered.map((e, i) => (
                <React.Fragment key={i}>
                  <tr
                    className={`plt-row ${expanded.includes(i) ? 'exp' : ''}`}
                    onClick={() => toggle(i)}
                  >
                    <td className="plt-ts">{e.timestamp}</td>
                    <td>
                      <span
                        className="plt-proto"
                        style={{ background: protocolColor(e.protocol) + '22', color: protocolColor(e.protocol), border: `1px solid ${protocolColor(e.protocol)}44` }}
                      >
                        {e.protocol}
                      </span>
                    </td>
                    <td className="plt-raw">{e.raw}</td>
                    <td className="plt-decoded">{e.decoded}</td>
                  </tr>
                  {expanded.includes(i) && (
                    <tr className="plt-detail-row">
                      <td colSpan={4}>
                        <div className="plt-analysis">
                          <div className="plt-analysis-header">
                            🔬 엔지니어 해석 — {e.protocol} @ {e.timestamp}
                          </div>
                          <AnalysisPanel entry={e} />
                        </div>
                      </td>
                    </tr>
                  )}
                </React.Fragment>
              ))}
            </tbody>
          </table>
        </div>
      )}

      <style>{`
        .plt-wrapper { display: flex; flex-direction: column; height: 100%; font-family: 'Inter', sans-serif; background: #fff; }

        .plt-controls { display: flex; justify-content: space-between; align-items: center; padding: 8px 14px; background: #f8fafc; border-bottom: 1px solid #e2e8f0; gap: 12px; flex-wrap: wrap; }
        .plt-left  { display: flex; align-items: center; gap: 10px; }
        .plt-right { display: flex; align-items: center; gap: 8px; }
        .plt-title { font-weight: 700; font-size: 13px; color: #0f172a; }
        .plt-subtitle { font-size: 11px; color: #64748b; }
        .plt-subtitle code { background: #f1f5f9; padding: 1px 5px; border-radius: 4px; font-size: 10px; }

        .plt-datepicker { height: 28px; border: 1px solid #cbd5e1; border-radius: 4px; font-size: 12px; padding: 0 6px; }
        .plt-search { height: 28px; width: 180px; border: 1px solid #cbd5e1; border-radius: 4px; font-size: 12px; padding: 0 8px; }
        .plt-realtime { display: flex; align-items: center; gap: 4px; font-size: 12px; color: #475569; cursor: pointer; }
        .plt-btn { background: none; border: 1px solid #e2e8f0; border-radius: 4px; width: 28px; height: 28px; cursor: pointer; font-size: 16px; display: flex; align-items: center; justify-content: center; color: #475569; }
        .plt-btn:hover { background: #f1f5f9; }
        .spin { display: inline-block; animation: spin 1s linear infinite; }
        @keyframes spin { to { transform: rotate(360deg); } }

        .plt-error { margin: 12px 14px; padding: 10px 14px; background: #fef2f2; border: 1px solid #fecaca; border-radius: 6px; font-size: 12px; color: #dc2626; }
        .plt-empty { flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 12px; color: #94a3b8; font-size: 14px; padding: 40px; }
        .plt-empty-hint { font-size: 12px; color: #64748b; text-align: center; line-height: 1.7; background: #f8fafc; border: 1px dashed #cbd5e1; border-radius: 8px; padding: 14px 20px; }
        .plt-empty-hint code { background: #e2e8f0; padding: 1px 5px; border-radius: 3px; }

        .plt-table-wrap { flex: 1; overflow: auto; }
        .plt-table { width: 100%; border-collapse: collapse; font-size: 12px; table-layout: fixed; }
        .plt-table th { position: sticky; top: 0; background: #f1f5f9; padding: 7px 12px; text-align: left; font-weight: 600; color: #475569; border-bottom: 2px solid #e2e8f0; }
        .plt-table td { padding: 6px 12px; border-bottom: 1px solid #f1f5f9; vertical-align: middle; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
        .plt-row { cursor: pointer; }
        .plt-row:hover { background: #f8fafc; }
        .plt-row.exp { background: #eff6ff !important; }

        .plt-ts  { font-family: 'JetBrains Mono', monospace; font-size: 11px; color: #64748b; }
        .plt-proto { display: inline-block; font-size: 10px; font-weight: 700; padding: 2px 7px; border-radius: 4px; }
        .plt-raw     { font-family: monospace; font-size: 11px; color: #334155; }
        .plt-decoded { font-family: monospace; font-size: 11px; color: #475569; }

        .plt-detail-row td { background: #f0f9ff; padding: 0; }
        .plt-analysis { padding: 14px 18px; }
        .plt-analysis-header { font-size: 11px; font-weight: 700; color: #3b82f6; margin-bottom: 12px; padding-bottom: 6px; border-bottom: 1px solid #bfdbfe; }

        .analysis-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; }
        .a-item { display: flex; flex-direction: column; gap: 3px; background: white; border: 1px solid #e2e8f0; border-radius: 6px; padding: 8px 12px; font-size: 12px; color: #0f172a; }
        .a-item.a-full { grid-column: 1 / -1; }
        .a-label { font-size: 10px; font-weight: 600; color: #64748b; margin-bottom: 2px; }
        .payload-pre { margin: 0; font-family: monospace; font-size: 11px; white-space: pre-wrap; color: #334155; max-height: 120px; overflow: auto; }
        .hex-dump { margin: 0; font-family: 'JetBrains Mono', monospace; font-size: 11px; color: #334155; white-space: pre-wrap; word-break: break-all; }
      `}</style>
    </div>
  );
};

export default DeviceLogsTab;