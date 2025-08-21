import React, { useState, useEffect } from 'react';
import { AlarmApiService, AlarmRule, AlarmRuleCreateData } from '../../api/services/alarmApi';
import { ALARM_CONSTANTS } from '../../api/endpoints';
import '../../styles/alarm-settings.css';

interface DataPoint {
  id: number;
  name: string;
  device_id: number;
  device_name: string;
  data_type: string;
  unit?: string;
  address?: string;
}

interface Device {
  id: number;
  name: string;
  device_type: string;
  site_name?: string;
}

interface AlarmCreateEditModalProps {
  isOpen: boolean;
  mode: 'create' | 'edit';
  rule?: AlarmRule;
  onClose: () => void;
  onSave: () => void;
  dataPoints: DataPoint[];
  devices: Device[];
  loadingTargetData: boolean;
}

const AlarmCreateEditModal: React.FC<AlarmCreateEditModalProps> = ({
  isOpen,
  mode,
  rule,
  onClose,
  onSave,
  dataPoints,
  devices,
  loadingTargetData
}) => {
  const [loading, setLoading] = useState(false);
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    target_type: 'data_point' as const,
    target_id: '',
    selected_device_id: '',
    target_group: '',
    alarm_type: 'analog' as const,
    high_high_limit: '',
    high_limit: '',
    low_limit: '',
    low_low_limit: '',
    deadband: '2.0',
    rate_of_change: '',
    trigger_condition: '',
    condition_script: '',
    message_template: '',
    severity: 'medium' as const,
    priority: 100,
    auto_acknowledge: false,
    auto_clear: true,
    notification_enabled: true,
    is_enabled: true,
    escalation_enabled: false,
    escalation_max_level: 3,
    category: '',
    tags: [] as string[]
  });

  // 타겟 타입별 허용 알람 타입 - 핵심 수정사항
  const getAllowedAlarmTypes = (targetType: string) => {
    switch (targetType) {
      case 'data_point':
        return [
          { value: 'analog', label: '아날로그', description: '수치 임계값 기반 알람' },
          { value: 'digital', label: '디지털', description: '상태 변화 기반 알람' },
          { value: 'script', label: '스크립트', description: '사용자 정의 조건 알람' }
        ];
      case 'device':
        return [
          { value: 'digital', label: '디바이스 상태', description: '연결/통신 상태 알람' },
          { value: 'script', label: '스크립트', description: '복합 조건 알람' }
        ];
      case 'virtual_point':
        return [
          { value: 'analog', label: '아날로그', description: '계산된 수치 임계값 알람' },
          { value: 'digital', label: '디지털', description: '계산된 상태 알람' },
          { value: 'script', label: '스크립트', description: '사용자 정의 조건 알람' }
        ];
      default:
        return [];
    }
  };

  // 타겟 타입 변경 핸들러 - 핵심 수정사항
  const handleTargetTypeChange = (targetType: string) => {
    const allowedTypes = getAllowedAlarmTypes(targetType);
    const defaultAlarmType = allowedTypes.length > 0 ? allowedTypes[0].value : 'digital';
    
    setFormData(prev => ({
      ...prev,
      target_type: targetType as any,
      selected_device_id: '',
      target_id: '',
      alarm_type: defaultAlarmType as any,
      // 임계값들 초기화
      high_high_limit: '',
      high_limit: '',
      low_limit: '',
      low_low_limit: '',
      deadband: '2.0',
      trigger_condition: '',
      condition_script: ''
    }));
  };

  // 편집 모드일 때 기존 데이터로 폼 초기화
  useEffect(() => {
    if (mode === 'edit' && rule) {
      setFormData({
        name: rule.name,
        description: rule.description || '',
        target_type: rule.target_type as any,
        target_id: rule.target_id?.toString() || '',
        selected_device_id: '', // 편집 시에는 계층적 선택 비활성화
        target_group: rule.target_group || '',
        alarm_type: rule.alarm_type as any,
        high_high_limit: rule.high_high_limit?.toString() || '',
        high_limit: rule.high_limit?.toString() || '',
        low_limit: rule.low_limit?.toString() || '',
        low_low_limit: rule.low_low_limit?.toString() || '',
        deadband: rule.deadband?.toString() || '2.0',
        rate_of_change: rule.rate_of_change?.toString() || '',
        trigger_condition: rule.trigger_condition || '',
        condition_script: rule.condition_script || '',
        message_template: rule.message_template || '',
        severity: rule.severity as any,
        priority: rule.priority || 100,
        auto_acknowledge: rule.auto_acknowledge || false,
        auto_clear: rule.auto_clear || true,
        notification_enabled: rule.notification_enabled || true,
        is_enabled: rule.is_enabled,
        escalation_enabled: rule.escalation_enabled || false,
        escalation_max_level: rule.escalation_max_level || 3,
        category: rule.category || '',
        tags: AlarmApiService.formatTags(rule.tags || [])
      });
    } else if (mode === 'create') {
      resetForm();
    }
  }, [mode, rule, isOpen]);

  const resetForm = () => {
    setFormData({
      name: '',
      description: '',
      target_type: 'data_point',
      target_id: '',
      selected_device_id: '',
      target_group: '',
      alarm_type: 'analog', // data_point의 기본값
      high_high_limit: '',
      high_limit: '',
      low_limit: '',
      low_low_limit: '',
      deadband: '2.0',
      rate_of_change: '',
      trigger_condition: '',
      condition_script: '',
      message_template: '',
      severity: 'medium',
      priority: 100,
      auto_acknowledge: false,
      auto_clear: true,
      notification_enabled: true,
      is_enabled: true,
      escalation_enabled: false,
      escalation_max_level: 3,
      category: '',
      tags: []
    });
  };

  // 디바이스 선택 변경
  const handleDeviceChange = (deviceId: string) => {
    setFormData(prev => ({
      ...prev,
      selected_device_id: deviceId,
      target_id: ''
    }));
  };

  // 디바이스 옵션 생성
  const getDeviceOptions = () => {
    if (!Array.isArray(devices)) return [];
    
    return devices
      .sort((a, b) => a.id - b.id)
      .map(device => ({
        value: device.id.toString(),
        label: `[${device.id}] ${device.name} (${device.device_type || 'Unknown'})`,
        extra: device.site_name ? ` - ${device.site_name}` : ''
      }));
  };

  // 필터링된 데이터포인트
  const getFilteredDataPoints = () => {
    if (!formData.selected_device_id || !Array.isArray(dataPoints)) return [];
    
    const deviceId = parseInt(formData.selected_device_id);
    return dataPoints
      .filter(dp => dp.device_id === deviceId)
      .sort((a, b) => a.id - b.id);
  };

  // 데이터포인트 옵션 생성
  const getDataPointOptions = () => {
    const filteredPoints = getFilteredDataPoints();
    
    return filteredPoints.map(dp => ({
      value: dp.id.toString(),
      label: `[${dp.id}] ${dp.name}`,
      extra: dp.unit ? ` (${dp.unit})` : (dp.data_type ? ` (${dp.data_type})` : ''),
      address: dp.address ? ` - ${dp.address}` : ''
    }));
  };

  // 전체 타겟 옵션 (편집 모드용)
  const getAllTargetOptions = () => {
    switch (formData.target_type) {
      case 'data_point':
        if (!Array.isArray(dataPoints)) return [];
        return dataPoints
          .sort((a, b) => a.id - b.id)
          .map(dp => ({
            value: dp.id.toString(),
            label: `[${dp.id}] ${dp.name} (${getDeviceName(dp.device_id)})`,
            extra: dp.unit ? ` - ${dp.unit}` : ''
          }));
      case 'device':
        return getDeviceOptions();
      case 'virtual_point':
        return [{ value: '', label: '가상포인트 API 구현 필요', extra: '' }];
      default:
        return [];
    }
  };

  const getDeviceName = (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    return device ? device.name : `Device #${deviceId}`;
  };

  const getSelectedDeviceName = () => {
    if (!formData.selected_device_id) return '';
    const device = devices.find(d => d.id === parseInt(formData.selected_device_id));
    return device ? device.name : '';
  };

  // 태그 관리
  const addTag = (tag: string) => {
    if (tag.trim() && !formData.tags.includes(tag.trim())) {
      setFormData(prev => ({
        ...prev,
        tags: [...prev.tags, tag.trim()]
      }));
    }
  };

  const removeTag = (tagToRemove: string) => {
    setFormData(prev => ({
      ...prev,
      tags: prev.tags.filter(tag => tag !== tagToRemove)
    }));
  };

  // 저장 핸들러
  const handleSubmit = async () => {
    try {
      setLoading(true);
      
      const submitData = {
        name: formData.name,
        description: formData.description,
        target_type: formData.target_type,
        target_id: formData.target_id ? parseInt(formData.target_id) : undefined,
        target_group: formData.target_group || undefined,
        alarm_type: formData.alarm_type,
        
        // 아날로그 알람에만 임계값 포함
        ...(formData.alarm_type === 'analog' && {
          high_high_limit: formData.high_high_limit ? parseFloat(formData.high_high_limit) : undefined,
          high_limit: formData.high_limit ? parseFloat(formData.high_limit) : undefined,
          low_limit: formData.low_limit ? parseFloat(formData.low_limit) : undefined,
          low_low_limit: formData.low_low_limit ? parseFloat(formData.low_low_limit) : undefined,
          deadband: formData.deadband ? parseFloat(formData.deadband) : undefined,
          rate_of_change: formData.rate_of_change ? parseFloat(formData.rate_of_change) : undefined,
        }),
        
        // 디지털 알람에만 트리거 조건 포함
        ...(formData.alarm_type === 'digital' && {
          trigger_condition: formData.trigger_condition || undefined,
        }),
        
        // 스크립트 알람에만 스크립트 포함
        ...(formData.alarm_type === 'script' && {
          condition_script: formData.condition_script || undefined,
        }),
        
        message_template: formData.message_template || `${formData.name} 알람이 발생했습니다`,
        severity: formData.severity,
        priority: formData.priority,
        auto_acknowledge: formData.auto_acknowledge,
        auto_clear: formData.auto_clear,
        notification_enabled: formData.notification_enabled,
        is_enabled: formData.is_enabled,
        escalation_enabled: formData.escalation_enabled,
        escalation_max_level: formData.escalation_max_level,
        category: formData.category || undefined,
        tags: formData.tags.length > 0 ? formData.tags : undefined
      };
      
      let response;
      if (mode === 'create') {
        response = await AlarmApiService.createAlarmRule(submitData as AlarmRuleCreateData);
      } else {
        response = await AlarmApiService.updateAlarmRule(rule!.id, submitData);
      }
      
      if (response.success) {
        onSave();
        onClose();
        alert(mode === 'create' ? '알람 규칙이 성공적으로 생성되었습니다.' : '알람 규칙이 성공적으로 수정되었습니다.');
      } else {
        throw new Error(response.message || `알람 규칙 ${mode === 'create' ? '생성' : '수정'}에 실패했습니다.`);
      }
    } catch (error) {
      console.error(`알람 규칙 ${mode === 'create' ? '생성' : '수정'} 실패:`, error);
      alert(error instanceof Error ? error.message : `알람 규칙 ${mode === 'create' ? '생성' : '수정'}에 실패했습니다.`);
    } finally {
      setLoading(false);
    }
  };

  if (!isOpen) return null;

  const allowedAlarmTypes = getAllowedAlarmTypes(formData.target_type);

  return (
    <div className="modal-overlay">
      <div className="modal">
        <div className="modal-header">
          <h2 className="modal-title">
            {mode === 'create' ? '새 알람 규칙 추가' : `알람 규칙 수정: ${rule?.name}`}
          </h2>
          <button className="close-button" onClick={onClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        <div className="modal-content">
          {/* 기본 정보 */}
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
                <label className="form-label">카테고리</label>
                <select
                  className="form-select"
                  value={formData.category}
                  onChange={(e) => setFormData(prev => ({ ...prev, category: e.target.value }))}
                >
                  <option value="">선택하세요</option>
                  {ALARM_CONSTANTS.DEFAULT_CATEGORIES.map(category => (
                    <option key={category} value={category}>
                      {AlarmApiService.getCategoryDisplayName(category)}
                    </option>
                  ))}
                </select>
              </div>
            </div>

            <div className="form-row">
              <div className="form-group">
                <label className="form-label">태그</label>
                <div className="tags-input">
                  <div className="tags-list">
                    {formData.tags.map(tag => (
                      <span key={tag} className="tag-item">
                        {tag}
                        <button type="button" onClick={() => removeTag(tag)}>×</button>
                      </span>
                    ))}
                  </div>
                  <input
                    type="text"
                    className="form-input"
                    placeholder="태그를 입력하고 Enter를 누르세요"
                    onKeyPress={(e) => {
                      if (e.key === 'Enter') {
                        e.preventDefault();
                        addTag(e.currentTarget.value);
                        e.currentTarget.value = '';
                      }
                    }}
                  />
                </div>
              </div>
            </div>
          </div>

          {/* 타겟 설정 */}
          <div className="form-section">
            <div className="section-title">타겟 설정</div>
            
            <div className="form-row">
              <div className="form-group">
                <label className="form-label">타겟 타입 *</label>
                <select
                  className="form-select"
                  value={formData.target_type}
                  onChange={(e) => handleTargetTypeChange(e.target.value)}
                  disabled={mode === 'edit'} // 편집 시 타겟 타입 변경 불가
                >
                  <option value="data_point">데이터포인트</option>
                  <option value="device">디바이스</option>
                  <option value="virtual_point">가상포인트</option>
                </select>
                {mode === 'edit' && (
                  <small className="form-help">편집 시 타겟 타입은 변경할 수 없습니다</small>
                )}
              </div>
            </div>

            {/* 생성 모드 - 계층적 선택 */}
            {mode === 'create' && formData.target_type === 'data_point' && (
              <>
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">디바이스 선택 *</label>
                    {loadingTargetData ? (
                      <div className="form-input loading-input">
                        <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                      </div>
                    ) : (
                      <select
                        className="form-select"
                        value={formData.selected_device_id}
                        onChange={(e) => handleDeviceChange(e.target.value)}
                      >
                        <option value="">디바이스를 선택하세요</option>
                        {getDeviceOptions().map(option => (
                          <option key={option.value} value={option.value}>
                            {option.label}{option.extra}
                          </option>
                        ))}
                      </select>
                    )}
                  </div>
                </div>

                {formData.selected_device_id && (
                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">데이터포인트 선택 *</label>
                      <select
                        className="form-select"
                        value={formData.target_id}
                        onChange={(e) => setFormData(prev => ({ ...prev, target_id: e.target.value }))}
                      >
                        <option value="">데이터포인트를 선택하세요</option>
                        {getDataPointOptions().map(option => (
                          <option key={option.value} value={option.value}>
                            {option.label}{option.extra}{option.address}
                          </option>
                        ))}
                      </select>
                      <small className="form-help">
                        {getSelectedDeviceName()}의 데이터포인트들 (ID 순 정렬)
                      </small>
                    </div>
                  </div>
                )}
              </>
            )}

            {/* 생성 모드 - 디바이스 직접 선택 또는 편집 모드 */}
            {((mode === 'create' && formData.target_type !== 'data_point') || mode === 'edit') && (
              <div className="form-row">
                <div className="form-group">
                  <label className="form-label">
                    {formData.target_type === 'data_point' ? '데이터포인트' : 
                     formData.target_type === 'device' ? '디바이스' : '가상포인트'} 선택 *
                  </label>
                  {loadingTargetData ? (
                    <div className="form-input loading-input">
                      <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                    </div>
                  ) : (
                    <select
                      className="form-select"
                      value={formData.target_id}
                      onChange={(e) => setFormData(prev => ({ ...prev, target_id: e.target.value }))}
                    >
                      <option value="">선택하세요</option>
                      {getAllTargetOptions().map(option => (
                        <option key={option.value} value={option.value}>
                          {option.label}{option.extra}
                        </option>
                      ))}
                    </select>
                  )}
                </div>
              </div>
            )}

            <div className="form-row">
              <div className="form-group">
                <label className="form-label">타겟 그룹</label>
                <input
                  type="text"
                  className="form-input"
                  value={formData.target_group}
                  onChange={(e) => setFormData(prev => ({ ...prev, target_group: e.target.value }))}
                  placeholder="타겟 그룹 (선택사항)"
                />
              </div>
            </div>
          </div>

          {/* 조건 설정 */}
          <div className="form-section">
            <div className="section-title">조건 설정</div>
            
            <div className="form-row">
              <div className="form-group">
                <label className="form-label">알람 타입 *</label>
                <select
                  className="form-select"
                  value={formData.alarm_type}
                  onChange={(e) => setFormData(prev => ({ ...prev, alarm_type: e.target.value as any }))}
                  disabled={mode === 'edit'} // 편집 시 알람 타입 변경 불가
                >
                  {allowedAlarmTypes.map(type => (
                    <option key={type.value} value={type.value}>
                      {type.label}
                    </option>
                  ))}
                </select>
                <small className="form-help">
                  {allowedAlarmTypes.find(t => t.value === formData.alarm_type)?.description || ''}
                  {mode === 'edit' && ' (편집 시 변경 불가)'}
                </small>
              </div>
              
              <div className="form-group">
                <label className="form-label">심각도 *</label>
                <select
                  className="form-select"
                  value={formData.severity}
                  onChange={(e) => setFormData(prev => ({ ...prev, severity: e.target.value as any }))}
                >
                  <option value="critical">Critical</option>
                  <option value="high">High</option>
                  <option value="medium">Medium</option>
                  <option value="low">Low</option>
                  <option value="info">Info</option>
                </select>
              </div>
            </div>

            {/* 아날로그 알람 임계값 - 조건부 렌더링 */}
            {formData.alarm_type === 'analog' && (
              <div className="form-subsection">
                <div className="subsection-title">아날로그 임계값</div>
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">HH (High High)</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.high_high_limit}
                      onChange={(e) => setFormData(prev => ({ ...prev, high_high_limit: e.target.value }))}
                      placeholder="최고 상한값"
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">H (High)</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.high_limit}
                      onChange={(e) => setFormData(prev => ({ ...prev, high_limit: e.target.value }))}
                      placeholder="상한값"
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">L (Low)</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.low_limit}
                      onChange={(e) => setFormData(prev => ({ ...prev, low_limit: e.target.value }))}
                      placeholder="하한값"
                    />
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">LL (Low Low)</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.low_low_limit}
                      onChange={(e) => setFormData(prev => ({ ...prev, low_low_limit: e.target.value }))}
                      placeholder="최저 하한값"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">데드밴드</label>
                    <input
                      type="number"
                      step="0.1"
                      className="form-input"
                      value={formData.deadband}
                      onChange={(e) => setFormData(prev => ({ ...prev, deadband: e.target.value }))}
                      placeholder="데드밴드 값 (0은 비활성화)"
                      min="0"
                    />
                    <small className="form-help">알람 chattering 방지용 데드밴드 (0 = 비활성화)</small>
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">변화율</label>
                    <input
                      type="number"
                      step="0.01"
                      className="form-input"
                      value={formData.rate_of_change}
                      onChange={(e) => setFormData(prev => ({ ...prev, rate_of_change: e.target.value }))}
                      placeholder="변화율 임계값"
                    />
                  </div>
                </div>
              </div>
            )}

            {/* 디지털 알람 조건 */}
            {formData.alarm_type === 'digital' && (
              <div className="form-subsection">
                <div className="subsection-title">디지털 조건</div>
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">트리거 조건</label>
                    <select
                      className="form-select"
                      value={formData.trigger_condition}
                      onChange={(e) => setFormData(prev => ({ ...prev, trigger_condition: e.target.value }))}
                    >
                      <option value="">선택하세요</option>
                      <option value="on_true">True가 될 때</option>
                      <option value="on_false">False가 될 때</option>
                      <option value="on_change">값이 변경될 때</option>
                      {formData.target_type === 'device' && (
                        <>
                          <option value="connection_lost">연결 끊김</option>
                          <option value="communication_error">통신 오류</option>
                          <option value="device_error">디바이스 에러</option>
                        </>
                      )}
                    </select>
                    <small className="form-help">
                      {formData.target_type === 'device' ? 
                        '디바이스 상태 변화 또는 통신 문제를 감지합니다' : 
                        '데이터포인트의 디지털 값 변화를 감지합니다'}
                    </small>
                  </div>
                </div>
              </div>
            )}

            {/* 스크립트 알람 조건 */}
            {formData.alarm_type === 'script' && (
              <div className="form-subsection">
                <div className="subsection-title">스크립트 조건</div>
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">조건 스크립트</label>
                    <textarea
                      className="form-textarea"
                      value={formData.condition_script}
                      onChange={(e) => setFormData(prev => ({ ...prev, condition_script: e.target.value }))}
                      placeholder={formData.target_type === 'device' ? 
                        "// 디바이스 복합 조건 예시\nreturn device.status === 'error' && device.errorCount > 5;" :
                        "// JavaScript 조건식을 입력하세요\nreturn value > 100 && previousValue < 90;"}
                      rows={4}
                    />
                    <small className="form-help">
                      {formData.target_type === 'device' ? 
                        '디바이스 객체와 관련 데이터포인트들을 활용한 복합 조건을 작성하세요' :
                        'value, previousValue, timestamp 등의 변수를 사용할 수 있습니다'}
                    </small>
                  </div>
                </div>
              </div>
            )}

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

          {/* 동작 설정 */}
          <div className="form-section">
            <div className="section-title">동작 설정</div>
            
            <div className="form-row">
              <div className="form-group">
                <label className="form-label">우선순위</label>
                <input
                  type="number"
                  className="form-input"
                  value={formData.priority}
                  onChange={(e) => setFormData(prev => ({ ...prev, priority: parseInt(e.target.value) || 100 }))}
                  min="1"
                  max="999"
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

            <div className="checkbox-group">
              <input
                type="checkbox"
                className="checkbox"
                checked={formData.escalation_enabled}
                onChange={(e) => setFormData(prev => ({ ...prev, escalation_enabled: e.target.checked }))}
              />
              <label className="checkbox-label">에스컬레이션 활성화</label>
            </div>

            {formData.escalation_enabled && (
              <div className="form-row">
                <div className="form-group">
                  <label className="form-label">최대 에스컬레이션 레벨</label>
                  <input
                    type="number"
                    className="form-input"
                    value={formData.escalation_max_level}
                    onChange={(e) => setFormData(prev => ({ ...prev, escalation_max_level: parseInt(e.target.value) || 3 }))}
                    min="1"
                    max="10"
                  />
                </div>
              </div>
            )}
          </div>
        </div>

        <div className="modal-footer">
          <button
            className="btn btn-outline"
            onClick={onClose}
            disabled={loading}
          >
            취소
          </button>
          <button
            className="btn btn-primary"
            onClick={handleSubmit}
            disabled={!formData.name || !formData.target_id || loading}
          >
            {loading ? <i className="fas fa-spinner fa-spin"></i> : 
             mode === 'create' ? <i className="fas fa-plus"></i> : <i className="fas fa-save"></i>}
            {mode === 'create' ? '생성' : '수정'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default AlarmCreateEditModal;