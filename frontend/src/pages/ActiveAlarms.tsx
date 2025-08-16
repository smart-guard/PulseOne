// ============================================================================
// frontend/src/pages/ActiveAlarms.tsx
// 📝 활성 알람 페이지 - 새로운 AlarmApiService 완전 연결
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import '../styles/base.css';
import '../styles/active-alarms.css';
import '../styles/pagination.css';

// 🚨 알람 관련 인터페이스들
interface ActiveAlarm {
  id: number;
  rule_id: number;
  rule_name: string;
  device_id?: number;
  device_name?: string;
  data_point_id?: number;
  data_point_name?: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  priority: number;
  message: string;
  description?: string;
  triggered_value?: any;
  threshold_value?: any;
  condition_type?: string;
  triggered_at: string;
  acknowledged_at?: string;
  acknowledged_by?: string;
  acknowledgment_comment?: string;
  state: 'active' | 'acknowledged' | 'cleared';
  quality: string;
  tags?: string[];
  metadata?: any;
}

interface AlarmStats {
  total_active: number;
  critical_count: number;
  high_count: number;
  medium_count: number;
  low_count: number;
  unacknowledged_count: number;
  acknowledged_count: number;
  by_device: Array<{
    device_id: number;
    device_name: string;
    alarm_count: number;
  }>;
  by_severity: Array<{
    severity: string;
    count: number;
  }>;
}

const ActiveAlarms: React.FC = () => {
  // 🔧 기본 상태들
  const [alarms, setAlarms] = useState<ActiveAlarm[]>([]);
  const [alarmStats, setAlarmStats] = useState<AlarmStats | null>(null);
  const [selectedAlarms, setSelectedAlarms] = useState<number[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [isProcessing, setIsProcessing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 필터 상태
  const [severityFilter, setSeverityFilter] = useState<string>('all');
  const [deviceFilter, setDeviceFilter] = useState<string>('all');
  const [acknowledgedFilter, setAcknowledgedFilter] = useState<string>('all');
  const [searchTerm, setSearchTerm] = useState('');
  
  // 실시간 업데이트
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5000);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // 알람 확인 모달
  const [showAckModal, setShowAckModal] = useState(false);
  const [ackComment, setAckComment] = useState('');
  const [selectedAlarmForAck, setSelectedAlarmForAck] = useState<ActiveAlarm | null>(null);

  // 페이징
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 50,
    totalCount: 0
  });

  // =============================================================================
  // 🔄 데이터 로드 함수들 (새로운 알람 API 사용)
  // =============================================================================

  /**
   * 활성 알람 목록 로드
   */
  const loadActiveAlarms = useCallback(async () => {
    try {
      setIsLoading(true);
      setError(null);

      console.log('🚨 활성 알람 목록 로드 시작...');

      // 새로운 알람 API 호출
      const response = await fetch('/api/alarms/active?' + new URLSearchParams({
        page: pagination.currentPage.toString(),
        limit: pagination.pageSize.toString(),
        severity: severityFilter !== 'all' ? severityFilter : '',
        device_id: deviceFilter !== 'all' ? deviceFilter : '',
        acknowledged: acknowledgedFilter !== 'all' ? acknowledgedFilter : '',
        search: searchTerm || ''
      }).toString());

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();

      if (data.success && data.data) {
        setAlarms(data.data.items);
        pagination.updateTotalCount(data.data.pagination.total);
        
        console.log(`✅ 활성 알람 ${data.data.items.length}개 로드 완료`);
      } else {
        throw new Error(data.message || '활성 알람 로드 실패');
      }

    } catch (err) {
      console.error('❌ 활성 알람 로드 실패:', err);
      setError(err instanceof Error ? err.message : '알 수 없는 오류');
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  }, [pagination.currentPage, pagination.pageSize, severityFilter, deviceFilter, acknowledgedFilter, searchTerm]);

  /**
   * 알람 통계 로드
   */
  const loadAlarmStats = useCallback(async () => {
    try {
      console.log('📊 알람 통계 로드 시작...');

      const response = await fetch('/api/alarms/statistics');
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();

      if (data.success && data.data) {
        setAlarmStats(data.data);
        console.log('✅ 알람 통계 로드 완료');
      } else {
        console.warn('⚠️ 알람 통계 로드 실패:', data.message);
      }
    } catch (err) {
      console.warn('⚠️ 알람 통계 로드 실패:', err);
    }
  }, []);

  // =============================================================================
  // 🔄 알람 액션 함수들
  // =============================================================================

  /**
   * 알람 확인 처리
   */
  const handleAcknowledgeAlarm = async (alarmId: number, comment: string = '') => {
    try {
      setIsProcessing(true);
      console.log(`✅ 알람 ${alarmId} 확인 처리 시작...`);

      const response = await fetch(`/api/alarms/occurrences/${alarmId}/acknowledge`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({ comment })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();

      if (data.success) {
        console.log(`✅ 알람 ${alarmId} 확인 처리 완료`);
        await loadActiveAlarms(); // 목록 새로고침
        await loadAlarmStats(); // 통계 새로고침
        setShowAckModal(false);
        setAckComment('');
        setSelectedAlarmForAck(null);
      } else {
        throw new Error(data.message || '알람 확인 처리 실패');
      }
    } catch (err) {
      console.error(`❌ 알람 ${alarmId} 확인 처리 실패:`, err);
      setError(err instanceof Error ? err.message : '알람 확인 처리 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 알람 해제 처리
   */
  const handleClearAlarm = async (alarmId: number, comment: string = '') => {
    try {
      setIsProcessing(true);
      console.log(`🗑️ 알람 ${alarmId} 해제 처리 시작...`);

      const response = await fetch(`/api/alarms/occurrences/${alarmId}/clear`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({ comment })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();

      if (data.success) {
        console.log(`✅ 알람 ${alarmId} 해제 처리 완료`);
        await loadActiveAlarms();
        await loadAlarmStats();
      } else {
        throw new Error(data.message || '알람 해제 처리 실패');
      }
    } catch (err) {
      console.error(`❌ 알람 ${alarmId} 해제 처리 실패:`, err);
      setError(err instanceof Error ? err.message : '알람 해제 처리 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 일괄 확인 처리
   */
  const handleBulkAcknowledge = async () => {
    if (selectedAlarms.length === 0) {
      alert('확인할 알람을 선택해주세요.');
      return;
    }

    if (!window.confirm(`선택된 ${selectedAlarms.length}개 알람을 확인 처리하시겠습니까?`)) {
      return;
    }

    try {
      setIsProcessing(true);
      console.log('🔄 일괄 확인 처리 시작:', selectedAlarms);

      const results = await Promise.allSettled(
        selectedAlarms.map(alarmId => 
          fetch(`/api/alarms/occurrences/${alarmId}/acknowledge`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ comment: '일괄 확인 처리' })
          })
        )
      );

      const successful = results.filter(r => r.status === 'fulfilled').length;
      const failed = results.filter(r => r.status === 'rejected').length;

      alert(`일괄 처리 완료: 성공 ${successful}개, 실패 ${failed}개`);
      
      setSelectedAlarms([]);
      await loadActiveAlarms();
      await loadAlarmStats();

    } catch (err) {
      console.error('❌ 일괄 확인 처리 실패:', err);
      setError(err instanceof Error ? err.message : '일괄 확인 처리 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 🔄 이벤트 핸들러들
  // =============================================================================

  const handleSearch = useCallback((term: string) => {
    setSearchTerm(term);
    pagination.goToFirst();
  }, [pagination]);

  const handleFilterChange = useCallback((filterType: string, value: string) => {
    switch (filterType) {
      case 'severity':
        setSeverityFilter(value);
        break;
      case 'device':
        setDeviceFilter(value);
        break;
      case 'acknowledged':
        setAcknowledgedFilter(value);
        break;
    }
    pagination.goToFirst();
  }, [pagination]);

  const handleAlarmSelect = (alarmId: number, selected: boolean) => {
    setSelectedAlarms(prev => 
      selected 
        ? [...prev, alarmId]
        : prev.filter(id => id !== alarmId)
    );
  };

  const handleSelectAll = (selected: boolean) => {
    setSelectedAlarms(selected ? alarms.map(a => a.id) : []);
  };

  const handleAckModalOpen = (alarm: ActiveAlarm) => {
    setSelectedAlarmForAck(alarm);
    setAckComment('');
    setShowAckModal(true);
  };

  const handleAckModalSubmit = async () => {
    if (selectedAlarmForAck) {
      await handleAcknowledgeAlarm(selectedAlarmForAck.id, ackComment);
    }
  };

  // =============================================================================
  // 🔄 라이프사이클 hooks
  // =============================================================================

  useEffect(() => {
    loadActiveAlarms();
    loadAlarmStats();
  }, [loadActiveAlarms, loadAlarmStats]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      loadActiveAlarms();
      loadAlarmStats();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, loadActiveAlarms, loadAlarmStats]);

  // =============================================================================
  // 🎨 렌더링 헬퍼 함수들
  // =============================================================================

  const getSeverityBadgeClass = (severity: string) => {
    switch (severity.toLowerCase()) {
      case 'critical': return 'severity-badge severity-critical';
      case 'high': return 'severity-badge severity-high';
      case 'medium': return 'severity-badge severity-medium';
      case 'low': return 'severity-badge severity-low';
      default: return 'severity-badge severity-unknown';
    }
  };

  const getSeverityIcon = (severity: string) => {
    switch (severity.toLowerCase()) {
      case 'critical': return 'fas fa-exclamation-triangle';
      case 'high': return 'fas fa-exclamation-circle';
      case 'medium': return 'fas fa-exclamation';
      case 'low': return 'fas fa-info-circle';
      default: return 'fas fa-question-circle';
    }
  };

  const getStateBadgeClass = (state: string) => {
    switch (state) {
      case 'active': return 'state-badge state-active';
      case 'acknowledged': return 'state-badge state-acknowledged';
      case 'cleared': return 'state-badge state-cleared';
      default: return 'state-badge state-unknown';
    }
  };

  const formatTimestamp = (timestamp: string) => {
    const date = new Date(timestamp);
    return date.toLocaleString();
  };

  const formatDuration = (triggeredAt: string) => {
    const now = new Date();
    const triggered = new Date(triggeredAt);
    const diffMs = now.getTime() - triggered.getTime();
    
    const hours = Math.floor(diffMs / (1000 * 60 * 60));
    const minutes = Math.floor((diffMs % (1000 * 60 * 60)) / (1000 * 60));
    
    if (hours > 0) {
      return `${hours}시간 ${minutes}분`;
    } else {
      return `${minutes}분`;
    }
  };

  // =============================================================================
  // 🎨 UI 렌더링
  // =============================================================================

  return (
    <div className="active-alarms-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">활성 알람</h1>
          <div className="page-subtitle">
            현재 발생 중인 알람을 실시간으로 모니터링하고 관리합니다
          </div>
        </div>
        <div className="header-right">
          <div className="header-actions">
            <button 
              className="btn btn-secondary"
              onClick={() => setAutoRefresh(!autoRefresh)}
            >
              <i className={`fas fa-${autoRefresh ? 'pause' : 'play'}`}></i>
              {autoRefresh ? '자동새로고침 중지' : '자동새로고침 시작'}
            </button>
            <button 
              className="btn btn-warning"
              onClick={handleBulkAcknowledge}
              disabled={selectedAlarms.length === 0 || isProcessing}
            >
              <i className="fas fa-check"></i>
              일괄 확인
            </button>
          </div>
        </div>
      </div>

      {/* 알람 통계 대시보드 */}
      {alarmStats && (
        <div className="alarm-stats-grid">
          <div className="stat-card critical">
            <div className="stat-icon">
              <i className="fas fa-exclamation-triangle"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">심각</div>
              <div className="stat-value">{alarmStats.critical_count}</div>
            </div>
          </div>
          <div className="stat-card high">
            <div className="stat-icon">
              <i className="fas fa-exclamation-circle"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">높음</div>
              <div className="stat-value">{alarmStats.high_count}</div>
            </div>
          </div>
          <div className="stat-card medium">
            <div className="stat-icon">
              <i className="fas fa-exclamation"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">보통</div>
              <div className="stat-value">{alarmStats.medium_count}</div>
            </div>
          </div>
          <div className="stat-card low">
            <div className="stat-icon">
              <i className="fas fa-info-circle"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">낮음</div>
              <div className="stat-value">{alarmStats.low_count}</div>
            </div>
          </div>
          <div className="stat-card unack">
            <div className="stat-icon">
              <i className="fas fa-bell"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">미확인</div>
              <div className="stat-value">{alarmStats.unacknowledged_count}</div>
            </div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div className="filters-section">
        <div className="search-box">
          <i className="fas fa-search"></i>
          <input
            type="text"
            placeholder="알람 메시지 또는 디바이스 이름 검색..."
            value={searchTerm}
            onChange={(e) => handleSearch(e.target.value)}
          />
        </div>
        
        <div className="filter-group">
          <select
            value={severityFilter}
            onChange={(e) => handleFilterChange('severity', e.target.value)}
          >
            <option value="all">모든 심각도</option>
            <option value="critical">심각</option>
            <option value="high">높음</option>
            <option value="medium">보통</option>
            <option value="low">낮음</option>
          </select>

          <select
            value={acknowledgedFilter}
            onChange={(e) => handleFilterChange('acknowledged', e.target.value)}
          >
            <option value="all">모든 상태</option>
            <option value="false">미확인</option>
            <option value="true">확인됨</option>
          </select>

          <select
            value={deviceFilter}
            onChange={(e) => handleFilterChange('device', e.target.value)}
          >
            <option value="all">모든 디바이스</option>
            {/* TODO: 디바이스 목록을 실제 API에서 가져와서 렌더링 */}
          </select>
        </div>

        {selectedAlarms.length > 0 && (
          <div className="bulk-actions">
            <span className="selected-count">
              {selectedAlarms.length}개 선택됨
            </span>
            <button 
              onClick={handleBulkAcknowledge}
              disabled={isProcessing}
              className="btn btn-sm btn-warning"
            >
              일괄 확인
            </button>
            <button 
              onClick={() => setSelectedAlarms([])}
              className="btn btn-sm btn-secondary"
            >
              선택 해제
            </button>
          </div>
        )}
      </div>

      {/* 에러 표시 */}
      {error && (
        <div className="error-message">
          <i className="fas fa-exclamation-circle"></i>
          {error}
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 알람 목록 */}
      <div className="alarms-content">
        {isLoading ? (
          <div className="loading-spinner">
            <i className="fas fa-spinner fa-spin"></i>
            <span>활성 알람을 불러오는 중...</span>
          </div>
        ) : alarms.length === 0 ? (
          <div className="empty-state">
            <i className="fas fa-check-circle text-success"></i>
            <h3>활성 알람이 없습니다</h3>
            <p>현재 발생 중인 알람이 없습니다. 시스템이 정상적으로 동작 중입니다.</p>
          </div>
        ) : (
          <div className="alarms-table-container">
            <table className="alarms-table">
              <thead>
                <tr>
                  <th>
                    <input
                      type="checkbox"
                      checked={selectedAlarms.length === alarms.length}
                      onChange={(e) => handleSelectAll(e.target.checked)}
                    />
                  </th>
                  <th>심각도</th>
                  <th>메시지</th>
                  <th>디바이스</th>
                  <th>발생 시간</th>
                  <th>지속 시간</th>
                  <th>상태</th>
                  <th>작업</th>
                </tr>
              </thead>
              <tbody>
                {alarms.map((alarm) => (
                  <tr 
                    key={alarm.id}
                    className={`${selectedAlarms.includes(alarm.id) ? 'selected' : ''} severity-${alarm.severity}`}
                  >
                    <td>
                      <input
                        type="checkbox"
                        checked={selectedAlarms.includes(alarm.id)}
                        onChange={(e) => handleAlarmSelect(alarm.id, e.target.checked)}
                      />
                    </td>
                    <td>
                      <span className={getSeverityBadgeClass(alarm.severity)}>
                        <i className={getSeverityIcon(alarm.severity)}></i>
                        {alarm.severity.toUpperCase()}
                      </span>
                    </td>
                    <td>
                      <div className="alarm-message">
                        <div className="message-text">{alarm.message}</div>
                        {alarm.description && (
                          <div className="message-description">{alarm.description}</div>
                        )}
                        {alarm.triggered_value !== undefined && (
                          <div className="triggered-value">
                            현재값: {alarm.triggered_value}
                            {alarm.threshold_value !== undefined && ` (임계값: ${alarm.threshold_value})`}
                          </div>
                        )}
                      </div>
                    </td>
                    <td>
                      <div className="device-info">
                        <div className="device-name">{alarm.device_name || `Device ${alarm.device_id}`}</div>
                        {alarm.data_point_name && (
                          <div className="data-point-name">{alarm.data_point_name}</div>
                        )}
                      </div>
                    </td>
                    <td>
                      <span className="timestamp">
                        {formatTimestamp(alarm.triggered_at)}
                      </span>
                    </td>
                    <td>
                      <span className="duration">
                        {formatDuration(alarm.triggered_at)}
                      </span>
                    </td>
                    <td>
                      <span className={getStateBadgeClass(alarm.state)}>
                        {alarm.state === 'active' ? '활성' : 
                         alarm.state === 'acknowledged' ? '확인됨' : 
                         alarm.state === 'cleared' ? '해제됨' : alarm.state}
                      </span>
                      {alarm.acknowledged_at && (
                        <div className="ack-info">
                          <small>확인: {alarm.acknowledged_by}</small>
                        </div>
                      )}
                    </td>
                    <td>
                      <div className="alarm-actions">
                        {alarm.state === 'active' && (
                          <button 
                            onClick={() => handleAckModalOpen(alarm)}
                            disabled={isProcessing}
                            className="btn btn-sm btn-warning"
                            title="확인"
                          >
                            <i className="fas fa-check"></i>
                          </button>
                        )}
                        <button 
                          onClick={() => handleClearAlarm(alarm.id, '수동 해제')}
                          disabled={isProcessing}
                          className="btn btn-sm btn-success"
                          title="해제"
                        >
                          <i className="fas fa-times"></i>
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* 페이징 */}
      {alarms.length > 0 && (
        <div className="pagination-section">
          <Pagination
            current={pagination.currentPage}
            total={pagination.totalCount}
            pageSize={pagination.pageSize}
            pageSizeOptions={[25, 50, 100]}
            showSizeChanger={true}
            showTotal={true}
            onChange={(page, pageSize) => {
              pagination.goToPage(page);
              if (pageSize !== pagination.pageSize) {
                pagination.changePageSize(pageSize);
              }
            }}
            onShowSizeChange={(page, pageSize) => {
              pagination.changePageSize(pageSize);
              pagination.goToPage(1);
            }}
          />
        </div>
      )}

      {/* 상태 정보 */}
      <div className="status-bar">
        <div className="status-info">
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          {isProcessing && (
            <span className="processing-indicator">
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
          {autoRefresh && (
            <span className="auto-refresh-indicator">
              <i className="fas fa-sync-alt"></i>
              {refreshInterval / 1000}초마다 자동 새로고침
            </span>
          )}
        </div>
      </div>

      {/* 알람 확인 모달 */}
      {showAckModal && selectedAlarmForAck && (
        <div className="modal-overlay" onClick={() => setShowAckModal(false)}>
          <div className="modal-content" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h3>알람 확인</h3>
              <button onClick={() => setShowAckModal(false)}>
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div className="modal-body">
              <div className="alarm-summary">
                <div className="alarm-info">
                  <span className={getSeverityBadgeClass(selectedAlarmForAck.severity)}>
                    <i className={getSeverityIcon(selectedAlarmForAck.severity)}></i>
                    {selectedAlarmForAck.severity.toUpperCase()}
                  </span>
                  <div className="alarm-message">{selectedAlarmForAck.message}</div>
                  <div className="alarm-device">
                    {selectedAlarmForAck.device_name || `Device ${selectedAlarmForAck.device_id}`}
                  </div>
                </div>
              </div>
              <div className="form-group">
                <label>확인 코멘트:</label>
                <textarea
                  value={ackComment}
                  onChange={(e) => setAckComment(e.target.value)}
                  placeholder="알람 확인에 대한 설명을 입력하세요 (선택사항)"
                  rows={3}
                />
              </div>
            </div>
            <div className="modal-footer">
              <button 
                className="btn btn-secondary"
                onClick={() => setShowAckModal(false)}
              >
                취소
              </button>
              <button 
                className="btn btn-warning"
                onClick={handleAckModalSubmit}
                disabled={isProcessing}
              >
                {isProcessing ? (
                  <><i className="fas fa-spinner fa-spin"></i> 처리 중...</>
                ) : (
                  <><i className="fas fa-check"></i> 확인</>
                )}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default ActiveAlarms;