// ============================================================================
// frontend/src/pages/ActiveAlarms.tsx
// 활성 알람 모니터링 페이지 - 공통 UI 시스템 적용 버전
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { AlarmApiService } from '../api/services/alarmApi';
import {
  alarmWebSocketService,
  WebSocketAlarmEvent,
  AlarmAcknowledgment,
  ConnectionStatus
} from '../services/AlarmWebSocketService';
import { Pagination } from '../components/common/Pagination';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { useAlarmContext } from '../contexts/AlarmContext';
import '../styles/management.css';
import '../styles/active-alarms.css';

interface ActiveAlarm {
  id: number;
  rule_name: string;
  device_name?: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  message: string;
  triggered_at: string;
  state: 'active' | 'acknowledged' | 'cleared';
  is_new?: boolean;
}

const ActiveAlarms: React.FC = () => {
  const { updateAlarmCount, decrementAlarmCount } = useAlarmContext();
  const [alarms, setAlarms] = useState<ActiveAlarm[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [severityFilter, setSeverityFilter] = useState('all');
  const [stateFilter, setStateFilter] = useState('active');
  const [searchTerm, setSearchTerm] = useState('');
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(25);
  const [totalCount, setTotalCount] = useState(0);
  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>({ status: 'connecting', timestamp: new Date().toISOString() });

  const fetchActiveAlarms = useCallback(async () => {
    try {
      setLoading(true);
      const response = await AlarmApiService.getActiveAlarms({
        page: currentPage,
        limit: pageSize,
        severity: severityFilter !== 'all' ? severityFilter : undefined,
        state: stateFilter !== 'all' ? stateFilter : undefined,
        search: searchTerm || undefined
      });

      if (response.success && response.data) {
        const data = response.data as any;
        const items = data.items || data.rows || (Array.isArray(data) ? data : []);
        setAlarms(items.map((a: any) => ({
          id: a.id,
          rule_name: a.rule_name || `Rule ${a.rule_id}`,
          device_name: a.device_name || `Device ${a.device_id}`,
          severity: a.severity?.toLowerCase() || 'low',
          message: a.alarm_message || a.message,
          triggered_at: a.occurrence_time || a.triggered_at,
          state: a.state || 'active'
        })));
        setTotalCount(data.pagination?.total || data.rowCount || items.length);
      }
    } catch (err) { setError('알람 조회 실패'); }
    finally { setLoading(false); }
  }, [currentPage, pageSize, severityFilter, stateFilter, searchTerm]);

  useEffect(() => { fetchActiveAlarms(); }, [fetchActiveAlarms]);

  const handleAcknowledge = async (id: number) => {
    try {
      const response = await AlarmApiService.acknowledgeAlarm(id, { comment: '확인됨' });
      if (response.success) await fetchActiveAlarms();
    } catch (err) { alert('확인 처리 실패'); }
  };

  const handleClear = async (id: number) => {
    try {
      const response = await AlarmApiService.clearAlarm(id, { comment: '해제됨', clearedValue: null });
      if (response.success) await fetchActiveAlarms();
    } catch (err) { alert('해제 처리 실패'); }
  };

  return (
    <ManagementLayout>
      <PageHeader
        title="활성 알람 모니터링"
        description="현재 발생 중인 알람을 실시간으로 확인하고 조치합니다."
        icon="fas fa-exclamation-triangle"
        actions={
          <div className="connection-status">
            <span className={connectionStatus.status === 'connected' ? 'status-pill active' : 'status-pill inactive'}>
              {connectionStatus.status === 'connected' ? '실시간 연결됨' : '연결 끊김'}
            </span>
          </div>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard title="전체 활성" value={alarms.filter(a => a.state === 'active').length} icon="fas fa-bell" type="error" />
        <StatCard title="Critical" value={alarms.filter(a => a.severity === 'critical' && a.state === 'active').length} icon="fas fa-skull-crossbones" type="error" />
        <StatCard title="High" value={alarms.filter(a => a.severity === 'high' && a.state === 'active').length} icon="fas fa-exclamation-circle" type="warning" />
        <StatCard title="확인 대기" value={alarms.filter(a => a.state === 'active').length} icon="fas fa-clock" type="primary" />
      </div>

      <FilterBar
        searchTerm={searchTerm}
        onSearchChange={setSearchTerm}
        onReset={() => { setSearchTerm(''); setSeverityFilter('all'); setStateFilter('active'); }}
        activeFilterCount={(searchTerm ? 1 : 0) + (severityFilter !== 'all' ? 1 : 0) + (stateFilter !== 'all' ? 1 : 0)}
        filters={[
          {
            label: '심각도',
            value: severityFilter,
            onChange: setSeverityFilter,
            options: [
              { value: 'all', label: '전체' },
              { value: 'critical', label: 'Critical' },
              { value: 'high', label: 'High' },
              { value: 'medium', label: 'Medium' },
              { value: 'low', label: 'Low' }
            ]
          },
          {
            label: '상태',
            value: stateFilter,
            onChange: setStateFilter,
            options: [
              { value: 'all', label: '전체' },
              { value: 'active', label: '미확인/활성' },
              { value: 'acknowledged', label: '확인됨' }
            ]
          }
        ]}
      />

      <div className="mgmt-card table-container">
        <table className="mgmt-table">
          <thead>
            <tr>
              <th>발생 시간</th>
              <th>심각도</th>
              <th>디바이스 / 규칙</th>
              <th>메시지</th>
              <th>상태</th>
              <th>액션</th>
            </tr>
          </thead>
          <tbody>
            {alarms.map(alarm => (
              <tr key={alarm.id}>
                <td>{new Date(alarm.triggered_at).toLocaleString()}</td>
                <td><span className={`status-pill ${alarm.severity === 'critical' || alarm.severity === 'high' ? 'error' : 'warning'}`}>{alarm.severity.toUpperCase()}</span></td>
                <td>
                  <div><strong>{alarm.device_name}</strong></div>
                  <div style={{ fontSize: '12px', color: '#64748b' }}>{alarm.rule_name}</div>
                </td>
                <td>{alarm.message}</td>
                <td>{alarm.state === 'active' ? '활성' : '확인됨'}</td>
                <td>
                  <div className="table-actions">
                    {alarm.state === 'active' && (
                      <button onClick={() => handleAcknowledge(alarm.id)} className="btn-status active">확인</button>
                    )}
                    <button onClick={() => handleClear(alarm.id)} className="btn-outline">해제</button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        <Pagination current={currentPage} total={totalCount} pageSize={pageSize} onChange={setCurrentPage} />
      </div>
    </ManagementLayout>
  );
};

export default ActiveAlarms;