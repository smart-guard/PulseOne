// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointModal/InputVariableEditor.tsx
// 입력 변수 편집기 컴포넌트
// ============================================================================

import React, { useState, useCallback } from 'react';
import { VirtualPointInput } from '../../../types/virtualPoints';

interface InputVariableEditorProps {
  variables: VirtualPointInput[];
  onChange: (variables: VirtualPointInput[]) => void;
}

const InputVariableEditor: React.FC<InputVariableEditorProps> = ({
  variables,
  onChange
}) => {
  const [showAddModal, setShowAddModal] = useState(false);
  const [editingIndex, setEditingIndex] = useState<number | null>(null);
  const [formData, setFormData] = useState<Partial<VirtualPointInput>>({
    variable_name: '',
    source_type: 'data_point',
    source_id: undefined,
    data_type: 'number',
    description: '',
    is_required: true
  });

  // ========================================================================
  // 이벤트 핸들러
  // ========================================================================
  
  const handleAddVariable = useCallback(() => {
    setFormData({
      variable_name: '',
      source_type: 'data_point',
      source_id: undefined,
      data_type: 'number',
      description: '',
      is_required: true
    });
    setEditingIndex(null);
    setShowAddModal(true);
  }, []);

  const handleEditVariable = useCallback((index: number) => {
    setFormData({ ...variables[index] });
    setEditingIndex(index);
    setShowAddModal(true);
  }, [variables]);

  const handleDeleteVariable = useCallback((index: number) => {
    if (window.confirm('이 입력 변수를 삭제하시겠습니까?')) {
      const newVariables = variables.filter((_, i) => i !== index);
      onChange(newVariables);
    }
  }, [variables, onChange]);

  const handleSaveVariable = useCallback(() => {
    if (!formData.variable_name?.trim() || formData.source_id === undefined) {
      alert('필수 필드를 모두 입력해주세요.');
      return;
    }

    const newVariable: VirtualPointInput = {
      id: editingIndex !== null ? variables[editingIndex].id : Date.now(),
      variable_name: formData.variable_name!,
      source_type: formData.source_type!,
      source_id: formData.source_id!,
      data_type: formData.data_type!,
      description: formData.description || '',
      is_required: formData.is_required ?? true
    };

    let newVariables: VirtualPointInput[];
    if (editingIndex !== null) {
      // 편집 모드
      newVariables = variables.map((variable, index) =>
        index === editingIndex ? newVariable : variable
      );
    } else {
      // 추가 모드
      newVariables = [...variables, newVariable];
    }

    onChange(newVariables);
    setShowAddModal(false);
    setEditingIndex(null);
  }, [formData, editingIndex, variables, onChange]);

  const handleMoveVariable = useCallback((fromIndex: number, toIndex: number) => {
    if (toIndex < 0 || toIndex >= variables.length) return;
    
    const newVariables = [...variables];
    const [moved] = newVariables.splice(fromIndex, 1);
    newVariables.splice(toIndex, 0, moved);
    onChange(newVariables);
  }, [variables, onChange]);

  const validateVariableName = (name: string): string | null => {
    if (!name.trim()) return '변수명은 필수입니다';
    if (!/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(name)) {
      return '변수명은 문자, 숫자, 언더스코어만 사용 가능하며 문자로 시작해야 합니다';
    }
    if (variables.some((v, i) => v.variable_name === name && i !== editingIndex)) {
      return '이미 사용 중인 변수명입니다';
    }
    return null;
  };

  // ========================================================================
  // 렌더링
  // ========================================================================
  
  return (
    <div className="input-variable-editor">
      {/* 헤더 */}
      <div className="editor-header">
        <div className="header-info">
          <h3>
            <i className="fas fa-plug"></i>
            입력 변수 설정
          </h3>
          <p>가상포인트 계산에 사용할 입력 데이터를 설정합니다.</p>
        </div>
        
        <button
          type="button"
          className="btn-primary"
          onClick={handleAddVariable}
        >
          <i className="fas fa-plus"></i>
          변수 추가
        </button>
      </div>

      {/* 변수 목록 */}
      {variables.length === 0 ? (
        <div className="empty-state">
          <i className="fas fa-info-circle"></i>
          <h4>입력 변수가 없습니다</h4>
          <p>가상포인트 계산에 사용할 데이터 소스를 추가하세요.</p>
          <button
            type="button"
            className="btn-primary"
            onClick={handleAddVariable}
          >
            <i className="fas fa-plus"></i>
            첫 번째 변수 추가
          </button>
        </div>
      ) : (
        <div className="variable-list">
          {variables.map((variable, index) => (
            <div key={variable.id} className="variable-item">
              <div className="variable-handle">
                <i className="fas fa-grip-vertical"></i>
              </div>
              
              <div className="variable-content">
                <div className="variable-header">
                  <div className="variable-name">
                    <code>{variable.variable_name}</code>
                    {variable.is_required && (
                      <span className="required-badge">필수</span>
                    )}
                  </div>
                  <div className="variable-type">
                    <span className={`type-badge ${variable.data_type}`}>
                      {variable.data_type}
                    </span>
                  </div>
                </div>
                
                <div className="variable-details">
                  <div className="detail-item">
                    <span className="detail-label">소스 타입:</span>
                    <span className="detail-value">
                      {variable.source_type === 'data_point' ? '데이터포인트' : 
                       variable.source_type === 'virtual_point' ? '가상포인트' : '상수'}
                    </span>
                  </div>
                  <div className="detail-item">
                    <span className="detail-label">소스 ID:</span>
                    <span className="detail-value">{variable.source_id}</span>
                  </div>
                  {variable.description && (
                    <div className="detail-item">
                      <span className="detail-label">설명:</span>
                      <span className="detail-value">{variable.description}</span>
                    </div>
                  )}
                </div>
              </div>
              
              <div className="variable-actions">
                <button
                  type="button"
                  className="action-btn move-up"
                  onClick={() => handleMoveVariable(index, index - 1)}
                  disabled={index === 0}
                  title="위로 이동"
                >
                  <i className="fas fa-arrow-up"></i>
                </button>
                <button
                  type="button"
                  className="action-btn move-down"
                  onClick={() => handleMoveVariable(index, index + 1)}
                  disabled={index === variables.length - 1}
                  title="아래로 이동"
                >
                  <i className="fas fa-arrow-down"></i>
                </button>
                <button
                  type="button"
                  className="action-btn edit"
                  onClick={() => handleEditVariable(index)}
                  title="편집"
                >
                  <i className="fas fa-edit"></i>
                </button>
                <button
                  type="button"
                  className="action-btn delete"
                  onClick={() => handleDeleteVariable(index)}
                  title="삭제"
                >
                  <i className="fas fa-trash"></i>
                </button>
              </div>
            </div>
          ))}
        </div>
      )}

      {/* 추가/편집 모달 */}
      {showAddModal && (
        <div className="modal-overlay" onClick={(e) => e.target === e.currentTarget && setShowAddModal(false)}>
          <div className="modal-container variable-modal">
            <div className="modal-header">
              <h3>
                <i className="fas fa-plug"></i>
                {editingIndex !== null ? '입력 변수 편집' : '새 입력 변수 추가'}
              </h3>
              <button
                className="modal-close-btn"
                onClick={() => setShowAddModal(false)}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>
            
            <div className="modal-content">
              <div className="form-section">
                <div className="form-grid">
                  <div className="form-group">
                    <label className="required">변수명</label>
                    <input
                      type="text"
                      value={formData.variable_name || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, variable_name: e.target.value }))}
                      className="form-input"
                      placeholder="예: temperature, pressure"
                      pattern="[a-zA-Z_][a-zA-Z0-9_]*"
                    />
                    {formData.variable_name && (
                      <small className={`form-hint ${validateVariableName(formData.variable_name) ? 'error' : 'success'}`}>
                        {validateVariableName(formData.variable_name) || '사용 가능한 변수명입니다'}
                      </small>
                    )}
                  </div>

                  <div className="form-group">
                    <label className="required">데이터 타입</label>
                    <select
                      value={formData.data_type || 'number'}
                      onChange={(e) => setFormData(prev => ({ ...prev, data_type: e.target.value as any }))}
                      className="form-select"
                    >
                      <option value="number">숫자</option>
                      <option value="boolean">참/거짓</option>
                      <option value="string">문자열</option>
                    </select>
                  </div>

                  <div className="form-group">
                    <label className="required">소스 타입</label>
                    <select
                      value={formData.source_type || 'data_point'}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        source_type: e.target.value as any,
                        source_id: undefined 
                      }))}
                      className="form-select"
                    >
                      <option value="data_point">데이터포인트</option>
                      <option value="virtual_point">가상포인트</option>
                      <option value="constant">상수값</option>
                    </select>
                  </div>

                  <div className="form-group">
                    <label className="required">
                      {formData.source_type === 'constant' ? '상수값' : '소스 ID'}
                    </label>
                    {formData.source_type === 'constant' ? (
                      <input
                        type={formData.data_type === 'number' ? 'number' : 'text'}
                        value={formData.source_id || ''}
                        onChange={(e) => {
                          const value = formData.data_type === 'number' 
                            ? parseFloat(e.target.value) || 0
                            : formData.data_type === 'boolean'
                            ? e.target.value === 'true'
                            : e.target.value;
                          setFormData(prev => ({ ...prev, source_id: value as any }));
                        }}
                        className="form-input"
                        placeholder="상수값을 입력하세요"
                      />
                    ) : (
                      <input
                        type="number"
                        value={formData.source_id || ''}
                        onChange={(e) => setFormData(prev => ({ 
                          ...prev, 
                          source_id: parseInt(e.target.value) || undefined 
                        }))}
                        className="form-input"
                        placeholder="해당 포인트의 ID"
                        min={1}
                      />
                    )}
                  </div>

                  <div className="form-group full-width">
                    <label>설명</label>
                    <input
                      type="text"
                      value={formData.description || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                      className="form-input"
                      placeholder="변수에 대한 설명 (선택사항)"
                      maxLength={200}
                    />
                  </div>

                  <div className="form-group">
                    <label className="checkbox-label">
                      <input
                        type="checkbox"
                        checked={formData.is_required ?? true}
                        onChange={(e) => setFormData(prev => ({ ...prev, is_required: e.target.checked }))}
                        className="form-checkbox"
                      />
                      필수 변수
                    </label>
                    <small className="form-hint">
                      필수 변수가 없으면 가상포인트 계산이 실행되지 않습니다
                    </small>
                  </div>
                </div>
              </div>
            </div>
            
            <div className="modal-footer">
              <button
                type="button"
                className="btn-secondary"
                onClick={() => setShowAddModal(false)}
              >
                취소
              </button>
              <button
                type="button"
                className="btn-primary"
                onClick={handleSaveVariable}
                disabled={!formData.variable_name?.trim() || formData.source_id === undefined}
              >
                <i className="fas fa-save"></i>
                {editingIndex !== null ? '저장' : '추가'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default InputVariableEditor;