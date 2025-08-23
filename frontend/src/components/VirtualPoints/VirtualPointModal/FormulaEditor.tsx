// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointModal/FormulaEditor.tsx
// 수식 편집기 컴포넌트
// ============================================================================

import React, { useState, useRef, useEffect } from 'react';
import { VirtualPointInput, ScriptFunction, ScriptValidationResult } from '../../../types/virtualPoints';

interface FormulaEditorProps {
  expression: string;
  inputVariables: VirtualPointInput[];
  functions: ScriptFunction[];
  onChange: (expression: string) => void;
  validationResult?: ScriptValidationResult | null;
  isValidating?: boolean;
}

const FormulaEditor: React.FC<FormulaEditorProps> = ({
  expression,
  inputVariables,
  functions,
  onChange,
  validationResult,
  isValidating = false
}) => {
  const [showFunctionPanel, setShowFunctionPanel] = useState(false);
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [searchTerm, setSearchTerm] = useState('');
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // ========================================================================
  // 필터링된 함수 목록
  // ========================================================================
  
  const filteredFunctions = functions.filter(func => {
    const matchesCategory = selectedCategory === 'all' || func.category === selectedCategory;
    const matchesSearch = searchTerm === '' || 
      func.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      func.displayName.toLowerCase().includes(searchTerm.toLowerCase()) ||
      func.description.toLowerCase().includes(searchTerm.toLowerCase());
    
    return matchesCategory && matchesSearch;
  });

  // 함수 카테고리 목록
  const functionCategories = [
    { value: 'all', label: '모든 함수' },
    { value: 'math', label: '수학' },
    { value: 'logic', label: '논리' },
    { value: 'time', label: '시간' },
    { value: 'string', label: '문자열' },
    { value: 'conversion', label: '변환' },
    { value: 'custom', label: '사용자 정의' }
  ];

  // ========================================================================
  // 이벤트 핸들러
  // ========================================================================
  
  const handleInsertFunction = (func: ScriptFunction) => {
    if (!textareaRef.current) return;

    const textarea = textareaRef.current;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const currentValue = expression;

    // 함수 삽입 텍스트 생성
    const parameters = func.parameters.map(param => {
      if (param.defaultValue !== undefined) {
        return `${param.name}=${param.defaultValue}`;
      }
      return param.name;
    }).join(', ');
    
    const insertText = `${func.name}(${parameters})`;

    // 텍스트 삽입
    const newValue = currentValue.substring(0, start) + insertText + currentValue.substring(end);
    onChange(newValue);

    // 커서 위치 조정
    setTimeout(() => {
      if (textareaRef.current) {
        const newCursorPos = start + insertText.length;
        textareaRef.current.setSelectionRange(newCursorPos, newCursorPos);
        textareaRef.current.focus();
      }
    }, 0);
  };

  const handleInsertVariable = (variable: VirtualPointInput) => {
    if (!textareaRef.current) return;

    const textarea = textareaRef.current;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const currentValue = expression;

    const insertText = variable.variable_name;
    const newValue = currentValue.substring(0, start) + insertText + currentValue.substring(end);
    onChange(newValue);

    // 커서 위치 조정
    setTimeout(() => {
      if (textareaRef.current) {
        const newCursorPos = start + insertText.length;
        textareaRef.current.setSelectionRange(newCursorPos, newCursorPos);
        textareaRef.current.focus();
      }
    }, 0);
  };

  // ========================================================================
  // 공통 수식 예제
  // ========================================================================
  
  const commonExamples = [
    {
      name: '평균값',
      code: '(input1 + input2 + input3) / 3',
      description: '여러 입력값의 평균'
    },
    {
      name: '온도 변환',
      code: '(fahrenheit - 32) * 5 / 9',
      description: '화씨를 섭씨로 변환'
    },
    {
      name: '조건부 값',
      code: 'temperature > 30 ? "높음" : "정상"',
      description: '조건에 따른 값 반환'
    },
    {
      name: '범위 제한',
      code: 'Math.max(0, Math.min(100, inputValue))',
      description: '0-100 범위로 값 제한'
    },
    {
      name: '델타 계산',
      code: 'Math.abs(current - previous)',
      description: '이전값과의 차이 계산'
    }
  ];

  return (
    <div className="formula-editor">
      {/* 도구모음 */}
      <div className="editor-toolbar">
        <div className="toolbar-left">
          <button
            type="button"
            className={`toolbar-btn ${showFunctionPanel ? 'active' : ''}`}
            onClick={() => setShowFunctionPanel(!showFunctionPanel)}
          >
            <i className="fas fa-function"></i>
            함수 라이브러리
          </button>
        </div>
        
        <div className="toolbar-right">
          {isValidating && (
            <div className="validation-indicator validating">
              <i className="fas fa-spinner fa-spin"></i>
              검증 중...
            </div>
          )}
          {validationResult && (
            <div className={`validation-indicator ${validationResult.isValid ? 'valid' : 'invalid'}`}>
              <i className={`fas ${validationResult.isValid ? 'fa-check' : 'fa-exclamation-triangle'}`}></i>
              {validationResult.isValid ? '유효함' : `${validationResult.errors.length}개 오류`}
            </div>
          )}
        </div>
      </div>

      <div className="editor-layout">
        {/* 함수 패널 */}
        {showFunctionPanel && (
          <div className="function-panel">
            <div className="panel-header">
              <div className="search-container">
                <input
                  type="text"
                  placeholder="함수 검색..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="search-input"
                />
                <i className="fas fa-search search-icon"></i>
              </div>
              
              <select
                value={selectedCategory}
                onChange={(e) => setSelectedCategory(e.target.value)}
                className="category-select"
              >
                {functionCategories.map(cat => (
                  <option key={cat.value} value={cat.value}>{cat.label}</option>
                ))}
              </select>
            </div>

            <div className="function-list">
              {filteredFunctions.length === 0 ? (
                <div className="no-functions">
                  <i className="fas fa-info-circle"></i>
                  함수가 없습니다
                </div>
              ) : (
                filteredFunctions.map(func => (
                  <div
                    key={func.id}
                    className="function-item"
                    onClick={() => handleInsertFunction(func)}
                  >
                    <div className="function-header">
                      <span className="function-name">{func.displayName}</span>
                      <span className={`function-category ${func.category}`}>
                        {func.category}
                      </span>
                    </div>
                    <div className="function-syntax">
                      <code>{func.syntax}</code>
                    </div>
                    <div className="function-description">
                      {func.description}
                    </div>
                    {func.examples && func.examples.length > 0 && (
                      <div className="function-examples">
                        {func.examples.slice(0, 1).map((example, idx) => (
                          <div key={idx} className="function-example">
                            <strong>예제:</strong> <code>{example.code}</code>
                          </div>
                        ))}
                      </div>
                    )}
                  </div>
                ))
              )}
            </div>
          </div>
        )}

        {/* 메인 편집 영역 */}
        <div className="editor-main">
          {/* 입력 변수 패널 */}
          {inputVariables.length > 0 && (
            <div className="variables-panel">
              <h4>
                <i className="fas fa-plug"></i>
                사용 가능한 변수
              </h4>
              <div className="variable-list">
                {inputVariables.map(variable => (
                  <div
                    key={variable.id}
                    className="variable-item"
                    onClick={() => handleInsertVariable(variable)}
                  >
                    <span className="variable-name">{variable.variable_name}</span>
                    <span className="variable-type">{variable.data_type}</span>
                    {variable.description && (
                      <span className="variable-description">{variable.description}</span>
                    )}
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* 수식 편집기 */}
          <div className="code-editor">
            <label>
              <i className="fas fa-code"></i>
              수식 작성
            </label>
            <textarea
              ref={textareaRef}
              value={expression}
              onChange={(e) => onChange(e.target.value)}
              className={`code-textarea ${validationResult && !validationResult.isValid ? 'error' : ''}`}
              placeholder="수식을 작성하세요... 예: (input1 + input2) / 2"
              rows={8}
              spellCheck={false}
            />
            
            {/* 검증 결과 */}
            {validationResult && !validationResult.isValid && (
              <div className="validation-errors">
                <h5>
                  <i className="fas fa-exclamation-triangle"></i>
                  오류 목록
                </h5>
                {validationResult.errors.map((error, idx) => (
                  <div key={idx} className="error-item">
                    <span className="error-line">라인 {error.line}:</span>
                    <span className="error-message">{error.message}</span>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* 공통 예제 */}
          <div className="examples-panel">
            <h4>
              <i className="fas fa-lightbulb"></i>
              공통 수식 예제
            </h4>
            <div className="example-list">
              {commonExamples.map((example, idx) => (
                <div
                  key={idx}
                  className="example-item"
                  onClick={() => onChange(example.code)}
                >
                  <div className="example-header">
                    <span className="example-name">{example.name}</span>
                  </div>
                  <div className="example-code">
                    <code>{example.code}</code>
                  </div>
                  <div className="example-description">
                    {example.description}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default FormulaEditor;