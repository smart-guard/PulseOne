// ============================================================================
// frontend/src/components/modals/TemplateCreateModal.tsx
// 알람 템플릿 생성 모달 - 독립 컴포넌트
// ============================================================================

import React, { useState, useEffect } from 'react';

export interface CreateTemplateRequest {
  name: string;
  description: string;
  category: string;
  template_type: 'simple' | 'advanced' | 'script';
  condition_type: 'threshold' | 'range' | 'pattern' | 'script';
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

  const [activeStep, setActiveStep] = useState(1);
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
      setActiveStep(1);
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
    
    // 기본 설정 초기화
    let defaultConfig = {};
    switch (newType) {
      case 'threshold':
        defaultConfig = { threshold: 80, deadband: 2 };
        break;
      case 'range':
        defaultConfig = { 
          high_high_limit: 100, 
          high_limit: 80, 
          low_limit: 20, 
          low_low_limit: 0,
          deadband: 2 
        };
        break;
      case 'pattern':
        defaultConfig = { trigger_state: 'on_true', hold_time: 1000 };
        break;
      case 'script':
        defaultConfig = { script_condition: 'return value > 80;' };
        break;
    }
    updateField('default_config', defaultConfig);
  };

  // 유효성 검사
  const validateStep = (step: number): boolean => {
    const newErrors: Record<string, string> = {};

    if (step === 1) {
      if (!formData.name.trim()) newErrors.name = '템플릿 이름은 필수입니다';
      if (!formData.description.trim()) newErrors.description = '설명은 필수입니다';
      if (!formData.message_template.trim()) newErrors.message_template = '메시지 템플릿은 필수입니다';
    }

    if (step === 2) {
      if (formData.condition_type === 'threshold') {
        if (!formData.default_config.threshold) newErrors.threshold = '임계값은 필수입니다';
      }
      if (formData.condition_type === 'range') {
        if (!formData.default_config.high_limit) newErrors.high_limit = '상한값은 필수입니다';
        if (!formData.default_config.low_limit) newErrors.low_limit = '하한값은 필수입니다';
      }
      if (formData.condition_type === 'script') {
        if (!formData.default_config.script_condition) newErrors.script_condition = '스크립트 조건은 필수입니다';
      }
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  // 다음 단계
  const handleNext = () => {
    if (validateStep(activeStep)) {
      setActiveStep(prev => Math.min(prev + 1, 3));
    }
  };

  // 이전 단계
  const handlePrev = () => {
    setActiveStep(prev => Math.max(prev - 1, 1));
  };

  // 생성 처리
  const handleCreate = async () => {
    if (!validateStep(activeStep)) return;

    try {
      await onCreate(formData);
    } catch (error) {
      console.error('템플릿 생성 실패:', error);
    }
  };

  // 렌더링 헬퍼
  const renderStepIndicator = () => (
    <div style={{ display: 'flex', justifyContent: 'center', marginBottom: '32px' }}>
      {[1, 2, 3].map(step => (
        <div key={step} style={{ display: 'flex', alignItems: 'center' }}>
          <div style={{
            width: '32px',
            height: '32px',
            borderRadius: '50%',
            background: step <= activeStep ? '#3b82f6' : '#e5e7eb',
            color: step <= activeStep ? 'white' : '#9ca3af',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: '14px',
            fontWeight: '600'
          }}>
            {step}
          </div>
          {step < 3 && (
            <div style={{
              width: '40px',
              height: '2px',
              background: step < activeStep ? '#3b82f6' : '#e5e7eb',
              margin: '0 8px'
            }} />
          )}
        </div>
      ))}
    </div>
  );

  const renderStep1 = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      <h3 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: '600' }}>기본 정보</h3>
      
      <div>
        <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>
          템플릿 이름 *
        </label>
        <input
          type="text"
          value={formData.name}
          onChange={(e) => updateField('name', e.target.value)}
          placeholder="예: 고온 경고 템플릿"
          style={{
            width: '100%',
            padding: '12px',
            border: `1px solid ${errors.name ? '#ef4444' : '#e5e7eb'}`,
            borderRadius: '8px',
            fontSize: '14px'
          }}
        />
        {errors.name && <div style={{ color: '#ef4444', fontSize: '12px', marginTop: '4px' }}>{errors.name}</div>}
      </div>

      <div>
        <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>
          설명 *
        </label>
        <textarea
          value={formData.description}
          onChange={(e) => updateField('description', e.target.value)}
          placeholder="이 템플릿의 용도와 특징을 설명해주세요"
          rows={3}
          style={{
            width: '100%',
            padding: '12px',
            border: `1px solid ${errors.description ? '#ef4444' : '#e5e7eb'}`,
            borderRadius: '8px',
            fontSize: '14px',
            resize: 'vertical'
          }}
        />
        {errors.description && <div style={{ color: '#ef4444', fontSize: '12px', marginTop: '4px' }}>{errors.description}</div>}
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
        <div>
          <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>카테고리</label>
          <select
            value={formData.category}
            onChange={(e) => updateField('category', e.target.value)}
            style={{
              width: '100%',
              padding: '12px',
              border: '1px solid #e5e7eb',
              borderRadius: '8px',
              fontSize: '14px',
              background: 'white'
            }}
          >
            <option value="temperature">온도</option>
            <option value="pressure">압력</option>
            <option value="flow">유량</option>
            <option value="level">레벨</option>
            <option value="vibration">진동</option>
            <option value="digital">디지털</option>
            <option value="custom">커스텀</option>
          </select>
        </div>

        <div>
          <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>심각도</label>
          <select
            value={formData.severity}
            onChange={(e) => updateField('severity', e.target.value)}
            style={{
              width: '100%',
              padding: '12px',
              border: '1px solid #e5e7eb',
              borderRadius: '8px',
              fontSize: '14px',
              background: 'white'
            }}
          >
            <option value="low">낮음</option>
            <option value="medium">보통</option>
            <option value="high">높음</option>
            <option value="critical">심각</option>
          </select>
        </div>
      </div>

      <div>
        <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>
          메시지 템플릿 *
        </label>
        <input
          type="text"
          value={formData.message_template}
          onChange={(e) => updateField('message_template', e.target.value)}
          placeholder="{device_name} {point_name}이 {threshold}를 초과했습니다 (현재: {value})"
          style={{
            width: '100%',
            padding: '12px',
            border: `1px solid ${errors.message_template ? '#ef4444' : '#e5e7eb'}`,
            borderRadius: '8px',
            fontSize: '14px'
          }}
        />
        {errors.message_template && <div style={{ color: '#ef4444', fontSize: '12px', marginTop: '4px' }}>{errors.message_template}</div>}
        <div style={{ fontSize: '12px', color: '#6b7280', marginTop: '4px' }}>
          사용 가능한 변수: {'{device_name}, {point_name}, {value}, {threshold}, {unit}'}
        </div>
      </div>
    </div>
  );

  const renderStep2 = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      <h3 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: '600' }}>알람 조건 설정</h3>
      
      <div>
        <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>조건 타입</label>
        <select
          value={formData.condition_type}
          onChange={(e) => handleConditionTypeChange(e.target.value)}
          style={{
            width: '100%',
            padding: '12px',
            border: '1px solid #e5e7eb',
            borderRadius: '8px',
            fontSize: '14px',
            background: 'white'
          }}
        >
          <option value="threshold">임계값 (단일값 비교)</option>
          <option value="range">범위 (최대/최소값)</option>
          <option value="pattern">패턴 (디지털 상태)</option>
          <option value="script">스크립트 (사용자 정의)</option>
        </select>
      </div>

      {/* 조건별 설정 UI */}
      {formData.condition_type === 'threshold' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>임계값 *</label>
            <input
              type="number"
              value={formData.default_config.threshold || ''}
              onChange={(e) => updateConfig('threshold', parseFloat(e.target.value))}
              placeholder="80"
              style={{
                width: '100%',
                padding: '12px',
                border: `1px solid ${errors.threshold ? '#ef4444' : '#e5e7eb'}`,
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
            {errors.threshold && <div style={{ color: '#ef4444', fontSize: '12px', marginTop: '4px' }}>{errors.threshold}</div>}
          </div>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>데드밴드</label>
            <input
              type="number"
              value={formData.default_config.deadband || ''}
              onChange={(e) => updateConfig('deadband', parseFloat(e.target.value))}
              placeholder="2"
              style={{
                width: '100%',
                padding: '12px',
                border: '1px solid #e5e7eb',
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
          </div>
        </div>
      )}

      {formData.condition_type === 'range' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>상한값 *</label>
            <input
              type="number"
              value={formData.default_config.high_limit || ''}
              onChange={(e) => updateConfig('high_limit', parseFloat(e.target.value))}
              placeholder="80"
              style={{
                width: '100%',
                padding: '12px',
                border: `1px solid ${errors.high_limit ? '#ef4444' : '#e5e7eb'}`,
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
            {errors.high_limit && <div style={{ color: '#ef4444', fontSize: '12px', marginTop: '4px' }}>{errors.high_limit}</div>}
          </div>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>하한값 *</label>
            <input
              type="number"
              value={formData.default_config.low_limit || ''}
              onChange={(e) => updateConfig('low_limit', parseFloat(e.target.value))}
              placeholder="20"
              style={{
                width: '100%',
                padding: '12px',
                border: `1px solid ${errors.low_limit ? '#ef4444' : '#e5e7eb'}`,
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
            {errors.low_limit && <div style={{ color: '#ef4444', fontSize: '12px', marginTop: '4px' }}>{errors.low_limit}</div>}
          </div>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>상상한값 (HH)</label>
            <input
              type="number"
              value={formData.default_config.high_high_limit || ''}
              onChange={(e) => updateConfig('high_high_limit', parseFloat(e.target.value))}
              placeholder="100"
              style={{
                width: '100%',
                padding: '12px',
                border: '1px solid #e5e7eb',
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
          </div>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>하하한값 (LL)</label>
            <input
              type="number"
              value={formData.default_config.low_low_limit || ''}
              onChange={(e) => updateConfig('low_low_limit', parseFloat(e.target.value))}
              placeholder="0"
              style={{
                width: '100%',
                padding: '12px',
                border: '1px solid #e5e7eb',
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
          </div>
        </div>
      )}

      {formData.condition_type === 'pattern' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>트리거 상태</label>
            <select
              value={formData.default_config.trigger_state || 'on_true'}
              onChange={(e) => updateConfig('trigger_state', e.target.value)}
              style={{
                width: '100%',
                padding: '12px',
                border: '1px solid #e5e7eb',
                borderRadius: '8px',
                fontSize: '14px',
                background: 'white'
              }}
            >
              <option value="on_true">True 상태에서</option>
              <option value="on_false">False 상태에서</option>
              <option value="state_change">상태 변화시</option>
            </select>
          </div>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>지연 시간 (ms)</label>
            <input
              type="number"
              value={formData.default_config.hold_time || ''}
              onChange={(e) => updateConfig('hold_time', parseInt(e.target.value))}
              placeholder="1000"
              style={{
                width: '100%',
                padding: '12px',
                border: '1px solid #e5e7eb',
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
          </div>
        </div>
      )}

      {formData.condition_type === 'script' && (
        <div>
          <label style={{ display: 'block', marginBottom: '8px', fontWeight: '500' }}>스크립트 조건 *</label>
          <textarea
            value={formData.default_config.script_condition || ''}
            onChange={(e) => updateConfig('script_condition', e.target.value)}
            placeholder="return value > 80;"
            rows={4}
            style={{
              width: '100%',
              padding: '12px',
              border: `1px solid ${errors.script_condition ? '#ef4444' : '#e5e7eb'}`,
              borderRadius: '8px',
              fontSize: '14px',
              fontFamily: 'monospace',
              resize: 'vertical'
            }}
          />
          {errors.script_condition && <div style={{ color: '#ef4444', fontSize: '12px', marginTop: '4px' }}>{errors.script_condition}</div>}
          <div style={{ fontSize: '12px', color: '#6b7280', marginTop: '4px' }}>
            사용 가능한 변수: value, device_name, point_name, unit
          </div>
        </div>
      )}
    </div>
  );

  const renderStep3 = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      <h3 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: '600' }}>알림 및 기타 설정</h3>
      
      <div>
        <label style={{ display: 'block', marginBottom: '12px', fontWeight: '500' }}>적용 가능한 데이터 타입</label>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
          {['float', 'int', 'uint32', 'bool', 'string'].map(type => (
            <label key={type} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type="checkbox"
                checked={formData.applicable_data_types.includes(type)}
                onChange={(e) => {
                  if (e.target.checked) {
                    updateField('applicable_data_types', [...formData.applicable_data_types, type]);
                  } else {
                    updateField('applicable_data_types', formData.applicable_data_types.filter(t => t !== type));
                  }
                }}
              />
              <span style={{ fontSize: '14px' }}>{type}</span>
            </label>
          ))}
        </div>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '20px' }}>
        <div>
          <h4 style={{ margin: '0 0 12px 0', fontSize: '16px', fontWeight: '500' }}>알림 설정</h4>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type="checkbox"
                checked={formData.notification_enabled}
                onChange={(e) => updateField('notification_enabled', e.target.checked)}
              />
              <span style={{ fontSize: '14px' }}>알림 활성화</span>
            </label>
            <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type="checkbox"
                checked={formData.email_notification}
                onChange={(e) => updateField('email_notification', e.target.checked)}
              />
              <span style={{ fontSize: '14px' }}>이메일 알림</span>
            </label>
            <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type="checkbox"
                checked={formData.sms_notification}
                onChange={(e) => updateField('sms_notification', e.target.checked)}
              />
              <span style={{ fontSize: '14px' }}>SMS 알림</span>
            </label>
          </div>
        </div>

        <div>
          <h4 style={{ margin: '0 0 12px 0', fontSize: '16px', fontWeight: '500' }}>동작 설정</h4>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type="checkbox"
                checked={formData.auto_acknowledge}
                onChange={(e) => updateField('auto_acknowledge', e.target.checked)}
              />
              <span style={{ fontSize: '14px' }}>자동 확인</span>
            </label>
            <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type="checkbox"
                checked={formData.auto_clear}
                onChange={(e) => updateField('auto_clear', e.target.checked)}
              />
              <span style={{ fontSize: '14px' }}>자동 해제</span>
            </label>
          </div>
        </div>
      </div>
    </div>
  );

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 9999,
        padding: '20px'
      }}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div
        style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.1)',
          width: '100%',
          maxWidth: '800px',
          maxHeight: '90vh',
          display: 'flex',
          flexDirection: 'column'
        }}
      >
        {/* 헤더 */}
        <div style={{ padding: '24px', borderBottom: '1px solid #e5e7eb' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <h2 style={{ fontSize: '24px', fontWeight: '700', margin: 0, color: '#111827' }}>
              새 알람 템플릿 생성
            </h2>
            <button
              onClick={onClose}
              style={{
                background: 'none',
                border: 'none',
                fontSize: '24px',
                cursor: 'pointer',
                color: '#6b7280',
                padding: '4px'
              }}
            >
              ×
            </button>
          </div>
        </div>

        {/* 컨텐츠 */}
        <div style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>
          {renderStepIndicator()}
          
          {activeStep === 1 && renderStep1()}
          {activeStep === 2 && renderStep2()}
          {activeStep === 3 && renderStep3()}
        </div>

        {/* 푸터 */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          padding: '24px',
          borderTop: '1px solid #e5e7eb',
          background: '#f8fafc'
        }}>
          <div>
            <button
              onClick={handlePrev}
              disabled={activeStep === 1}
              style={{
                padding: '12px 24px',
                border: '1px solid #d1d5db',
                borderRadius: '8px',
                background: activeStep === 1 ? '#f3f4f6' : '#ffffff',
                color: activeStep === 1 ? '#9ca3af' : '#374151',
                cursor: activeStep === 1 ? 'not-allowed' : 'pointer',
                fontSize: '14px',
                fontWeight: '500'
              }}
            >
              이전
            </button>
          </div>

          <div style={{ display: 'flex', gap: '12px' }}>
            <button
              onClick={onClose}
              disabled={loading}
              style={{
                padding: '12px 24px',
                border: '1px solid #d1d5db',
                borderRadius: '8px',
                background: '#ffffff',
                color: '#374151',
                cursor: loading ? 'not-allowed' : 'pointer',
                fontSize: '14px',
                fontWeight: '500',
                opacity: loading ? 0.6 : 1
              }}
            >
              취소
            </button>

            {activeStep < 3 ? (
              <button
                onClick={handleNext}
                style={{
                  padding: '12px 24px',
                  border: 'none',
                  borderRadius: '8px',
                  background: '#3b82f6',
                  color: 'white',
                  cursor: 'pointer',
                  fontSize: '14px',
                  fontWeight: '500'
                }}
              >
                다음
              </button>
            ) : (
              <button
                onClick={handleCreate}
                disabled={loading}
                style={{
                  padding: '12px 24px',
                  border: 'none',
                  borderRadius: '8px',
                  background: loading ? '#9ca3af' : '#10b981',
                  color: 'white',
                  cursor: loading ? 'not-allowed' : 'pointer',
                  fontSize: '14px',
                  fontWeight: '500'
                }}
              >
                {loading ? '생성 중...' : '템플릿 생성'}
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default TemplateCreateModal;