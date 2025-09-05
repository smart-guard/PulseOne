import React, { useState, useRef, useCallback } from 'react';

interface ScriptFunction {
  id: string;
  name: string;
  displayName: string;
  description: string;
  category: string;
  syntax: string;
  returnType: string;
  parameters: Array<{
    name: string;
    type: string;
    required: boolean;
    defaultValue?: any;
    description: string;
  }>;
}

interface ScriptEditorEnhancedProps {
  value: string;
  onChange: (value: string) => void;
  variables?: Record<string, any>;
  functions?: ScriptFunction[];
  onValidate?: (script: string) => void;
}

const ScriptEditorEnhanced: React.FC<ScriptEditorEnhancedProps> = ({
  value,
  onChange,
  variables = {},
  functions = [],
  onValidate
}) => {
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const [showSuggestions, setShowSuggestions] = useState(false);
  const [suggestions, setSuggestions] = useState<ScriptFunction[]>([]);

  // 함수 삽입 핸들러
  const handleFunctionClick = useCallback((func: ScriptFunction) => {
    const textarea = textareaRef.current;
    if (!textarea) return;

    const cursorPosition = textarea.selectionStart;
    const beforeCursor = value.substring(0, cursorPosition);
    const afterCursor = value.substring(cursorPosition);

    // 함수 호출 생성
    let functionCall;
    if (func.parameters.length === 0) {
      functionCall = `${func.name}()`;
    } else {
      const params = func.parameters.map(param => {
        if (param.required) {
          return `<${param.name}>`;
        } else {
          return param.defaultValue !== undefined ? 
            JSON.stringify(param.defaultValue) : `[${param.name}]`;
        }
      });
      functionCall = `${func.name}(${params.join(', ')})`;
    }

    // 앞뒤 공백 처리
    let prefix = '';
    let suffix = '';
    if (beforeCursor && !/\s$/.test(beforeCursor)) {
      prefix = ' ';
    }
    if (afterCursor && !/^\s/.test(afterCursor)) {
      suffix = ' ';
    }

    const insertText = `${prefix}${functionCall}${suffix}`;
    const newScript = beforeCursor + insertText + afterCursor;
    
    onChange(newScript);

    // 커서 위치 업데이트
    setTimeout(() => {
      textarea.focus();
      const newCursorPosition = cursorPosition + prefix.length + functionCall.length;
      textarea.setSelectionRange(newCursorPosition, newCursorPosition);
    }, 0);

    if (onValidate) {
      onValidate(newScript);
    }
  }, [value, onChange, onValidate]);

  // 입력 핸들러
  const handleInput = useCallback((e: React.ChangeEvent<HTMLTextAreaElement>) => {
    const newValue = e.target.value;
    onChange(newValue);

    // 자동완성 로직
    const cursorPosition = e.target.selectionStart;
    const textBeforeCursor = newValue.substring(0, cursorPosition);
    const lastWord = textBeforeCursor.split(/\W/).pop() || '';

    if (lastWord.length >= 2) {
      const matchingFunctions = functions.filter(func =>
        func.name.toLowerCase().startsWith(lastWord.toLowerCase()) ||
        func.displayName.toLowerCase().includes(lastWord.toLowerCase())
      );
      
      if (matchingFunctions.length > 0) {
        setSuggestions(matchingFunctions);
        setShowSuggestions(true);
      } else {
        setShowSuggestions(false);
      }
    } else {
      setShowSuggestions(false);
    }

    if (onValidate) {
      onValidate(newValue);
    }
  }, [onChange, functions, onValidate]);

  // 카테고리별 함수 그룹핑
  const groupedFunctions = React.useMemo(() => {
    const groups: Record<string, ScriptFunction[]> = {};
    functions.forEach(func => {
      if (!groups[func.category]) {
        groups[func.category] = [];
      }
      groups[func.category].push(func);
    });
    return groups;
  }, [functions]);

  return (
    <div className="script-editor-enhanced">
      <div className="editor-container">
        <textarea
          ref={textareaRef}
          value={value}
          onChange={handleInput}
          className="script-textarea"
          placeholder="수식을 입력하거나 오른쪽 함수 라이브러리에서 함수를 선택하세요"
          rows={4}
          spellCheck={false}
        />
        
        {showSuggestions && (
          <div className="autocomplete-dropdown">
            {suggestions.slice(0, 5).map((func) => (
              <div
                key={func.id}
                className="autocomplete-item"
                onClick={() => handleFunctionClick(func)}
              >
                <span className="func-name">{func.name}</span>
                <span className="func-desc">{func.displayName}</span>
              </div>
            ))}
          </div>
        )}
      </div>
      
      <div className="function-library">
        <h4>함수 라이브러리</h4>
        <div className="function-categories">
          {Object.entries(groupedFunctions).map(([category, categoryFunctions]) => (
            <div key={category} className="function-category">
              <h5>{category}</h5>
              <div className="function-list">
                {categoryFunctions.map((func) => (
                  <div
                    key={func.id}
                    className="function-item"
                    onClick={() => handleFunctionClick(func)}
                    title={`${func.description}\n문법: ${func.syntax}`}
                  >
                    <div className="func-name">{func.displayName}</div>
                    <div className="func-syntax">{func.syntax}</div>
                  </div>
                ))}
              </div>
            </div>
          ))}
        </div>
      </div>
      
      <div className="editor-help">
        <h4>사용법</h4>
        <ul>
          <li>함수를 클릭하면 자동으로 올바른 형태로 삽입됩니다</li>
          <li>&lt;파라미터&gt; 부분을 실제 값으로 바꾸세요</li>
          <li>함수명을 입력하면 자동완성이 나타납니다</li>
        </ul>
      </div>

      <style jsx>{`
        .script-editor-enhanced {
          display: flex;
          gap: 20px;
          height: 400px;
        }

        .editor-container {
          flex: 2;
          position: relative;
        }

        .script-textarea {
          width: 100%;
          height: 200px;
          padding: 12px;
          border: 2px solid #e1e5e9;
          border-radius: 8px;
          font-family: 'Consolas', 'Monaco', monospace;
          font-size: 14px;
          line-height: 1.5;
          resize: vertical;
          transition: border-color 0.2s;
        }

        .script-textarea:focus {
          outline: none;
          border-color: #4285f4;
          box-shadow: 0 0 0 3px rgba(66, 133, 244, 0.1);
        }

        .autocomplete-dropdown {
          position: absolute;
          top: 100%;
          left: 0;
          right: 0;
          background: white;
          border: 1px solid #e1e5e9;
          border-radius: 6px;
          box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
          z-index: 1000;
          max-height: 200px;
          overflow-y: auto;
        }

        .autocomplete-item {
          display: flex;
          justify-content: space-between;
          padding: 8px 12px;
          cursor: pointer;
          border-bottom: 1px solid #f5f5f5;
        }

        .autocomplete-item:hover {
          background-color: #f8f9fa;
        }

        .function-library {
          flex: 1;
          border: 1px solid #e1e5e9;
          border-radius: 8px;
          padding: 16px;
          overflow-y: auto;
          background: #fafbfc;
        }

        .function-categories {
          display: flex;
          flex-direction: column;
          gap: 16px;
        }

        .function-category h5 {
          margin: 0 0 8px 0;
          color: #1a73e8;
          font-weight: 600;
          font-size: 14px;
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }

        .function-list {
          display: flex;
          flex-direction: column;
          gap: 4px;
        }

        .function-item {
          padding: 8px 12px;
          background: white;
          border: 1px solid #e8eaed;
          border-radius: 6px;
          cursor: pointer;
          transition: all 0.2s;
        }

        .function-item:hover {
          background: #e3f2fd;
          border-color: #1976d2;
          transform: translateX(2px);
        }

        .func-name {
          font-weight: 600;
          color: #1565c0;
          font-size: 13px;
        }

        .func-syntax {
          font-family: 'Consolas', monospace;
          font-size: 11px;
          color: #666;
          margin-top: 2px;
        }

        .editor-help {
          position: absolute;
          bottom: 0;
          left: 0;
          right: 0;
          background: #e8f0fe;
          padding: 12px;
          border-radius: 6px;
          margin-top: 16px;
        }

        .editor-help h4 {
          margin: 0 0 8px 0;
          color: #1565c0;
          font-size: 14px;
        }

        .editor-help ul {
          margin: 0;
          padding-left: 16px;
          font-size: 12px;
          color: #333;
        }

        .editor-help li {
          margin: 4px 0;
        }
      `}</style>
    </div>
  );
};

export default ScriptEditorEnhanced;