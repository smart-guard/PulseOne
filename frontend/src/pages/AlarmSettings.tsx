// ============================================================================
// frontend/src/pages/AlarmSettings.tsx
// 알람 설정 관리 페이지 - API Service 통합 버전
// ============================================================================

import React, { useState, useEffect } from 'react';
import { AlarmApiService, AlarmRule } from '../api/services/alarmApi';
import { DataApiService } from '../api/services/dataApi';
import { DeviceApiService } from '../api/services/deviceApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import AlarmCreateEditModal from '../components/modals/AlarmCreateEditModal';
import '../styles/alarm-settings.css';

interface AlarmSettingsProps {}

// 데이터포인트 목록을 위한 타입
interface DataPoint {
  id: number;
  name: string;
  device_id: number;
  device_name: string;
  data_type: string;
  unit?: string;
  address?: string;
}

// 디바이스 목록을 위한 타입
interface Device {
  id: number;
  name: string;
  device_type: string;
  site_name?: string;
}

const AlarmSettings: React.FC<AlarmSettingsProps> = () => {
  // ConfirmProvider 사용
  const { confirm } = useConfirmContext();
  
  // 상태 관리
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 개별 토글 로딩 상태 관리
  const [toggleLoading, setToggleLoading] = useState<Set<number>>(new Set());
  
  // 데이터포인트 및 디바이스 목록
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [devices, setDevices] = useState<Device[]>([]);
  const [loadingTargetData, setLoadingTargetData] = useState(false);
  
  // 모달 상태 - 분리된 모달 사용
  const [showModal, setShowModal] = useState(false);
  const [modalMode, setModalMode] = useState<'create' | 'edit'>('create');
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  
  // 뷰 타입 상태
  const [viewType, setViewType] = useState<'card' | 'table'>('card');
  
  // 필터 상태
  const [filters, setFilters] = useState({
    search: '',
    severity: 'all',
    status: 'all',
    category: 'all',
    tag: ''
  });

  // 초기 데이터 로딩
  useEffect(() => {
    loadAlarmRules();
    loadTargetData();
  }, []);

  const loadAlarmRules = async () => {
    try {
      setLoading(true);
      setError(null);
      
      const response = await AlarmApiService.getAlarmRules({
        page: 1,
        limit: 100,
        ...filters
      });
      
      if (response.success && response.data) {
        const rules = response.data.items || [];
        setAlarmRules(rules);
        console.log('로드된 알람 규칙들:', rules);
      } else {
        setError(response.message || '알람 규칙을 불러오는데 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 로딩 실패:', error);
      setError(error instanceof Error ? error.message : '데이터 로딩에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  /**
   * 🔥 타겟 데이터 로딩 - API 서비스 사용으로 변경
   */
  const loadTargetData = async () => {
    try {
      setLoadingTargetData(true);
      
      // 🔥 API 서비스 사용으로 변경
      const [dataPointsResponse, devicesResponse] = await Promise.all([
        DataApiService.searchDataPoints({
          page: 1,
          limit: 1000,
          enabled_only: false,
          include_current_value: false
        }),
        DeviceApiService.getDevices({
          page: 1,
          limit: 100,
          enabled_only: false
        })
      ]);
      
      // 데이터포인트 처리
      if (dataPointsResponse.success && dataPointsResponse.data) {
        const points = dataPointsResponse.data.items || [];
        setDataPoints(points);
        console.log(`✅ 데이터포인트 ${points.length}개 로드 완료`);
      } else {
        console.warn('데이터포인트 로딩 실패:', dataPointsResponse.message);
        setDataPoints([]);
      }
      
      // 디바이스 처리
      if (devicesResponse.success && devicesResponse.data) {
        const deviceList = devicesResponse.data.items || [];
        setDevices(deviceList);
        console.log(`✅ 디바이스 ${deviceList.length}개 로드 완료`);
      } else {
        console.warn('디바이스 로딩 실패:', devicesResponse.message);
        setDevices([]);
      }
      
    } catch (error) {
      console.error('❌ 타겟 데이터 로딩 실패:', error);
      setDataPoints([]);
      setDevices([]);
    } finally {
      setLoadingTargetData(false);
    }
  };

  // 디바이스 이름을 매핑하는 함수
  const getDeviceName = (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    return device ? device.name : `Device #${deviceId}`;
  };

  // 알람 규칙 삭제 (새로운 confirm 다이얼로그 사용)
  const handleDeleteRule = async (ruleId: number, ruleName: string) => {
    try {
      const confirmed = await confirm({
        title: '알람 규칙 삭제',
        message: `"${ruleName}" 알람 규칙을 정말로 삭제하시겠습니까?\n\n이 작업은 되돌릴 수 없습니다.`,
        confirmText: '삭제',
        cancelText: '취소',
        confirmButtonType: 'danger'
      });

      if (!confirmed) {
        console.log('사용자가 삭제 취소함');
        return;
      }

      setLoading(true);
      
      const response = await AlarmApiService.deleteAlarmRule(ruleId);
      
      if (response.success) {
        await loadAlarmRules();
        
        // 성공 알림
        await confirm({
          title: '삭제 완료',
          message: `"${ruleName}" 알람 규칙이 성공적으로 삭제되었습니다.`,
          confirmText: '확인',
          confirmButtonType: 'primary',
          showCancelButton: false
        });
      } else {
        throw new Error(response.message || '알람 규칙 삭제에 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 삭제 실패:', error);
      
      const errorMessage = error instanceof Error ? error.message : '알람 규칙 삭제에 실패했습니다.';
      
      // 에러 다이얼로그
      await confirm({
        title: '삭제 실패',
        message: `삭제 중 오류가 발생했습니다:\n${errorMessage}`,
        confirmText: '확인',
        confirmButtonType: 'danger',
        showCancelButton: false
      });
    } finally {
      setLoading(false);
    }
  };

  // 알람 규칙 활성화/비활성화 토글
  const handleToggleRule = async (ruleId: number, currentStatus: boolean, ruleName: string) => {
    const newStatus = !currentStatus;
    const action = newStatus ? '활성화' : '비활성화';
    
    try {
      const confirmed = await confirm({
        title: '알람 규칙 상태 변경',
        message: `"${ruleName}" 알람 규칙을 ${action}하시겠습니까?\n\n이 작업은 즉시 적용됩니다.`,
        confirmText: action,
        cancelText: '취소',
        confirmButtonType: newStatus ? 'primary' : 'warning'
      });

      if (!confirmed) {
        return;
      }

      setToggleLoading(prev => new Set([...prev, ruleId]));
      
      const response = await AlarmApiService.toggleAlarmRule(ruleId, newStatus);
      
      if (response.success) {
        setAlarmRules(prev => 
          prev.map(rule =>
            rule.id === ruleId ? { ...rule, is_enabled: newStatus } : rule
          )
        );
        
        await confirm({
          title: '상태 변경 완료',
          message: `"${ruleName}" 알람 규칙이 성공적으로 ${action}되었습니다.`,
          confirmText: '확인',
          confirmButtonType: 'primary',
          showCancelButton: false
        });
      } else {
        throw new Error(response.message || `${action}에 실패했습니다.`);
      }
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : `알람 규칙 ${action}에 실패했습니다.`;
      
      await confirm({
        title: '상태 변경 실패',
        message: `${action} 중 오류가 발생했습니다:\n${errorMessage}`,
        confirmText: '확인',
        confirmButtonType: 'danger',
        showCancelButton: false
      });
      
      await loadAlarmRules();
    } finally {
      setToggleLoading(prev => {
        const newSet = new Set(prev);
        newSet.delete(ruleId);
        return newSet;
      });
    }
  };

  // 모달 핸들러들
  const handleCreateRule = () => {
    setModalMode('create');
    setSelectedRule(null);
    setShowModal(true);
  };

  const handleEditRule = (rule: AlarmRule) => {
    setModalMode('edit');
    setSelectedRule(rule);
    setShowModal(true);
  };

  const handleModalClose = () => {
    setShowModal(false);
    setSelectedRule(null);
  };

  const handleModalSave = () => {
    // 모달에서 저장 후 데이터 새로고침
    loadAlarmRules();
  };

  // 필터링된 알람 규칙
  const filteredRules = alarmRules.filter(rule => {
    if (filters.search && !rule.name.toLowerCase().includes(filters.search.toLowerCase())) {
      return false;
    }
    if (filters.severity !== 'all' && rule.severity !== filters.severity) {
      return false;
    }
    if (filters.status === 'enabled' && !rule.is_enabled) {
      return false;
    }
    if (filters.status === 'disabled' && rule.is_enabled) {
      return false;
    }
    if (filters.category !== 'all' && rule.category !== filters.category) {
      return false;
    }
    if (filters.tag && (!rule.tags || !rule.tags.some(tag => 
      String(tag).toLowerCase().includes(filters.tag.toLowerCase())
    ))) {
      return false;
    }
    return true;
  });

  // 타겟 표시 함수
  const getTargetDisplay = (rule: AlarmRule) => {
    if (rule.target_display) {
      return rule.target_display;
    }
    
    switch (rule.target_type) {
      case 'device':
        return rule.device_name || `디바이스 #${rule.target_id}`;
      case 'data_point':
        return rule.data_point_name || `데이터포인트 #${rule.target_id}`;
      case 'virtual_point':
        return rule.virtual_point_name || `가상포인트 #${rule.target_id}`;
      default:
        return rule.target_id ? `${rule.target_type} #${rule.target_id}` : 'N/A';
    }
  };

  // 조건 표시 함수
  const getConditionDisplay = (rule: AlarmRule) => {
    if (rule.condition_display) {
      return rule.condition_display;
    }
    
    const parts = [];
    
    if (rule.high_high_limit !== null && rule.high_high_limit !== undefined) {
      parts.push(`HH: ${rule.high_high_limit}`);
    }
    if (rule.high_limit !== null && rule.high_limit !== undefined) {
      parts.push(`H: ${rule.high_limit}`);
    }
    if (rule.low_limit !== null && rule.low_limit !== undefined) {
      parts.push(`L: ${rule.low_limit}`);
    }
    if (rule.low_low_limit !== null && rule.low_low_limit !== undefined) {
      parts.push(`LL: ${rule.low_low_limit}`);
    }
    
    return parts.length > 0 ? parts.join(' | ') : rule.alarm_type;
  };

  // 단위 추론 함수
  const getUnit = (rule: AlarmRule) => {
    if (rule.unit) return rule.unit;
    
    if (rule.name.toLowerCase().includes('temp') || rule.name.toLowerCase().includes('온도')) {
      return '°C';
    }
    if (rule.name.toLowerCase().includes('current') || rule.name.toLowerCase().includes('전류')) {
      return 'A';
    }
    if (rule.name.toLowerCase().includes('speed') || rule.name.toLowerCase().includes('속도')) {
      return ' m/min';
    }
    if (rule.name.toLowerCase().includes('count') || rule.name.toLowerCase().includes('개수')) {
      return ' pcs';
    }
    return '';
  };

  // 카테고리 표시명
  const getCategoryDisplayName = (category?: string) => {
    if (!category) return '';
    return AlarmApiService.getCategoryDisplayName(category);
  };

  const getTagColorClass = (tag: string): string => {
    const colorClasses = [
      'tag-color-1', 'tag-color-2', 'tag-color-3', 'tag-color-4', 'tag-color-5',
      'tag-color-6', 'tag-color-7', 'tag-color-8', 'tag-color-9', 'tag-color-10'
    ];
    
    let hash = 0;
    for (let i = 0; i < tag.length; i++) {
      hash = ((hash << 5) - hash + tag.charCodeAt(i)) & 0xffffffff;
    }
    
    const classIndex = (hash >>> 0) % colorClasses.length;
    return colorClasses[classIndex];
  };

  // 개별 토글 버튼 컴포넌트
  const ToggleButton: React.FC<{ rule: AlarmRule; size?: 'sm' | 'normal' }> = ({ rule, size = 'normal' }) => {
    const isToggling = toggleLoading.has(rule.id);
    
    const handleClick = () => {
      handleToggleRule(rule.id, rule.is_enabled, rule.name);
    };
    
    return (
      <button
        className={`btn ${rule.is_enabled ? 'btn-warning' : 'btn-success'} ${size === 'sm' ? 'btn-sm' : ''}`}
        onClick={handleClick}
        title={rule.is_enabled ? '비활성화' : '활성화'}
        disabled={isToggling || loading}
        style={{ minWidth: '40px' }}
      >
        {isToggling ? (
          <i className="fas fa-spinner fa-spin"></i>
        ) : (
          <i className={`fas ${rule.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>
        )}
      </button>
    );
  };

  return (
    <div className="alarm-settings-container">
      {/* 헤더 */}
      <div className="header">
        <h1>
          <i className="fas fa-bell"></i>
          알람 설정 관리
        </h1>
        <button 
          className="btn btn-primary"
          onClick={handleCreateRule}
        >
          <i className="fas fa-plus"></i>
          새 알람 규칙 추가
        </button>
      </div>

      {/* 필터 */}
      <div className="filters">
        <div className="filter-group flex-1">
          <label className="filter-label">검색</label>
          <input
            type="text"
            className="input"
            placeholder="알람 규칙 이름 검색..."
            value={filters.search}
            onChange={(e) => setFilters(prev => ({ ...prev, search: e.target.value }))}
          />
        </div>
        
        <div className="filter-group">
          <label className="filter-label">심각도</label>
          <select
            className="select"
            value={filters.severity}
            onChange={(e) => setFilters(prev => ({ ...prev, severity: e.target.value }))}
          >
            <option value="all">전체</option>
            <option value="critical">Critical</option>
            <option value="high">High</option>
            <option value="medium">Medium</option>
            <option value="low">Low</option>
            <option value="info">Info</option>
          </select>
        </div>
        
        <div className="filter-group">
          <label className="filter-label">상태</label>
          <select
            className="select"
            value={filters.status}
            onChange={(e) => setFilters(prev => ({ ...prev, status: e.target.value }))}
          >
            <option value="all">전체</option>
            <option value="enabled">활성</option>
            <option value="disabled">비활성</option>
          </select>
        </div>

        <div className="filter-group">
          <label className="filter-label">카테고리</label>
          <select
            className="select"
            value={filters.category}
            onChange={(e) => setFilters(prev => ({ ...prev, category: e.target.value }))}
          >
            <option value="all">전체</option>
            <option value="temperature">온도</option>
            <option value="pressure">압력</option>
            <option value="flow">유량</option>
            <option value="level">레벨</option>
            <option value="vibration">진동</option>
            <option value="electrical">전기</option>
            <option value="safety">안전</option>
            <option value="general">일반</option>
          </select>
        </div>

        <div className="filter-group">
          <label className="filter-label">보기 방식</label>
          <div className="view-toggle">
            <button
              className={`view-toggle-btn ${viewType === 'card' ? 'active' : ''}`}
              onClick={() => setViewType('card')}
            >
              <i className="fas fa-th-large"></i>
              카드
            </button>
            <button
              className={`view-toggle-btn ${viewType === 'table' ? 'active' : ''}`}
              onClick={() => setViewType('table')}
            >
              <i className="fas fa-list"></i>
              테이블
            </button>
          </div>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="error">
          <i className="fas fa-exclamation-triangle"></i>
          {error}
        </div>
      )}

      {/* 로딩 상태 */}
      {loading && (
        <div className="loading">
          <i className="fas fa-spinner fa-spin"></i>
          데이터를 불러오는 중...
        </div>
      )}

      {/* 알람 규칙 목록 */}
      {!loading && (
        <div className={`alarm-list ${viewType}-view`}>
          {filteredRules.length === 0 ? (
            <div className={`empty-state ${viewType}-view`}>
              <div className="empty-state-icon">🔔</div>
              <p>설정된 알람 규칙이 없습니다.</p>
              <button 
                className="btn btn-primary"
                onClick={handleCreateRule}
              >
                첫 번째 알람 규칙 만들기
              </button>
            </div>
          ) : viewType === 'table' ? (
            // 테이블 뷰
            <table className="alarm-table">
              <thead>
                <tr>
                  <th>알람 규칙</th>
                  <th>타겟</th>
                  <th>조건</th>
                  <th>심각도</th>
                  <th>카테고리</th>
                  <th>상태</th>
                  <th>작업</th>
                </tr>
              </thead>
              <tbody>
                {filteredRules.map(rule => (
                  <tr key={rule.id}>
                    <td>
                      <div className="table-rule-name">{rule.name}</div>
                      {rule.description && (
                        <div className="table-rule-description">{rule.description}</div>
                      )}
                      {rule.tags && rule.tags.length > 0 && (
                        <div className="table-tags">
                          {AlarmApiService.formatTags(rule.tags).map(tag => (
                            <span key={tag} className={`tag-badge ${getTagColorClass(tag)}`}>
                              {tag}
                            </span>
                          ))}
                        </div>
                      )}
                    </td>
                    <td>
                      <div className="table-target-info">
                        {getTargetDisplay(rule)}
                      </div>
                      <div className="table-target-type">{rule.target_type}</div>
                    </td>
                    <td>
                      <div className="table-condition">
                        <div className="table-condition-type">{rule.alarm_type}</div>
                        <div className="table-condition-values">
                          {getConditionDisplay(rule)}
                        </div>
                      </div>
                    </td>
                    <td>
                      <span className={`severity-badge severity-${rule.severity}`}>
                        {rule.severity}
                      </span>
                    </td>
                    <td>
                      {rule.category && (
                        <span className="category-badge">
                          {getCategoryDisplayName(rule.category)}
                        </span>
                      )}
                    </td>
                    <td>
                      <span className={`status-badge ${rule.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                        {rule.is_enabled ? '활성' : '비활성'}
                      </span>
                    </td>
                    <td>
                      <div className="table-actions">
                        <ToggleButton rule={rule} />
                        <button
                          className="btn btn-secondary"
                          onClick={() => handleEditRule(rule)}
                          title="수정"
                        >
                          <i className="fas fa-edit"></i>
                        </button>
                        <button
                          className="btn btn-danger"
                          onClick={() => handleDeleteRule(rule.id, rule.name)}
                          title="삭제"
                        >
                          <i className="fas fa-trash"></i>
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            // 카드 뷰
            filteredRules.map(rule => (
              <div key={rule.id} className="alarm-item card-item">
                {/* 카드 헤더 */}
                <div className="card-header-top">
                  <div className="card-title">{rule.name}</div>
                  <div className="card-badges">
                    <span className={`severity-badge severity-${rule.severity}`}>
                      {rule.severity.toUpperCase()}
                    </span>
                    <span className={`status-badge ${rule.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                      {rule.is_enabled ? '활성' : '비활성'}
                    </span>
                    {rule.category && (
                      <span className="category-badge">
                        {getCategoryDisplayName(rule.category)}
                      </span>
                    )}
                  </div>
                </div>
                
                {/* 조건 정보 블록 */}
                <div className="card-condition-block">
                  <div className="condition-row">
                    <span className="condition-type-badge">
                      {rule.alarm_type}
                    </span>
                    <div className="condition-values">
                      {rule.high_high_limit && (
                        <span className="condition-item">
                          <span className="condition-label">HH</span>
                          <span className="condition-value">{rule.high_high_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.high_limit && (
                        <span className="condition-item">
                          <span className="condition-label">H</span>
                          <span className="condition-value">{rule.high_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.low_limit && (
                        <span className="condition-item">
                          <span className="condition-label">L</span>
                          <span className="condition-value">{rule.low_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.low_low_limit && (
                        <span className="condition-item">
                          <span className="condition-label">LL</span>
                          <span className="condition-value">{rule.low_low_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.deadband !== undefined && rule.deadband !== null && (
                        <span className="condition-item">
                          <span className="condition-label">DB</span>
                          <span className="condition-value">{rule.deadband}{getUnit(rule)}</span>
                        </span>
                      )}
                    </div>
                  </div>
                </div>
                
                {/* 태그 표시 */}
                {rule.tags && rule.tags.length > 0 && (
                  <div className="card-tags-block">
                    {AlarmApiService.formatTags(rule.tags).map(tag => (
                      <span key={tag} className={`tag-badge ${getTagColorClass(tag)}`}>{tag}</span>
                    ))}
                  </div>
                )}
                
                {/* 설명 */}
                {rule.description && (
                  <div className="card-description-block">
                    {rule.description}
                  </div>
                )}

                {/* 하단 정보 및 액션 */}
                <div className="card-footer">
                  <div className="card-target-compact">
                    <span className="target-label">타겟</span>
                    <span className="target-value">{getTargetDisplay(rule)}</span>
                    <span className="target-type">({rule.target_type})</span>
                  </div>
                  
                  <div className="card-actions">
                    <ToggleButton rule={rule} size="sm" />
                    <button
                      className="btn btn-secondary btn-sm"
                      onClick={() => handleEditRule(rule)}
                      title="수정"
                    >
                      <i className="fas fa-edit"></i>
                    </button>
                    <button
                      className="btn btn-danger btn-sm"
                      onClick={() => handleDeleteRule(rule.id, rule.name)}
                      title="삭제"
                    >
                      <i className="fas fa-trash"></i>
                    </button>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* 분리된 생성/편집 모달 */}
      <AlarmCreateEditModal
        isOpen={showModal}
        mode={modalMode}
        rule={selectedRule}
        onClose={handleModalClose}
        onSave={handleModalSave}
        dataPoints={dataPoints}
        devices={devices}
        loadingTargetData={loadingTargetData}
      />
    </div>
  );
};

export default AlarmSettings;