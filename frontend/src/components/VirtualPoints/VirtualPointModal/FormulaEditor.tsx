// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointModal/FormulaEditor.tsx
// 간단하고 직관적인 가상포인트 수식 편집기 (완전 수정 버전)
// ============================================================================

import React, { useState, useRef, useCallback } from 'react';
import { VirtualPointInput, ScriptFunction, ScriptValidationResult } from '../../../types/virtualPoints';

interface FormulaEditorProps {
  expression: string;
  inputVariables: VirtualPointInput[];
  functions?: ScriptFunction[];
  onChange: (expression: string) => void;
  validationResult?: ScriptValidationResult | null;
  isValidating?: boolean;
}

const FormulaEditor: React.FC<FormulaEditorProps> = ({
  expression,
  inputVariables = [],
  functions = [],
  onChange,
  validationResult,
  isValidating = false
}) => {
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const [testResult, setTestResult] = useState<any>(null);
  const [testing, setTesting] = useState(false);

  // 실제 입력변수 사용 (목 데이터 제거)
  const actualInputs = inputVariables.length > 0 ? inputVariables.map(input => ({
    ...input,
    // 실제 현재값이 없으면 샘플값 표시 (개발용)
    current_value: input.current_value ?? (input.data_type === 'number' ? 0 : input.data_type === 'boolean' ? false : 'N/A')
  })) : [];

  // 입력변수가 없을 때 메시지
  const hasVariables = actualInputs.length > 0;

  // 수식 템플릿
  const commonFormulas = [
    { name: '평균값', formula: '(temp1 + temp2 + temp3) / 3', desc: '여러 센서의 평균값', icon: '📊' },
    { name: '최댓값', formula: 'Math.max(temp1, temp2, temp3)', desc: '가장 높은 값', icon: '📈' },
    { name: '최솟값', formula: 'Math.min(temp1, temp2, temp3)', desc: '가장 낮은 값', icon: '📉' },
    { name: '차이값', formula: 'Math.abs(temp1 - temp2)', desc: '두 값의 절대 차이', icon: '🔄' },
    { name: '조건값', formula: 'temp1 > 30 ? "높음" : "정상"', desc: '조건에 따른 결과', icon: '❓' },
    { name: '범위제한', formula: 'Math.max(0, Math.min(100, temp1))', desc: '0-100 사이로 제한', icon: '🎯' }
  ];

  // 수식 선택 핸들러
  const handleFormulaSelect = useCallback((selectedFormula: string) => {
    onChange(selectedFormula);
    setTimeout(() => {
      if (textareaRef.current) {
        textareaRef.current.focus();
      }
    }, 100);
  }, [onChange]);

  // 변수 삽입 핸들러
  const handleVariableInsert = useCallback((variableName: string) => {
    const textarea = textareaRef.current;
    if (!textarea) return;

    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const newFormula = expression.substring(0, start) + variableName + expression.substring(end);
    
    onChange(newFormula);
    
    setTimeout(() => {
      const newPosition = start + variableName.length;
      textarea.setSelectionRange(newPosition, newPosition);
      textarea.focus();
    }, 0);
  }, [expression, onChange]);

  // 테스트와 계산에서 실제 입력변수 사용
  const handleTest = useCallback(async () => {
    if (!hasVariables) {
      alert('먼저 입력 변수를 설정해주세요.');
      return;
    }
    
    setTesting(true);
    try {
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      let testFormula = expression;
      actualInputs.forEach(input => {
        const regex = new RegExp(`\\b${input.variable_name}\\b`, 'g');
        const value = input.current_value ?? (input.data_type === 'number' ? 0 : input.data_type === 'boolean' ? false : '""');
        testFormula = testFormula.replace(regex, String(value));
      });
      
      const result = eval(testFormula);
      setTestResult(result);
    } catch (error) {
      setTestResult('계산 오류: ' + (error as Error).message);
    } finally {
      setTesting(false);
    }
  }, [expression, actualInputs, hasVariables]);

  // 결과 계산에서도 실제 입력변수 사용
  const calculateResult = useCallback(() => {
    if (!hasVariables || !expression.trim()) {
      return '입력 변수 필요';
    }
    
    try {
      let testFormula = expression;
      actualInputs.forEach(input => {
        const regex = new RegExp(`\\b${input.variable_name}\\b`, 'g');
        const value = input.current_value ?? (input.data_type === 'number' ? 0 : input.data_type === 'boolean' ? false : '""');
        testFormula = testFormula.replace(regex, String(value));
      });
      return eval(testFormula);
    } catch (error) {
      return '수식 오류';
    }
  }, [expression, actualInputs, hasVariables]);

  const simulatedResult = calculateResult();

  return (
    <div style={{ 
      display: 'flex', 
      flexDirection: 'column',
      gap: '20px', 
      height: 'auto',
      fontFamily: 'Arial, sans-serif',
      padding: '0'
    }}>
      
      {/* 상단: 입력 데이터 - 실제 데이터 사용 */}
      {hasVariables ? (
        <div style={{
          background: 'linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%)',
          padding: '16px',
          borderRadius: '8px',
          border: '1px solid #dee2e6'
        }}>
          <h4 style={{ 
            margin: '0 0 12px 0', 
            color: '#495057',
            fontSize: '16px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}>
            📊 사용 가능한 변수 (클릭해서 수식에 삽입)
          </h4>
          
          <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
            {actualInputs.map((input, index) => (
              <div key={input.id} 
                onClick={() => handleVariableInsert(input.variable_name)}
                style={{
                  background: 'white',
                  padding: '8px 12px',
                  borderRadius: '20px',
                  border: '1px solid #e9ecef',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  transition: 'all 0.2s ease',
                  fontSize: '14px'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.background = '#e3f2fd';
                  e.currentTarget.style.borderColor = '#90caf9';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.background = 'white';
                  e.currentTarget.style.borderColor = '#e9ecef';
                }}
              >
                <span style={{ fontWeight: 'bold', color: '#007bff' }}>
                  {input.variable_name}
                </span>
                <span style={{ color: '#6c757d', fontSize: '12px' }}>
                  ({input.data_type})
                </span>
                {input.current_value !== undefined && (
                  <span style={{ color: '#28a745', fontWeight: 'bold' }}>
                    = {input.current_value}
                  </span>
                )}
              </div>
            ))}
          </div>
        </div>
      ) : (
        <div style={{
          background: '#fff3cd',
          padding: '16px',
          borderRadius: '8px',
          border: '1px solid #ffeaa7',
          textAlign: 'center'
        }}>
          <div style={{ color: '#856404', marginBottom: '8px' }}>
            ⚠️ <strong>입력 변수가 없습니다</strong>
          </div>
          <div style={{ fontSize: '14px', color: '#6c757d' }}>
            먼저 "입력 변수" 탭에서 계산에 사용할 데이터포인트를 설정하세요.
          </div>
        </div>
      )}

      {/* 중앙: 수식 편집 (세로로 변경) */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        
        {/* 수식 입력 */}
        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '8px', 
            fontWeight: 'bold',
            color: '#495057',
            fontSize: '16px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}>
            🧮 계산 수식
          </label>
          <textarea
            ref={textareaRef}
            value={expression}
            onChange={(e) => onChange(e.target.value)}
            style={{
              width: '100%',
              height: '100px',
              padding: '16px',
              border: validationResult && !validationResult.isValid ? '2px solid #dc3545' : '2px solid #007bff',
              borderRadius: '8px',
              fontFamily: 'Consolas, Monaco, monospace',
              fontSize: '16px',
              lineHeight: '1.5',
              resize: 'none',
              background: validationResult && !validationResult.isValid ? '#fff5f5' : '#fafbfc'
            }}
            placeholder="수식을 입력하세요... 예: (temp1 + temp2 + temp3) / 3"
          />

          {/* 유효성 검사 결과 */}
          {validationResult && !validationResult.isValid && (
            <div style={{
              marginTop: '8px',
              padding: '10px 12px',
              background: '#f8d7da',
              border: '1px solid #f5c6cb',
              borderRadius: '6px',
              color: '#721c24',
              fontSize: '14px'
            }}>
              <strong>수식 오류:</strong> {validationResult.errors[0]?.message || '구문 오류가 있습니다'}
            </div>
          )}
        </div>

        {/* 테스트 버튼과 결과를 한 줄에 */}
        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-start' }}>
          
          {/* 테스트 버튼 */}
          <button
            onClick={handleTest}
            disabled={testing || !expression.trim()}
            style={{
              padding: '12px 24px',
              background: testing ? '#6c757d' : '#28a745',
              color: 'white',
              border: 'none',
              borderRadius: '8px',
              cursor: testing ? 'not-allowed' : 'pointer',
              fontSize: '16px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px',
              whiteSpace: 'nowrap',
              minWidth: '140px'
            }}
          >
            {testing ? '🔄 계산 중...' : '🔍 수식 테스트'}
          </button>

          {/* 결과 */}
          <div style={{
            flex: 1,
            padding: '20px',
            background: '#e8f5e8',
            borderRadius: '8px',
            border: '2px solid #28a745',
            textAlign: 'center'
          }}>
            <div style={{ 
              fontSize: '14px', 
              color: '#155724', 
              marginBottom: '6px',
              fontWeight: 'bold'
            }}>
              📈 계산 결과
            </div>
            <div style={{ 
              fontSize: '32px', 
              fontWeight: 'bold', 
              color: '#28a745',
              fontFamily: 'monospace'
            }}>
              {testResult !== null ? testResult : simulatedResult}
            </div>
          </div>
        </div>

        {/* 계산 과정 */}
        <div style={{
          background: '#f8f9fa',
          padding: '16px',
          borderRadius: '8px',
          border: '1px solid #dee2e6'
        }}>
          <div style={{ 
            fontSize: '14px', 
            color: '#495057',
            fontWeight: 'bold',
            marginBottom: '12px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}>
            🔢 계산 과정
          </div>
          <div style={{ 
            fontFamily: 'Consolas, Monaco, monospace',
            fontSize: '14px',
            lineHeight: '1.6',
            background: 'white',
            padding: '16px',
            borderRadius: '6px',
            border: '1px solid #e9ecef'
                      }}>
              {hasVariables ? (
                actualInputs.map((input, idx) => (
                  <div key={input.id} style={{ color: '#6c757d', marginBottom: '4px' }}>
                    {input.variable_name} = {input.current_value ?? 'N/A'} ({input.data_type})
                  </div>
                ))
              ) : (
                <div style={{ color: '#856404', fontStyle: 'italic' }}>
                  입력 변수를 먼저 설정하세요
                </div>
              )}
            <div style={{ 
              borderTop: '1px solid #dee2e6', 
              marginTop: '12px', 
              paddingTop: '12px',
              color: '#495057'
            }}>
              <div style={{ marginBottom: '4px' }}>수식: {expression}</div>
              <div style={{ 
                color: '#28a745',
                fontWeight: 'bold'
              }}>
                결과: {testResult !== null ? testResult : simulatedResult}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* 하단: 수식 템플릿 */}
      <div style={{
        background: '#f8f9fa',
        padding: '16px',
        borderRadius: '8px',
        border: '1px solid #dee2e6'
      }}>
        <h4 style={{ 
          margin: '0 0 12px 0', 
          color: '#495057',
          fontSize: '14px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px'
        }}>
          💡 자주 사용하는 수식 (클릭해서 적용)
        </h4>

        <div style={{ 
          display: 'flex', 
          gap: '8px', 
          overflowX: 'auto',
          paddingBottom: '4px'
        }}>
          {commonFormulas.map((template, index) => (
            <div
              key={index}
              onClick={() => handleFormulaSelect(template.formula)}
              style={{
                minWidth: '140px',
                padding: '12px',
                background: 'white',
                border: '1px solid #dee2e6',
                borderRadius: '6px',
                cursor: 'pointer',
                transition: 'all 0.2s ease',
                textAlign: 'center'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.background = '#e3f2fd';
                e.currentTarget.style.borderColor = '#90caf9';
                e.currentTarget.style.transform = 'scale(1.02)';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.background = 'white';
                e.currentTarget.style.borderColor = '#dee2e6';
                e.currentTarget.style.transform = 'scale(1)';
              }}
            >
              <div style={{ fontSize: '16px', marginBottom: '4px' }}>
                {template.icon}
              </div>
              <div style={{ 
                fontWeight: 'bold', 
                color: '#1976d2',
                fontSize: '12px',
                marginBottom: '4px'
              }}>
                {template.name}
              </div>
              <div style={{ 
                fontSize: '9px', 
                color: '#6c757d',
                lineHeight: '1.2'
              }}>
                {template.desc}
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* CSS 스타일 */}
      <style>{`
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};

export default FormulaEditor;