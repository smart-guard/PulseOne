// ============================================================================
// FormulaEditor - 함수 도우미 사이드 패널 포함
// ============================================================================

import React, { useState, useRef, useCallback } from 'react';

interface VirtualPointInput {
  id: number;
  variable_name: string;
  data_type: 'number' | 'boolean' | 'string';
  current_value?: any;
}

interface FormulaEditorProps {
  expression: string;
  inputVariables: VirtualPointInput[];
  onChange: (expression: string) => void;
}

interface FunctionItem {
  name: string;
  displayName: string;
  category: string;
  description: string;
  syntax: string;
  example: string;
}

const FormulaEditor: React.FC<FormulaEditorProps> = ({
  expression,
  inputVariables = [],
  onChange
}) => {
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const [testResult, setTestResult] = useState<any>(null);
  const [testing, setTesting] = useState(false);
  const [showFunctionHelper, setShowFunctionHelper] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedCategory, setSelectedCategory] = useState('all');

  // 함수 라이브러리
  const functions: FunctionItem[] = [
    {
      name: 'Math.max',
      displayName: '최댓값',
      category: 'math',
      description: '여러 값 중 최댓값을 반환합니다',
      syntax: 'Math.max(a, b, ...)',
      example: 'Math.max(10, 20, 5) → 20'
    },
    {
      name: 'Math.min',
      displayName: '최솟값',
      category: 'math',
      description: '여러 값 중 최솟값을 반환합니다',
      syntax: 'Math.min(a, b, ...)',
      example: 'Math.min(10, 20, 5) → 5'
    },
    {
      name: 'Math.abs',
      displayName: '절댓값',
      category: 'math',
      description: '절댓값을 반환합니다',
      syntax: 'Math.abs(value)',
      example: 'Math.abs(-5) → 5'
    },
    {
      name: 'Math.round',
      displayName: '반올림',
      category: 'math',
      description: '가장 가까운 정수로 반올림합니다',
      syntax: 'Math.round(value)',
      example: 'Math.round(4.7) → 5'
    },
    {
      name: 'Math.floor',
      displayName: '내림',
      category: 'math',
      description: '소수점 이하를 버립니다',
      syntax: 'Math.floor(value)',
      example: 'Math.floor(4.7) → 4'
    },
    {
      name: 'Math.ceil',
      displayName: '올림',
      category: 'math',
      description: '소수점 이하를 올림합니다',
      syntax: 'Math.ceil(value)',
      example: 'Math.ceil(4.1) → 5'
    },
    {
      name: 'Math.pow',
      displayName: '제곱',
      category: 'math',
      description: '거듭제곱을 계산합니다',
      syntax: 'Math.pow(base, exponent)',
      example: 'Math.pow(2, 3) → 8'
    },
    {
      name: 'Math.sqrt',
      displayName: '제곱근',
      category: 'math',
      description: '제곱근을 계산합니다',
      syntax: 'Math.sqrt(value)',
      example: 'Math.sqrt(16) → 4'
    }
  ];

  const categories = [
    { value: 'all', label: '모든 함수' },
    { value: 'math', label: '수학 함수' },
    { value: 'logic', label: '논리 함수' },
    { value: 'string', label: '문자열 함수' }
  ];

  // 실제 입력변수 사용
  const actualInputs = inputVariables.length > 0 ? inputVariables.map(input => ({
    ...input,
    current_value: input.current_value ?? (input.data_type === 'number' ? 0 : input.data_type === 'boolean' ? false : 'N/A')
  })) : [];

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

  // 함수 필터링
  const filteredFunctions = functions.filter(func => {
    const matchesCategory = selectedCategory === 'all' || func.category === selectedCategory;
    const matchesSearch = searchTerm === '' || 
      func.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      func.displayName.toLowerCase().includes(searchTerm.toLowerCase());
    
    return matchesCategory && matchesSearch;
  });

  // 수식 선택 핸들러
  const handleFormulaSelect = useCallback((selectedFormula: string) => {
    onChange(selectedFormula);
    setTimeout(() => {
      if (textareaRef.current) {
        textareaRef.current.focus();
      }
    }, 100);
  }, [onChange]);

  // 함수 삽입 핸들러
  const handleFunctionInsert = useCallback((functionSyntax: string) => {
    const textarea = textareaRef.current;
    if (!textarea) return;

    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const newFormula = expression.substring(0, start) + functionSyntax + expression.substring(end);
    
    onChange(newFormula);
    
    setTimeout(() => {
      const newPosition = start + functionSyntax.length;
      textarea.setSelectionRange(newPosition, newPosition);
      textarea.focus();
    }, 0);
  }, [expression, onChange]);

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

  // 테스트 핸들러
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

  // 결과 계산
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
      height: '600px',
      gap: '16px',
      fontFamily: 'Arial, sans-serif'
    }}>
      
      {/* 왼쪽: 함수 도우미 패널 */}
      <div style={{
        width: showFunctionHelper ? '320px' : '0px',
        transition: 'width 0.3s ease',
        overflow: 'hidden',
        background: 'white',
        border: '1px solid #dee2e6',
        borderRadius: '8px',
        display: 'flex',
        flexDirection: 'column'
      }}>
        
        {showFunctionHelper && (
          <>
            {/* 함수 도우미 헤더 */}
            <div style={{
              padding: '16px',
              borderBottom: '1px solid #dee2e6',
              background: '#f8f9fa'
            }}>
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '12px'
              }}>
                <h4 style={{ margin: 0, fontSize: '16px', color: '#495057' }}>
                  🔧 함수 도우미
                </h4>
                <button
                  onClick={() => setShowFunctionHelper(false)}
                  style={{
                    background: 'none',
                    border: 'none',
                    fontSize: '18px',
                    cursor: 'pointer',
                    color: '#6c757d'
                  }}
                >
                  ✕
                </button>
              </div>

              {/* 검색 */}
              <input
                type="text"
                placeholder="함수 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px 12px',
                  border: '1px solid #dee2e6',
                  borderRadius: '4px',
                  fontSize: '14px',
                  marginBottom: '8px'
                }}
              />

              {/* 카테고리 선택 */}
              <select
                value={selectedCategory}
                onChange={(e) => setSelectedCategory(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px 12px',
                  border: '1px solid #dee2e6',
                  borderRadius: '4px',
                  fontSize: '14px'
                }}
              >
                {categories.map(cat => (
                  <option key={cat.value} value={cat.value}>{cat.label}</option>
                ))}
              </select>
            </div>

            {/* 함수 목록 */}
            <div style={{
              flex: 1,
              overflow: 'auto',
              padding: '8px'
            }}>
              {filteredFunctions.length === 0 ? (
                <div style={{ textAlign: 'center', padding: '40px 20px', color: '#6c757d' }}>
                  검색된 함수가 없습니다
                </div>
              ) : (
                filteredFunctions.map((func, index) => (
                  <div
                    key={index}
                    onClick={() => handleFunctionInsert(func.syntax)}
                    style={{
                      padding: '12px',
                      margin: '4px 0',
                      background: 'white',
                      border: '1px solid #dee2e6',
                      borderRadius: '6px',
                      cursor: 'pointer',
                      transition: 'all 0.2s ease'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#e3f2fd';
                      e.currentTarget.style.borderColor = '#90caf9';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = 'white';
                      e.currentTarget.style.borderColor = '#dee2e6';
                    }}
                  >
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      marginBottom: '4px'
                    }}>
                      <div style={{ fontWeight: 'bold', color: '#495057', fontSize: '14px' }}>
                        {func.displayName}
                      </div>
                      <div style={{
                        fontSize: '10px',
                        padding: '2px 6px',
                        background: '#007bff',
                        color: 'white',
                        borderRadius: '10px'
                      }}>
                        {func.category.toUpperCase()}
                      </div>
                    </div>
                    
                    <div style={{
                      background: '#f1f3f4',
                      padding: '6px 8px',
                      borderRadius: '4px',
                      marginBottom: '6px'
                    }}>
                      <code style={{
                        fontSize: '12px',
                        color: '#495057',
                        fontFamily: 'Consolas, Monaco, monospace'
                      }}>
                        {func.syntax}
                      </code>
                    </div>
                    
                    <div style={{ fontSize: '12px', color: '#6c757d', marginBottom: '4px' }}>
                      {func.description}
                    </div>
                    
                    <div style={{ fontSize: '11px', color: '#28a745' }}>
                      예제: <code style={{ background: '#e8f5e8', padding: '1px 3px', borderRadius: '2px' }}>
                        {func.example}
                      </code>
                    </div>
                  </div>
                ))
              )}
            </div>
          </>
        )}
      </div>

      {/* 오른쪽: 메인 편집 영역 */}
      <div style={{ 
        flex: 1,
        display: 'flex', 
        flexDirection: 'column',
        gap: '16px'
      }}>
        
        {/* 함수 도우미 토글 버튼 */}
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <button
            onClick={() => setShowFunctionHelper(!showFunctionHelper)}
            style={{
              padding: '8px 16px',
              background: showFunctionHelper ? '#007bff' : '#6c757d',
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
            🔧 함수 도우미 {showFunctionHelper ? '숨기기' : '보기'}
          </button>
        </div>

        {/* 입력 변수 */}
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
              fontSize: '16px'
            }}>
              📊 사용 가능한 변수 (클릭해서 수식에 삽입)
            </h4>
            
            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
              {actualInputs.map((input) => (
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
              ⚠️ 입력 변수가 없습니다
            </div>
            <div style={{ fontSize: '14px', color: '#6c757d' }}>
              먼저 "입력 변수" 탭에서 계산에 사용할 데이터포인트를 설정하세요.
            </div>
          </div>
        )}

        {/* 수식 편집 */}
        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '8px', 
            fontWeight: 'bold',
            color: '#495057',
            fontSize: '16px'
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
              border: '2px solid #007bff',
              borderRadius: '8px',
              fontFamily: 'Consolas, Monaco, monospace',
              fontSize: '16px',
              lineHeight: '1.5',
              resize: 'none',
              background: '#fafbfc'
            }}
            placeholder="수식을 입력하세요... 예: (temp1 + temp2 + temp3) / 3"
          />
        </div>

        {/* 테스트 버튼과 결과 */}
        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-start' }}>
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
              whiteSpace: 'nowrap',
              minWidth: '140px'
            }}
          >
            {testing ? '🔄 계산 중...' : '🔍 수식 테스트'}
          </button>

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

        {/* 자주 사용하는 수식 */}
        <div style={{
          background: '#f8f9fa',
          padding: '16px',
          borderRadius: '8px',
          border: '1px solid #dee2e6'
        }}>
          <h4 style={{ 
            margin: '0 0 12px 0', 
            color: '#495057',
            fontSize: '14px'
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
      </div>
    </div>
  );
};

export default FormulaEditor;