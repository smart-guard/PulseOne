// ============================================================================
// VirtualPointModal.tsx - Hook 에러 수정
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

// ============================================================================
// 🔥 Hook 에러 수정: 컴포넌트 함수 시작 부분에 모든 Hook을 올바르게 배치
// ============================================================================

const VirtualPointModal: React.FC<VirtualPointModalProps> = ({
  isOpen,
  mode,
  virtualPoint,
  onSave,
  onClose,
  loading = false
}) => {
  // 🔥 Hook 호출 순서 수정: 모든 useState를 맨 위에 배치
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
  
  const [originalData, setOriginalData] = useState<FormData | null>(null);
  const [hasChanges, setHasChanges] = useState(false);
  const [validationErrors, setValidationErrors] = useState<Record<string, string>>({});
  const [validationWarnings, setValidationWarnings] = useState<Record<string, string>>({});
  const [isSaving, setIsSaving] = useState(false);
  const [isDataLoaded, setIsDataLoaded] = useState(false);

  // 🔥 useScriptEngine 훅도 올바른 위치에 배치
  const {
    functions,
    validationResult,
    isValidating,
    validateScript,
    loadFunctions
  } = useScriptEngine();

  // ========================================================================
  // 자동 저장 (localStorage) - 데이터 손실 방지
  // ========================================================================
  
  const STORAGE_KEY = `virtual_point_draft_${mode}_${virtualPoint?.id || 'new'}`;

  const saveToLocalStorage = useCallback((data: FormData) => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify({
        ...data,
        timestamp: Date.now()
      }));
      console.log('Draft 자동 저장됨');
    } catch (error) {
      console.warn('Draft 저장 실패:', error);
    }
  }, [STORAGE_KEY]);

  const loadFromLocalStorage = useCallback((): Partial<FormData> | null => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY);
      if (saved) {
        const data = JSON.parse(saved);
        // 1시간 이내 데이터만 복원
        if (Date.now() - data.timestamp < 60 * 60 * 1000) {
          console.log('Draft 복원됨');
          return data;
        }
      }
    } catch (error) {
      console.warn('Draft 복원 실패:', error);
    }
    return null;
  }, [STORAGE_KEY]);

  const clearLocalStorage = useCallback(() => {
    try {
      localStorage.removeItem(STORAGE_KEY);
      console.log('Draft 정리됨');
    } catch (error) {
      console.warn('Draft 정리 실패:', error);
    }
  }, [STORAGE_KEY]);

  // ========================================================================
  // 🔥 Effects - Hook 호출 순서 보장
  // ========================================================================
  
  useEffect(() => {
    if (isOpen) {
      console.log('VirtualPointModal 열림:', { mode, virtualPoint });
      
      // 🔥 loadFunctions가 함수인지 확인 후 호출
      if (typeof loadFunctions === 'function') {
        loadFunctions().catch(console.warn);
      }
      
      // 기존 데이터 로드 또는 초기화
      let initialData: FormData;
      
      if (mode === 'edit' && virtualPoint) {
        console.log('편집 모드 - 데이터 로딩:', virtualPoint);
        initialData = {
          name: virtualPoint.name || '',
          description: virtualPoint.description || '',
          category: virtualPoint.category || '',
          tags: Array.isArray(virtualPoint.tags) ? virtualPoint.tags : 
                typeof virtualPoint.tags === 'string' ? virtualPoint.tags.split(',').map(t => t.trim()) : [],
          expression: virtualPoint.expression || virtualPoint.formula || '',
          execution_type: virtualPoint.execution_type || 'periodic',
          execution_interval: virtualPoint.execution_interval || 5000,
          priority: virtualPoint.priority || 0,
          data_type: virtualPoint.data_type || 'number',
          unit: virtualPoint.unit || '',
          decimal_places: virtualPoint.decimal_places || 2,
          input_variables: Array.isArray(virtualPoint.input_variables) ? virtualPoint.input_variables : [],
          timeout_ms: virtualPoint.timeout_ms || 10000,
          error_handling: virtualPoint.error_handling || 'propagate',
          default_value: virtualPoint.default_value !== undefined ? virtualPoint.default_value : 0,
          is_enabled: virtualPoint.is_enabled !== undefined ? virtualPoint.is_enabled : true,
          scope_type: virtualPoint.scope_type || 'device',
          scope_id: virtualPoint.scope_id,
          min_value: virtualPoint.min_value,
          max_value: virtualPoint.max_value
        };
        
        console.log('초기화된 formData:', initialData);
        
      } else {
        initialData = {
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
        };
      }

      // Draft 복원 확인 (생성 모드에서만)
      const savedDraft = loadFromLocalStorage();
      if (savedDraft && mode === 'create') {
        const shouldRestore = window.confirm(
          '이전에 작성하던 내용이 있습니다. 복원하시겠습니까?'
        );
        if (shouldRestore) {
          initialData = { ...initialData, ...savedDraft };
        }
      }
      
      setFormData(initialData);
      setOriginalData(initialData);
      setActiveTab('basic');
      setHasChanges(false);
      setValidationErrors({});
      setValidationWarnings({});
      setIsDataLoaded(true);
      
      // 초기 검증 실행 (데이터 로딩 완료 후)
      setTimeout(() => {
        validateForm(initialData);
      }, 100);
    } else {
      setIsDataLoaded(false);
    }
  }, [isOpen, mode, virtualPoint, loadFunctions, loadFromLocalStorage]);

  // 폼 데이터 변경 시 자동 저장 (데이터 로딩 완료 후에만)
  useEffect(() => {
    if (isOpen && isDataLoaded) {
      setHasChanges(true);
      
      // 자동 저장 (500ms 디바운스)
      const timer = setTimeout(() => {
        saveToLocalStorage(formData);
      }, 500);
      
      // 실시간 검증
      validateForm(formData);

      return () => clearTimeout(timer);
    }
  }, [formData, isOpen, isDataLoaded, saveToLocalStorage]);

  // ========================================================================
  // Validation
  // ========================================================================
  
  const validateFormula = useCallback(async () => {
    if (!formData.expression.trim()) return;
    
    try {
      const context: Record<string, any> = {};
      formData.input_variables.forEach(input => {
        context[input.variable_name] = getSampleValue(input.data_type);
      });
      
      // 🔥 validateScript가 함수인지 확인 후 호출
      if (typeof validateScript === 'function') {
        await validateScript(formData.expression, context);
      }
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

  const validateForm = useCallback((dataToValidate: FormData = formData): { errors: Record<string, string>, warnings: Record<string, string> } => {
    const errors: Record<string, string> = {};
    const warnings: Record<string, string> = {};

    // 🚨 치명적 오류 (저장 불가)
    if (!dataToValidate.name.trim()) {
      errors.name = '이름은 필수입니다';
    }

    // ⚠️ 경고사항 (저장 가능하지만 권장하지 않음)
    if (!dataToValidate.category.trim()) {
      warnings.category = '카테고리를 설정하는 것이 좋습니다';
    }

    if (!dataToValidate.expression.trim()) {
      warnings.expression = '수식을 작성하는 것이 좋습니다';
    }

    // 수식 변수 검증
    if (dataToValidate.expression) {
      const usedVariables = extractVariablesFromExpression(dataToValidate.expression);
      const definedVariables = dataToValidate.input_variables.map(v => v.variable_name);
      const missingVariables = usedVariables.filter(v => !definedVariables.includes(v));
      
      if (missingVariables.length > 0) {
        warnings.expression = `정의되지 않은 변수: ${missingVariables.join(', ')}`;
      }
    }

    // 범위 검증
    if (dataToValidate.min_value !== undefined && dataToValidate.max_value !== undefined) {
      if (dataToValidate.min_value >= dataToValidate.max_value) {
        warnings.range = '최솟값이 최댓값보다 크거나 같습니다';
      }
    }

    // 실행 간격 검증
    if (dataToValidate.execution_type === 'periodic' && dataToValidate.execution_interval < 1000) {
      warnings.execution_interval = '실행 간격이 너무 짧을 수 있습니다 (1초 미만)';
    }

    setValidationErrors(errors);
    setValidationWarnings(warnings);
    
    console.log('검증 결과:', { 
      dataToValidate: { name: dataToValidate.name, category: dataToValidate.category }, 
      errors, 
      warnings 
    });
    
    return { errors, warnings };
  }, [formData]);

  const extractVariablesFromExpression = (expression: string): string[] => {
    const matches = expression.match(/[a-zA-Z_][a-zA-Z0-9_]*/g);
    return matches ? Array.from(new Set(matches)) : [];
  };

  // ========================================================================
  // Event Handlers
  // ========================================================================
  
  const handleFormDataChange = useCallback((field: keyof FormData, value: any) => {
    console.log('필드 변경:', field, value);
    setFormData(prev => ({ ...prev, [field]: value }));
  }, []);

  const handleInputVariableChange = useCallback((variables: VirtualPointInput[]) => {
    console.log('입력변수 즉시 업데이트:', variables);
    setFormData(prev => ({ 
      ...prev, 
      input_variables: variables 
    }));
  }, []);

  const handleMoveToFormulaTab = useCallback(() => {
    console.log('수식 편집 탭으로 이동');
    setActiveTab('formula');
  }, []);

  const handleSave = async () => {
    console.log('🚀 저장 시작:', { mode, formData: { name: formData.name, category: formData.category } });
    
    const validation = validateForm();
    
    // 치명적 오류가 있으면 저장 중단
    if (Object.keys(validation.errors).length > 0) {
      alert('필수 항목을 입력해주세요:\n' + Object.values(validation.errors).join('\n'));
      
      if (validation.errors.name) {
        setActiveTab('basic');
      }
      return;
    }

    // 저장 확인
    let confirmMessage = `가상포인트를 ${mode === 'create' ? '생성' : '수정'}하시겠습니까?\n\n`;
    confirmMessage += `이름: ${formData.name}\n`;
    confirmMessage += `카테고리: ${formData.category || '미설정'}\n`;
    confirmMessage += `수식: ${formData.expression || '미설정'}\n`;
    confirmMessage += `입력변수: ${formData.input_variables.length}개`;
    
    if (Object.keys(validation.warnings).length > 0) {
      confirmMessage += '\n\n⚠️ 경고사항:\n' + Object.values(validation.warnings).join('\n');
    }
        
    if (!window.confirm(confirmMessage)) {
      return;
    }

    setIsSaving(true);
    try {
      console.log('📤 onSave 호출 시작...');
      
      await onSave(formData);
      
      console.log('✅ onSave 완료 - 부모에서 새로고침됨');
      
      // 성공 후 처리
      setHasChanges(false);
      clearLocalStorage();
      
      console.log(`🎉 가상포인트 ${mode === 'create' ? '생성' : '수정'} 완료: ${formData.name}`);
      
      onClose();
      
    } catch (error) {
      console.error('❌ 저장 실패:', error);
      alert('저장 중 오류가 발생했습니다:\n' + (error instanceof Error ? error.message : '알 수 없는 오류'));
    } finally {
      setIsSaving(false);
    }
  };

  const handleClose = () => {
    if (hasChanges) {
      const choice = window.confirm(
        '변경사항이 저장되지 않았습니다.\n\n' +
        '- 확인: 변경사항을 버리고 닫기\n' +
        '- 취소: 계속 편집하기\n\n' +
        '(참고: 작업 내용은 자동으로 임시 저장되므로 나중에 복원할 수 있습니다)'
      );
      
      if (choice) {
        onClose();
      }
    } else {
      clearLocalStorage();
      onClose();
    }
  };

  const handleTabChange = (tab: typeof activeTab) => {
    setActiveTab(tab);
  };

  // ========================================================================
  // 🔥 조건부 렌더링 수정 - Hook 규칙 준수
  // ========================================================================
  
  // Hook 호출이 완료된 후 조건부 반환
  if (!isOpen) {
    return null;
  }

  const totalErrors = Object.keys(validationErrors).length;
  const totalWarnings = Object.keys(validationWarnings).length;
  const canSave = totalErrors === 0;

  return (
    <div className="modal-overlay" onClick={(e) => e.target === e.currentTarget && handleClose()}>
      <div className="modal-container virtual-point-modal">
        {/* 모달 헤더 */}
        <div className="modal-header">
          <h2>
            <i className="fas fa-function"></i>
            {mode === 'create' ? '새 가상포인트 생성' : '가상포인트 편집'}
            {hasChanges && <span style={{ color: '#f59e0b', marginLeft: '8px' }}>●</span>}
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
            {validationErrors.name && <span className="tab-error">!</span>}
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
            className={`tab-button ${activeTab === 'formula' ? 'active' : ''}`}
            onClick={() => handleTabChange('formula')}
          >
            <i className="fas fa-code"></i>
            수식 편집
            {validationWarnings.expression && <span className="tab-warning">⚠</span>}
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
              warnings={validationWarnings}
            />
          )}

          {activeTab === 'inputs' && (
            <InputVariableEditor
              variables={formData.input_variables}
              onChange={handleInputVariableChange}
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

          {activeTab === 'validation' && (
            <ValidationPanel
              formData={formData}
              validationResult={validationResult}
              onValidate={validateFormula}
              isValidating={isValidating}
              errors={validationErrors}
              warnings={validationWarnings}
            />
          )}
        </div>

        {/* 모달 푸터 */}
        <div className="modal-footer">
          <div className="footer-left">
            <span className="auto-save-indicator">
              <i className="fas fa-save" style={{ color: '#10b981' }}></i>
              자동 저장됨
            </span>
            {totalErrors > 0 && (
              <span className="error-indicator">
                <i className="fas fa-exclamation-circle"></i>
                {totalErrors}개 오류
              </span>
            )}
            {totalWarnings > 0 && (
              <span className="warning-indicator">
                <i className="fas fa-exclamation-triangle"></i>
                {totalWarnings}개 경고
              </span>
            )}
            <span className="variables-count">
              입력변수: {formData.input_variables.length}개
            </span>
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
              className={canSave ? "btn-primary" : "btn-primary-disabled"}
              onClick={handleSave}
              disabled={isSaving || loading || !canSave}
              title={!canSave ? "필수 항목을 입력해주세요" : ""}
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
                  {totalWarnings > 0 && ' (경고 있음)'}
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

// ============================================================================
// 🔥 수정 사항 요약
// ============================================================================

/*
1. Hook 호출 순서 수정:
   - 모든 useState 호출을 컴포넌트 함수 최상단에 배치
   - useScriptEngine 훅도 올바른 위치에 배치
   - 조건부 반환(return null)을 모든 Hook 호출 이후로 이동

2. 함수 호출 안전성 추가:
   - loadFunctions와 validateScript 함수 존재 여부 확인 후 호출
   - try-catch로 에러 처리 강화

3. Effect 의존성 정리:
   - useEffect 의존성 배열에서 불필요한 함수 제거
   - loadFromLocalStorage 등 콜백 함수의 의존성 최적화

이제 Hook 규칙을 준수하여 에러가 발생하지 않을 것입니다.
*/