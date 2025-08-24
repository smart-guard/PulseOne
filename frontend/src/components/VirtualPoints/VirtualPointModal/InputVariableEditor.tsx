// ============================================================================
// InputVariableEditor.tsx - API 연동 버전
// ============================================================================

import React, { useState, useCallback } from 'react';
import { VirtualPointInput } from '../../../types/virtualPoints';
import InputVariableSourceSelector from './InputVariableSourceSelector';

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
    if (!formData.variable_name?.trim()) {
      alert('변수명을 입력해주세요.');
      return;
    }

    if (formData.source_type !== 'constant' && !formData.source_id) {
      alert('데이터 소스를 선택해주세요.');
      return;
    }

    if (formData.source_type === 'constant' && formData.constant_value === undefined) {
      alert('상수값을 입력해주세요.');
      return;
    }

    const newVariable: VirtualPointInput = {
      id: editingIndex !== null ? variables[editingIndex].id : Date.now(),
      variable_name: formData.variable_name!,
      source_type: formData.source_type!,
      source_id: formData.source_id,
      constant_value: formData.constant_value,
      data_type: formData.data_type!,
      description: formData.description || '',
      is_required: formData.is_required ?? true,
      // API에서 가져온 소스 정보 추가
      source_name: formData.source_name
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
    setShowAddModal(false);
    setEditingIndex(null);
  }, [formData, editingIndex, variables, onChange]);

  // 소스 선택 핸들러
  const handleSourceSelect = useCallback((id: number, source: any) => {
    setFormData(prev => ({
      ...prev,
      source_id: id,
      source_name: source.name,
      data_type: source.data_type, // 선택된 소스의 데이터 타입으로 자동 설정
      description: prev.description || source.description // 설명이 비어있으면 소스 설명 사용
    }));
  }, []);

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
    <div style={{ padding: '20px' }}>
      
      {/* 헤더 */}
      <div style={{ 
        display: 'flex', 
        justifyContent: 'space-between', 
        alignItems: 'flex-start',
        marginBottom: '20px',
        paddingBottom: '16px',
        borderBottom: '1px solid #e9ecef'
      }}>
        <div>
          <h3 style={{ 
            margin: '0 0 8px 0',
            color: '#495057',
            display: 'flex',
            alignItems: 'center',
            gap: '8px'
          }}>
            <i className="fas fa-plug" style={{ color: '#007bff' }}></i>
            입력 변수 설정
          </h3>
          <p style={{ margin: 0, color: '#6c757d', fontSize: '14px' }}>
            가상포인트 계산에 사용할 데이터 소스를 설정합니다.
          </p>
        </div>
        
        <button
          onClick={handleAddVariable}
          style={{
            padding: '8px 16px',
            background: '#007bff',
            color: 'white',
            border: 'none',
            borderRadius: '6px',
            cursor: 'pointer',
            fontSize: '14px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}
        >
          <i className="fas fa-plus"></i>
          변수 추가
        </button>
      </div>

      {/* 변수 목록 */}
      {variables.length === 0 ? (
        <div style={{
          textAlign: 'center',
          padding: '60px 20px',
          background: '#f8f9fa',
          borderRadius: '8px',
          color: '#6c757d'
        }}>
          <i className="fas fa-info-circle" style={{ fontSize: '48px', marginBottom: '16px', color: '#dee2e6' }}></i>
          <h4 style={{ margin: '0 0 8px 0', color: '#495057' }}>입력 변수가 없습니다</h4>
          <p style={{ margin: '0 0 20px 0' }}>가상포인트 계산에 사용할 데이터 소스를 추가하세요.</p>
          <button
            onClick={handleAddVariable}
            style={{
              padding: '10px 20px',
              background: '#007bff',
              color: 'white',
              border: 'none',
              borderRadius: '6px',
              cursor: 'pointer',
              fontSize: '14px'
            }}
          >
            <i className="fas fa-plus" style={{ marginRight: '6px' }}></i>
            첫 번째 변수 추가
          </button>
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {variables.map((variable, index) => (
            <div key={variable.id} style={{
              background: 'white',
              border: '1px solid #e9ecef',
              borderRadius: '8px',
              padding: '16px',
              boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                
                {/* 변수 정보 */}
                <div style={{ flex: 1 }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '8px' }}>
                    <code style={{ 
                      background: '#e3f2fd',
                      color: '#1565c0',
                      padding: '4px 8px',
                      borderRadius: '4px',
                      fontWeight: 'bold',
                      fontSize: '14px'
                    }}>
                      {variable.variable_name}
                    </code>
                    
                    <span style={{
                      background: variable.source_type === 'data_point' ? '#e8f5e8' : 
                                variable.source_type === 'virtual_point' ? '#f3e5f5' : '#fff3cd',
                      color: variable.source_type === 'data_point' ? '#155724' : 
                             variable.source_type === 'virtual_point' ? '#6f42c1' : '#856404',
                      padding: '2px 8px',
                      borderRadius: '12px',
                      fontSize: '12px',
                      fontWeight: 'bold'
                    }}>
                      {variable.source_type === 'data_point' ? '📊 데이터포인트' : 
                       variable.source_type === 'virtual_point' ? '🔮 가상포인트' : '📝 상수'}
                    </span>
                    
                    <span style={{
                      background: '#f8f9fa',
                      color: '#495057',
                      padding: '2px 6px',
                      borderRadius: '8px',
                      fontSize: '11px'
                    }}>
                      {variable.data_type}
                    </span>
                    
                    {variable.is_required && (
                      <span style={{
                        background: '#dc3545',
                        color: 'white',
                        padding: '2px 6px',
                        borderRadius: '8px',
                        fontSize: '10px'
                      }}>
                        필수
                      </span>
                    )}
                  </div>
                  
                  <div style={{ marginBottom: '8px' }}>
                    <div style={{ fontSize: '14px', color: '#495057', marginBottom: '4px' }}>
                      <strong>소스:</strong> {variable.source_name || `ID ${variable.source_id}`}
                      {variable.source_type === 'constant' && ` = ${variable.constant_value}`}
                    </div>
                    {variable.description && (
                      <div style={{ fontSize: '13px', color: '#6c757d' }}>
                        <strong>설명:</strong> {variable.description}
                      </div>
                    )}
                  </div>
                  
                  {variable.current_value !== undefined && (
                    <div style={{
                      display: 'inline-block',
                      background: '#e8f5e8',
                      color: '#155724',
                      padding: '4px 8px',
                      borderRadius: '4px',
                      fontSize: '12px',
                      fontWeight: 'bold'
                    }}>
                      현재값: {variable.current_value}
                    </div>
                  )}
                </div>
                
                {/* 액션 버튼 */}
                <div style={{ display: 'flex', gap: '6px' }}>
                  <button
                    onClick={() => handleEditVariable(index)}
                    style={{
                      padding: '6px 10px',
                      background: '#17a2b8',
                      color: 'white',
                      border: 'none',
                      borderRadius: '4px',
                      cursor: 'pointer',
                      fontSize: '12px'
                    }}
                    title="편집"
                  >
                    <i className="fas fa-edit"></i>
                  </button>
                  <button
                    onClick={() => handleDeleteVariable(index)}
                    style={{
                      padding: '6px 10px',
                      background: '#dc3545',
                      color: 'white',
                      border: 'none',
                      borderRadius: '4px',
                      cursor: 'pointer',
                      fontSize: '12px'
                    }}
                    title="삭제"
                  >
                    <i className="fas fa-trash"></i>
                  </button>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}

      {/* 추가/편집 모달 */}
      {showAddModal && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'rgba(0,0,0,0.5)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            background: 'white',
            borderRadius: '8px',
            width: '90%',
            maxWidth: '800px',
            maxHeight: '90vh',
            overflow: 'auto',
            boxShadow: '0 4px 20px rgba(0,0,0,0.3)'
          }}>
            
            {/* 모달 헤더 */}
            <div style={{
              padding: '20px',
              borderBottom: '1px solid #e9ecef',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <h3 style={{ margin: 0 }}>
                <i className="fas fa-plug" style={{ marginRight: '8px', color: '#007bff' }}></i>
                {editingIndex !== null ? '입력 변수 편집' : '새 입력 변수 추가'}
              </h3>
              <button
                onClick={() => setShowAddModal(false)}
                style={{
                  padding: '6px',
                  background: 'none',
                  border: 'none',
                  cursor: 'pointer',
                  fontSize: '18px',
                  color: '#6c757d'
                }}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>
            
            {/* 모달 내용 */}
            <div style={{ padding: '20px' }}>
              
              {/* 변수명 입력 */}
              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', marginBottom: '6px', fontWeight: 'bold' }}>
                  변수명 <span style={{ color: '#dc3545' }}>*</span>
                </label>
                <input
                  type="text"
                  value={formData.variable_name || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, variable_name: e.target.value }))}
                  placeholder="예: temperature, pressure"
                  style={{
                    width: '100%',
                    padding: '10px',
                    border: '1px solid #dee2e6',
                    borderRadius: '6px',
                    fontSize: '14px'
                  }}
                />
                {formData.variable_name && (
                  <small style={{ 
                    color: validateVariableName(formData.variable_name) ? '#dc3545' : '#28a745',
                    fontSize: '12px',
                    marginTop: '4px',
                    display: 'block'
                  }}>
                    {validateVariableName(formData.variable_name) || '✓ 사용 가능한 변수명입니다'}
                  </small>
                )}
              </div>

              {/* 소스 타입 선택 */}
              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', marginBottom: '6px', fontWeight: 'bold' }}>
                  데이터 소스 타입 <span style={{ color: '#dc3545' }}>*</span>
                </label>
                <select
                  value={formData.source_type || 'data_point'}
                  onChange={(e) => setFormData(prev => ({ 
                    ...prev, 
                    source_type: e.target.value as any,
                    source_id: undefined,
                    source_name: undefined
                  }))}
                  style={{
                    width: '100%',
                    padding: '10px',
                    border: '1px solid #dee2e6',
                    borderRadius: '6px',
                    fontSize: '14px'
                  }}
                >
                  <option value="data_point">📊 데이터포인트 (센서, PLC 등)</option>
                  <option value="virtual_point">🔮 가상포인트 (계산된 값)</option>
                  <option value="constant">📝 상수값 (고정된 값)</option>
                </select>
              </div>

              {/* 상수값 입력 */}
              {formData.source_type === 'constant' && (
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', marginBottom: '6px', fontWeight: 'bold' }}>
                    상수값 <span style={{ color: '#dc3545' }}>*</span>
                  </label>
                  <input
                    type={formData.data_type === 'number' ? 'number' : 'text'}
                    value={formData.constant_value || ''}
                    onChange={(e) => {
                      const value = formData.data_type === 'number' ? 
                        (e.target.value ? Number(e.target.value) : undefined) : 
                        e.target.value;
                      setFormData(prev => ({ ...prev, constant_value: value }));
                    }}
                    placeholder={formData.data_type === 'number' ? '예: 25.5' : '예: "정상"'}
                    style={{
                      width: '100%',
                      padding: '10px',
                      border: '1px solid #dee2e6',
                      borderRadius: '6px',
                      fontSize: '14px'
                    }}
                  />
                </div>
              )}

              {/* 데이터 타입 */}
              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', marginBottom: '6px', fontWeight: 'bold' }}>
                  데이터 타입
                </label>
                <select
                  value={formData.data_type || 'number'}
                  onChange={(e) => setFormData(prev => ({ ...prev, data_type: e.target.value as any }))}
                  style={{
                    width: '100%',
                    padding: '10px',
                    border: '1px solid #dee2e6',
                    borderRadius: '6px',
                    fontSize: '14px'
                  }}
                >
                  <option value="number">숫자</option>
                  <option value="boolean">참/거짓</option>
                  <option value="string">문자열</option>
                </select>
              </div>

              {/* 소스 선택기 */}
              {formData.source_type !== 'constant' && (
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', marginBottom: '6px', fontWeight: 'bold' }}>
                    데이터 소스 선택 <span style={{ color: '#dc3545' }}>*</span>
                  </label>
                  <div style={{
                    border: '1px solid #dee2e6',
                    borderRadius: '6px',
                    padding: '12px',
                    background: '#fafbfc'
                  }}>
                    <InputVariableSourceSelector
                      sourceType={formData.source_type!}
                      selectedId={formData.source_id}
                      onSelect={handleSourceSelect}
                      dataType={formData.data_type}
                    />
                  </div>
                </div>
              )}

              {/* 설명 */}
              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', marginBottom: '6px', fontWeight: 'bold' }}>
                  설명
                </label>
                <textarea
                  value={formData.description || ''}
                  onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                  placeholder="이 변수에 대한 설명을 입력하세요"
                  rows={3}
                  style={{
                    width: '100%',
                    padding: '10px',
                    border: '1px solid #dee2e6',
                    borderRadius: '6px',
                    fontSize: '14px',
                    resize: 'vertical'
                  }}
                />
              </div>

              {/* 필수 여부 */}
              <div style={{ marginBottom: '20px' }}>
                <label style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  cursor: 'pointer'
                }}>
                  <input
                    type="checkbox"
                    checked={formData.is_required ?? true}
                    onChange={(e) => setFormData(prev => ({ ...prev, is_required: e.target.checked }))}
                  />
                  필수 변수로 설정
                </label>
                <small style={{ color: '#6c757d', fontSize: '12px', marginTop: '4px', display: 'block' }}>
                  필수 변수가 없으면 가상포인트 계산이 실행되지 않습니다
                </small>
              </div>
            </div>
            
            {/* 모달 푸터 */}
            <div style={{
              padding: '20px',
              borderTop: '1px solid #e9ecef',
              display: 'flex',
              justifyContent: 'flex-end',
              gap: '12px'
            }}>
              <button
                onClick={() => setShowAddModal(false)}
                style={{
                  padding: '10px 20px',
                  background: '#6c757d',
                  color: 'white',
                  border: 'none',
                  borderRadius: '6px',
                  cursor: 'pointer',
                  fontSize: '14px'
                }}
              >
                취소
              </button>
              <button
                onClick={handleSaveVariable}
                disabled={!formData.variable_name?.trim() || 
                         (formData.source_type !== 'constant' && !formData.source_id) ||
                         (formData.source_type === 'constant' && formData.constant_value === undefined)}
                style={{
                  padding: '10px 20px',
                  background: '#007bff',
                  color: 'white',
                  border: 'none',
                  borderRadius: '6px',
                  cursor: 'pointer',
                  fontSize: '14px',
                  opacity: (!formData.variable_name?.trim() || 
                           (formData.source_type !== 'constant' && !formData.source_id) ||
                           (formData.source_type === 'constant' && formData.constant_value === undefined)) ? 0.6 : 1
                }}
              >
                <i className="fas fa-save" style={{ marginRight: '6px' }}></i>
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