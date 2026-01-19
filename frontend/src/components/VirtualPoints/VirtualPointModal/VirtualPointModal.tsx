// ============================================================================
// VirtualPointModal.tsx - Unified Simulation & Hook Correction
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { VirtualPoint, VirtualPointInput } from '../../../types/virtualPoints';
import { useScriptEngine } from '../../../hooks/shared/useScriptEngine';
import BasicInfoForm from './BasicInfoForm';
import FormulaEditor from './FormulaEditor';
import InputVariableEditor from './InputVariableEditor';
import ValidationPanel from './ValidationPanel';
import { useConfirmContext } from '../../common/ConfirmProvider';
import './VirtualPointModal.css';

interface VirtualPointModalProps {
  isOpen: boolean;
  mode: 'create' | 'edit';
  virtualPoint?: VirtualPoint;
  onSave: (data: Partial<VirtualPoint>) => Promise<void>;
  onDelete?: (pointId: number) => Promise<void>;
  onRestore?: (pointId: number) => Promise<void>;
  onClose: () => void;
  loading?: boolean;
}

interface FormData {
  name: string;
  description: string;
  category: string;
  tags: string[];
  expression: string;
  execution_type: 'periodic' | 'on_change' | 'manual';
  execution_interval: number;
  priority: number;
  data_type: 'number' | 'boolean' | 'string';
  unit: string;
  decimal_places: number;
  input_variables: VirtualPointInput[];
  timeout_ms: number;
  error_handling: 'propagate' | 'default_value' | 'previous_value';
  default_value: any;
  is_enabled: boolean;
  scope_type: 'device' | 'site' | 'global';
  scope_id?: number;
  min_value?: number;
  max_value?: number;
}

const VirtualPointModal: React.FC<VirtualPointModalProps> = ({
  isOpen,
  mode,
  virtualPoint,
  onSave,
  onDelete,
  onRestore,
  onClose,
  loading = false
}) => {
  const { confirm } = useConfirmContext();

  const [currentStep, setCurrentStep] = useState(1);
  const totalSteps = 4;
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
  const [simulationResult, setSimulationResult] = useState<number | null>(null);
  const [isSimulating, setIsSimulating] = useState(false);

  const {
    validationResult,
    isValidating,
    validateScript,
    testScript,
    isTesting,
    loadFunctions
  } = useScriptEngine();

  const STORAGE_KEY = `virtual_point_draft_${mode}_${virtualPoint?.id || 'new'}`;

  const saveToLocalStorage = useCallback((data: FormData) => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify({
        ...data,
        timestamp: Date.now()
      }));
    } catch (error) {
      console.warn('Draft 저장 실패:', error);
    }
  }, [STORAGE_KEY]);

  const loadFromLocalStorage = useCallback((): Partial<FormData> | null => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY);
      if (saved) {
        const data = JSON.parse(saved);
        if (Date.now() - data.timestamp < 60 * 60 * 1000) {
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
    } catch (error) {
      console.warn('Draft 정리 실패:', error);
    }
  }, [STORAGE_KEY]);

  useEffect(() => {
    if (isOpen) {
      if (typeof loadFunctions === 'function') {
        loadFunctions().catch(console.warn);
      }

      let initialData: FormData;
      if (mode === 'edit' && virtualPoint) {
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

      const savedDraft = loadFromLocalStorage();
      if (savedDraft && mode === 'create') {
        confirm({
          title: '작성 중인 내용 복원',
          message: '이전에 작성하던 내용이 있습니다. 복원하시겠습니까?',
          confirmText: '복원하기',
          cancelText: '무시하기'
        }).then(shouldRestore => {
          if (shouldRestore) {
            setFormData(prev => ({ ...prev, ...savedDraft }));
          }
        });
      }

      setFormData(initialData);
      setOriginalData(initialData);
      setCurrentStep(1);
      setHasChanges(false);
      setValidationErrors({});
      setValidationWarnings({});
      setIsDataLoaded(true);
    } else {
      setIsDataLoaded(false);
    }
  }, [isOpen, mode, virtualPoint, loadFunctions, loadFromLocalStorage]);

  useEffect(() => {
    if (isOpen && isDataLoaded) {
      setHasChanges(true);
      const timer = setTimeout(() => {
        saveToLocalStorage(formData);
      }, 500);
      validateForm(formData);
      return () => clearTimeout(timer);
    }
  }, [formData, isOpen, isDataLoaded, saveToLocalStorage]);

  const handleApplyTemplate = useCallback((template: Partial<FormData>) => {
    setFormData(prev => ({ ...prev, ...template }));
    setSimulationResult(null); // Reset simulation
    console.log('템플릿 적용:', template.name);
  }, []);

  const validateFormula = useCallback(async () => {
    if (!formData.expression.trim()) return;
    try {
      const context: Record<string, any> = {};
      formData.input_variables.forEach(input => {
        context[input.variable_name] = input.data_type === 'boolean' ? true : 100;
      });
      if (typeof validateScript === 'function') {
        await validateScript(formData.expression, context);
      }
    } catch (error) {
      console.error('공식 검증 실패:', error);
    }
  }, [formData.expression, formData.input_variables, validateScript]);

  const validateForm = useCallback((dataToValidate: FormData = formData) => {
    const errors: Record<string, string> = {};
    const warnings: Record<string, string> = {};

    if (!dataToValidate.name.trim()) errors.name = '이름은 필수입니다';
    if (!dataToValidate.category.trim()) warnings.category = '카테고리를 설정하는 것이 좋습니다';
    if (!dataToValidate.expression.trim()) warnings.expression = '수식을 작성하는 것이 좋습니다';

    if (dataToValidate.expression) {
      const matches = dataToValidate.expression.match(/[a-zA-Z_][a-zA-Z0-9_]*/g);
      const usedVariables = matches ? Array.from(new Set(matches)) : [];
      const definedVariables = dataToValidate.input_variables.map(v => v.input_name);
      const missingVariables = usedVariables.filter(v =>
        !definedVariables.includes(v) &&
        !['SUM', 'AVG', 'MIN', 'MAX', 'IF'].includes(v)
      );

      if (missingVariables.length > 0) {
        warnings.expression = `정의되지 않은 변수: ${missingVariables.join(', ')}`;
      }
    }

    setValidationErrors(errors);
    setValidationWarnings(warnings);
    return { errors, warnings };
  }, [formData]);

  const handleRunSimulation = useCallback(async (inputs?: Record<string, any>) => {
    if (!formData.expression.trim()) return;

    try {
      const context: Record<string, any> = {};
      // 폼에 정의된 변수들의 테스트 값을 컨텍스트로 구성
      formData.input_variables.forEach(input => {
        const val = inputs?.[input.variable_name] ?? (input.data_type === 'boolean' ? false : 0);
        context[input.variable_name] = val;
      });

      const response = await testScript(formData.expression, context);
      console.log('시뮬레이션 결과:', response);

      if (response && response.success) {
        setSimulationResult(response.result);
      } else {
        setSimulationResult(null);
        // 에러를 validationErrors에 표시할 수도 있음
        setValidationErrors(prev => ({
          ...prev,
          expression: response?.error || '계산 오류가 발생했습니다.'
        }));
      }
    } catch (error) {
      console.error('시뮬레이션 중 오류:', error);
      setSimulationResult(null);
    }
  }, [formData.expression, formData.input_variables, testScript]);

  const handleFormDataChange = useCallback((field: keyof FormData, value: any) => {
    setFormData(prev => ({ ...prev, [field]: value }));
  }, []);

  const handleInputVariableChange = useCallback((variables: VirtualPointInput[]) => {
    setFormData(prev => ({ ...prev, input_variables: variables }));
  }, []);

  const handleNextStep = () => { if (currentStep < totalSteps) setCurrentStep(currentStep + 1); };
  const handlePrevStep = () => { if (currentStep > 1) setCurrentStep(currentStep - 1); };
  const canProceedToNext = () => {
    switch (currentStep) {
      case 1: return !!formData.name.trim();
      case 3: return !!formData.expression.trim();
      default: return true;
    }
  };

  const handleDelete = async () => {
    if (!onDelete || !virtualPoint?.id) return;
    const confirmed = await confirm({
      title: '가상포인트 삭제',
      message: `'${formData.name}' 가상포인트를 삭제하시겠습니까?\n삭제된 항목은 '삭제된 항목 보기' 필터를 통해 복원할 수 있습니다.`,
      confirmText: '삭제',
      confirmButtonType: 'danger'
    });

    if (confirmed) {
      setIsSaving(true);
      try {
        await onDelete(virtualPoint.id);
        clearLocalStorage();
        onClose();
      } catch (error) {
        console.error('삭제 실패:', error);
      } finally {
        setIsSaving(false);
      }
    }
  };

  const handleRestore = async () => {
    if (!onRestore || !virtualPoint?.id) return;
    const confirmed = await confirm({
      title: '가상포인트 복원',
      message: `'${formData.name}' 가상포인트를 복원하시겠습니까?`,
      confirmText: '복원',
      confirmButtonType: 'primary'
    });

    if (confirmed) {
      setIsSaving(true);
      try {
        await onRestore(virtualPoint.id);
        onClose();
      } catch (error) {
        console.error('복원 실패:', error);
      } finally {
        setIsSaving(false);
      }
    }
  };

  const handleSave = async () => {
    const validation = validateForm();
    if (Object.keys(validation.errors).length > 0) {
      await confirm({
        title: '입력 오류',
        message: '필수 항목을 입력해주세요.',
        confirmText: '확인',
        showCancelButton: false
      });
      return;
    }

    // 서버 사이드 검증 결과 최종 확인
    if (validationResult && !validationResult.isValid) {
      await confirm({
        title: '수식 문법 오류',
        message: '수식에 오류가 있습니다. 3단계 또는 4단계에서 오류 내역을 확인하고 수정해 주세요.',
        confirmText: '확인',
        showCancelButton: false
      });
      return;
    }

    setIsSaving(true);
    try {
      await onSave(formData);
      setHasChanges(false);
      clearLocalStorage();
      onClose();
    } catch (error) {
      console.error('저장 실패:', error);
    } finally {
      setIsSaving(false);
    }
  };

  if (!isOpen) return null;

  const totalErrors = Object.keys(validationErrors).length;
  const totalWarnings = Object.keys(validationWarnings).length;

  return (
    <div className="modal-overlay" onClick={(e) => e.target === e.currentTarget && onClose()}>
      <div className="modal-container virtual-point-modal">
        <div className="modal-header">
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <h2 style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              {mode === 'create' ? '새 가상포인트 생성' : '가상포인트 편집'}
              {!!virtualPoint?.is_deleted && (
                <span className="badge danger" style={{ fontSize: '12px', padding: '2px 8px', borderRadius: '12px' }}>
                  <i className="fas fa-trash-alt"></i> 삭제됨
                </span>
              )}
            </h2>
          </div>
          <button className="modal-close-btn" onClick={onClose}><i className="fas fa-times"></i></button>
        </div>

        <div className="wizard-steps-header">
          {[1, 2, 3, 4].map((step) => (
            <div key={step} className={`wizard-step-item ${step === currentStep ? 'active' : ''} ${step < currentStep ? 'completed' : ''}`}>
              <div className="step-number">{step < currentStep ? '✓' : step}</div>
              <div className="step-label">{['IDENTITY', 'INPUT', 'FORMULA', 'FINALIZE'][step - 1]}</div>
            </div>
          ))}
        </div>

        <div className="wizard-content">
          {currentStep === 1 && <BasicInfoForm data={formData} onChange={handleFormDataChange} onApplyTemplate={handleApplyTemplate} errors={validationErrors} />}
          {currentStep === 2 && <InputVariableEditor variables={formData.input_variables} onChange={handleInputVariableChange} />}
          {currentStep === 3 && (
            <FormulaEditor
              expression={formData.expression}
              inputVariables={formData.input_variables}
              onChange={(expr) => { handleFormDataChange('expression', expr); setSimulationResult(null); }}
              validationResult={validationResult}
              isValidating={isValidating}
              simulationResult={simulationResult}
              isSimulating={isTesting}
              onRunSimulation={handleRunSimulation}
            />
          )}
          {currentStep === 4 && (
            <ValidationPanel
              formData={formData}
              validationResult={validationResult}
              onValidate={validateFormula}
              isValidating={isValidating}
              errors={validationErrors}
              warnings={validationWarnings}
              simulationResult={simulationResult}
              isSimulating={isTesting}
              onRunSimulation={handleRunSimulation}
            />
          )}
        </div>

        <div className="wizard-navigation">
          <div style={{ display: 'flex', gap: '8px' }}>
            <button className="wizard-nav-btn btn-prev" onClick={handlePrevStep} disabled={currentStep === 1}>이전</button>
            {mode === 'edit' && !virtualPoint?.is_deleted && currentStep === 1 && (
              <button
                className="wizard-nav-btn"
                onClick={handleDelete}
                style={{ background: '#fff1f2', color: '#be123c', borderColor: '#fecdd3' }}
              >
                <i className="fas fa-trash-alt"></i> 삭제
              </button>
            )}
            {mode === 'edit' && !!virtualPoint?.is_deleted && currentStep === 1 && (
              <button
                className="wizard-nav-btn"
                onClick={handleRestore}
                style={{ background: '#ecfdf5', color: '#10b981', borderColor: '#d1fae5' }}
              >
                <i className="fas fa-undo"></i> 복원
              </button>
            )}
          </div>
          <div style={{ flex: 1, display: 'flex', gap: '12px', alignItems: 'center', overflow: 'hidden' }}>
            {totalErrors > 0 && (
              <div className="error-indicator" style={{ display: 'flex', alignItems: 'center', gap: '6px', whiteSpace: 'nowrap' }}>
                <i className="fas fa-exclamation-circle"></i>
                <span style={{ fontWeight: 800 }}>{totalErrors} 오류:</span>
                <span style={{ fontSize: '12px', fontWeight: 400 }}>{Object.values(validationErrors).join(', ')}</span>
              </div>
            )}
            {totalWarnings > 0 && (
              <div className="warning-indicator" style={{ display: 'flex', alignItems: 'center', gap: '6px', whiteSpace: 'nowrap' }}>
                <i className="fas fa-exclamation-triangle"></i>
                <span style={{ fontWeight: 800 }}>{totalWarnings} 경고:</span>
                <span style={{ fontSize: '12px', fontWeight: 400 }}>{Object.values(validationWarnings).join(', ')}</span>
              </div>
            )}
          </div>
          {currentStep < totalSteps ? (
            <button className="wizard-nav-btn btn-next" onClick={handleNextStep} disabled={!canProceedToNext()}>다음</button>
          ) : (
            <button className="wizard-nav-btn btn-save" onClick={handleSave} disabled={isSaving || totalErrors > 0}>
              {isSaving ? '저장 중...' : (mode === 'create' ? '생성' : '저장')}
            </button>
          )}
        </div>
      </div>
    </div>
  );
};

export default VirtualPointModal;