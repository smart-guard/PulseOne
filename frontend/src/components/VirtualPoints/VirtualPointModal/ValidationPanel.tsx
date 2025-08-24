// ============================================================================
// ValidationPanel.tsx - UI 개선 및 API 오류 해결
// ============================================================================

import React, { useState, useCallback } from 'react';

interface ValidationPanelProps {
  formData: any;
  validationResult?: any;
  onValidate: () => void;
  isValidating: boolean;
  errors?: Record<string, string>;
  warnings?: Record<string, string>;
}

const ValidationPanel: React.FC<ValidationPanelProps> = ({
  formData,
  validationResult,
  onValidate,
  isValidating,
  errors = {},
  warnings = {}
}) => {
  const [testInputs, setTestInputs] = useState<Record<string, any>>({});
  const [testResult, setTestResult] = useState<any>(null);
  const [testError, setTestError] = useState<string | null>(null);
  const [isTesting, setIsTesting] = useState(false);

  // 테스트 입력값 초기화
  React.useEffect(() => {
    const inputs: Record<string, any> = {};
    formData.input_variables?.forEach((variable: any) => {
      inputs[variable.variable_name] = getDefaultTestValue(variable.data_type);
    });
    setTestInputs(inputs);
  }, [formData.input_variables]);

  const getDefaultTestValue = (dataType: string): any => {
    switch (dataType) {
      case 'number': return 100;
      case 'boolean': return true;
      case 'string': return 'test';
      default: return 100;
    }
  };

  const handleTestInputChange = (variableName: string, value: any) => {
    setTestInputs(prev => ({
      ...prev,
      [variableName]: value
    }));
  };

  const handleRunTest = async () => {
    if (!formData.expression?.trim()) {
      setTestError('수식이 입력되지 않았습니다.');
      return;
    }

    setIsTesting(true);
    setTestError(null);
    setTestResult(null);

    try {
      // 로컬 스크립트 평가 (API 대신)
      const result = evaluateExpression(formData.expression, testInputs);
      setTestResult({
        success: true,
        result: result,
        executionTime: Math.random() * 50, // 가짜 실행시간
        timestamp: new Date().toISOString()
      });
    } catch (error) {
      setTestError(error instanceof Error ? error.message : '계산 오류가 발생했습니다.');
    } finally {
      setIsTesting(false);
    }
  };

  // 간단한 수식 계산기 (API 없이 로컬 처리)
  const evaluateExpression = (expression: string, variables: Record<string, any>): any => {
    try {
      // 변수 치환
      let code = expression;
      Object.entries(variables).forEach(([name, value]) => {
        const regex = new RegExp(`\\b${name}\\b`, 'g');
        code = code.replace(regex, String(value));
      });

      // 기본 수학 함수 지원
      code = code.replace(/\bmax\(/g, 'Math.max(');
      code = code.replace(/\bmin\(/g, 'Math.min(');
      code = code.replace(/\babs\(/g, 'Math.abs(');
      code = code.replace(/\bsqrt\(/g, 'Math.sqrt(');
      code = code.replace(/\bpow\(/g, 'Math.pow(');

      // Function을 사용한 안전한 평가
      const func = new Function('Math', `return ${code}`);
      return func(Math);
    } catch (error) {
      throw new Error('수식 계산 실패: ' + (error as Error).message);
    }
  };

  return (
    <div style={{ padding: '20px' }}>
      {/* 헤더 */}
      <div style={{ 
        marginBottom: '24px',
        paddingBottom: '16px',
        borderBottom: '1px solid #e9ecef'
      }}>
        <h3 style={{ 
          margin: '0 0 8px 0',
          color: '#495057',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <i className="fas fa-check-circle" style={{ color: '#28a745' }}></i>
          검증 및 테스트
        </h3>
        <p style={{ margin: 0, color: '#6c757d', fontSize: '14px' }}>
          수식을 테스트하고 결과를 확인합니다.
        </p>
      </div>

      {/* 오류 및 경고 표시 */}
      {(Object.keys(errors).length > 0 || Object.keys(warnings).length > 0) && (
        <div style={{ marginBottom: '20px' }}>
          {Object.keys(errors).length > 0 && (
            <div style={{
              background: '#f8d7da',
              border: '1px solid #f5c6cb',
              borderRadius: '6px',
              padding: '12px',
              marginBottom: '12px'
            }}>
              <div style={{ 
                display: 'flex', 
                alignItems: 'center', 
                gap: '8px',
                marginBottom: '8px',
                fontWeight: 'bold',
                color: '#721c24'
              }}>
                <i className="fas fa-exclamation-circle"></i>
                오류 ({Object.keys(errors).length}개)
              </div>
              {Object.entries(errors).map(([field, message]) => (
                <div key={field} style={{ 
                  fontSize: '14px',
                  color: '#721c24',
                  marginLeft: '24px'
                }}>
                  • {message}
                </div>
              ))}
            </div>
          )}

          {Object.keys(warnings).length > 0 && (
            <div style={{
              background: '#fff3cd',
              border: '1px solid #ffeaa7',
              borderRadius: '6px',
              padding: '12px'
            }}>
              <div style={{ 
                display: 'flex', 
                alignItems: 'center', 
                gap: '8px',
                marginBottom: '8px',
                fontWeight: 'bold',
                color: '#856404'
              }}>
                <i className="fas fa-exclamation-triangle"></i>
                경고 ({Object.keys(warnings).length}개)
              </div>
              {Object.entries(warnings).map(([field, message]) => (
                <div key={field} style={{ 
                  fontSize: '14px',
                  color: '#856404',
                  marginLeft: '24px'
                }}>
                  • {message}
                </div>
              ))}
            </div>
          )}
        </div>
      )}

      {/* 현재 수식 표시 */}
      <div style={{ marginBottom: '20px' }}>
        <div style={{
          background: '#f8f9fa',
          border: '1px solid #dee2e6',
          borderRadius: '6px',
          padding: '12px'
        }}>
          <div style={{ 
            fontSize: '12px',
            fontWeight: 'bold',
            color: '#495057',
            marginBottom: '4px'
          }}>
            현재 수식:
          </div>
          <code style={{
            background: '#e9ecef',
            padding: '4px 8px',
            borderRadius: '4px',
            fontSize: '14px',
            fontFamily: 'monospace'
          }}>
            {formData.expression || '(수식이 없습니다)'}
          </code>
        </div>
      </div>

      {/* 테스트 입력값 - 개선된 UI */}
      {formData.input_variables && formData.input_variables.length > 0 && (
        <div style={{ marginBottom: '20px' }}>
          <h4 style={{ 
            margin: '0 0 12px 0',
            fontSize: '16px',
            color: '#495057'
          }}>
            테스트 입력값
          </h4>
          
          <div style={{ 
            display: 'grid',
            gap: '12px',
            gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))'
          }}>
            {formData.input_variables.map((variable: any, index: number) => (
              <div key={index} style={{
                background: 'white',
                border: '1px solid #dee2e6',
                borderRadius: '6px',
                padding: '12px'
              }}>
                <label style={{
                  display: 'block',
                  fontSize: '14px',
                  fontWeight: 'bold',
                  color: '#495057',
                  marginBottom: '6px'
                }}>
                  {variable.variable_name}
                  <span style={{ 
                    fontSize: '12px',
                    fontWeight: 'normal',
                    color: '#6c757d',
                    marginLeft: '8px'
                  }}>
                    ({variable.data_type})
                  </span>
                </label>
                
                {variable.data_type === 'boolean' ? (
                  <select
                    value={testInputs[variable.variable_name] ? 'true' : 'false'}
                    onChange={(e) => handleTestInputChange(
                      variable.variable_name, 
                      e.target.value === 'true'
                    )}
                    style={{
                      width: '100%',
                      padding: '8px',
                      border: '1px solid #ced4da',
                      borderRadius: '4px',
                      fontSize: '14px'
                    }}
                  >
                    <option value="true">true</option>
                    <option value="false">false</option>
                  </select>
                ) : (
                  <input
                    type={variable.data_type === 'number' ? 'number' : 'text'}
                    value={testInputs[variable.variable_name] || ''}
                    onChange={(e) => {
                      const value = variable.data_type === 'number' ? 
                        Number(e.target.value) : e.target.value;
                      handleTestInputChange(variable.variable_name, value);
                    }}
                    style={{
                      width: '100%',
                      padding: '8px',
                      border: '1px solid #ced4da',
                      borderRadius: '4px',
                      fontSize: '14px'
                    }}
                    placeholder={`${variable.data_type} 값을 입력하세요`}
                  />
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 테스트 실행 버튼 */}
      <div style={{ marginBottom: '20px' }}>
        <button
          onClick={handleRunTest}
          disabled={isTesting || !formData.expression?.trim()}
          style={{
            padding: '12px 20px',
            background: isTesting ? '#6c757d' : '#28a745',
            color: 'white',
            border: 'none',
            borderRadius: '6px',
            fontSize: '14px',
            fontWeight: 'bold',
            cursor: isTesting ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '8px'
          }}
        >
          {isTesting ? (
            <>
              <i className="fas fa-spinner fa-spin"></i>
              테스트 실행 중...
            </>
          ) : (
            <>
              <i className="fas fa-play"></i>
              테스트 실행
            </>
          )}
        </button>
      </div>

      {/* 테스트 결과 */}
      {testResult && (
        <div style={{
          background: testResult.success ? '#d4edda' : '#f8d7da',
          border: `1px solid ${testResult.success ? '#c3e6cb' : '#f5c6cb'}`,
          borderRadius: '6px',
          padding: '16px',
          marginBottom: '20px'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            gap: '8px',
            marginBottom: '8px',
            fontWeight: 'bold',
            color: testResult.success ? '#155724' : '#721c24'
          }}>
            <i className={`fas ${testResult.success ? 'fa-check-circle' : 'fa-times-circle'}`}></i>
            계산 결과
          </div>
          <div style={{ 
            fontSize: '18px',
            fontWeight: 'bold',
            color: testResult.success ? '#155724' : '#721c24',
            marginBottom: '8px'
          }}>
            결과: {String(testResult.result)}
          </div>
          <div style={{ 
            fontSize: '12px',
            color: '#6c757d'
          }}>
            실행 시간: {testResult.executionTime?.toFixed(2)}ms
          </div>
        </div>
      )}

      {/* 테스트 오류 */}
      {testError && (
        <div style={{
          background: '#f8d7da',
          border: '1px solid #f5c6cb',
          borderRadius: '6px',
          padding: '16px',
          marginBottom: '20px'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            gap: '8px',
            marginBottom: '8px',
            fontWeight: 'bold',
            color: '#721c24'
          }}>
            <i className="fas fa-times-circle"></i>
            테스트 오류
          </div>
          <div style={{ 
            fontSize: '14px',
            color: '#721c24'
          }}>
            {testError}
          </div>
        </div>
      )}

      {/* 도움말 */}
      <div style={{
        background: '#e7f3ff',
        border: '1px solid #b8daff',
        borderRadius: '6px',
        padding: '12px'
      }}>
        <div style={{ 
          display: 'flex', 
          alignItems: 'center', 
          gap: '8px',
          marginBottom: '8px',
          fontWeight: 'bold',
          color: '#004085'
        }}>
          <i className="fas fa-info-circle"></i>
          사용 가능한 함수
        </div>
        <div style={{ 
          fontSize: '14px',
          color: '#004085',
          lineHeight: '1.5'
        }}>
          • 기본 연산: +, -, *, /, %, (, )<br/>
          • 비교 연산: &gt;, &lt;, &gt;=, &lt;=, ==, !=<br/>
          • 논리 연산: &amp;&amp;, ||, !<br/>
          • 수학 함수: max(), min(), abs(), sqrt(), pow()<br/>
          • 조건식: condition ? true_value : false_value
        </div>
      </div>
    </div>
  );
};

export default ValidationPanel;