// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointModal/VirtualPointModal.tsx
// 가상포인트 생성/편집 모달 - 완전한 구현
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { VirtualPoint, VirtualPointInput, ScriptFunction } from '../../../types/virtualPoints';
import { useScriptEngine } from '../../../hooks/shared/useScriptEngine';
import BasicInfoForm from './BasicInfoForm';
import FormulaEditor from './FormulaEditor';
import InputVariableEditor from './InputVariableEditor';
import ValidationPanel from './ValidationPanel';
import './VirtualPointModal.css';

interface VirtualPointModalProps {
  isOpen: boolean;
  mode: 'create' | 'edit';
  virtualPoint?: VirtualPoint;
  onSave: (data: Partial<VirtualPoint>) => Promise<void>;
  onClose: () => void;
  loading?: boolean;
}

interface FormData {
  // 기본 정보
  name: string;
  description: string;
  category: string;
  tags: string[];
  
  // 계산 설정
  expression: string;
  execution_type: 'periodic' | 'on_change' | 'manual';
  execution_interval: number;
  priority: number;
  
  // 데이터 타입
  data_type: 'number' | 'boolean' | 'string';
  unit: string;
  decimal_places: number;
  
  // 입력 변수
  input_variables: VirtualPointInput[];
  
  // 고급 설정
  timeout_ms: number;
  error_handling: 'propagate' | 'default_value' | 'previous_value';
  default_value: any;
  is_enabled: boolean;
  
  // 범위 검증
  min_value?: number;
  max_value?: number;
  
  // 스케줄링
  scope_type: 'device' | 'site' | 'global';
  scope_id?: number;
}

const VirtualPointModal: React.FC<VirtualPointModalProps> = ({
  isOpen,
  mode,
  virtualPoint,
  onSave,
  onClose,
  loading = false
}) => {
  // ========================================================================
  // State
  // ========================================================================
  
  const [activeTab, setActiveTab] = useState<'basic' | 'formula' | 'inputs' | 'validation'>('basic');
  const [formData, setFormData] = useState<FormData>({
    name: '',
    description: '',
    category: '',
    tags: [],
    expression: '',
    execution_type: 'periodic',
    execution_interval: 5000,
    priority: 0,
    data_type: 'number',
    unit: '',
    decimal_places: 2,
    input_variables: [],
    timeout_ms: 10000,
    error_handling: 'propagate',
    default_value: 0,
    is_enabled: true,
    scope_type: 'device'
  });
  
  const [hasChanges, setHasChanges] = useState(false);
  const [validationErrors, setValidationErrors] = useState<Record<string, string>>({});
  const [isSaving, setIsSaving] = useState(false);
  
  // 스크립트 엔진 훅
  const {
    functions,
    validationResult,
    isValidating,
    validateScript,
    loadFunctions
  } = useScriptEngine();

  // ========================================================================
  // Effects
  // ========================================================================
  
  useEffect(() => {
    if (isOpen) {
      loadFunctions();
      
      if (mode === 'edit' && virtualPoint) {
        setFormData({
          name: virtualPoint.name,
          description: virtualPoint.description || '',
          category: virtualPoint.category,
          tags: virtualPoint.tags || [],
          expression: virtualPoint.expression,
          execution_type: virtualPoint.execution_type,
          execution_interval: virtualPoint.execution_interval,
          priority: virtualPoint.priority || 0,
          data_type: virtualPoint.data_type,
          unit: virtualPoint.unit || '',
          decimal_places: virtualPoint.decimal_places || 2,
          input_variables: virtualPoint.input_variables || [],
          timeout_ms: virtualPoint.timeout_ms || 10000,
          error_handling: virtualPoint.error_handling,
          default_value: virtualPoint.default_value,
          is_enabled: virtualPoint.is_enabled,
          scope_type: virtualPoint.scope_type,
          scope_id: virtualPoint.scope_id,
          min_value: virtualPoint.min_value,
          max_value: virtualPoint.max_value
        });
      } else {
        // 초기화
        setFormData({
          name: '',
          description: '',
          category: '',
          tags: [],
          expression: '',
          execution_type: 'periodic',
          execution_interval: 5000,
          priority: 0,
          data_type: 'number',
          unit: '',
          decimal_places: 2,
          input_variables: [],
          timeout_ms: 10000,
          error_handling: 'propagate',
          default_value: 0,
          is_enabled: true,
          scope_type: 'device'
        });
      }
      
      setActiveTab('basic');
      setHasChanges(false);
      setValidationErrors({});
    }
  }, [isOpen, mode, virtualPoint, loadFunctions]);

  // 폼 데이터 변경 감지
  useEffect(() => {
    if (isOpen) {
      setHasChanges(true);
      
      // 실시간 검증
      if (formData.expression.trim()) {
        validateFormula();
      }
    }
  }, [formData, isOpen]);

  // ========================================================================
  // Validation
  // ========================================================================
  
  const validateFormula = useCallback(async () => {
    if (!formData.expression.trim()) return;
    
    try {
      // 입력 변수들을 컨텍스트로 변환
      const context: Record<string, any> = {};
      formData.input_variables.forEach(input => {
        context[input.variable_name] = getSampleValue(input.data_type);
      });
      
      await validateScript(formData.expression, context);
    } catch (error) {
      console.error('공식 검증 실패:', error);
    }
  }, [formData.expression, formData.input_variables, validateScript]);

  const getSampleValue = (dataType: string): any => {
    switch (dataType) {
      case 'number': return 100;
      case 'boolean': return true;
      case 'string': return 'sample';
      default: return null;
    }
  };

  const validateForm = (): boolean => {
    const errors: Record<string, string> = {};

    // 필수 필드 검증
    if (!formData.name.trim()) {
      errors.name = '이름은 필수입니다';
    }

    if (!formData.category.trim()) {
      errors.category = '카테고리는 필수입니다';
    }

    if (!formData.expression.trim()) {
      errors.expression = '수식은 필수입니다';
    }

    // 범위 검증
    if (formData.min_value !== undefined && formData.max_value !== undefined) {
      if (formData.min_value >= formData.max_value) {
        errors.range = '최솟값은 최댓값보다 작아야 합니다';
      }
    }

    // 실행 간격 검증
    if (formData.execution_type === 'periodic' && formData.execution_interval < 1000) {
      errors.execution_interval = '실행 간격은 1초 이상이어야 합니다';
    }

    setValidationErrors(errors);
    return Object.keys(errors).length === 0;
  };

  // ========================================================================
  // Event Handlers
  // ========================================================================
  
  const handleFormDataChange = useCallback((field: keyof FormData, value: any) => {
    setFormData(prev => ({ ...prev, [field]: value }));
  }, []);

  const handleInputVariableChange = useCallback((variables: VirtualPointInput[]) => {
    setFormData(prev => ({ ...prev, input_variables: variables }));
  }, []);

  const handleSave = async () => {
    if (!validateForm()) {
      return;
    }

    setIsSaving(true);
    try {
      await onSave(formData);
      setHasChanges(false);
      onClose();
    } catch (error) {
      console.error('저장 실패:', error);
    } finally {
      setIsSaving(false);
    }
  };

  const handleClose = () => {
    if (hasChanges) {
      if (window.confirm('변경사항이 저장되지 않았습니다. 정말 닫으시겠습니까?')) {
        onClose();
      }
    } else {
      onClose();
    }
  };

  const handleTabChange = (tab: typeof activeTab) => {
    setActiveTab(tab);
  };

  // ========================================================================
  // Render
  // ========================================================================
  
  if (!isOpen) return null;

  return (
    <div className="modal-overlay" onClick={(e) => e.target === e.currentTarget && handleClose()}>
      <div className="modal-container virtual-point-modal">
        {/* 모달 헤더 */}
        <div className="modal-header">
          <h2>
            <i className="fas fa-function"></i>
            {mode === 'create' ? '새 가상포인트 생성' : '가상포인트 편집'}
          </h2>
          <button className="modal-close-btn" onClick={handleClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 탭 네비게이션 */}
        <div className="modal-tabs">
          <button
            className={`tab-button ${activeTab === 'basic' ? 'active' : ''}`}
            onClick={() => handleTabChange('basic')}
          >
            <i className="fas fa-info-circle"></i>
            기본 정보
          </button>
          <button
            className={`tab-button ${activeTab === 'formula' ? 'active' : ''}`}
            onClick={() => handleTabChange('formula')}
          >
            <i className="fas fa-code"></i>
            수식 편집
          </button>
          <button
            className={`tab-button ${activeTab === 'inputs' ? 'active' : ''}`}
            onClick={() => handleTabChange('inputs')}
          >
            <i className="fas fa-plug"></i>
            입력 변수
            {formData.input_variables.length > 0 && (
              <span className="tab-badge">{formData.input_variables.length}</span>
            )}
          </button>
          <button
            className={`tab-button ${activeTab === 'validation' ? 'active' : ''}`}
            onClick={() => handleTabChange('validation')}
          >
            <i className="fas fa-check-circle"></i>
            검증 및 테스트
          </button>
        </div>

        {/* 모달 콘텐츠 */}
        <div className="modal-content">
          {activeTab === 'basic' && (
            <BasicInfoForm
              data={formData}
              onChange={handleFormDataChange}
              errors={validationErrors}
            />
          )}

          {activeTab === 'formula' && (
            <FormulaEditor
              expression={formData.expression}
              inputVariables={formData.input_variables}
              functions={functions}
              onChange={(expression) => handleFormDataChange('expression', expression)}
              validationResult={validationResult}
              isValidating={isValidating}
            />
          )}

          {activeTab === 'inputs' && (
            <InputVariableEditor
              variables={formData.input_variables}
              onChange={handleInputVariableChange}
            />
          )}

          {activeTab === 'validation' && (
            <ValidationPanel
              formData={formData}
              validationResult={validationResult}
              onValidate={validateFormula}
              isValidating={isValidating}
            />
          )}
        </div>

        {/* 모달 푸터 */}
        <div className="modal-footer">
          <div className="footer-left">
            {hasChanges && (
              <span className="changes-indicator">
                <i className="fas fa-circle"></i>
                변경사항 있음
              </span>
            )}
            {Object.keys(validationErrors).length > 0 && (
              <span className="error-indicator">
                <i className="fas fa-exclamation-triangle"></i>
                {Object.keys(validationErrors).length}개 오류
              </span>
            )}
          </div>
          
          <div className="footer-actions">
            <button
              type="button"
              className="btn-secondary"
              onClick={handleClose}
              disabled={isSaving || loading}
            >
              취소
            </button>
            <button
              type="button"
              className="btn-primary"
              onClick={handleSave}
              disabled={isSaving || loading || Object.keys(validationErrors).length > 0}
            >
              {isSaving || loading ? (
                <>
                  <i className="fas fa-spinner fa-spin"></i>
                  저장 중...
                </>
              ) : (
                <>
                  <i className="fas fa-save"></i>
                  {mode === 'create' ? '생성' : '저장'}
                </>
              )}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default VirtualPointModal;