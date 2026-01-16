// ============================================================================
// frontend/src/pages/ActiveAlarms.tsx
// 활성 알람 모니터링 페이지 - 공통 UI 시스템 적용 버전
// ============================================================================

import React, { useState, useEffect, useCallback, useRef, useMemo } from 'react';
import { AlarmApiService } from '../api/services/alarmApi';
import { ConnectionStatus } from '../services/AlarmWebSocketService';
import { Pagination } from '../components/common/Pagination';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { useAlarmContext } from '../contexts/AlarmContext';
import { useAlarmPagination } from '../hooks/usePagination';
import '../styles/management.css';
import '../styles/pagination.css';

interface ActiveAlarm {
  id: number;
  rule_name: string;
  device_name?: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  message: string;
  triggered_at: string;
  state: 'active' | 'acknowledged' | 'cleared';
  is_new?: boolean;
  comment?: string;
  acknowledged_time?: string;
  acknowledged_by?: string | number;
  acknowledged_by_name?: string;
  acknowledged_by_company?: string;
  cleared_time?: string;
  cleared_by?: string | number;
  cleared_by_name?: string;
  cleared_by_company?: string;
  trigger_value?: any;
  rule_id?: number;
}

const ActiveAlarms: React.FC = () => {
  const { confirm } = useConfirmContext();
  const { decrementAlarmCount } = useAlarmContext();
  const [alarms, setAlarms] = useState<ActiveAlarm[]>([]);
  const [statistics, setStatistics] = useState<any>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [severityFilter, setSeverityFilter] = useState('all');
  const [stateFilter, setStateFilter] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedAlarm, setSelectedAlarm] = useState<ActiveAlarm | null>(null);
  const [showDetailsModal, setShowDetailsModal] = useState(false);

  const pagination = useAlarmPagination();

  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>({ status: 'connecting', timestamp: new Date().toISOString() });

  const fetchStatistics = useCallback(async () => {
    try {
      const response = await AlarmApiService.getAlarmStatistics();
      if (response.success) {
        setStatistics(response.data);
      }
    } catch (err) {
      console.error('Failed to fetch stats:', err);
    }
  }, []);

  const fetchActiveAlarms = useCallback(async () => {
    try {
      setLoading(true);
      const response = await AlarmApiService.getActiveAlarms({
        page: pagination.currentPage,
        limit: pagination.pageSize,
        severity: severityFilter !== 'all' ? severityFilter : undefined,
        state: stateFilter === 'all' ? undefined : stateFilter, // Backend now handles default states
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
          state: (a.state || 'active').toLowerCase(),
          comment: a.acknowledge_comment || a.clear_comment || '',
          acknowledged_time: a.acknowledged_time,
          acknowledged_by: a.acknowledged_by,
          acknowledged_by_name: a.acknowledged_by_name,
          acknowledged_by_company: a.acknowledged_by_company,
          cleared_time: a.cleared_time,
          cleared_by: a.cleared_by,
          cleared_by_name: a.cleared_by_name,
          cleared_by_company: a.cleared_by_company,
          trigger_value: a.trigger_value,
          rule_id: a.rule_id
        })));

        const totalFromServer = data.pagination?.total || data.rowCount || items.length;
        pagination.updateTotalCount(totalFromServer);
      }
    } catch (err) {
      console.error('Active alarm fetch error:', err);
      setError('알람 정보를 불러오는 중 오류가 발생했습니다.');
    }
    finally { setLoading(false); }
  }, [pagination.currentPage, pagination.pageSize, severityFilter, stateFilter, searchTerm]);

  useEffect(() => {
    fetchActiveAlarms();
    fetchStatistics();
  }, [fetchActiveAlarms, fetchStatistics]);

  // 계산된 통계 (현재 필터링된 결과와 서버 통계 병합)
  const computedStats = useMemo(() => {
    const s = statistics?.occurrences;
    const localActive = alarms.filter(a => a.state === 'active').length;
    const localAck = alarms.filter(a => a.state === 'acknowledged').length;

    return {
      totalActive: (s?.active_alarms || (localActive + localAck)),
      critical: s?.by_severity?.critical || alarms.filter(a => a.severity === 'critical' && a.state === 'active').length,
      high: s?.by_severity?.high || alarms.filter(a => a.severity === 'high' && a.state === 'active').length,
      unacknowledged: s?.unacknowledged_alarms || localActive,
      acknowledged: s?.acknowledged_alarms || localAck
    };
  }, [statistics, alarms]);

  const handleAcknowledge = async (id: number) => {
    const comment = window.prompt('알람 확인 사유를 입력해주세요 (선택사항):', '시스템 확인');
    if (comment === null) return;

    try {
      const response = await AlarmApiService.acknowledgeAlarm(id, { comment: comment || '시스템 확인' });
      if (response.success) {
        decrementAlarmCount();
        await Promise.all([fetchActiveAlarms(), fetchStatistics()]);
      }
    } catch (err) { alert('확인 처리 실패'); }
  };

  const handleClear = async (id: number) => {
    const comment = window.prompt('알람 해제 사유를 입력해주세요 (선택사항):', '조치 완료');
    if (comment === null) return;

    const ok = await confirm({
      title: '알람 해제',
      message: '해당 알람을 해제 처리하시겠습니까?\n해제된 알람은 이력 페이지에서 확인 가능합니다.',
      confirmButtonType: 'danger'
    });
    if (!ok) return;

    try {
      const response = await AlarmApiService.clearAlarm(id, { comment: comment || '조치 완료' });
      if (response.success) {
        decrementAlarmCount();
        await Promise.all([fetchActiveAlarms(), fetchStatistics()]);
      }
    } catch (err) { alert('해제 처리 실패'); }
  };

  const handleViewDetails = (alarm: ActiveAlarm) => {
    setSelectedAlarm(alarm);
    setShowDetailsModal(true);
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

      <div className="mgmt-stats-panel" style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '16px', alignItems: 'stretch', marginBottom: '16px' }}>
        <StatCard title="전체 활성" value={computedStats.totalActive} icon="fas fa-bell" type="error" />
        <StatCard title="확인 대기" value={computedStats.unacknowledged} icon="fas fa-clock" type="error" />
        <StatCard title="확인됨" value={computedStats.acknowledged} icon="fas fa-check-circle" type="warning" />
        <StatCard title="Critical (미확인)" value={computedStats.critical} icon="fas fa-skull-crossbones" type="error" />
      </div>

      {error && (
        <div style={{ margin: '16px 0', padding: '12px 16px', background: '#fef2f2', border: '1px solid #fecaca', borderRadius: '8px', color: '#dc2626', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <i className="fas fa-exclamation-circle"></i>
          <span>{error}</span>
          <button onClick={() => { setError(null); fetchActiveAlarms(); fetchStatistics(); }} style={{ marginLeft: 'auto', background: 'none', border: 'none', color: '#dc2626', textDecoration: 'underline', cursor: 'pointer', fontSize: '12px' }}>다시 시도</button>
        </div>
      )}

      <FilterBar
        searchTerm={searchTerm}
        onSearchChange={setSearchTerm}
        onReset={() => { setSearchTerm(''); setSeverityFilter('all'); setStateFilter('all'); }}
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
              { value: 'active', label: '확인 대기' },
              { value: 'acknowledged', label: '확인됨' }
            ]
          }
        ]}
      />

      <div className="mgmt-card" style={{ padding: 0, overflow: 'hidden', display: 'flex', flexDirection: 'column', minHeight: '400px' }}>
        <div style={{ overflowX: 'auto', flex: '1 1 auto' }}>
          <table className="mgmt-table">
            <thead>
              <tr>
                <th>발생 시간</th>
                <th>심각도</th>
                <th>디바이스 / 규칙</th>
                <th>메시지</th>
                <th>메모</th>
                <th>상태</th>
                <th style={{ textAlign: 'center' }}>상세</th>
                <th>액션</th>
              </tr>
            </thead>
            <tbody>
              {alarms.length > 0 ? (
                alarms.map(alarm => (
                  <tr key={alarm.id}>
                    <td>{new Date(alarm.triggered_at).toLocaleString()}</td>
                    <td><span className={`status-pill ${alarm.severity === 'critical' || alarm.severity === 'high' ? 'error' : 'warning'}`}>{alarm.severity.toUpperCase()}</span></td>
                    <td>
                      <div><strong>{alarm.device_name}</strong></div>
                      <div style={{ fontSize: '12px', color: '#64748b' }}>{alarm.rule_name}</div>
                    </td>
                    <td>{alarm.message}</td>
                    <td>
                      <div style={{ fontSize: '12px', color: '#64748b', maxWidth: '150px', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }} title={alarm.comment}>
                        {alarm.comment || '-'}
                      </div>
                    </td>
                    <td>{alarm.state === 'active' ? '확인 대기' : '확인됨'}</td>
                    <td style={{ textAlign: 'center' }}>
                      <button
                        onClick={() => handleViewDetails(alarm)}
                        className="btn-icon"
                        title="상세 정보 보기"
                        style={{ background: 'none', border: 'none', color: '#64748b', cursor: 'pointer', padding: '4px' }}
                      >
                        <i className="far fa-eye"></i>
                      </button>
                    </td>
                    <td>
                      <div className="table-actions">
                        {alarm.state === 'active' ? (
                          <button
                            onClick={() => handleAcknowledge(alarm.id)}
                            className="btn-primary btn-action"
                            title="알람 확인 (확인 후 해제 가능)"
                          >
                            확인
                          </button>
                        ) : (
                          <button
                            onClick={() => handleClear(alarm.id)}
                            className="btn-outline btn-action"
                            title="알람 해제"
                          >
                            해제
                          </button>
                        )}
                      </div>
                    </td>
                  </tr>
                ))
              ) : (
                <tr>
                  <td colSpan={8} style={{ textAlign: 'center', padding: '48px', color: '#64748b' }}>
                    {loading ? '데이터를 불러오는 중...' : '발생한 알람이 없습니다.'}
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>

        {/* 페이지네이션 - 명확히 분리된 하단 영역 */}
        <div style={{ padding: '16px 24px', borderTop: '1px solid #e2e8f0', background: '#f8fafc', flexShrink: 0, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ fontSize: '13px', color: '#64748b' }}>
            전체 <strong>{pagination.totalCount}</strong>개 중 {alarms.length}개 표시
          </div>
          <Pagination
            current={pagination.currentPage}
            total={pagination.totalCount}
            pageSize={pagination.pageSize}
            onChange={(page, size) => {
              pagination.goToPage(page);
              if (size && size !== pagination.pageSize) pagination.changePageSize(size);
            }}
            showSizeChanger={true}
          />
        </div>
      </div>

      {/* 상세 정보 모달 */}
      {showDetailsModal && selectedAlarm && (
        <div className="modal-overlay" style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.5)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000 }}>
          <div className="modal-content" style={{ background: 'white', borderRadius: '12px', width: '90%', maxWidth: '500px', maxHeight: '90vh', overflowY: 'auto', boxShadow: '0 20px 25px -5px rgba(0,0,0,0.1)' }}>
            <div className="modal-header" style={{ padding: '16px 20px', borderBottom: '1px solid #e2e8f0', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <h3 style={{ margin: 0, fontSize: '18px', fontWeight: 600 }}>알람 상세 정보</h3>
              <button onClick={() => setShowDetailsModal(false)} style={{ background: 'none', border: 'none', fontSize: '20px', cursor: 'pointer', color: '#64748b' }}>&times;</button>
            </div>

            <div className="modal-body" style={{ padding: '20px' }}>
              <div style={{ marginBottom: '20px' }}>
                <h4 style={{ margin: '0 0 12px 0', fontSize: '14px', fontWeight: 600, color: '#3b82f6', borderLeft: '3px solid #3b82f6', paddingLeft: '8px' }}>기본 정보</h4>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                  <div>
                    <label style={{ display: 'block', fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>알람 ID</label>
                    <div style={{ fontWeight: 600 }}>#{selectedAlarm.id}</div>
                  </div>
                  <div>
                    <label style={{ display: 'block', fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>심각도</label>
                    <span className={`status-pill ${selectedAlarm.severity === 'critical' || selectedAlarm.severity === 'high' ? 'error' : 'warning'}`} style={{ display: 'inline-block' }}>
                      {selectedAlarm.severity.toUpperCase()}
                    </span>
                  </div>
                  <div style={{ gridColumn: '1 / -1' }}>
                    <label style={{ display: 'block', fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>디바이스 / 규칙</label>
                    <div style={{ fontWeight: 500 }}>
                      <strong style={{ color: '#1e293b' }}>{selectedAlarm.device_name}</strong>
                      <span style={{ margin: '0 8px', color: '#cbd5e1' }}>/</span>
                      <span style={{ color: '#475569' }}>{selectedAlarm.rule_name}</span>
                    </div>
                  </div>
                </div>
              </div>

              <div>
                <h4 style={{ margin: '0 0 12px 0', fontSize: '14px', fontWeight: 600, color: '#3b82f6', borderLeft: '3px solid #3b82f6', paddingLeft: '8px' }}>발생 및 조치 이력</h4>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                  <div style={{ display: 'flex', gap: '12px' }}>
                    <div style={{ minWidth: '60px', fontSize: '13px', color: '#64748b', paddingTop: '2px' }}>발생</div>
                    <div style={{ flex: 1 }}>
                      <div style={{ fontSize: '14px', fontWeight: 500, marginBottom: '4px' }}>{new Date(selectedAlarm.triggered_at).toLocaleString()}</div>
                      <div style={{ fontSize: '13px', color: '#475569', background: '#f8fafc', padding: '8px 12px', borderRadius: '6px', border: '1px solid #e2e8f0' }}>
                        {selectedAlarm.message}
                      </div>
                    </div>
                  </div>

                  {selectedAlarm.acknowledged_time && (
                    <div style={{ display: 'flex', gap: '12px' }}>
                      <div style={{ minWidth: '60px', fontSize: '13px', color: '#64748b', paddingTop: '2px' }}>확인</div>
                      <div style={{ flex: 1 }}>
                        <div style={{ fontSize: '14px', fontWeight: 500, marginBottom: '4px' }}>
                          {new Date(selectedAlarm.acknowledged_time).toLocaleString()}
                        </div>
                        <div style={{ fontSize: '13px', color: '#64748b', marginBottom: '4px' }}>
                          확인자: {selectedAlarm.acknowledged_by_company ? `${selectedAlarm.acknowledged_by_company} / ` : ''}{selectedAlarm.acknowledged_by_name || selectedAlarm.acknowledged_by || '-'}
                        </div>
                        {selectedAlarm.comment && selectedAlarm.state !== 'cleared' && (
                          <div style={{ fontSize: '13px', color: '#475569', background: '#fffbeb', padding: '8px 12px', borderRadius: '6px', border: '1px solid #fcd34d', whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                            확인 메모: {selectedAlarm.comment}
                          </div>
                        )}
                      </div>
                    </div>
                  )}

                  {selectedAlarm.cleared_time && (
                    <div style={{ display: 'flex', gap: '12px' }}>
                      <div style={{ minWidth: '60px', fontSize: '13px', color: '#64748b', paddingTop: '2px' }}>해제</div>
                      <div style={{ flex: 1 }}>
                        <div style={{ fontSize: '14px', fontWeight: 500, marginBottom: '4px' }}>
                          {new Date(selectedAlarm.cleared_time).toLocaleString()}
                        </div>
                        <div style={{ fontSize: '13px', color: '#64748b', marginBottom: '4px' }}>
                          해제자: {selectedAlarm.cleared_by_company ? `${selectedAlarm.cleared_by_company} / ` : ''}{selectedAlarm.cleared_by_name || selectedAlarm.cleared_by || '-'}
                        </div>
                        {selectedAlarm.comment && selectedAlarm.state === 'cleared' && (
                          <div style={{ fontSize: '13px', color: '#475569', background: '#f0fdf4', padding: '8px 12px', borderRadius: '6px', border: '1px solid #86efac', whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                            해제 메모: {selectedAlarm.comment}
                          </div>
                        )}
                      </div>
                    </div>
                  )}
                </div>
              </div>
            </div>

            <div className="modal-footer" style={{ padding: '16px 20px', borderTop: '1px solid #e2e8f0', display: 'flex', justifyContent: 'flex-end', background: '#f8fafc', borderRadius: '0 0 12px 12px' }}>
              <button
                onClick={() => setShowDetailsModal(false)}
                style={{ padding: '8px 16px', background: 'white', border: '1px solid #cbd5e1', borderRadius: '6px', cursor: 'pointer', fontSize: '13px', fontWeight: 500, color: '#475569' }}
              >
                닫기
              </button>
            </div>
          </div>
        </div>
      )}
    </ManagementLayout>
  );
};

export default ActiveAlarms;