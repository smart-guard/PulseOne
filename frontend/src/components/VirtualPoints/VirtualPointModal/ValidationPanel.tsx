// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointModal/ValidationPanel.tsx
// 검증 및 테스트 패널 컴포넌트
// ============================================================================

import React, { useState, useCallback, useEffect } from 'react';
import { ScriptValidationResult, ScriptTestResult } from '../../../types/virtualPoints';

interface FormData {
  name: string;
  expression: string;
  input_variables: Array<{
    id: number;
    variable_name: string;
    data_type: 'number' | 'boolean' | 'string';
    source_type: 'data_point' | 'virtual_point' | 'constant';
    source_id: any;
  }>;
  data_type: 'number' | 'boolean' | 'string';
}

interface ValidationPanelProps {
  formData: FormData;
  validationResult?: ScriptValidationResult | null;
  onValidate: () => Promise<void>;
  isValidating?: boolean;
}

const ValidationPanel: React.FC<ValidationPanelProps> = ({
  formData,
  validationResult,
  onValidate,
  isValidating = false
}) => {
  const [testValues, setTestValues] = useState<Record<string, any>>({});
  const [testResult, setTestResult] = useState<ScriptTestResult | null>(null);
  const [isTesting, setIsTesting] = useState(false);
  const [testHistory, setTestHistory] = useState<Array<{
    timestamp: string;
    inputs: Record<string, any>;
    result: ScriptTestResult;
  }>>([]);

  // ========================================================================
  // Effects
  // ========================================================================
  
  // 입력 변수가 변경되면 테스트 값 초기화
  useEffect(() => {
    const newTestValues: Record<string, any> = {};
    formData.input_variables.forEach(variable => {
      if (variable.source_type === 'constant') {
        newTestValues[variable.variable_name] = variable.source_id;
      } else {
        // 타입별 기본 테스트 값
        switch (variable.data_type) {
          case 'number':
            newTestValues[variable.variable_name] = 100;
            break;
          case 'boolean':
            newTestValues[variable.variable_name] = true;
            break;
          case 'string':
            newTestValues[variable.variable_name] = 'test';
            break;
          default:
            newTestValues[variable.variable_name] = null;
        }
      }
    });
    setTestValues(newTestValues);
  }, [formData.input_variables]);

  // ========================================================================
  // 이벤트 핸들러
  // ========================================================================
  
  const handleTestValueChange = useCallback((variableName: string, value: any) => {
    setTestValues(prev => ({ ...prev, [variableName]: value }));
  }, []);

  const handleRunTest = useCallback(async () => {
    if (!formData.expression.trim()) {
      alert('수식을 입력해주세요.');
      return;
    }

    setIsTesting(true);
    setTestResult(null);

    try {
      // 실제 테스트 실행 (API 호출)
      const response = await fetch('/api/virtual-points/test-script', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          script: formData.expression,
          context: testValues,
          timeout: 10000
        })
      });

      const result: ScriptTestResult = await response.json();
      setTestResult(result);

      // 테스트 이력에 추가
      setTestHistory(prev => [{
        timestamp: new Date().toISOString(),
        inputs: { ...testValues },
        result
      }, ...prev].slice(0, 10)); // 최대 10개까지 보관

    } catch (error) {
      const errorResult: ScriptTestResult = {
        success: false,
        result: null,
        executionTime: 0,
        error: error instanceof Error ? error.message : '테스트 실행 실패',
        logs: []
      };
      setTestResult(errorResult);
    } finally {
      setIsTesting(false);
    }
  }, [formData.expression, testValues]);

  const handleRandomizeValues = useCallback(() => {
    const randomValues: Record<string, any> = {};
    
    formData.input_variables.forEach(variable => {
      if (variable.source_type === 'constant') {
        randomValues[variable.variable_name] = variable.source_id;
      } else {
        switch (variable.data_type) {
          case 'number':
            randomValues[variable.variable_name] = Math.round((Math.random() * 200 - 100) * 100) / 100;
            break;
          case 'boolean':
            randomValues[variable.variable_name] = Math.random() > 0.5;
            break;
          case 'string':
            const strings = ['test', 'sample', 'data', 'value', 'input'];
            randomValues[variable.variable_name] = strings[Math.floor(Math.random() * strings.length)];
            break;
        }
      }
    });
    
    setTestValues(randomValues);
  }, [formData.input_variables]);

  const handleApplyTestCase = useCallback((testCase: typeof testHistory[0]) => {
    setTestValues(testCase.inputs);
    setTestResult(testCase.result);
  }, []);

  // ========================================================================
  // 렌더링 헬퍼
  // ========================================================================
  
  const renderTestValue = (variable: typeof formData.input_variables[0]) => {
    const value = testValues[variable.variable_name];

    if (variable.source_type === 'constant') {
      return (
        <div className="test-value-readonly">
          <code>{String(variable.source_id)}</code>
          <small>(상수값)</small>
        </div>
      );
    }

    switch (variable.data_type) {
      case 'number':
        return (
          <input
            type="number"
            value={value || 0}
            onChange={(e) => handleTestValueChange(variable.variable_name, parseFloat(e.target.value) || 0)}
            className="test-value-input"
            step="0.01"
          />
        );
      
      case 'boolean':
        return (
          <select
            value={String(value)}
            onChange={(e) => handleTestValueChange(variable.variable_name, e.target.value === 'true')}
            className="test-value-select"
          >
            <option value="true">참 (true)</option>
            <option value="false">거짓 (false)</option>
          </select>
        );
      
      case 'string':
        return (
          <input
            type="text"
            value={value || ''}
            onChange={(e) => handleTestValueChange(variable.variable_name, e.target.value)}
            className="test-value-input"
            placeholder="텍스트 입력"
          />
        );
      
      default:
        return <span>지원하지 않는 타입</span>;
    }
  };

  const formatTestResult = (result: any) => {
    if (result === null || result === undefined) {
      return 'null';
    }
    
    if (typeof result === 'object') {
      return JSON.stringify(result, null, 2);
    }
    
    return String(result);
  };

  // ========================================================================
  // 메인 렌더링
  // ========================================================================
  
  return (
    <div className="validation-panel">
      {/* 검증 결과 섹션 */}
      <div className="validation-section">
        <div className="section-header">
          <h3>
            <i className="fas fa-check-circle"></i>
            구문 검증
          </h3>
          <button
            type="button"
            className="btn-secondary"
            onClick={onValidate}
            disabled={isValidating || !formData.expression.trim()}
          >
            {isValidating ? (
              <>
                <i className="fas fa-spinner fa-spin"></i>
                검증 중...
              </>
            ) : (
              <>
                <i className="fas fa-sync"></i>
                다시 검증
              </>
            )}
          </button>
        </div>

        {validationResult ? (
          <div className={`validation-result ${validationResult.isValid ? 'valid' : 'invalid'}`}>
            <div className="result-header">
              <i className={`fas ${validationResult.isValid ? 'fa-check' : 'fa-exclamation-triangle'}`}></i>
              <span>
                {validationResult.isValid 
                  ? '구문이 올바릅니다' 
                  : `${validationResult.errors.length}개의 오류가 발견되었습니다`
                }
              </span>
            </div>
            
            {!validationResult.isValid && (
              <div className="error-list">
                {validationResult.errors.map((error, idx) => (
                  <div key={idx} className="error-item">
                    <span className="error-location">라인 {error.line}, 열 {error.column}:</span>
                    <span className="error-message">{error.message}</span>
                    <span className="error-type">({error.type})</span>
                  </div>
                ))}
              </div>
            )}
            
            {validationResult.warnings && validationResult.warnings.length > 0 && (
              <div className="warning-list">
                <h4>경고:</h4>
                {validationResult.warnings.map((warning, idx) => (
                  <div key={idx} className="warning-item">
                    <span className="warning-message">{warning.message}</span>
                  </div>
                ))}
              </div>
            )}

            {validationResult.usedVariables && validationResult.usedVariables.length > 0 && (
              <div className="used-variables">
                <h4>사용된 변수:</h4>
                <div className="variable-tags">
                  {validationResult.usedVariables.map(variable => (
                    <span key={variable} className="variable-tag">{variable}</span>
                  ))}
                </div>
              </div>
            )}
          </div>
        ) : (
          <div className="validation-placeholder">
            <i className="fas fa-info-circle"></i>
            수식을 입력하고 검증 버튼을 클릭하세요.
          </div>
        )}
      </div>

      {/* 테스트 실행 섹션 */}
      <div className="test-section">
        <div className="section-header">
          <h3>
            <i className="fas fa-play-circle"></i>
            실행 테스트
          </h3>
          <div className="test-actions">
            <button
              type="button"
              className="btn-secondary btn-sm"
              onClick={handleRandomizeValues}
              disabled={formData.input_variables.length === 0}
            >
              <i className="fas fa-dice"></i>
              랜덤값
            </button>
            <button
              type="button"
              className="btn-primary"
              onClick={handleRunTest}
              disabled={isTesting || !formData.expression.trim()}
            >
              {isTesting ? (
                <>
                  <i className="fas fa-spinner fa-spin"></i>
                  실행 중...
                </>
              ) : (
                <>
                  <i className="fas fa-play"></i>
                  테스트 실행
                </>
              )}
            </button>
          </div>
        </div>

        {/* 입력 값 설정 */}
        {formData.input_variables.length > 0 && (
          <div className="test-inputs">
            <h4>테스트 입력값</h4>
            <div className="input-grid">
              {formData.input_variables.map(variable => (
                <div key={variable.id} className="test-input-item">
                  <label>
                    <code>{variable.variable_name}</code>
                    <span className={`type-indicator ${variable.data_type}`}>
                      {variable.data_type}
                    </span>
                  </label>
                  {renderTestValue(variable)}
                </div>
              ))}
            </div>
          </div>
        )}

        {/* 테스트 결과 */}
        {testResult && (
          <div className={`test-result ${testResult.success ? 'success' : 'error'}`}>
            <div className="result-header">
              <i className={`fas ${testResult.success ? 'fa-check' : 'fa-exclamation-triangle'}`}></i>
              <span>
                {testResult.success ? '테스트 성공' : '테스트 실패'}
              </span>
              <span className="execution-time">
                실행 시간: {testResult.executionTime}ms
              </span>
            </div>
            
            <div className="result-content">
              {testResult.success ? (
                <div className="success-result">
                  <h5>결과값:</h5>
                  <div className="result-value">
                    <code>{formatTestResult(testResult.result)}</code>
                  </div>
                  {testResult.result !== null && formData.data_type && (
                    <div className="result-info">
                      <span>타입: {typeof testResult.result}</span>
                      <span>예상 타입: {formData.data_type}</span>
                      {typeof testResult.result !== formData.data_type && (
                        <span className="type-mismatch">⚠️ 타입 불일치</span>
                      )}
                    </div>
                  )}
                </div>
              ) : (
                <div className="error-result">
                  <h5>오류:</h5>
                  <div className="error-message">{testResult.error}</div>
                </div>
              )}
              
              {testResult.logs && testResult.logs.length > 0 && (
                <div className="test-logs">
                  <h5>실행 로그:</h5>
                  <div className="log-list">
                    {testResult.logs.map((log, idx) => (
                      <div key={idx} className="log-item">{log}</div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          </div>
        )}
      </div>

      {/* 테스트 이력 */}
      {testHistory.length > 0 && (
        <div className="test-history-section">
          <div className="section-header">
            <h3>
              <i className="fas fa-history"></i>
              테스트 이력
            </h3>
            <button
              type="button"
              className="btn-secondary btn-sm"
              onClick={() => setTestHistory([])}
            >
              <i className="fas fa-trash"></i>
              이력 삭제
            </button>
          </div>
          
          <div className="history-list">
            {testHistory.map((item, idx) => (
              <div
                key={idx}
                className={`history-item ${item.result.success ? 'success' : 'error'}`}
                onClick={() => handleApplyTestCase(item)}
              >
                <div className="history-header">
                  <span className="history-time">
                    {new Date(item.timestamp).toLocaleString()}
                  </span>
                  <span className={`history-status ${item.result.success ? 'success' : 'error'}`}>
                    <i className={`fas ${item.result.success ? 'fa-check' : 'fa-times'}`}></i>
                    {item.result.success ? '성공' : '실패'}
                  </span>
                </div>
                <div className="history-result">
                  결과: <code>{formatTestResult(item.result.result)}</code>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};

export default ValidationPanel;