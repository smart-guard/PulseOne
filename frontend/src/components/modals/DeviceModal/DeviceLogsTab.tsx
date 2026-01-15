// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceLogsTab.tsx
// 📄 디바이스 로그 탭 컴포넌트 - 시스템 로그 & 통신 패킷 뷰어 (Modbus 파서 포함)
// ============================================================================

import React, { useState, useEffect, useRef } from 'react';
import { DeviceLogsTabProps, DeviceLogEntry, PacketLogEntry } from './types';
import '../../../styles/management.css';

const DeviceLogsTab: React.FC<DeviceLogsTabProps> = ({ deviceId }) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [logType, setLogType] = useState<'system' | 'packet'>('system');

  const [logs, setLogs] = useState<DeviceLogEntry[]>([]);
  const [packetLogs, setPacketLogs] = useState<PacketLogEntry[]>([]);

  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // 필터 상태
  const [filterLevel, setFilterLevel] = useState<string>('ALL');
  const [filterCategory, setFilterCategory] = useState<string>('ALL');
  const [searchTerm, setSearchTerm] = useState('');

  const [isRealTime, setIsRealTime] = useState(false);
  const [expandedLogs, setExpandedLogs] = useState<number[]>([]);

  const logsEndRef = useRef<HTMLTableRowElement>(null);

  // ========================================================================
  // Modbus 패킷 파서
  // ========================================================================
  const parseModbusPacket = (hexStr: string, direction: 'TX' | 'RX') => {
    // 공백 제거 및 바이트 배열 변환
    const cleanHex = hexStr.replace(/\s/g, '');
    const bytes = [];
    for (let i = 0; i < cleanHex.length; i += 2) {
      bytes.push(parseInt(cleanHex.substr(i, 2), 16));
    }

    if (bytes.length < 7) return null; // 최소 헤더 길이 미달

    const txId = (bytes[0] << 8) | bytes[1];
    const protoId = (bytes[2] << 8) | bytes[3];
    const len = (bytes[4] << 8) | bytes[5];
    const unitId = bytes[6];
    const funcCode = bytes[7];

    let type = 'UNKNOWN';
    let details = '';

    // Function Code 해석
    if (funcCode === 0x01) type = 'Read Coils (01)';
    else if (funcCode === 0x02) type = 'Read Discrete Inputs (02)';
    else if (funcCode === 0x03) type = 'Read Holding Regs (03)';
    else if (funcCode === 0x04) type = 'Read Input Regs (04)';
    else if (funcCode === 0x05) type = 'Write Single Coil (05)';
    else if (funcCode === 0x06) type = 'Write Single Reg (06)';
    else if (funcCode === 0x0F) type = 'Write Multiple Coils (15)';
    else if (funcCode === 0x10) type = 'Write Multiple Regs (16)';
    else if (funcCode & 0x80) type = `Exception (Error ${bytes[8]})`;

    // 상세 내용 해석 (간략화)
    if (direction === 'TX') {
      if ([0x01, 0x02, 0x03, 0x04].includes(funcCode) && bytes.length >= 12) {
        const addr = (bytes[8] << 8) | bytes[9];
        const count = (bytes[10] << 8) | bytes[11];
        details = `Start Addr: ${addr}, Count: ${count}`;
      } else if ([0x05, 0x06].includes(funcCode) && bytes.length >= 12) {
        const addr = (bytes[8] << 8) | bytes[9];
        const val = (bytes[10] << 8) | bytes[11];
        details = `Addr: ${addr}, Value: ${val}`;
      }
    } else {
      if ([0x01, 0x02, 0x03, 0x04].includes(funcCode)) {
        const byteCount = bytes[8];
        details = `Byte Count: ${byteCount}, Data: ${cleanHex.substring(18)}`;
      }
    }

    return { txId, unitId, type, details };
  };

  // ========================================================================
  // Mock 데이터 생성
  // ========================================================================
  const generateMockLogs = (): DeviceLogEntry[] => {
    const levels = ['DEBUG', 'INFO', 'WARN', 'ERROR'] as const;
    const categories = ['COMMUNICATION', 'DATA_COLLECTION', 'CONNECTION', 'SYSTEM', 'PROTOCOL'];
    const messages = [
      'Successfully connected to device', 'Polling cycle started', 'Error parsing data frame',
      'Heartbeat received', 'Diagnostic mode enabled', 'TCP connection timeout after 30 seconds',
      'Modbus register 40001 read successfully',
    ];

    return Array.from({ length: 50 }, (_, i) => {
      const timestamp = new Date(Date.now() - (i * 60000));
      const level = levels[Math.floor(Math.random() * levels.length)];
      return {
        id: Date.now() - i,
        device_id: deviceId,
        timestamp: timestamp.toISOString(),
        level,
        category: categories[Math.floor(Math.random() * categories.length)],
        message: messages[Math.floor(Math.random() * messages.length)],
        details: level === 'ERROR' ? { error: 'Timeout', code: 504 } : undefined
      };
    });
  };

  const generateMockPacketLogs = (): PacketLogEntry[] => {
    return Array.from({ length: 50 }, (_, i) => {
      const isTx = i % 2 === 0;
      const timestamp = new Date(Date.now() - (i * 1000));
      const dataHex = isTx
        ? '00 01 00 00 00 06 01 03 00 00 00 0A'
        : '00 01 00 00 00 17 01 03 14 00 0A 00 0B 00 0C 00 00 00 00 00 00 00 00 00 00 00 00';

      return {
        id: i,
        device_id: deviceId,
        timestamp: timestamp.toISOString(),
        direction: isTx ? 'TX' : 'RX',
        protocol: 'MODBUS_TCP',
        length: dataHex.replace(/ /g, '').length / 2,
        data_hex: dataHex,
        data_ascii: '....................'
      };
    });
  };

  const loadLogs = async () => {
    setIsLoading(true);
    setError(null);
    try {
      await new Promise(resolve => setTimeout(resolve, 300));
      if (logType === 'system') {
        setLogs(generateMockLogs());
      } else {
        setPacketLogs(generateMockPacketLogs());
      }
    } catch (err) {
      setError('로그 로드 실패');
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => { loadLogs(); }, [deviceId, logType]);

  // ========================================================================
  // 헬퍼 함수
  // ========================================================================
  const formatTimestamp = (timestamp: string) => {
    return new Date(timestamp).toLocaleString('ko-KR', {
      year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false
    });
  };

  const filteredLogs = logs.filter(log => {
    if (filterLevel !== 'ALL' && log.level !== filterLevel) return false;
    if (filterCategory !== 'ALL' && log.category !== filterCategory) return false;
    if (searchTerm && !log.message.toLowerCase().includes(searchTerm.toLowerCase())) return false;
    return true;
  });

  const filteredPackets = packetLogs.filter(pkt => {
    if (searchTerm && !pkt.data_hex.toLowerCase().includes(searchTerm.toLowerCase())) return false;
    return true;
  });

  const toggleExpand = (id: number) => {
    setExpandedLogs(prev => prev.includes(id) ? prev.filter(x => x !== id) : [...prev, id]);
  };

  // ========================================================================
  // 렌더링
  // ========================================================================
  return (
    <div className="logs-wrapper">
      {/* 컨트롤 바 */}
      <div className="logs-controls-bar">
        <div className="left-controls">
          <div className="log-type-toggle">
            <button
              className={`toggle-btn ${logType === 'system' ? 'active' : ''}`}
              onClick={() => setLogType('system')}
            >시스템 로그</button>
            <button
              className={`toggle-btn ${logType === 'packet' ? 'active' : ''}`}
              onClick={() => setLogType('packet')}
            >통신 패킷</button>
          </div>

          <div className="divider"></div>

          <div className="search-wrapper">
            <i className="fas fa-search"></i>
            <input
              type="text"
              placeholder={logType === 'system' ? "메시지 검색..." : "Hex 데이터 검색..."}
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
            />
          </div>

          {logType === 'system' && (
            <>
              <select value={filterLevel} onChange={(e) => setFilterLevel(e.target.value)} className="compact-select">
                <option value="ALL">모든 레벨</option>
                <option value="ERROR">ERROR</option>
                <option value="WARN">WARN</option>
                <option value="INFO">INFO</option>
                <option value="DEBUG">DEBUG</option>
              </select>
              <select value={filterCategory} onChange={(e) => setFilterCategory(e.target.value)} className="compact-select">
                <option value="ALL">모든 카테고리</option>
                <option value="COMMUNICATION">통신</option>
                <option value="SYSTEM">시스템</option>
                <option value="CONNECTION">연결</option>
              </select>
            </>
          )}
        </div>

        <div className="right-controls">
          <label className="checkbox-label">
            <input type="checkbox" checked={isRealTime} onChange={(e) => setIsRealTime(e.target.checked)} />
            <span>실시간</span>
          </label>
          <button className="btn-icon" onClick={loadLogs} title="새로고침">
            <i className={`fas fa-sync ${isLoading ? 'fa-spin' : ''}`}></i>
          </button>
        </div>
      </div>

      {/* 테이블 영역 */}
      <div className="logs-table-container">
        {logType === 'system' ? (
          <table className="logs-table system">
            <thead>
              <tr>
                <th style={{ width: '200px' }}>시간</th>
                <th style={{ width: '80px' }}>레벨</th>
                <th style={{ width: '160px' }}>카테고리</th>
                <th>메시지</th>
              </tr>
            </thead>
            <tbody>
              {filteredLogs.map(log => (
                <React.Fragment key={log.id}>
                  <tr onClick={() => log.details && toggleExpand(log.id)} className={expandedLogs.includes(log.id) ? 'expanded' : ''}>
                    <td className="col-time">{formatTimestamp(log.timestamp)}</td>
                    <td><span className={`badge ${log.level}`}>{log.level}</span></td>
                    <td className="col-cat">{log.category}</td>
                    <td className="col-msg">{log.message} {log.details && <i className="fas fa-chevron-down"></i>}</td>
                  </tr>
                  {expandedLogs.includes(log.id) && log.details && (
                    <tr className="details-row"><td colSpan={4}><pre>{JSON.stringify(log.details, null, 2)}</pre></td></tr>
                  )}
                </React.Fragment>
              ))}
            </tbody>
          </table>
        ) : (
          <table className="logs-table packet">
            <thead>
              <tr>
                <th style={{ width: '200px' }}>시간</th>
                <th style={{ width: '60px' }}>방향</th>
                <th style={{ width: '100px' }}>프로토콜</th>
                <th style={{ width: '60px', textAlign: 'right' }}>길이</th>
                <th>Raw Data (Hex)</th>
              </tr>
            </thead>
            <tbody>
              {filteredPackets.map(pkt => {
                const parsed = parseModbusPacket(pkt.data_hex, pkt.direction);
                return (
                  <React.Fragment key={pkt.id}>
                    <tr onClick={() => toggleExpand(pkt.id)} className={expandedLogs.includes(pkt.id) ? 'expanded' : ''} style={{ cursor: 'pointer' }}>
                      <td className="col-time">{formatTimestamp(pkt.timestamp)}</td>
                      <td><span className={`dir-badge ${pkt.direction}`}>{pkt.direction}</span></td>
                      <td className="col-proto">{pkt.protocol}</td>
                      <td align="right" className="col-len">{pkt.length}B</td>
                      <td className="col-hex">
                        <span className="hex-text">{pkt.data_hex}</span>
                        {expandedLogs.includes(pkt.id) ?
                          <i className="fas fa-chevron-up expand-icon"></i> :
                          <i className="fas fa-chevron-down expand-icon"></i>
                        }
                      </td>
                    </tr>
                    {expandedLogs.includes(pkt.id) && parsed && (
                      <tr className="packet-details-row">
                        <td colSpan={5}>
                          <div className="packet-analysis">
                            <div className="analysis-item"><span className="label">Transaction ID:</span> {parsed.txId}</div>
                            <div className="analysis-item"><span className="label">Unit ID:</span> {parsed.unitId}</div>
                            <div className="analysis-item"><span className="label">Function:</span> {parsed.type}</div>
                            <div className="analysis-item"><span className="label">Details:</span> {parsed.details}</div>
                          </div>
                        </td>
                      </tr>
                    )}
                  </React.Fragment>
                );
              })}
            </tbody>
          </table>
        )}
        <div ref={logsEndRef}></div>
      </div>

      <style>{`
         .logs-wrapper { display: flex; flex-direction: column; height: 100%; background: white; font-family: 'Inter', sans-serif; padding: 0; }
         .logs-controls-bar { display: flex; justify-content: space-between; align-items: center; padding: 8px 16px; background: #f8fafc; border-bottom: 1px solid #e2e8f0; }
         .left-controls, .right-controls { display: flex; align-items: center; gap: 12px; }
         
         .log-type-toggle { display: flex; background: #e2e8f0; padding: 2px; border-radius: 6px; }
         .toggle-btn { border: none; background: none; padding: 4px 12px; font-size: 12px; font-weight: 500; color: #64748b; cursor: pointer; border-radius: 4px; }
         .toggle-btn.active { background: white; color: #0f172a; shadow: 0 1px 2px rgba(0,0,0,0.1); font-weight: 600; }
         
         .divider { width: 1px; height: 20px; background: #cbd5e1; margin: 0 4px; }
         
         .search-wrapper { position: relative; width: 220px; }
         .search-wrapper i { position: absolute; left: 8px; top: 50%; transform: translateY(-50%); font-size: 12px; color: #94a3b8; }
         .search-wrapper input { width: 100%; height: 30px; padding: 0 8px 0 28px; border: 1px solid #cbd5e1; border-radius: 4px; font-size: 12px; outline: none; }
         
         .compact-select { height: 30px; padding: 0 24px 0 8px; border: 1px solid #cbd5e1; border-radius: 4px; font-size: 12px; background: white; outline: none; }
         
         .btn-icon { background: none; border: none; cursor: pointer; font-size: 14px; color: #64748b; padding: 4px; }
         .btn-icon:hover { color: #3b82f6; }

         .logs-table-container { flex: 1; overflow: auto; }
         .logs-table { width: 100%; border-collapse: collapse; font-size: 12px; table-layout: fixed; }
         .logs-table th { position: sticky; top: 0; background: #f1f5f9; padding: 8px 12px; text-align: left; font-weight: 600; color: #475569; border-bottom: 1px solid #e2e8f0; }
         .logs-table td { padding: 6px 12px; border-bottom: 1px solid #f1f5f9; color: #334155; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; vertical-align: middle; }
         .logs-table tr:hover { background: #f8fafc; }
         
         .col-time { font-family: 'JetBrains Mono', monospace; color: #64748b; font-size: 11px; }
         .col-cat { color: #475569; }
         .col-msg { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
         
         .badge { padding: 2px 6px; border-radius: 4px; font-weight: 600; font-size: 10px; border: 1px solid transparent; }
         .badge.DEBUG { background: #f1f5f9; color: #64748b; border-color: #e2e8f0; }
         .badge.INFO { background: #f0f9ff; color: #0284c7; border-color: #bae6fd; }
         .badge.WARN { background: #fffbeb; color: #d97706; border-color: #fde68a; }
         .badge.ERROR { background: #fef2f2; color: #dc2626; border-color: #fecaca; }

         /* Packet Log Styles */
         .dir-badge { font-weight: 700; font-size: 10px; padding: 2px 6px; border-radius: 4px; }
         .dir-badge.TX { background: #ecfdf5; color: #059669; border: 1px solid #a7f3d0; }
         .dir-badge.RX { background: #eff6ff; color: #2563eb; border: 1px solid #bfdbfe; }
         
         .col-proto { font-weight: 500; color: #475569; }
         .col-len { font-family: monospace; color: #64748b; }
         .col-hex { font-family: 'JetBrains Mono', monospace; color: #334155; font-size: 11px; display: flex; align-items: center; justify-content: space-between; }
         .hex-text { background: #f8fafc; padding: 2px 6px; border-radius: 3px; border: 1px solid #e2e8f0; display: inline-block; overflow: hidden; text-overflow: ellipsis; max-width: 90%; }
         
         .expand-icon { font-size: 10px; color: #cbd5e1; margin-left: 8px; }

         .details-row td { background: #f8fafc; padding: 0; }
         .details-row pre { margin: 0; padding: 12px; font-family: monospace; font-size: 11px; color: #475569; white-space: pre-wrap; }
         
         .packet-details-row td { background: #f1f5f9; padding: 0; }
         .packet-analysis { padding: 12px 16px; display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; }
         .analysis-item { font-size: 11px; color: #334155; }
         .analysis-item .label { font-weight: 600; color: #64748b; margin-right: 4px; }
         
         .expanded { background: #f0f9ff !important; }
      `}</style>
    </div>
  );
};

export default DeviceLogsTab;