// ============================================================================
// frontend/src/components/modals/TemplateCreateModal.tsx
// 알람 템플릿 생성 모달 - 단일 페이지 (Single Page) 리팩토링
// ============================================================================

import React, { useState, useEffect } from 'react';

export interface CreateTemplateRequest {
  name: string;
  description: string;
  category: string;
  template_type: 'simple' | 'advanced' | 'script';
  condition_type: 'threshold' | 'range' | 'pattern' | 'script';
  condition_template: string;
  default_config: any;
  severity: 'low' | 'medium' | 'high' | 'critical';
  message_template: string;
  applicable_data_types: string[];
  notification_enabled: boolean;
  email_notification: boolean;
  sms_notification: boolean;
  auto_acknowledge: boolean;
  auto_clear: boolean;
}

export interface TemplateCreateModalProps {
  isOpen: boolean;
  onClose: () => void;
  onCreate: (templateData: CreateTemplateRequest) => Promise<void>;
  loading?: boolean;
}

const TemplateCreateModal: React.FC<TemplateCreateModalProps> = ({
  isOpen,
  onClose,
  onCreate,
  loading = false
}) => {
  // 폼 상태
  const [formData, setFormData] = useState<CreateTemplateRequest>({
    name: '',
    description: '',
    category: 'temperature',
    template_type: 'simple',
    condition_type: 'threshold',
    condition_template: '',
    default_config: {},
    severity: 'medium',
    message_template: '',
    applicable_data_types: ['float'],
    notification_enabled: true,
    email_notification: false,
    sms_notification: false,
    auto_acknowledge: false,
    auto_clear: true
  });

  const [errors, setErrors] = useState<Record<string, string>>({});

  // 모달이 열릴 때마다 초기화
  useEffect(() => {
    if (isOpen) {
      setFormData({
        name: '',
        description: '',
        category: 'temperature',
        template_type: 'simple',
        condition_type: 'threshold',
        condition_template: '',
        default_config: {},
        severity: 'medium',
        message_template: '',
        applicable_data_types: ['float'],
        notification_enabled: true,
        email_notification: false,
        sms_notification: false,
        auto_acknowledge: false,
        auto_clear: true
      });
      setErrors({});
    }
  }, [isOpen]);

  if (!isOpen) return null;

  // 폼 업데이트
  const updateField = (field: keyof CreateTemplateRequest, value: any) => {
    setFormData(prev => ({
      ...prev,
      [field]: value
    }));
    // 에러 클리어
    if (errors[field]) {
      setErrors(prev => ({
        ...prev,
        [field]: ''
      }));
    }
  };

  // 설정 업데이트
  const updateConfig = (key: string, value: any) => {
    setFormData(prev => ({
      ...prev,
      default_config: {
        ...prev.default_config,
        [key]: value
      }
    }));
  };

  // 조건 타입 변경 시 설정 초기화
  const handleConditionTypeChange = (newType: string) => {
    updateField('condition_type', newType);

    // 기본 설정 및 condition_template 초기화
    let defaultConfig = {};
    let conditionTemplate = '';

    switch (newType) {
      case 'threshold':
        defaultConfig = { threshold: 80, deadband: 2 };
        conditionTemplate = 'value > {threshold}';
        break;
      case 'range':
        defaultConfig = {
          high_limit: 80,
          low_limit: 20,
          deadband: 2
        };
        conditionTemplate = 'value < {low_limit} || value > {high_limit}';
        break;
      case 'pattern':
        defaultConfig = { trigger_state: 'on_true', hold_time: 1000 };
        conditionTemplate = 'state == {trigger_state} FOR {hold_time}ms';
        break;
      case 'script':
        defaultConfig = { script_condition: 'return value > 80;' };
        conditionTemplate = '{script_condition}';
        break;
    }
    updateField('default_config', defaultConfig);
    updateField('condition_template', conditionTemplate);
  };

  // 유효성 검사
  const validateForm = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!formData.name.trim()) newErrors.name = '템플릿 이름은 필수입니다';
    if (!formData.description.trim()) newErrors.description = '설명은 필수입니다';

    // 조건 검증
    if (formData.condition_type === 'threshold') {
      if (formData.default_config.threshold === undefined) newErrors.threshold = '임계값은 필수입니다';
    }
    if (formData.condition_type === 'range') {
      if (formData.default_config.high_limit === undefined) newErrors.high_limit = '상한값은 필수입니다';
      if (formData.default_config.low_limit === undefined) newErrors.low_limit = '하한값은 필수입니다';
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  // 생성 버튼 클릭
  const handleCreateClick = async () => {
    if (!validateForm()) return;
    try {
      await onCreate(formData);
      onClose();
      alert('템플릿이 성공적으로 생성되었습니다.');
    } catch (error) {
      console.error(error);
      alert('템플릿 생성 실패');
    }
  };

  // 프리셋 템플릿 정의
  const PRESET_TEMPLATES: Array<{
    id: string;
    icon: string;
    title: string;
    desc: string;
    data: Partial<CreateTemplateRequest>;
  }> = [
      {
        id: 'temp_warning',
        icon: '🔥',
        title: '고온 경고',
        desc: '표준 고온 알람 템플릿',
        data: {
          category: 'temperature',
          condition_type: 'threshold',
          condition_template: 'value > {threshold}',
          default_config: { threshold: 80, deadband: 2 },
          message_template: '{device_name}의 온도가 {threshold}°C를 초과했습니다 (현재: {value}°C)'
        }
      },
      {
        id: 'pressure_range',
        icon: '⚖️',
        title: '압력 범위 이탈',
        desc: '정상 범위(Low~High) 이탈 감지',
        data: {
          category: 'pressure',
          condition_type: 'range',
          condition_template: 'value < {low_limit} || value > {high_limit}',
          default_config: { high_limit: 80, low_limit: 20 },
          message_template: '{device_name} 압력 범위 이탈! (현재: {value} bar)'
        }
      },
      {
        id: 'pump_status',
        icon: '🔄',
        title: '펌프 상태 모니터링',
        desc: '가동 상태 모니터링',
        data: {
          category: 'digital',
          condition_type: 'pattern',
          condition_template: 'state == {trigger_state} FOR {hold_time}ms',
          default_config: { trigger_state: 'on_false', hold_time: 5000 },
          message_template: '{device_name} 펌프가 5초 이상 정지 상태입니다'
        }
      }
    ];

  const handlePresetSelect = (preset: typeof PRESET_TEMPLATES[0]) => {
    setFormData(prev => ({
      ...prev,
      ...preset.data,
      name: prev.name || preset.title,
      description: prev.description || preset.desc
    }));
  };

  const insertVariable = (variable: string) => {
    const templateWithVar = formData.condition_template + ` {${variable}}`;
    updateField('condition_template', templateWithVar);
  };

  return (
    <div className="modal-overlay">
      <div className="modal modal-container large">
        <div className="modal-header">
          <h2 className="modal-title">새 템플릿 생성</h2>
          <button className="close-button" onClick={onClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        <div className="modal-content">
          {/* 1. 스마트 프리셋 섹션 */}
          <div className="form-section">
            <div className="section-title">
              <i className="fas fa-magic"></i> 빠른 시작 (템플릿 프리셋)
            </div>
            <div className="preset-container">
              {PRESET_TEMPLATES.map(preset => (
                <div
                  key={preset.id}
                  className="preset-card"
                  onClick={() => handlePresetSelect(preset)}
                >
                  <div className="preset-icon">{preset.icon}</div>
                  <div className="preset-title">{preset.title}</div>
                  <div className="preset-desc">{preset.desc}</div>
                </div>
              ))}
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '32px' }}>
            {/* 왼쪽 컬럼: 기본 정보 & 알림 설정 */}
            <div className="left-column">
              <div className="form-section">
                <div className="section-title">기본 정보</div>

                <div className="form-group">
                  <label className="form-label">템플릿 이름 *</label>
                  <input
                    type="text"
                    className="form-input"
                    value={formData.name}
                    onChange={(e) => updateField('name', e.target.value)}
                    placeholder="예: 고온 경고 템플릿"
                  />
                  {errors.name && <div style={{ color: '#ef4444', fontSize: '12px' }}>{errors.name}</div>}
                </div>

                <div className="form-group" style={{ marginTop: '16px' }}>
                  <label className="form-label">설명 *</label>
                  <textarea
                    className="form-textarea"
                    value={formData.description}
                    onChange={(e) => updateField('description', e.target.value)}
                    placeholder="템플릿 설명"
                    rows={2}
                  />
                </div>

                <div className="form-row" style={{ marginTop: '16px' }}>
                  <div className="form-group">
                    <label className="form-label">카테고리</label>
                    <select
                      className="form-select"
                      value={formData.category}
                      onChange={(e) => updateField('category', e.target.value)}
                    >
                      <option value="temperature">온도</option>
                      <option value="pressure">압력</option>
                      <option value="flow">유량</option>
                      <option value="digital">디지털</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label className="form-label">심각도</label>
                    <select
                      className="form-select"
                      value={formData.severity}
                      onChange={(e) => updateField('severity', e.target.value)}
                    >
                      <option value="low">Low</option>
                      <option value="medium">Medium</option>
                      <option value="high">High</option>
                      <option value="critical">Critical</option>
                    </select>
                  </div>
                </div>
              </div>

              <div className="form-section">
                <div className="section-title">알림 옵션</div>
                <div className="checkbox-group">
                  <input type="checkbox" checked={formData.notification_enabled} onChange={(e) => updateField('notification_enabled', e.target.checked)} />
                  <label>알림 활성화</label>
                </div>
                <div className="checkbox-group">
                  <input type="checkbox" checked={formData.auto_clear} onChange={(e) => updateField('auto_clear', e.target.checked)} />
                  <label>자동 해제</label>
                </div>
              </div>
            </div>

            {/* 오른쪽 컬럼: 로직 빌더 */}
            <div className="right-column">
              <div className="form-section">
                <div className="section-title">
                  <i className="fas fa-code-branch"></i> 로직 빌더 (Logic Builder)
                </div>

                <div className="form-group">
                  <label className="form-label">조건 타입</label>
                  <div className="btn-group" style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
                    {['threshold', 'range', 'pattern'].map(type => (
                      <button
                        key={type}
                        className={`btn ${formData.condition_type === type ? 'btn-primary' : 'btn-outline'}`}
                        onClick={() => handleConditionTypeChange(type)}
                        type="button"
                      >
                        {type === 'threshold' && '단일값'}
                        {type === 'range' && '범위'}
                        {type === 'pattern' && '패턴'}
                      </button>
                    ))}
                  </div>
                </div>

                <div className="form-group">
                  <label className="form-label">조건식 (자동 완성)</label>
                  {/* 문장형 빌더 도우미 */}
                  <div style={{ display: 'flex', gap: '8px', marginBottom: '8px', flexWrap: 'wrap' }}>
                    {['value', 'threshold', 'high_limit', 'low_limit', '>', '<', '==', 'AND', 'OR'].map(token => (
                      <span
                        key={token}
                        className="tag-badge tag-color-1"
                        style={{ cursor: 'pointer', padding: '4px 8px', borderRadius: '4px' }}
                        onClick={() => insertVariable(token)}
                      >
                        {token}
                      </span>
                    ))}
                  </div>
                  <input
                    type="text"
                    className="form-input"
                    value={formData.condition_template}
                    onChange={(e) => updateField('condition_template', e.target.value)}
                    placeholder="상단 칩을 클릭하여 조건식 완성"
                    style={{ fontFamily: 'monospace', fontWeight: 'bold', color: '#2563eb' }}
                  />
                  <small className="form-help">예: value {'>'} {'{threshold}'}</small>
                </div>

                {/* 동적 설정 필드 */}
                <div className="form-subsection" style={{ background: '#f8fafc', padding: '16px', borderRadius: '8px', marginTop: '16px' }}>
                  {formData.condition_type === 'threshold' && (
                    <div className="form-group">
                      <label className="form-label">기본 임계값</label>
                      <input
                        type="number"
                        className="form-input"
                        value={formData.default_config.threshold || ''}
                        onChange={(e) => updateConfig('threshold', parseFloat(e.target.value))}
                      />
                    </div>
                  )}
                  {formData.condition_type === 'range' && (
                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">상한값</label>
                        <input
                          type="number"
                          className="form-input"
                          value={formData.default_config.high_limit || ''}
                          onChange={(e) => updateConfig('high_limit', parseFloat(e.target.value))}
                        />
                      </div>
                      <div className="form-group">
                        <label className="form-label">하한값</label>
                        <input
                          type="number"
                          className="form-input"
                          value={formData.default_config.low_limit || ''}
                          onChange={(e) => updateConfig('low_limit', parseFloat(e.target.value))}
                        />
                      </div>
                    </div>
                  )}
                </div>

                <div className="form-group" style={{ marginTop: '24px' }}>
                  <label className="form-label">메시지 템플릿</label>
                  <input
                    type="text"
                    className="form-input"
                    value={formData.message_template}
                    onChange={(e) => updateField('message_template', e.target.value)}
                  />
                </div>
              </div>
            </div>
          </div>
        </div>

        <div className="modal-footer">
          <button className="btn btn-secondary" onClick={onClose}>취소</button>
          <button className="btn btn-primary" onClick={handleCreateClick} disabled={loading}>
            {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-check"></i>}
            템플릿 생성
          </button>
        </div>
      </div>
    </div>
  );
};

export default TemplateCreateModal;