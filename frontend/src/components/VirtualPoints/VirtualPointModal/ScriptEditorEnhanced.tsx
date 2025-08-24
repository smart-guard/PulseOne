// ============================================================================
// 개선된 스크립트 에디터 - 지능형 함수 삽입 시스템
// ============================================================================

interface ScriptEditorEnhancedProps {
  value: string;
  onChange: (value: string) => void;
  variables?: Record<string, any>;
  functions?: ScriptFunction[];
  onValidate?: (script: string) => void;
}

// ============================================================================
// 지능형 함수 삽입 로직
// ============================================================================

class ScriptInsertion {
  /**
   * 커서 위치에 함수를 스마트하게 삽입
   */
  static insertFunction(
    currentScript: string,
    cursorPosition: number,
    func: ScriptFunction
  ): { newScript: string; newCursorPosition: number } {
    
    const beforeCursor = currentScript.substring(0, cursorPosition);
    const afterCursor = currentScript.substring(cursorPosition);
    
    // 1. 현재 위치 분석
    const context = this.analyzeContext(beforeCursor, afterCursor);
    
    // 2. 삽입 방식 결정
    const insertion = this.determineInsertionMethod(func, context);
    
    // 3. 스마트 삽입 실행
    return this.executeInsertion(beforeCursor, afterCursor, insertion);
  }
  
  /**
   * 현재 커서 위치의 컨텍스트 분석
   */
  private static analyzeContext(before: string, after: string) {
    const lastChar = before.slice(-1);
    const nextChar = after.charAt(0);
    
    // 연산자나 구분자가 필요한지 확인
    const needsOperatorBefore = lastChar && /[a-zA-Z0-9)]/.test(lastChar);
    const needsOperatorAfter = nextChar && /[a-zA-Z0-9(]/.test(nextChar);
    
    // 현재 위치가 식의 일부인지 확인
    const isInExpression = this.isInExpression(before);
    
    return {
      needsOperatorBefore,
      needsOperatorAfter,
      isInExpression,
      lastChar,
      nextChar
    };
  }
  
  /**
   * 삽입 방식 결정
   */
  private static determineInsertionMethod(func: ScriptFunction, context: any) {
    let prefix = '';
    let suffix = '';
    let functionCall = '';
    
    // 함수 호출 형태 생성
    if (func.parameters.length === 0) {
      functionCall = `${func.name}()`;
    } else {
      // 파라미터가 있는 경우 - 플레이스홀더 추가
      const params = func.parameters.map(param => {
        if (param.required) {
          return `<${param.name}>`;  // 필수 파라미터는 <> 로 표시
        } else {
          return param.defaultValue !== undefined ? 
            JSON.stringify(param.defaultValue) : `[${param.name}]`;
        }
      });
      functionCall = `${func.name}(${params.join(', ')})`;
    }
    
    // 컨텍스트에 따른 접두사/접미사 결정
    if (context.needsOperatorBefore) {
      prefix = this.suggestOperatorBefore(func);
    }
    
    if (context.needsOperatorAfter) {
      suffix = this.suggestOperatorAfter(func);
    }
    
    return {
      prefix,
      functionCall,
      suffix,
      selectPlaceholder: func.parameters.length > 0
    };
  }
  
  /**
   * 실제 삽입 실행
   */
  private static executeInsertion(
    before: string, 
    after: string, 
    insertion: any
  ) {
    const insertText = `${insertion.prefix}${insertion.functionCall}${insertion.suffix}`;
    const newScript = before + insertText + after;
    
    // 새로운 커서 위치 계산
    let newCursorPosition = before.length + insertion.prefix.length;
    
    if (insertion.selectPlaceholder) {
      // 첫 번째 플레이스홀더 위치로 이동
      const placeholderMatch = insertion.functionCall.match(/<[^>]+>/);
      if (placeholderMatch) {
        newCursorPosition += placeholderMatch.index!;
      }
    } else {
      // 함수 끝으로 이동
      newCursorPosition += insertion.functionCall.length;
    }
    
    return {
      newScript,
      newCursorPosition,
      hasPlaceholders: insertion.selectPlaceholder
    };
  }
  
  /**
   * 앞쪽 연산자 제안
   */
  private static suggestOperatorBefore(func: ScriptFunction): string {
    // 함수 카테고리에 따른 연산자 제안
    switch (func.category) {
      case 'math':
      case 'statistical':
        return ' + ';  // 수학 함수는 대부분 더하기
        
      case 'logic':
      case 'conditional':
        return ' && '; // 논리 함수는 AND 연산자
        
      default:
        return ' ';    // 기본적으로 공백만
    }
  }
  
  /**
   * 뒤쪽 연산자 제안
   */
  private static suggestOperatorAfter(func: ScriptFunction): string {
    if (func.returnType === 'boolean') {
      return ' ? "참" : "거짓"'; // Boolean 반환 시 삼항연산자 제안
    }
    return '';
  }
  
  /**
   * 현재 위치가 식의 일부인지 확인
   */
  private static isInExpression(beforeText: string): boolean {
    // 간단한 휴리스틱으로 판단
    return /[+\-*/=<>!&|?:]$/.test(beforeText.trim());
  }
}

// ============================================================================
// 향상된 스크립트 에디터 컴포넌트
// ============================================================================

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
  const [placeholders, setPlaceholders] = useState<Array<{start: number, end: number, text: string}>>([]);
  
  // ========================================================================
  // 지능형 함수 삽입 핸들러
  // ========================================================================
  
  const handleFunctionClick = useCallback((func: ScriptFunction) => {
    const textarea = textareaRef.current;
    if (!textarea) return;
    
    const cursorPosition = textarea.selectionStart;
    
    // 스마트 삽입 실행
    const result = ScriptInsertion.insertFunction(value, cursorPosition, func);
    
    // 스크립트 업데이트
    onChange(result.newScript);
    
    // 커서 위치 업데이트 (다음 프레임에서)
    setTimeout(() => {
      textarea.focus();
      textarea.setSelectionRange(result.newCursorPosition, result.newCursorPosition);
      
      // 플레이스홀더가 있는 경우 첫 번째 플레이스홀더 선택
      if (result.hasPlaceholders) {
        selectNextPlaceholder(result.newScript, result.newCursorPosition);
      }
    }, 0);
    
    // 유효성 검사 실행
    if (onValidate) {
      onValidate(result.newScript);
    }
  }, [value, onChange, onValidate]);
  
  // ========================================================================
  // 플레이스홀더 관리
  // ========================================================================
  
  const selectNextPlaceholder = useCallback((script: string, fromPosition: number) => {
    const textarea = textareaRef.current;
    if (!textarea) return;
    
    // < > 로 둘러싸인 플레이스홀더 찾기
    const placeholderRegex = /<([^>]+)>/g;
    let match;
    
    while ((match = placeholderRegex.exec(script)) !== null) {
      if (match.index >= fromPosition) {
        // 플레이스홀더 선택
        textarea.setSelectionRange(match.index, match.index + match[0].length);
        break;
      }
    }
  }, []);
  
  // Tab 키로 다음 플레이스홀더로 이동
  const handleKeyDown = useCallback((e: React.KeyboardEvent) => {
    if (e.key === 'Tab' && !e.shiftKey) {
      const textarea = textareaRef.current;
      if (!textarea) return;
      
      const selectionStart = textarea.selectionStart;
      const selectionEnd = textarea.selectionEnd;
      
      // 현재 선택된 텍스트가 플레이스홀더인지 확인
      const selectedText = value.substring(selectionStart, selectionEnd);
      if (selectedText.startsWith('<') && selectedText.endsWith('>')) {
        e.preventDefault();
        selectNextPlaceholder(value, selectionEnd);
      }
    }
  }, [value, selectNextPlaceholder]);
  
  // ========================================================================
  // 자동완성 시스템
  // ========================================================================
  
  const handleInput = useCallback((e: React.ChangeEvent<HTMLTextAreaElement>) => {
    const newValue = e.target.value;
    onChange(newValue);
    
    // 자동완성 트리거 체크
    const cursorPosition = e.target.selectionStart;
    const textBeforeCursor = newValue.substring(0, cursorPosition);
    const lastWord = textBeforeCursor.split(/\W/).pop() || '';
    
    if (lastWord.length >= 2) {
      // 함수명 자동완성
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
    
    // 실시간 유효성 검사
    if (onValidate) {
      onValidate(newValue);
    }
  }, [onChange, functions, onValidate]);
  
  // ========================================================================
  // UI 렌더링
  // ========================================================================
  
  return (
    <div className="script-editor-enhanced">
      <div className="editor-container">
        <textarea
          ref={textareaRef}
          value={value}
          onChange={handleInput}
          onKeyDown={handleKeyDown}
          className="script-textarea"
          placeholder="수식을 입력하거나 오른쪽 함수 라이브러리에서 함수를 선택하세요"
          rows={4}
          spellCheck={false}
        />
        
        {/* 자동완성 드롭다운 */}
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
      
      {/* 함수 라이브러리 */}
      <div className="function-library">
        <h4>함수 라이브러리</h4>
        <div className="function-categories">
          {functions.reduce((acc, func) => {
            if (!acc[func.category]) acc[func.category] = [];
            acc[func.category].push(func);
            return acc;
          }, {} as Record<string, ScriptFunction[]>))
          .map(([category, categoryFunctions]) => (
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
      
      {/* 사용법 안내 */}
      <div className="editor-help">
        <h4>💡 사용법</h4>
        <ul>
          <li>함수를 클릭하면 자동으로 올바른 형태로 삽입됩니다</li>
          <li>&lt;파라미터&gt; 부분을 실제 값으로 바꾸세요</li>
          <li>Tab 키로 다음 파라미터로 이동할 수 있습니다</li>
          <li>함수명을 입력하면 자동완성이 나타납니다</li>
        </ul>
      </div>
    </div>
  );
};

// ============================================================================
// 스타일링
// ============================================================================

const styles = `
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
`;

export default ScriptEditorEnhanced;