// ============================================================================
// frontend/src/pages/ActiveAlarms.tsx
// 활성 알람 모니터링 페이지 - 공통 UI 시스템 적용 버전
// ============================================================================

import React, { useState, useEffect, useCallback, useRef, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { AlarmApiService } from '../api/services/alarmApi';
import { ConnectionStatus, alarmWebSocketService } from '../services/AlarmWebSocketService';
import { Pagination } from '../components/common/Pagination';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { useAlarmContext } from '../contexts/AlarmContext';
import { useAlarmPagination } from '../hooks/usePagination';
import '../styles/active-alarms.css';
import '../styles/management.css';
import '../styles/pagination.css';
import AlarmActionModal from '../components/modals/AlarmActionModal';
import ActiveAlarmDetailModal from '../components/modals/ActiveAlarmDetailModal';

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
  const { t } = useTranslation(['alarms', 'common']);
  const { decrementAlarmCount, refreshAlarmCount } = useAlarmContext();
  const [alarms, setAlarms] = useState<ActiveAlarm[]>([]);
  const [statistics, setStatistics] = useState<any>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [severityFilter, setSeverityFilter] = useState('all');
  const [stateFilter, setStateFilter] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedAlarm, setSelectedAlarm] = useState<ActiveAlarm | null>(null);
  const [showDetailsModal, setShowDetailsModal] = useState(false);
  const [showActionModal, setShowActionModal] = useState(false);
  const [actionType, setActionType] = useState<'acknowledge' | 'clear'>('acknowledge');
  const [targetAlarmId, setTargetAlarmId] = useState<number | null>(null);
  const [isBulkAction, setIsBulkAction] = useState(false);
  const [actionLoading, setActionLoading] = useState(false);

  const pagination = useAlarmPagination();

  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>(alarmWebSocketService.getConnectionStatus());

  // Auto-refresh
  const AUTO_REFRESH_INTERVAL = 10;
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [countdown, setCountdown] = useState(AUTO_REFRESH_INTERVAL);
  const countdownRef = useRef<ReturnType<typeof setInterval> | null>(null);

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
      setError('Error loading alarm information.');
    }
    finally { setLoading(false); }
  }, [pagination.currentPage, pagination.pageSize, severityFilter, stateFilter, searchTerm]);

  useEffect(() => {
    fetchActiveAlarms();
    fetchStatistics();
  }, [fetchActiveAlarms, fetchStatistics]);

  // WebSocket 연결 및 이벤트 핸들링
  useEffect(() => {
    // 1. 초기 연결
    alarmWebSocketService.connect().catch(err => {
      console.error('WebSocket connection failed:', err);
    });

    // 2. 연결 상태 변화 구독
    const unsubscribeConnection = alarmWebSocketService.onConnectionChange((status) => {
      setConnectionStatus(status);
    });

    // 3. 새 알람 이벤트 구독 -> 자동 Refresh
    const unsubscribeAlarm = alarmWebSocketService.onAlarmEvent((event) => {
      console.log('Real-time alarm event received:', event);
      // 알람 목록과 통계 Refresh
      fetchActiveAlarms();
      fetchStatistics();
    });

    // 4. 클린업
    return () => {
      unsubscribeConnection();
      unsubscribeAlarm();
    };
  }, [fetchActiveAlarms, fetchStatistics]);

  // Auto-refresh countdown
  useEffect(() => {
    if (countdownRef.current) clearInterval(countdownRef.current);
    if (!autoRefresh) { setCountdown(AUTO_REFRESH_INTERVAL); return; }

    setCountdown(AUTO_REFRESH_INTERVAL);
    countdownRef.current = setInterval(() => {
      setCountdown(prev => {
        if (prev <= 1) {
          fetchActiveAlarms();
          fetchStatistics();
          return AUTO_REFRESH_INTERVAL;
        }
        return prev - 1;
      });
    }, 1000);

    return () => { if (countdownRef.current) clearInterval(countdownRef.current); };
  }, [autoRefresh, fetchActiveAlarms, fetchStatistics]);

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

  const handleAcknowledge = (id: number) => {
    const alarm = alarms.find(a => a.id === id);
    if (!alarm) return;

    setTargetAlarmId(id);
    setActionType('acknowledge');
    setSelectedAlarm(alarm);
    setShowActionModal(true);
  };

  const handleClear = (id: number) => {
    const alarm = alarms.find(a => a.id === id);
    if (!alarm) return;

    setTargetAlarmId(id);
    setIsBulkAction(false);
    setActionType('clear');
    setSelectedAlarm(alarm);
    setShowActionModal(true);
  };

  const handleBulkAcknowledge = () => {
    setIsBulkAction(true);
    setActionType('acknowledge');
    setShowActionModal(true);
  };

  const handleBulkClear = () => {
    setIsBulkAction(true);
    setActionType('clear');
    setShowActionModal(true);
  };

  const onConfirmAction = async (comment: string) => {
    if (!isBulkAction && !targetAlarmId) return;

    setActionLoading(true);
    try {
      let response;
      if (isBulkAction) {
        if (actionType === 'acknowledge') {
          response = await AlarmApiService.acknowledgeAllAlarms({ comment });
        } else {
          response = await AlarmApiService.clearAllAlarms({ comment });
        }
      } else if (targetAlarmId) {
        if (actionType === 'acknowledge') {
          response = await AlarmApiService.acknowledgeAlarm(targetAlarmId, { comment });
        } else {
          response = await AlarmApiService.clearAlarm(targetAlarmId, { comment });
        }
      }

      if (response && response.success) {
        // 페이지 로컈 데이터 갱신
        await Promise.all([fetchActiveAlarms(), fetchStatistics()]);
        // 전역 뱃지 재조회 (AlarmContext 미확인 개수 동기화)
        await refreshAlarmCount();
        setShowActionModal(false);
      } else {
        alert(`${actionType === 'acknowledge' ? 'Acknowledge' : 'Clear'} failed: ${response?.message || 'Unknown error'}`);
      }
    } catch (err) {
      alert(`Error processing ${actionType === 'acknowledge' ? 'acknowledge' : 'clear'} action.`);
    } finally {
      setActionLoading(false);
    }
  };

  const handleViewDetails = (alarm: ActiveAlarm) => {
    setSelectedAlarm(alarm);
    setShowDetailsModal(true);
  };

  return (
    <ManagementLayout className="page-active-alarms">
      <PageHeader
        title="Active Alarm Monitoring"
        description="Monitor and handle currently active alarms in real time."
        icon="fas fa-exclamation-triangle"
        actions={
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            {/* Auto-refresh pill */}
            <div style={{
              display: 'flex', alignItems: 'center',
              background: autoRefresh ? 'var(--primary-50, #eff6ff)' : 'var(--neutral-100)',
              border: `1px solid ${autoRefresh ? 'var(--primary-200, #bfdbfe)' : 'var(--neutral-200)'}`,
              borderRadius: '20px', padding: '4px 6px 4px 10px', gap: '6px',
              transition: 'all 0.2s ease',
            }}>
              <span style={{ fontSize: '12px', fontVariantNumeric: 'tabular-nums', color: autoRefresh ? 'var(--primary-600, #2563eb)' : 'var(--neutral-400)', minWidth: '36px', fontWeight: 500 }}>
                {autoRefresh
                  ? <><i className="fas fa-circle-notch fa-spin" style={{ marginRight: '4px', fontSize: '10px' }}></i>{countdown}s</>
                  : <><i className="fas fa-pause" style={{ marginRight: '4px', fontSize: '10px', color: 'var(--neutral-400)' }}></i>{t('labels.pause', { ns: 'alarms' })}</>}
              </span>
              <button
                style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', width: '22px', height: '22px', borderRadius: '50%', border: 'none', cursor: 'pointer', background: autoRefresh ? 'var(--primary-100, #dbeafe)' : 'var(--neutral-200)', color: autoRefresh ? 'var(--primary-600, #2563eb)' : 'var(--neutral-500)', fontSize: '9px', flexShrink: 0, transition: 'all 0.15s ease' }}
                onClick={() => setAutoRefresh(v => !v)}
                title={autoRefresh ? 'Stop auto-refresh' : 'Start auto-refresh'}
              >
                <i className={`fas ${autoRefresh ? 'fa-pause' : 'fa-play'}`}></i>
              </button>
            </div>
            {/* WebSocket status */}
            <span className={`mgmt-status-pill ${connectionStatus.status === 'connected' ? 'active' : connectionStatus.status === 'connecting' ? 'warning' : 'error'}`}>
              <i className={`fas ${connectionStatus.status === 'connected' ? 'fa-check-circle' : connectionStatus.status === 'connecting' ? 'fa-spinner fa-spin' : 'fa-exclamation-circle'}`} style={{ marginRight: '6px' }}></i>
              {connectionStatus.status === 'connected' ? 'Live Connected' : connectionStatus.status === 'connecting' ? 'Connecting...' : 'Disconnected'}
            </span>
          </div>
        }
      />

      <div className="mgmt-stats-panel" style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '16px', alignItems: 'stretch', marginBottom: '8px' }}>
        <StatCard title="Total Active" value={computedStats.totalActive} icon="fas fa-bell" type="error" />
        <StatCard title="Pending" value={computedStats.unacknowledged} icon="fas fa-clock" type="error" />
        <StatCard title="Acknowledged" value={computedStats.acknowledged} icon="fas fa-check-circle" type="warning" />
        <StatCard title="Critical (Unconfirmed)" value={computedStats.critical} icon="fas fa-skull-crossbones" type="error" />
      </div>

      {error && (
        <div style={{ margin: '16px 0', padding: '12px 16px', background: '#fef2f2', border: '1px solid #fecaca', borderRadius: '8px', color: '#dc2626', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <i className="fas fa-exclamation-circle"></i>
          <span>{error}</span>
          <button onClick={() => { setError(null); fetchActiveAlarms(); fetchStatistics(); }} style={{ marginLeft: 'auto', background: 'none', border: 'none', color: '#dc2626', textDecoration: 'underline', cursor: 'pointer', fontSize: '12px' }}>{t('labels.retry', { ns: 'alarms' })}</button>
        </div>
      )}

      <div style={{ marginBottom: '8px' }}>
        <FilterBar
          searchTerm={searchTerm}
          onSearchChange={setSearchTerm}
          onReset={() => { setSearchTerm(''); setSeverityFilter('all'); setStateFilter('all'); }}
          activeFilterCount={(searchTerm ? 1 : 0) + (severityFilter !== 'all' ? 1 : 0) + (stateFilter !== 'all' ? 1 : 0)}
          filters={[
            {
              label: 'Severity',
              value: severityFilter,
              onChange: setSeverityFilter,
              options: [
                { value: 'all', label: 'All' },
                { value: 'critical', label: 'Critical' },
                { value: 'high', label: 'High' },
                { value: 'medium', label: 'Medium' },
                { value: 'low', label: 'Low' }
              ]
            },
            {
              label: 'Status',
              value: stateFilter,
              onChange: setStateFilter,
              options: [
                { value: 'all', label: 'All' },
                { value: 'active', label: 'Pending' },
                { value: 'acknowledged', label: 'Acknowledged' }
              ]
            }
          ]}
          rightActions={
            <div style={{ display: 'flex', gap: '8px' }}>
              <button
                onClick={handleBulkAcknowledge}
                className="mgmt-btn mgmt-btn-primary"
                disabled={computedStats.unacknowledged === 0}
                title="Acknowledge all currently unacknowledged alarms."
              >
                <i className="fas fa-check-double" style={{ marginRight: '6px' }}></i>
                Bulk Acknowledge ({computedStats.unacknowledged})
              </button>
              <button
                onClick={handleBulkClear}
                className="mgmt-btn mgmt-btn-outline"
                disabled={computedStats.acknowledged === 0}
                title="Clear all currently acknowledged alarms."
              >
                <i className="fas fa-broom" style={{ marginRight: '6px' }}></i>
                Bulk Clear ({computedStats.acknowledged})
              </button>
            </div>
          }
        />
      </div>

      <div className="mgmt-card" style={{ padding: 0, overflow: 'hidden', display: 'flex', flexDirection: 'column', minHeight: '400px' }}>
        <div style={{ overflowX: 'auto', flex: '1 1 auto' }}>
          <table className="mgmt-table">
            <colgroup>
              <col style={{ width: '180px' }} />   {/* 발생 시간 */}
              <col style={{ width: '80px' }} />    {/* 심각도 */}
              <col style={{ width: '160px' }} />   {/* 디바이스 / 규칙 */}
              <col />                              {/* 메시지 - 나머지 공간 전부 */}
              <col style={{ width: '100px' }} />   {/* 메모 */}
              <col style={{ width: '110px' }} />   {/* 상태 */}
              <col style={{ width: '48px' }} />    {/* 상세 */}
              <col style={{ width: '64px' }} />    {/* 액션 */}
            </colgroup>
            <thead>
              <tr>
                <th>{t('modals.triggeredAt', { ns: 'alarms' })}</th>
                <th style={{ textAlign: 'center' }}>{t('severity.label', { ns: 'alarms' })}</th>
                <th>{t('modals.deviceRule', { ns: 'alarms' })}</th>
                <th>{t('columns.message', { ns: 'alarms' })}</th>
                <th>{t('labels.note', { ns: 'alarms' })}</th>
                <th style={{ textAlign: 'center' }}>{t('labels.status', { ns: 'alarms' })}</th>
                <th style={{ textAlign: 'center' }}>{t('labels.details', { ns: 'alarms' })}</th>
                <th style={{ textAlign: 'center' }}>{t('columns.action', { ns: 'alarms' })}</th>
              </tr>
            </thead>
            <tbody>
              {alarms.length > 0 ? (
                alarms.map(alarm => (
                  <tr key={alarm.id}>
                    <td style={{ whiteSpace: 'nowrap' }}>{new Date(alarm.triggered_at).toLocaleString()}</td>
                    <td style={{ textAlign: 'center' }}><span className={`mgmt-status-pill ${alarm.severity}`}>{alarm.severity.toUpperCase()}</span></td>
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
                    <td style={{ textAlign: 'center', whiteSpace: 'nowrap' }}>
                      <span className={`mgmt-status-pill ${alarm.state === 'active' ? 'error' : 'active'}`}>
                        <i className={`fas ${alarm.state === 'active' ? 'fa-clock' : 'fa-check-circle'}`} style={{ marginRight: '6px' }}></i>
                        {alarm.state === 'active' ? 'Pending' : 'Acknowledged'}
                      </span>
                    </td>
                    <td style={{ textAlign: 'center' }}>
                      <button
                        onClick={() => handleViewDetails(alarm)}
                        className="btn-icon"
                        title="View Details"
                        style={{ background: 'none', border: 'none', color: '#64748b', cursor: 'pointer', padding: '4px' }}
                      >
                        <i className="far fa-eye"></i>
                      </button>
                    </td>
                    <td style={{ textAlign: 'center' }}>
                      <div className="table-actions" style={{ justifyContent: 'center' }}>
                        {alarm.state === 'active' ? (
                          <button
                            onClick={() => handleAcknowledge(alarm.id)}
                            className="mgmt-btn mgmt-btn-primary btn-action"
                            title={t('acknowledge', { ns: 'alarms' })}
                          >
                            {t('ack', { ns: 'alarms' })}
                          </button>
                        ) : (
                          <button
                            onClick={() => handleClear(alarm.id)}
                            className="mgmt-btn mgmt-btn-outline btn-action"
                            title={t('clear', { ns: 'alarms' })}
                          >
                            {t('clear', { ns: 'alarms' })}
                          </button>
                        )}
                      </div>
                    </td>
                  </tr>
                ))
              ) : (
                <tr>
                  <td colSpan={8} style={{ textAlign: 'center', padding: '48px', color: '#64748b' }}>
                    {loading ? 'Loading...' : 'No active alarms.'}
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>

        {/* 페이지네이션 - 명확히 분리된 하단 영역 */}
        <div style={{ padding: '8px 24px', borderTop: '1px solid #e2e8f0', background: 'white', flexShrink: 0, display: 'flex', alignItems: 'center', justifyContent: 'flex-end' }}>
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
      {/* 상세 정보 모달 - Premium Layout */}
      {showDetailsModal && selectedAlarm && (
        <ActiveAlarmDetailModal
          alarm={selectedAlarm}
          onClose={() => setShowDetailsModal(false)}
          onAcknowledge={handleAcknowledge}
          onClear={handleClear}
        />
      )}
      {/* 알람 처리 모달 (확인/해제) */}
      <AlarmActionModal
        isOpen={showActionModal}
        type={actionType}
        alarmData={isBulkAction ? {
          rule_name: actionType === 'acknowledge' ? 'Bulk Acknowledge All Unacknowledged Alarms' : 'Bulk Clear All Acknowledged Alarms',
          alarm_message: actionType === 'acknowledge'
            ? `Bulk Acknowledge all unacknowledged alarms (${computedStats.unacknowledged}).`
            : `Bulk Clear all acknowledged alarms (${computedStats.acknowledged}).`,
          severity: 'multiple'
        } : {
          rule_name: selectedAlarm?.rule_name,
          alarm_message: selectedAlarm?.message,
          severity: selectedAlarm?.severity
        }}
        onConfirm={onConfirmAction}
        onCancel={() => setShowActionModal(false)}
        loading={actionLoading}
      />
    </ManagementLayout>
  );
};

export default ActiveAlarms;