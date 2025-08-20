import React, { useState, useEffect } from 'react';
import { AlarmApiService, AlarmRule, AlarmRuleCreateData } from '../api/services/alarmApi';
import '../styles/alarm-settings.css';

interface AlarmSettingsProps {}

const AlarmSettings: React.FC<AlarmSettingsProps> = () => {
  // 상태 관리
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 모달 상태
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  
  // 뷰 타입 상태
  const [viewType, setViewType] = useState<'card' | 'table'>('card');
  
  // 필터 상태
  const [filters, setFilters] = useState({
    search: '',
    severity: 'all',
    status: 'all',
    category: 'all'
  });

  // 폼 상태 (백엔드 스키마에 맞춤)
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    device_id: '',
    data_point_id: '',
    virtual_point_id: '',
    condition_type: 'threshold' as const,
    condition_config: {
      high_limit: '',
      low_limit: '',
      deadband: '2.0'
    },
    severity: 'major' as const,
    message_template: '',
    is_enabled: true,
    auto_clear: true,
    auto_acknowledge: false,
    acknowledgment_required: true,
    escalation_time_minutes: 15,
    notification_enabled: true,
    email_notification: false,
    sms_notification: false,
    email_recipients: '',
    sms_recipients: ''
  });

  // 초기 데이터 로딩
  useEffect(() => {
    loadAlarmRules();
  }, []);

  const loadAlarmRules = async () => {
    try {
      setLoading(true);
      setError(null);
      
      const response = await AlarmApiService.getAlarmRules({
        page: 1,
        limit: 100
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

  // 새 알람 규칙 생성
  const handleCreateRule = async () => {
    try {
      setLoading(true);
      
      // 백엔드 스키마에 맞는 데이터 구성
      const createData: AlarmRuleCreateData = {
        name: formData.name,
        description: formData.description,
        device_id: formData.device_id ? parseInt(formData.device_id) : undefined,
        data_point_id: formData.data_point_id ? parseInt(formData.data_point_id) : undefined,
        virtual_point_id: formData.virtual_point_id ? parseInt(formData.virtual_point_id) : undefined,
        condition_type: formData.condition_type,
        condition_config: JSON.stringify({
          high_limit: formData.condition_config.high_limit ? parseFloat(formData.condition_config.high_limit) : null,
          low_limit: formData.condition_config.low_limit ? parseFloat(formData.condition_config.low_limit) : null,
          deadband: parseFloat(formData.condition_config.deadband)
        }),
        severity: formData.severity,
        message_template: formData.message_template || `${formData.name} 알람이 발생했습니다`,
        auto_acknowledge: formData.auto_acknowledge,
        auto_clear: formData.auto_clear,
        acknowledgment_required: formData.acknowledgment_required,
        escalation_time_minutes: formData.escalation_time_minutes,
        notification_enabled: formData.notification_enabled,
        email_notification: formData.email_notification,
        sms_notification: formData.sms_notification,
        is_enabled: formData.is_enabled
      };
      
      console.log('생성할 알람 규칙 데이터:', createData);
      
      const response = await AlarmApiService.createAlarmRule(createData);
      
      if (response.success) {
        await loadAlarmRules();
        setShowCreateModal(false);
        resetForm();
        alert('알람 규칙이 성공적으로 생성되었습니다.');
      } else {
        throw new Error(response.message || '알람 규칙 생성에 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 생성 실패:', error);
      alert(error instanceof Error ? error.message : '알람 규칙 생성에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 알람 규칙 삭제
  const handleDeleteRule = async (ruleId: number) => {
    if (!confirm('정말로 이 알람 규칙을 삭제하시겠습니까?')) {
      return;
    }

    try {
      setLoading(true);
      
      const response = await AlarmApiService.deleteAlarmRule(ruleId);
      
      if (response.success) {
        await loadAlarmRules();
        alert('알람 규칙이 삭제되었습니다.');
      } else {
        throw new Error(response.message || '알람 규칙 삭제에 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 삭제 실패:', error);
      alert(error instanceof Error ? error.message : '알람 규칙 삭제에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 알람 규칙 활성화/비활성화 토글
  const handleToggleRule = async (ruleId: number, currentStatus: boolean) => {
    try {
      const response = await AlarmApiService.updateAlarmRule(ruleId, {
        is_enabled: !currentStatus
      });
      
      if (response.success) {
        await loadAlarmRules();
      } else {
        throw new Error(response.message || '상태 변경에 실패했습니다.');
      }
    } catch (error) {
      console.error('상태 변경 실패:', error);
      alert(error instanceof Error ? error.message : '상태 변경에 실패했습니다.');
    }
  };

  // 폼 초기화
  const resetForm = () => {
    setFormData({
      name: '',
      description: '',
      device_id: '',
      data_point_id: '',
      virtual_point_id: '',
      condition_type: 'threshold',
      condition_config: {
        high_limit: '',
        low_limit: '',
        deadband: '2.0'
      },
      severity: 'major',
      message_template: '',
      is_enabled: true,
      auto_clear: true,
      auto_acknowledge: false,
      acknowledgment_required: true,
      escalation_time_minutes: 15,
      notification_enabled: true,
      email_notification: false,
      sms_notification: false,
      email_recipients: '',
      sms_recipients: ''
    });
  };

  // 알람 규칙 편집을 위한 폼 데이터 설정
  const handleEditRule = (rule: AlarmRule) => {
    const config = rule.condition_config || {};
    setFormData({
      name: rule.name,
      description: rule.description || '',
      device_id: rule.device_id?.toString() || '',
      data_point_id: rule.data_point_id?.toString() || '',
      virtual_point_id: rule.virtual_point_id?.toString() || '',
      condition_type: rule.condition_type as any,
      condition_config: {
        high_limit: config.high_limit?.toString() || '',
        low_limit: config.low_limit?.toString() || '',
        deadband: config.deadband?.toString() || '2.0'
      },
      severity: rule.severity as any,
      message_template: rule.message_template || '',
      is_enabled: rule.is_enabled,
      auto_clear: rule.auto_clear || false,
      auto_acknowledge: rule.auto_acknowledge || false,
      acknowledgment_required: rule.acknowledgment_required !== false,
      escalation_time_minutes: rule.escalation_time_minutes || 15,
      notification_enabled: rule.notification_enabled || false,
      email_notification: rule.email_notification || false,
      sms_notification: rule.sms_notification || false,
      email_recipients: '',
      sms_recipients: ''
    });
    setSelectedRule(rule);
    setShowEditModal(true);
  };

  // 알람 규칙 수정
  const handleUpdateRule = async () => {
    if (!selectedRule) return;

    try {
      setLoading(true);
      
      const updateData = {
        name: formData.name,
        description: formData.description,
        device_id: formData.device_id ? parseInt(formData.device_id) : undefined,
        data_point_id: formData.data_point_id ? parseInt(formData.data_point_id) : undefined,
        virtual_point_id: formData.virtual_point_id ? parseInt(formData.virtual_point_id) : undefined,
        condition_type: formData.condition_type,
        condition_config: JSON.stringify({
          high_limit: formData.condition_config.high_limit ? parseFloat(formData.condition_config.high_limit) : null,
          low_limit: formData.condition_config.low_limit ? parseFloat(formData.condition_config.low_limit) : null,
          deadband: parseFloat(formData.condition_config.deadband)
        }),
        severity: formData.severity,
        message_template: formData.message_template,
        auto_acknowledge: formData.auto_acknowledge,
        auto_clear: formData.auto_clear,
        acknowledgment_required: formData.acknowledgment_required,
        escalation_time_minutes: formData.escalation_time_minutes,
        notification_enabled: formData.notification_enabled,
        email_notification: formData.email_notification,
        sms_notification: formData.sms_notification,
        is_enabled: formData.is_enabled
      };
      
      const response = await AlarmApiService.updateAlarmRule(selectedRule.id, updateData);
      
      if (response.success) {
        await loadAlarmRules();
        setShowEditModal(false);
        setSelectedRule(null);
        resetForm();
        alert('알람 규칙이 성공적으로 수정되었습니다.');
      } else {
        throw new Error(response.message || '알람 규칙 수정에 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 수정 실패:', error);
      alert(error instanceof Error ? error.message : '알람 규칙 수정에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 필터링된 알람 규칙 (백엔드 필드명 사용)
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
    return true;
  });

  // 타겟 표시 함수
  const getTargetDisplay = (rule: AlarmRule) => {
    if (rule.target_display) {
      return rule.target_display;
    }
    
    if (rule.device_id && rule.data_point_id) {
      return `디바이스 #${rule.device_id} - 포인트 #${rule.data_point_id}`;
    } else if (rule.device_id) {
      return `디바이스 #${rule.device_id}`;
    } else if (rule.data_point_id) {
      return `데이터포인트 #${rule.data_point_id}`;
    } else if (rule.virtual_point_id) {
      return `가상포인트 #${rule.virtual_point_id}`;
    }
    
    return 'N/A';
  };

  // 조건 표시 함수
  const getConditionDisplay = (rule: AlarmRule) => {
    if (rule.condition_display) {
      return rule.condition_display;
    }
    
    const config = rule.condition_config || {};
    const parts = [];
    
    if (config.high_limit !== null && config.high_limit !== undefined) {
      parts.push(`상한: ${config.high_limit}`);
    }
    if (config.low_limit !== null && config.low_limit !== undefined) {
      parts.push(`하한: ${config.low_limit}`);
    }
    
    return parts.length > 0 ? parts.join(' | ') : rule.condition_type;
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
          onClick={() => setShowCreateModal(true)}
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
            <option value="major">Major</option>
            <option value="minor">Minor</option>
            <option value="warning">Warning</option>
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
                onClick={() => setShowCreateModal(true)}
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
                    </td>
                    <td>
                      <div className="table-target-info">
                        {getTargetDisplay(rule)}
                      </div>
                      {rule.device_name && (
                        <div className="table-rule-description">{rule.device_name}</div>
                      )}
                    </td>
                    <td>
                      <div className="table-condition">
                        <div className="table-condition-type">{rule.condition_type}</div>
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
                      <span className={`status-badge ${rule.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                        {rule.is_enabled ? '활성' : '비활성'}
                      </span>
                    </td>
                    <td>
                      <div className="table-actions">
                        <button
                          className="btn btn-secondary"
                          onClick={() => handleToggleRule(rule.id, rule.is_enabled)}
                          title={rule.is_enabled ? '비활성화' : '활성화'}
                        >
                          <i className={`fas ${rule.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>
                        </button>
                        <button
                          className="btn btn-secondary"
                          onClick={() => handleEditRule(rule)}
                          title="수정"
                        >
                          <i className="fas fa-edit"></i>
                        </button>
                        <button
                          className="btn btn-danger"
                          onClick={() => handleDeleteRule(rule.id)}
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
                <div className="card-header">
                  <div className="card-title-section">
                    <div className="card-title">{rule.name}</div>
                    {rule.description && (
                      <div className="card-description">{rule.description}</div>
                    )}
                  </div>
                  <div className="card-badges">
                    <span className={`severity-badge severity-${rule.severity}`}>
                      {rule.severity}
                    </span>
                    <span className={`status-badge ${rule.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                      {rule.is_enabled ? '활성' : '비활성'}
                    </span>
                  </div>
                </div>

                <div className="card-content">
                  <div className="card-info-group">
                    <div className="card-info-item">
                      <div className="card-info-label">타겟</div>
                      <div className="card-info-value">{getTargetDisplay(rule)}</div>
                    </div>
                    {rule.device_name && (
                      <div className="card-info-item">
                        <div className="card-info-label">디바이스</div>
                        <div className="card-info-value">{rule.device_name}</div>
                      </div>
                    )}
                  </div>
                  <div className="card-info-group">
                    <div className="card-info-item">
                      <div className="card-info-label">조건</div>
                      <div className="card-info-value">{rule.condition_type}</div>
                    </div>
                    <div className="card-info-item">
                      <div className="card-info-label">임계값</div>
                      <div className="card-info-value">
                        {getConditionDisplay(rule)}
                      </div>
                    </div>
                  </div>
                </div>

                <div className="card-actions">
                  <button
                    className="btn btn-secondary"
                    onClick={() => handleToggleRule(rule.id, rule.is_enabled)}
                  >
                    <i className={`fas ${rule.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>
                    {rule.is_enabled ? '비활성화' : '활성화'}
                  </button>
                  <button
                    className="btn btn-secondary"
                    onClick={() => handleEditRule(rule)}
                  >
                    <i className="fas fa-edit"></i>
                    수정
                  </button>
                  <button
                    className="btn btn-danger"
                    onClick={() => handleDeleteRule(rule.id)}
                  >
                    <i className="fas fa-trash"></i>
                    삭제
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* 새 알람 규칙 생성 모달 */}
      {showCreateModal && (
        <div className="modal-overlay">
          <div className="modal">
            <div className="modal-header">
              <h2 className="modal-title">새 알람 규칙 추가</h2>
              <button
                className="close-button"
                onClick={() => {
                  setShowCreateModal(false);
                  resetForm();
                }}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="form-section">
                <div className="section-title">기본 정보</div>
                
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">규칙 이름 *</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.name}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                      placeholder="알람 규칙 이름을 입력하세요"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">설명</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.description}
                      onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                      placeholder="알람 규칙에 대한 설명을 입력하세요"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">디바이스 ID</label>
                    <input
                      type="number"
                      className="form-input"
                      value={formData.device_id}
                      onChange={(e) => setFormData(prev => ({ ...prev, device_id: e.target.value }))}
                      placeholder="디바이스 ID (선택사항)"
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">데이터포인트 ID</label>
                    <input
                      type="number"
                      className="form-input"
                      value={formData.data_point_id}
                      onChange={(e) => setFormData(prev => ({ ...prev, data_point_id: e.target.value }))}
                      placeholder="데이터포인트 ID (선택사항)"
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">가상포인트 ID</label>
                    <input
                      type="number"
                      className="form-input"
                      value={formData.virtual_point_id}
                      onChange={(e) => setFormData(prev => ({ ...prev, virtual_point_id: e.target.value }))}
                      placeholder="가상포인트 ID (선택사항)"
                    />
                  </div>
                </div>
              </div>

              <div className="form-section">
                <div className="section-title">조건 설정</div>
                
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">조건 타입 *</label>
                    <select
                      className="form-select"
                      value={formData.condition_type}
                      onChange={(e) => setFormData(prev => ({ ...prev, condition_type: e.target.value as any }))}
                    >
                      <option value="threshold">임계값</option>
                      <option value="range">범위</option>
                      <option value="change">변화량</option>
                      <option value="boolean">불린</option>
                    </select>
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">심각도 *</label>
                    <select
                      className="form-select"
                      value={formData.severity}
                      onChange={(e) => setFormData(prev => ({ ...prev, severity: e.target.value as any }))}
                    >
                      <option value="critical">Critical</option>
                      <option value="major">Major</option>
                      <option value="minor">Minor</option>
                      <option value="warning">Warning</option>
                      <option value="info">Info</option>
                    </select>
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">상한값</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.condition_config.high_limit}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        condition_config: { ...prev.condition_config, high_limit: e.target.value }
                      }))}
                      placeholder="상한값을 입력하세요"
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">하한값</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.condition_config.low_limit}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        condition_config: { ...prev.condition_config, low_limit: e.target.value }
                      }))}
                      placeholder="하한값을 입력하세요"
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">데드밴드</label>
                    <input
                      type="number"
                      step="0.1"
                      className="form-input"
                      value={formData.condition_config.deadband}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        condition_config: { ...prev.condition_config, deadband: e.target.value }
                      }))}
                      placeholder="데드밴드 값"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">메시지 템플릿</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.message_template}
                      onChange={(e) => setFormData(prev => ({ ...prev, message_template: e.target.value }))}
                      placeholder="알람 메시지 템플릿"
                    />
                  </div>
                </div>
              </div>

              <div className="form-section">
                <div className="section-title">동작 설정</div>
                
                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.is_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, is_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알람 규칙 활성화</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.auto_clear}
                    onChange={(e) => setFormData(prev => ({ ...prev, auto_clear: e.target.checked }))}
                  />
                  <label className="checkbox-label">자동 해제</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.auto_acknowledge}
                    onChange={(e) => setFormData(prev => ({ ...prev, auto_acknowledge: e.target.checked }))}
                  />
                  <label className="checkbox-label">자동 확인</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.notification_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, notification_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알림 활성화</label>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">에스컬레이션 시간 (분)</label>
                    <input
                      type="number"
                      className="form-input"
                      value={formData.escalation_time_minutes}
                      onChange={(e) => setFormData(prev => ({ ...prev, escalation_time_minutes: parseInt(e.target.value) || 0 }))}
                      placeholder="에스컬레이션 시간"
                    />
                  </div>
                </div>
              </div>

              <div className="form-section">
                <div className="section-title">알림 설정</div>
                
                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.email_notification}
                    onChange={(e) => setFormData(prev => ({ ...prev, email_notification: e.target.checked }))}
                  />
                  <label className="checkbox-label">이메일 알림</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.sms_notification}
                    onChange={(e) => setFormData(prev => ({ ...prev, sms_notification: e.target.checked }))}
                  />
                  <label className="checkbox-label">SMS 알림</label>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">이메일 수신자</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.email_recipients}
                      onChange={(e) => setFormData(prev => ({ ...prev, email_recipients: e.target.value }))}
                      placeholder="email1@example.com, email2@example.com"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">SMS 수신자</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.sms_recipients}
                      onChange={(e) => setFormData(prev => ({ ...prev, sms_recipients: e.target.value }))}
                      placeholder="010-1234-5678, 010-9876-5432"
                    />
                  </div>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-outline"
                onClick={() => {
                  setShowCreateModal(false);
                  resetForm();
                }}
              >
                취소
              </button>
              <button
                className="btn btn-primary"
                onClick={handleCreateRule}
                disabled={!formData.name || (!formData.device_id && !formData.data_point_id && !formData.virtual_point_id)}
              >
                <i className="fas fa-plus"></i>
                생성
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 알람 규칙 수정 모달 */}
      {showEditModal && selectedRule && (
        <div className="modal-overlay">
          <div className="modal">
            <div className="modal-header">
              <h2 className="modal-title">알람 규칙 수정: {selectedRule.name}</h2>
              <button
                className="close-button"
                onClick={() => {
                  setShowEditModal(false);
                  setSelectedRule(null);
                  resetForm();
                }}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="form-section readonly-section">
                <div className="section-title">기본 정보</div>
                
                <div className="readonly-grid">
                  <div className="readonly-item">
                    <label>규칙 ID</label>
                    <span>#{selectedRule.id}</span>
                  </div>
                  <div className="readonly-item">
                    <label>생성일</label>
                    <span>{new Date(selectedRule.created_at).toLocaleString()}</span>
                  </div>
                  <div className="readonly-item">
                    <label>수정일</label>
                    <span>{new Date(selectedRule.updated_at).toLocaleString()}</span>
                  </div>
                  <div className="readonly-item">
                    <label>타겟</label>
                    <span>{getTargetDisplay(selectedRule)}</span>
                  </div>
                </div>
              </div>

              {/* 수정 가능한 필드들은 생성 모달과 동일한 구조로 재사용 */}
              <div className="form-section">
                <div className="section-title">수정 가능한 설정</div>
                
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">규칙 이름 *</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.name}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">설명</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.description}
                      onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">상한값</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.condition_config.high_limit}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        condition_config: { ...prev.condition_config, high_limit: e.target.value }
                      }))}
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">하한값</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.condition_config.low_limit}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        condition_config: { ...prev.condition_config, low_limit: e.target.value }
                      }))}
                    />
                  </div>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.is_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, is_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알람 규칙 활성화</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.notification_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, notification_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알림 활성화</label>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-outline"
                onClick={() => {
                  setShowEditModal(false);
                  setSelectedRule(null);
                  resetForm();
                }}
              >
                취소
              </button>
              <button
                className="btn btn-primary"
                onClick={handleUpdateRule}
              >
                <i className="fas fa-save"></i>
                수정
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AlarmSettings;