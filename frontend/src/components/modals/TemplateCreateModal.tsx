// ============================================================================
// frontend/src/components/modals/TemplateCreateModal.tsx
// 알람 템플릿 생성 모달 - 단일 페이지 (Single Page) 리팩토링
// ============================================================================

import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';

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
    const { t } = useTranslation(['deviceTemplates', 'common']);

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

    if (!formData.name.trim()) newErrors.name = 'Template name is required';
    if (!formData.description.trim()) newErrors.description = 'Description is required';

    // 조건 검증
    if (formData.condition_type === 'threshold') {
      if (formData.default_config.threshold === undefined) newErrors.threshold = 'Threshold is required';
    }
    if (formData.condition_type === 'range') {
      if (formData.default_config.high_limit === undefined) newErrors.high_limit = 'High limit is required';
      if (formData.default_config.low_limit === undefined) newErrors.low_limit = 'Low limit is required';
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
      alert('Template created successfully.');
    } catch (error) {
      console.error(error);
      alert('Template creation failed');
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
        title: 'High Temperature Alert',
        desc: 'Standard high temperature alarm template',
        data: {
          category: 'temperature',
          condition_type: 'threshold',
          condition_template: 'value > {threshold}',
          default_config: { threshold: 80, deadband: 2 },
          message_template: '{device_name} temperature exceeded {threshold}°C (Current: {value}°C)'
        }
      },
      {
        id: 'pressure_range',
        icon: '⚖️',
        title: 'Pressure Out of Range',
        desc: 'Detects deviation from normal range (Low~High)',
        data: {
          category: 'pressure',
          condition_type: 'range',
          condition_template: 'value < {low_limit} || value > {high_limit}',
          default_config: { high_limit: 80, low_limit: 20 },
          message_template: '{device_name} pressure out of range! (Current: {value} bar)'
        }
      },
      {
        id: 'pump_status',
        icon: '🔄',
        title: 'Pump Status Monitoring',
        desc: 'Monitors operational status',
        data: {
          category: 'digital',
          condition_type: 'pattern',
          condition_template: 'state == {trigger_state} FOR {hold_time}ms',
          default_config: { trigger_state: 'on_false', hold_time: 5000 },
          message_template: '{device_name} pump has been stopped for more than 5 seconds'
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
          <h2 className="modal-title">{t('labels.createNewTemplate', {ns: 'deviceTemplates'})}</h2>
          <button className="close-button" onClick={onClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        <div className="modal-content">
          {/* 1. 스마트 프리셋 섹션 */}
          <div className="form-section">
            <div className="section-title">
              <i className="fas fa-magic"></i> Quick Start (Template Presets)
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
            {/* 왼쪽 컬럼: Basic Info & 알림 설정 */}
            <div className="left-column">
              <div className="form-section">
                <div className="section-title">{t('detailModal.basic', {ns: 'deviceTemplates'})}</div>

                <div className="form-group">
                  <label className="form-label">Template Name *</label>
                  <input
                    type="text"
                    className="form-input"
                    value={formData.name}
                    onChange={(e) => updateField('name', e.target.value)}
                    placeholder="e.g. High Temp Alert Template"
                  />
                  {errors.name && <div style={{ color: '#ef4444', fontSize: '12px' }}>{errors.name}</div>}
                </div>

                <div className="form-group" style={{ marginTop: '16px' }}>
                  <label className="form-label">Description *</label>
                  <textarea
                    className="form-textarea"
                    value={formData.description}
                    onChange={(e) => updateField('description', e.target.value)}
                    placeholder="Template description"
                    rows={2}
                  />
                </div>

                <div className="form-row" style={{ marginTop: '16px' }}>
                  <div className="form-group">
                    <label className="form-label">{t('createModal.category', {ns: 'deviceTemplates'})}</label>
                    <select
                      className="form-select"
                      value={formData.category}
                      onChange={(e) => updateField('category', e.target.value)}
                    >
                      <option value="temperature">{t('labels.temperature', {ns: 'deviceTemplates'})}</option>
                      <option value="pressure">{t('labels.pressure', {ns: 'deviceTemplates'})}</option>
                      <option value="flow">{t('labels.flowRate', {ns: 'deviceTemplates'})}</option>
                      <option value="digital">{t('labels.digital', {ns: 'deviceTemplates'})}</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label className="form-label">{t('labels.severity', {ns: 'deviceTemplates'})}</label>
                    <select
                      className="form-select"
                      value={formData.severity}
                      onChange={(e) => updateField('severity', e.target.value)}
                    >
                      <option value="low">{t('labels.low', {ns: 'deviceTemplates'})}</option>
                      <option value="medium">{t('labels.medium', {ns: 'deviceTemplates'})}</option>
                      <option value="high">{t('labels.high', {ns: 'deviceTemplates'})}</option>
                      <option value="critical">{t('labels.critical', {ns: 'deviceTemplates'})}</option>
                    </select>
                  </div>
                </div>
              </div>

              <div className="form-section">
                <div className="section-title">{t('labels.notificationOptions', {ns: 'deviceTemplates'})}</div>
                <div className="checkbox-group">
                  <input type="checkbox" checked={formData.notification_enabled} onChange={(e) => updateField('notification_enabled', e.target.checked)} />
                  <label>{t('labels.enableNotifications', {ns: 'deviceTemplates'})}</label>
                </div>
                <div className="checkbox-group">
                  <input type="checkbox" checked={formData.auto_clear} onChange={(e) => updateField('auto_clear', e.target.checked)} />
                  <label>{t('labels.autoClear', {ns: 'deviceTemplates'})}</label>
                </div>
              </div>
            </div>

            {/* 오른쪽 컬럼: 로직 빌더 */}
            <div className="right-column">
              <div className="form-section">
                <div className="section-title">
                  <i className="fas fa-code-branch"></i> Logic Builder
                </div>

                <div className="form-group">
                  <label className="form-label">{t('labels.conditionType', {ns: 'deviceTemplates'})}</label>
                  <div className="btn-group" style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
                    {['threshold', 'range', 'pattern'].map(type => (
                      <button
                        key={type}
                        className={`btn ${formData.condition_type === type ? 'btn-primary' : 'btn-outline'}`}
                        onClick={() => handleConditionTypeChange(type)}
                        type="button"
                      >
                        {type === 'threshold' && 'Single Value'}
                        {type === 'range' && 'Range'}
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
                  <label className="form-label">{t('labels.messageTemplate', {ns: 'deviceTemplates'})}</label>
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
          <button className="btn btn-secondary" onClick={onClose}>{t('cancel', {ns: 'common'})}</button>
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