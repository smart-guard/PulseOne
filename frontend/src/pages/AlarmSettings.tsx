// ============================================================================
// frontend/src/pages/AlarmSettings.tsx
// API 연결된 완성본 - 실제 백엔드와 연동
// ============================================================================

import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/alarm-settings.css';
import { AlarmApiService, AlarmRuleSettings, AlarmRuleStatistics } from '../api/services/alarmApi';
import AlarmSettingsModal from '../components/modals/AlarmSettingsModal';
import AlarmBulkUpdateModal from '../components/modals/AlarmBulkUpdateModal';

// 백엔드 API와 호환되는 알람 규칙 인터페이스
interface AlarmRule {
  id: number;
  name: string;
  description: string;
  target_type: 'data_point' | 'virtual_point' | 'device_status';
  target_id: number;
  device_name?: string;
  data_point_name?: string;
  severity: string;
  category: string;
  condition_type: string;
  
  // 임계값 필드들
  high_high_limit?: number;
  high_limit?: number;
  low_limit?: number;
  low_low_limit?: number;
  deadband?: number;
  
  // 설정 필드들
  auto_acknowledge: boolean;
  auto_clear: boolean;
  email_notification: boolean;
  sms_notification: boolean;
  message_template: string;
  is_enabled: boolean;
  
  // 메타데이터
  created_at: string;
  updated_at: string;
  created_by?: number;
}

// 통계 요약
interface StatsSummary {
  total: number;
  enabled: number;
  disabled: number;
  critical: number;
  pending_changes: number;
}

const AlarmSettings: React.FC = () => {
  // 상태 관리
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  const [selectedRules, setSelectedRules] = useState<number[]>([]);
  const [ruleStatistics, setRuleStatistics] = useState<Record<number, AlarmRuleStatistics>>({});
  
  // UI 상태
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [showSettingsModal, setShowSettingsModal] = useState(false);
  const [showBulkModal, setShowBulkModal] = useState(false);
  
  // 필터 상태
  const [filterCategory, setFilterCategory] = useState('all');
  const [filterPriority, setFilterPriority] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  const [viewMode, setViewMode] = useState<'list' | 'groups'>('list');

  // 설정 수정 상태
  const [pendingChanges, setPendingChanges] = useState<Record<number, Partial<AlarmRuleSettings>>>({});
  const [hasUnsavedChanges, setHasUnsavedChanges] = useState(false);

  // ===================================================================
  // 초기 데이터 로딩
  // ===================================================================
  useEffect(() => {
    loadAlarmRules();
  }, []);

  const loadAlarmRules = async () => {
    try {
      setLoading(true);
      setError(null);
      
      const response = await AlarmApiService.getAlarmRules({
        page: 1,
        limit: 100 // 모든 규칙 로드
      });
      
      if (response.success && response.data) {
        const rules = response.data.items || [];
        setAlarmRules(rules);
        
        // 각 규칙의 통계 정보 로드 (병렬로 처리)
        loadRulesStatistics(rules);
      } else {
        throw new Error(response.message || '알람 규칙을 불러오는데 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 로딩 실패:', error);
      setError(error instanceof Error ? error.message : '데이터 로딩에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  const loadRulesStatistics = async (rules: AlarmRule[]) => {
    const statsMap: Record<number, AlarmRuleStatistics> = {};
    
    // 병렬로 통계 로드 (최대 5개씩 동시 요청)
    const batchSize = 5;
    for (let i = 0; i < rules.length; i += batchSize) {
      const batch = rules.slice(i, i + batchSize);
      const promises = batch.map(async (rule) => {
        try {
          const response = await AlarmApiService.getAlarmRuleStatistics(rule.id, 30);
          if (response.success && response.data) {
            statsMap[rule.id] = response.data;
          }
        } catch (error) {
          console.warn(`규칙 ${rule.id} 통계 로딩 실패:`, error);
        }
      });
      
      await Promise.all(promises);
    }
    
    setRuleStatistics(statsMap);
  };

  // ===================================================================
  // 설정 변경 핸들러
  // ===================================================================
  const handleSettingChange = (ruleId: number, settingKey: string, value: any) => {
    setPendingChanges(prev => ({
      ...prev,
      [ruleId]: {
        ...prev[ruleId],
        [settingKey]: value
      }
    }));
    setHasUnsavedChanges(true);
  };

  const handleSaveSettings = async (ruleId?: number) => {
    try {
      setLoading(true);
      
      if (ruleId) {
        // 개별 규칙 저장
        const changes = pendingChanges[ruleId];
        if (changes) {
          const response = await AlarmApiService.updateAlarmRuleSettings(ruleId, changes);
          
          if (response.success) {
            // 로컬 상태 업데이트
            setAlarmRules(prev => prev.map(rule => 
              rule.id === ruleId ? { ...rule, ...response.data, updated_at: new Date().toISOString() } : rule
            ));
            
            // 변경사항 제거
            setPendingChanges(prev => {
              const { [ruleId]: removed, ...rest } = prev;
              return rest;
            });
            
            alert('설정이 저장되었습니다.');
          } else {
            throw new Error(response.message || '설정 저장에 실패했습니다.');
          }
        }
      } else {
        // 모든 변경사항 일괄 저장
        const ruleIds = Object.keys(pendingChanges).map(Number);
        const bulkRequest = {
          rule_ids: ruleIds,
          settings: {} as Partial<AlarmRuleSettings>
        };
        
        // 공통 설정 찾기 (모든 규칙에 동일하게 적용할 설정)
        const commonSettings: Partial<AlarmRuleSettings> = {};
        const firstRuleChanges = pendingChanges[ruleIds[0]];
        
        if (firstRuleChanges) {
          Object.keys(firstRuleChanges).forEach(key => {
            const value = firstRuleChanges[key as keyof AlarmRuleSettings];
            const isCommon = ruleIds.every(id => 
              pendingChanges[id] && pendingChanges[id][key as keyof AlarmRuleSettings] === value
            );
            
            if (isCommon) {
              commonSettings[key as keyof AlarmRuleSettings] = value;
            }
          });
        }
        
        if (Object.keys(commonSettings).length > 0) {
          bulkRequest.settings = commonSettings;
          
          const response = await AlarmApiService.bulkUpdateAlarmRules(bulkRequest);
          
          if (response.success) {
            await loadAlarmRules(); // 전체 리로드
            setPendingChanges({});
            alert(`${response.data?.successful_updates || 0}개 규칙이 업데이트되었습니다.`);
          } else {
            throw new Error(response.message || '일괄 저장에 실패했습니다.');
          }
        } else {
          // 개별 저장 실행
          for (const ruleId of ruleIds) {
            await handleSaveSettings(ruleId);
          }
        }
      }
      
      setHasUnsavedChanges(Object.keys(pendingChanges).length > (ruleId ? 1 : 0));
      
    } catch (error) {
      console.error('설정 저장 실패:', error);
      alert(error instanceof Error ? error.message : '설정 저장에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  const handleCancelChanges = (ruleId?: number) => {
    if (ruleId) {
      setPendingChanges(prev => {
        const { [ruleId]: removed, ...rest } = prev;
        return rest;
      });
    } else {
      setPendingChanges({});
    }
    setHasUnsavedChanges(Object.keys(pendingChanges).length > (ruleId ? 1 : 0));
  };

  const handleToggleRule = (ruleId: number) => {
    const rule = alarmRules.find(r => r.id === ruleId);
    if (rule) {
      handleSettingChange(ruleId, 'isEnabled', !rule.is_enabled);
    }
  };

  const handleBulkUpdate = async (settings: Partial<AlarmRuleSettings>) => {
    try {
      setLoading(true);
      
      const response = await AlarmApiService.bulkUpdateAlarmRules({
        rule_ids: selectedRules,
        settings
      });
      
      if (response.success) {
        await loadAlarmRules(); // 전체 리로드
        setSelectedRules([]);
        setShowBulkModal(false);
        alert(`${response.data?.successful_updates || 0}개 규칙이 업데이트되었습니다.`);
      } else {
        throw new Error(response.message || '일괄 업데이트에 실패했습니다.');
      }
    } catch (error) {
      console.error('일괄 업데이트 실패:', error);
      alert(error instanceof Error ? error.message : '일괄 업데이트에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // ===================================================================
  // UI 헬퍼 함수들
  // ===================================================================
  const getPriorityColor = (priority: string) => {
    switch (priority?.toLowerCase()) {
      case 'critical': return 'priority-critical';
      case 'high': return 'priority-high';
      case 'medium': return 'priority-medium';
      case 'low': return 'priority-low';
      default: return 'priority-medium';
    }
  };

  const getCurrentSettings = (rule: AlarmRule): AlarmRuleSettings => {
    const changes = pendingChanges[rule.id] || {};
    
    // 백엔드 데이터를 프론트엔드 형식으로 변환
    const baseSettings: AlarmRuleSettings = {
      highHighLimit: rule.high_high_limit,
      highLimit: rule.high_limit,
      lowLimit: rule.low_limit,
      lowLowLimit: rule.low_low_limit,
      deadband: rule.deadband || 0,
      priority: rule.severity as any || 'medium',
      severity: 3, // 기본값
      autoAcknowledge: rule.auto_acknowledge,
      autoReset: rule.auto_clear,
      suppressDuration: 300, // 기본값
      maxOccurrences: 0, // 기본값
      escalationTime: 15, // 기본값
      emailEnabled: rule.email_notification,
      emailRecipients: [],
      smsEnabled: rule.sms_notification,
      smsRecipients: [],
      soundEnabled: false,
      popupEnabled: true,
      webhookEnabled: false,
      webhookUrl: '',
      messageTemplate: rule.message_template || '',
      emailTemplate: '',
      schedule: { type: 'always' },
      isEnabled: rule.is_enabled
    };
    
    return { ...baseSettings, ...changes };
  };

  // ===================================================================
  // 필터링 및 계산
  // ===================================================================
  const filteredRules = alarmRules.filter(rule => {
    const matchesCategory = filterCategory === 'all' || rule.category === filterCategory;
    const matchesPriority = filterPriority === 'all' || rule.severity === filterPriority;
    const matchesStatus = filterStatus === 'all' || 
      (filterStatus === 'enabled' && rule.is_enabled) || 
      (filterStatus === 'disabled' && !rule.is_enabled);
    const matchesSearch = searchTerm === '' || 
      rule.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
      (rule.device_name && rule.device_name.toLowerCase().includes(searchTerm.toLowerCase()));
    
    return matchesCategory && matchesPriority && matchesStatus && matchesSearch;
  });

  const stats: StatsSummary = {
    total: alarmRules.length,
    enabled: alarmRules.filter(r => r.is_enabled).length,
    disabled: alarmRules.filter(r => !r.is_enabled).length,
    critical: alarmRules.filter(r => r.severity === 'critical').length,
    pending_changes: Object.keys(pendingChanges).length
  };

  // ===================================================================
  // 렌더링
  // ===================================================================
  return (
    <div className="alarm-settings-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div>
          <h1 className="page-title">알람 설정 조정</h1>
          <div className="page-subtitle">
            기존 알람 룰들의 임계값, 알림, 스케줄 등 설정을 조정합니다
          </div>
        </div>
        <div className="page-actions">
          {hasUnsavedChanges && (
            <div className="unsaved-indicator">
              <i className="fas fa-exclamation-triangle"></i>
              저장되지 않은 변경사항이 있습니다
            </div>
          )}
          <button 
            className="btn btn-outline"
            onClick={() => handleCancelChanges()}
            disabled={!hasUnsavedChanges || loading}
          >
            <i className="fas fa-undo"></i>
            모든 변경사항 취소
          </button>
          <button 
            className="btn btn-success"
            onClick={() => handleSaveSettings()}
            disabled={!hasUnsavedChanges || loading}
          >
            <i className="fas fa-save"></i>
            모든 변경사항 저장
          </button>
          <button 
            className="btn btn-primary"
            onClick={() => setShowBulkModal(true)}
            disabled={selectedRules.length === 0 || loading}
          >
            <i className="fas fa-edit"></i>
            일괄 수정 ({selectedRules.length}개)
          </button>
        </div>
      </div>

      {/* 에러 표시 */}
      {error && (
        <div className="error-banner">
          <div className="error-content">
            <i className="fas fa-exclamation-triangle"></i>
            <span>{error}</span>
            <button onClick={() => { setError(null); loadAlarmRules(); }}>
              다시 시도
            </button>
          </div>
        </div>
      )}

      {/* 로딩 표시 */}
      {loading && (
        <div className="loading-banner">
          <i className="fas fa-spinner fa-spin"></i>
          데이터 처리 중...
        </div>
      )}

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-row">
          <div className="filter-group">
            <label>보기 모드</label>
            <div className="view-toggle">
              <button 
                className={`toggle-btn ${viewMode === 'list' ? 'active' : ''}`}
                onClick={() => setViewMode('list')}
              >
                <i className="fas fa-list"></i> 목록
              </button>
              <button 
                className={`toggle-btn ${viewMode === 'groups' ? 'active' : ''}`}
                onClick={() => setViewMode('groups')}
              >
                <i className="fas fa-layer-group"></i> 그룹
              </button>
            </div>
          </div>
          
          <div className="filter-group">
            <label>카테고리</label>
            <select
              value={filterCategory}
              onChange={(e) => setFilterCategory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 카테고리</option>
              <option value="Safety">안전</option>
              <option value="Process">공정</option>
              <option value="Production">생산</option>
              <option value="System">시스템</option>
            </select>
          </div>

          <div className="filter-group">
            <label>우선순위</label>
            <select
              value={filterPriority}
              onChange={(e) => setFilterPriority(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 우선순위</option>
              <option value="critical">Critical</option>
              <option value="high">High</option>
              <option value="medium">Medium</option>
              <option value="low">Low</option>
            </select>
          </div>

          <div className="filter-group">
            <label>상태</label>
            <select
              value={filterStatus}
              onChange={(e) => setFilterStatus(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 상태</option>
              <option value="enabled">활성</option>
              <option value="disabled">비활성</option>
            </select>
          </div>

          <div className="filter-group flex-1">
            <label>검색</label>
            <div className="search-container">
              <input
                type="text"
                placeholder="알람명, 설명, 디바이스명 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="search-input"
              />
              <i className="fas fa-search search-icon"></i>
            </div>
          </div>
        </div>
      </div>

      {/* 통계 정보 */}
      <div className="stats-panel">
        <div className="stat-card">
          <div className="stat-value">{stats.total}</div>
          <div className="stat-label">전체 알람 룰</div>
        </div>
        <div className="stat-card status-enabled">
          <div className="stat-value">{stats.enabled}</div>
          <div className="stat-label">활성</div>
        </div>
        <div className="stat-card status-disabled">
          <div className="stat-value">{stats.disabled}</div>
          <div className="stat-label">비활성</div>
        </div>
        <div className="stat-card priority-critical">
          <div className="stat-value">{stats.critical}</div>
          <div className="stat-label">Critical</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{stats.pending_changes}</div>
          <div className="stat-label">변경 대기</div>
        </div>
      </div>

      {/* 알람 룰 목록 */}
      <div className="rules-list">
        <div className="rules-grid">
          {filteredRules.map(rule => {
            const currentSettings = getCurrentSettings(rule);
            const hasChanges = !!pendingChanges[rule.id];
            const isSelected = selectedRules.includes(rule.id);
            const stats = ruleStatistics[rule.id];
            
            return (
              <div key={rule.id} className={`rule-card ${currentSettings.isEnabled ? 'enabled' : 'disabled'} ${hasChanges ? 'has-changes' : ''} ${isSelected ? 'selected' : ''}`}>
                <div className="rule-header">
                  <div className="rule-select">
                    <input
                      type="checkbox"
                      checked={isSelected}
                      onChange={(e) => {
                        if (e.target.checked) {
                          setSelectedRules(prev => [...prev, rule.id]);
                        } else {
                          setSelectedRules(prev => prev.filter(id => id !== rule.id));
                        }
                      }}
                    />
                  </div>
                  <div className="rule-title">
                    <h3>{rule.name}</h3>
                    <div className="rule-badges">
                      <span className={`priority-badge ${getPriorityColor(currentSettings.priority)}`}>
                        {currentSettings.priority.toUpperCase()}
                      </span>
                      <span className="category-badge">{rule.category || 'General'}</span>
                      <span className={`status-badge ${currentSettings.isEnabled ? 'enabled' : 'disabled'}`}>
                        {currentSettings.isEnabled ? '활성' : '비활성'}
                      </span>
                      {hasChanges && <span className="changes-badge">변경사항</span>}
                    </div>
                  </div>
                  <div className="rule-actions">
                    <button
                      className={`toggle-btn ${currentSettings.isEnabled ? 'enabled' : 'disabled'}`}
                      onClick={() => handleToggleRule(rule.id)}
                      title={currentSettings.isEnabled ? '비활성화' : '활성화'}
                      disabled={loading}
                    >
                      <i className={`fas ${currentSettings.isEnabled ? 'fa-toggle-on' : 'fa-toggle-off'}`}></i>
                    </button>
                    <button
                      className="settings-btn"
                      onClick={() => {
                        setSelectedRule(rule);
                        setShowSettingsModal(true);
                      }}
                      title="설정 조정"
                      disabled={loading}
                    >
                      <i className="fas fa-cog"></i>
                    </button>
                    {hasChanges && (
                      <button
                        className="save-btn"
                        onClick={() => handleSaveSettings(rule.id)}
                        title="변경사항 저장"
                        disabled={loading}
                      >
                        <i className="fas fa-save"></i>
                      </button>
                    )}
                  </div>
                </div>

                <div className="rule-content">
                  <div className="rule-description">
                    {rule.description}
                  </div>

                  <div className="rule-source">
                    <label>데이터 소스:</label>
                    <span>{rule.data_point_name || `${rule.target_type} ${rule.target_id}`}</span>
                    {rule.device_name && <small>({rule.device_name})</small>}
                  </div>

                  <div className="rule-condition">
                    <label>현재 설정:</label>
                    <div className="condition-display">
                      {rule.condition_type === 'threshold' && (
                        <span>
                          임계값: HH={currentSettings.highHighLimit || 'N/A'}, H={currentSettings.highLimit || 'N/A'}, 
                          L={currentSettings.lowLimit || 'N/A'}, LL={currentSettings.lowLowLimit || 'N/A'}
                          {currentSettings.deadband > 0 && `, DB=${currentSettings.deadband}`}
                        </span>
                      )}
                      {rule.condition_type === 'range' && (
                        <span>범위: {currentSettings.lowLimit} ~ {currentSettings.highLimit}</span>
                      )}
                      {rule.condition_type === 'pattern' && (
                        <span>패턴 조건</span>
                      )}
                    </div>
                  </div>

                  <div className="rule-notifications">
                    <label>알림:</label>
                    <div className="notification-icons">
                      {currentSettings.emailEnabled && <i className="fas fa-envelope" title="이메일"></i>}
                      {currentSettings.smsEnabled && <i className="fas fa-sms" title="SMS"></i>}
                      {currentSettings.soundEnabled && <i className="fas fa-volume-up" title="소리"></i>}
                      {currentSettings.popupEnabled && <i className="fas fa-window-maximize" title="팝업"></i>}
                      {currentSettings.webhookEnabled && <i className="fas fa-link" title="웹훅"></i>}
                    </div>
                  </div>

                  <div className="rule-stats">
                    <div className="stat-item">
                      <span>발생:</span>
                      <span>{stats?.occurrence_summary.total_occurrences || 0}회</span>
                    </div>
                    <div className="stat-item">
                      <span>평균대응:</span>
                      <span>{stats?.performance_metrics.avg_response_time_minutes?.toFixed(1) || 'N/A'}분</span>
                    </div>
                    <div className="stat-item">
                      <span>억제시간:</span>
                      <span>{currentSettings.suppressDuration}초</span>
                    </div>
                    <div className="stat-item">
                      <span>최근 수정:</span>
                      <span>{new Date(rule.updated_at).toLocaleDateString()}</span>
                    </div>
                  </div>
                </div>
              </div>
            );
          })}
        </div>

        {filteredRules.length === 0 && !loading && (
          <div className="empty-state">
            <i className="fas fa-cog empty-icon"></i>
            <div className="empty-title">설정할 알람 룰이 없습니다</div>
            <div className="empty-description">
              필터 조건을 변경하거나 새로운 알람 룰을 생성해보세요.
            </div>
          </div>
        )}
      </div>

      {/* 설정 모달 */}
      {showSettingsModal && selectedRule && (
        <AlarmSettingsModal
          rule={selectedRule}
          currentSettings={getCurrentSettings(selectedRule)}
          statistics={ruleStatistics[selectedRule.id]}
          onClose={() => setShowSettingsModal(false)}
          onSave={(settings) => {
            Object.entries(settings).forEach(([key, value]) => {
              handleSettingChange(selectedRule.id, key, value);
            });
          }}
          onSaveAndClose={async (settings) => {
            Object.entries(settings).forEach(([key, value]) => {
              handleSettingChange(selectedRule.id, key, value);
            });
            await handleSaveSettings(selectedRule.id);
            setShowSettingsModal(false);
          }}
          hasChanges={!!pendingChanges[selectedRule.id]}
          loading={loading}
        />
      )}

      {/* 일괄 업데이트 모달 */}
      {showBulkModal && (
        <AlarmBulkUpdateModal
          selectedRules={selectedRules}
          rules={alarmRules.filter(r => selectedRules.includes(r.id))}
          onClose={() => setShowBulkModal(false)}
          onUpdate={handleBulkUpdate}
          loading={loading}
        />
      )}
    </div>
  );
};

export default AlarmSettings;