// ============================================================================
// InputVariableEditor.tsx - Clarity & Guidance Focused Version
// ============================================================================

import React, { useState, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { VirtualPointInput } from '../../../types/virtualPoints';
import InputVariableSourceSelector from './InputVariableSourceSelector';
import { useConfirmContext } from '../../common/ConfirmProvider';
import './VirtualPointModal.css';

interface InputVariableEditorProps {
  variables: VirtualPointInput[];
  onChange: (variables: VirtualPointInput[]) => void;
  onMoveToFormulaTab?: () => void;
  onPendingChange?: (isPending: boolean) => void;
}

const InputVariableEditor: React.FC<InputVariableEditorProps> = ({
  variables,
  onChange,
  onMoveToFormulaTab,
  onPendingChange
}) => {
  const { confirm } = useConfirmContext();
    const { t } = useTranslation(['virtualPoints', 'common']);
  const [showAddModal, setShowAddModal] = useState(false);
  const [editingIndex, setEditingIndex] = useState<number | null>(null);
  const [formData, setFormData] = useState<Partial<VirtualPointInput>>({
    input_name: '',
    source_type: 'data_point',
    source_id: undefined,
    data_type: undefined,
    description: '',
    is_required: true,
    constant_value: ''
  });

  // 작성 중인 내용이 있는지 부모 컴포넌트에 알리기 위함
  React.useEffect(() => {
    if (onPendingChange) {
      const isPending = !!formData.input_name?.trim() || !!formData.source_id || !!formData.constant_value;
      onPendingChange(isPending && editingIndex === null);
    }
  }, [formData.input_name, formData.source_id, formData.constant_value, editingIndex, onPendingChange]);

  const handleAddVariable = useCallback(() => {
    setFormData({
      input_name: '',
      source_type: 'data_point',
      source_id: undefined,
      data_type: undefined,
      description: '',
      is_required: true,
      constant_value: ''
    });
    setEditingIndex(null);
    setShowAddModal(true);
  }, []);

  const handleEditVariable = useCallback((index: number) => {
    setFormData({ ...variables[index] });
    setEditingIndex(index);
    setShowAddModal(true);
  }, [variables]);

  const handleDeleteVariable = useCallback(async (index: number) => {
    const variable = variables[index];

    const isConfirmed = await confirm({
      title: 'Delete Input Variable',
      message: `Delete input variable "${variable.input_name}"?`,
      confirmText: 'Delete',
      cancelText: 'Cancel',
      confirmButtonType: 'danger'
    });

    if (isConfirmed) {
      const newVariables = variables.filter((_, i) => i !== index);
      onChange(newVariables);
    }
  }, [variables, onChange, confirm]);

  const handleSaveVariable = useCallback(async () => {
    if (!formData.input_name?.trim()) {
      await confirm({ title: 'Input Error', message: 'Please enter a variable name.', showCancelButton: false });
      return;
    }

    if (formData.source_type !== 'constant' && !formData.source_id) {
      await confirm({ title: 'Input Error', message: 'Please select a data source.', showCancelButton: false });
      return;
    }

    const newVariable: VirtualPointInput = {
      id: editingIndex !== null ? variables[editingIndex].id : Date.now(),
      virtual_point_id: editingIndex !== null ? variables[editingIndex].virtual_point_id : 0,
      input_name: formData.input_name!,
      source_type: formData.source_type!,
      source_id: formData.source_id,
      constant_value: formData.constant_value,
      data_type: formData.data_type!,
      description: formData.description || '',
      is_required: formData.is_required ?? true,
      source_name: formData.source_name,
      created_at: editingIndex !== null ? variables[editingIndex].created_at : new Date().toISOString()
    };

    let newVariables: VirtualPointInput[];
    if (editingIndex !== null) {
      newVariables = variables.map((variable, index) =>
        index === editingIndex ? newVariable : variable
      );
    } else {
      newVariables = [...variables, newVariable];
    }

    onChange(newVariables);

    // 등록 완료 후 폼 초기화
    setFormData({
      input_name: '',
      source_type: 'data_point',
      source_id: undefined,
      data_type: undefined,
      description: '',
      is_required: true,
      constant_value: ''
    });

    setEditingIndex(null);
    setShowAddModal(false);

    if (onMoveToFormulaTab && editingIndex === null) {
      const moveToFormula = await confirm({
        title: 'Tab Navigation',
        message: 'Variable added. Move to the formula step?',
        confirmText: 'Go',
        cancelText: 'Add More'
      });
      if (moveToFormula) onMoveToFormulaTab();
    }
  }, [formData, editingIndex, variables, onChange, onMoveToFormulaTab, confirm]);

  const handleSourceSelect = useCallback((id: number, source: any) => {
    setFormData(prev => {
      // 새로운 소스를 선택한 경우 (이전 소스 id와 다름)
      const isNewSource = prev.source_id !== id;

      let autoName = prev.input_name;

      // 이름이 아예 비어있거나, '다른 소스를 새로 클릭'했을 경우에는 새 소스 이름으로 덮어씀
      if (!autoName || isNewSource) {
        autoName = source.name
          .replace(/[^a-zA-Z0-9_]/g, '_')
          .replace(/^_+|_+$/g, '')
          .toLowerCase();

        if (!autoName || /^[0-9]/.test(autoName)) {
          autoName = 'var_' + (autoName || id);
        }
      }

      return {
        ...prev,
        input_name: autoName,
        source_id: id,
        source_name: source.name,
        // 센서 선택 시 해당 센서의 데이터 타입으로 필터 형식도 자동 변경
        data_type: source.data_type === 'number' || source.data_type === 'boolean' ? source.data_type : undefined,
        description: isNewSource ? source.description || '' : prev.description || source.description
      };
    });
  }, []);

  const validateVariableName = (name: string): string | null => {
    if (!name.trim()) return 'Variable name is required';
    if (!/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(name)) {
      return 'Only letters, numbers, and underscores allowed; must start with a letter';
    }
    if (variables.some((v, i) => v.input_name === name && i !== editingIndex)) {
      return 'Variable name is already in use';
    }
    return null;
  };

  return (
    <div className="wizard-slide-active" style={{ width: '100%', flex: 1, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
      <div className="formula-grid-v3" style={{ flex: 1, minHeight: 0 }}>

        {/* Column 1: Variable Identity & Config (The "Form") */}
        <div className="vp-popup-column" style={{
          background: '#f8fafc',
          borderRadius: '16px',
          border: '1px solid #e2e8f0',
          display: 'flex',
          flexDirection: 'column',
          height: '100%',
          overflow: 'hidden'
        }}>
          {/* Header */}
          <div style={{ padding: '16px 20px', borderBottom: '1px solid #e2e8f0', background: 'white', borderTopLeftRadius: '16px', borderTopRightRadius: '16px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <span style={{ fontSize: '13px', fontWeight: 800, color: '#334155' }}>
              {editingIndex !== null ? 'Edit Variable' : '1. Set Variable Info (left panel)'}
            </span>
          </div>

          {/* Form Body - Scrollable */}
          <div style={{ padding: '24px 20px', display: 'flex', flexDirection: 'column', gap: '24px', flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
            <div className="vp-vertical-group">
              <div className="label">Short name used in formula (Variable ID)</div>
              <input
                type="text"
                className="form-input"
                style={{ height: '40px' }}
                value={formData.input_name || ''}
                onChange={(e) => setFormData(prev => ({ ...prev, input_name: e.target.value }))}
                placeholder="Click an item in the center list to auto-fill."
              />
              {formData.input_name && validateVariableName(formData.input_name) && (
                <div style={{ color: '#e11d48', fontSize: '11px', marginTop: '4px', fontWeight: 600 }}>
                  <i className="fas fa-exclamation-circle"></i> {validateVariableName(formData.input_name)}
                </div>
              )}
            </div>

            <div className="vp-vertical-group">
              <div className="label">What value should this be based on?</div>
              <div className="choice-group-v3">
                <button className={`choice-btn-v3 ${formData.source_type === 'data_point' ? 'active' : ''}`} onClick={() => setFormData(p => ({ ...p, source_type: 'data_point', source_id: undefined }))}>
                  <i className="fas fa-broadcast-tower"></i>
                  <span style={{ display: 'inline-block', textAlign: 'center', lineHeight: '1.3' }}>
                    Realtime<br />Sensor Value
                  </span>
                </button>
                <button className={`choice-btn-v3 ${formData.source_type === 'constant' ? 'active' : ''}`} onClick={() => setFormData(p => ({ ...p, source_type: 'constant', source_id: undefined }))}>
                  <i className="fas fa-equals"></i>
                  <span style={{ display: 'inline-block', textAlign: 'center', lineHeight: '1.3' }}>
                    Fixed Number<br />(Constant)
                  </span>
                </button>
              </div>
            </div>

            <div className="vp-vertical-group">
              <div className="label">Data Type (Filter)</div>
              <div className="choice-group-v3">
                <button className={`choice-btn-v3 ${!formData.data_type ? 'active' : ''}`} onClick={() => setFormData(p => ({ ...p, data_type: undefined }))}>ALL</button>
                <button className={`choice-btn-v3 ${formData.data_type === 'number' ? 'active' : ''}`} onClick={() => setFormData(p => ({ ...p, data_type: 'number' }))}>Number</button>
                <button className={`choice-btn-v3 ${formData.data_type === 'boolean' ? 'active' : ''}`} onClick={() => setFormData(p => ({ ...p, data_type: 'boolean' }))}>Boolean</button>
              </div>
            </div>

            <div className="vp-vertical-group">
              <div className="label">Additional Notes</div>
              <textarea
                value={formData.description || ''}
                onChange={(e) => setFormData(p => ({ ...p, description: e.target.value }))}
                className="form-textarea"
                style={{ height: '80px' }}
                placeholder="Briefly describe the variable role..."
              />
            </div>
          </div>

          {/* Register Button (Pinned to Bottom) */}
          <div style={{ padding: '16px 20px', borderTop: '1px solid #e2e8f0', background: 'white', borderBottomLeftRadius: '16px', borderBottomRightRadius: '16px' }}>
            <button
              className="btn btn-primary"
              onClick={handleSaveVariable}
              style={{ width: '100%', height: '50px', fontSize: '15px', fontWeight: 800, borderRadius: '12px', boxShadow: '0 4px 15px rgba(13, 148, 136, 0.2)' }}
              disabled={!!(formData.input_name && validateVariableName(formData.input_name)) || !formData.input_name?.trim() || (formData.source_type === 'data_point' && !formData.source_id) || (formData.source_type === 'constant' && !formData.constant_value?.trim())}
            >
              <i className="fas fa-check" style={{ marginRight: '8px' }}></i>
              {editingIndex !== null ? 'Save Changes' : 'Register this variable'}
            </button>
            {editingIndex !== null && (
              <button className="btn btn-ghost" style={{ width: '100%', marginTop: '8px', height: '36px', fontSize: '13px' }} onClick={() => { setEditingIndex(null); handleAddVariable(); }}>
                Cancel & Create New
              </button>
            )}
          </div>
        </div>

        {/* Column 2: Source Explorer */}
        <div className="vp-popup-column" style={{ minWidth: 0 }}>
          <div className="vp-popup-column-title">2. Search Sensor List (Center)</div>
          <div style={{ background: 'white', border: '1px solid #e2e8f0', borderRadius: '16px', minHeight: '500px' }}>
            {formData.source_type === 'constant' ? (
              <div className="concept-group-card" style={{ padding: '40px', textAlign: 'center' }}>
                <i className="fas fa-equals" style={{ fontSize: '40px', color: '#e2e8f0', marginBottom: '16px' }}></i>
                <div className="label">Enter Constant Value</div>
                <input type="text" className="form-input" style={{ fontSize: '20px', padding: '16px', textAlign: 'center' }} placeholder="수식에 사용될 고정값" value={formData.constant_value || ''} onChange={(e) => setFormData(p => ({ ...p, constant_value: e.target.value }))} />
              </div>
            ) : (
              <InputVariableSourceSelector
                sourceType={formData.source_type!}
                selectedId={formData.source_id}
                onSelect={handleSourceSelect}
                dataType={formData.data_type}
              />
            )}
          </div>
        </div>

        {/* Column 3: Variable List */}
        <div className="vp-popup-column" style={{ minWidth: 0 }}>
          <div className="vp-popup-column-title">3. 장바구니 리스트 ({variables.length})</div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
            {variables.length === 0 ? (
              <div style={{ textAlign: 'center', padding: '32px', background: '#f8fafc', borderRadius: '12px', border: '1px dashed #e2e8f0' }}>
                <p style={{ color: '#94a3b8', fontSize: '13px' }}>리스트가 비어 있습니다.<br />왼쪽 폼에서 변수를 등록하세요.</p>
              </div>
            ) : (
              variables.map((variable, index) => (
                <div
                  key={variable.id}
                  className={`variable-premium-card ${editingIndex === index ? 'active' : ''}`}
                  style={{
                    padding: '16px',
                    cursor: 'pointer',
                    background: editingIndex === index ? '#f0f9ff' : 'white'
                  }}
                  onClick={() => handleEditVariable(index)}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                    <span className="var-name-badge">{variable.input_name}</span>
                    <button className="btn-icon danger sm" onClick={(e) => { e.stopPropagation(); handleDeleteVariable(index); }}><i className="fas fa-trash"></i></button>
                  </div>
                  <div style={{ fontSize: '12px', color: '#64748b' }}>
                    {variable.source_name || `상수: ${variable.constant_value}`}
                  </div>
                </div>
              ))
            )}

            <div className="vp-popup-guide-panel" style={{ marginTop: '10px' }}>
              <div style={{ fontSize: '13px', fontWeight: 800, color: '#334155', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <i className="fas fa-lightbulb" style={{ color: '#f59e0b' }}></i> 사용 방법
              </div>
              <p style={{ fontSize: '12px', color: '#64748b', margin: 0, lineHeight: '1.5' }}>
                1. 왼쪽에서 <strong>정보 입력</strong><br />
                2. 리스트에 <strong>등록 완료</strong><br />
                3. 마지막에 하단 <strong>[다음]</strong> 클릭
              </p>
            </div>
          </div>
        </div>

      </div>
    </div>
  );
};

export default InputVariableEditor;