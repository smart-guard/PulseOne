// ============================================================================
// FormulaEditor.tsx - Clarity & Assistance Focused "Workstation"
// ============================================================================

import React, { useState, useRef, useMemo } from 'react';
import { VirtualPointInput } from '../../../types/virtualPoints';
import './VirtualPointModal.css';

interface FormulaEditorProps {
  expression: string;
  inputVariables?: VirtualPointInput[];
  onChange: (value: string) => void;
  validationResult?: any;
  isValidating?: boolean;
  simulationResult: any | null;
  isSimulating: boolean;
  onRunSimulation: () => void;
}

const FormulaEditor: React.FC<FormulaEditorProps> = ({
  expression,
  inputVariables = [],
  onChange,
  validationResult,
  isValidating,
  simulationResult,
  isSimulating,
  onRunSimulation
}) => {
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  const insertAtCursor = (text: string) => {
    if (!textareaRef.current) return;
    const start = textareaRef.current.selectionStart;
    const end = textareaRef.current.selectionEnd;
    const newValue = expression.substring(0, start) + text + expression.substring(end);
    onChange(newValue);
    setTimeout(() => {
      textareaRef.current?.focus();
      textareaRef.current?.setSelectionRange(start + text.length, start + text.length);
    }, 10);
  };

  const operators = ['+', '-', '*', '/', '(', ')', '>', '<', '=', '&&', '||'];

  // handleTest function removed as its logic is now handled by onRunSimulation prop
  // const handleTest = () => {
  //   setTesting(true);
  //   setTimeout(() => {
  //     setTestResult(Math.random() * 100);
  //     setTesting(false);
  //   }, 600);
  // };

  return (
    <div className="wizard-slide-active" style={{ width: '100%', flex: 1, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
      <div className="formula-grid-v3" style={{ flex: 1, minHeight: 0 }}>

        {/* Column 1: Variable Catalog */}
        <div className="variable-palette-card">
          <div className="palette-header">
            <i className="fas fa-list-ul"></i> 사용 가능한 변수
          </div>
          <div className="palette-content">
            {inputVariables.map(v => (
              <button
                key={v.id}
                className="palette-item-btn"
                onClick={() => insertAtCursor(v.input_name)}
                title={v.description}
              >
                <i className={`fas ${v.data_type === 'boolean' ? 'fa-toggle-on' : 'fa-hashtag'}`}></i>
                <span>{v.input_name}</span>
                <span style={{ marginLeft: 'auto', fontSize: '10px', color: '#94a3b8' }}>{v.data_type}</span>
              </button>
            ))}
            {inputVariables.length === 0 && (
              <div style={{ padding: '20px', textAlign: 'center', color: '#94a3b8', fontSize: '12px' }}>
                <i className="fas fa-exclamation-circle" style={{ display: 'block', fontSize: '24px', marginBottom: '8px' }}></i>
                2단계에서 변수를 먼저 등록하세요.
              </div>
            )}
          </div>
          <div style={{ padding: '12px', background: '#f1f5f9', fontSize: '11px', color: '#64748b', borderTop: '1px solid #e2e8f0' }}>
            <i className="fas fa-mouse-pointer"></i> 클릭하여 수식에 추가
          </div>
        </div>

        {/* Column 2: Formula Studio */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>

          {/* Logic Blocks Palette */}
          <div className="logic-palette-row">
            <div style={{ marginRight: '8px', paddingRight: '8px', borderRight: '1px solid #cbd5e1', display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
              {['+', '-', '*', '/', '(', ')'].map(op => (
                <button key={op} className="logic-block-btn" onClick={() => insertAtCursor(` ${op} `)}>{op}</button>
              ))}
            </div>
            <div style={{ marginRight: '8px', paddingRight: '8px', borderRight: '1px solid #cbd5e1', display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
              {['==', '!=', '>', '<', '>=', '<=', '&&', '||'].map(op => (
                <button key={op} className="logic-block-btn logic-compare-btn" onClick={() => insertAtCursor(` ${op} `)} style={{ color: '#0369a1', borderColor: '#bae6fd' }}>{op}</button>
              ))}
            </div>
            <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
              {['SUM', 'AVG', 'MIN', 'MAX', 'IF'].map(fn => (
                <button key={fn} className="logic-block-btn function" onClick={() => insertAtCursor(`${fn}( )`)}>
                  {fn}
                </button>
              ))}
            </div>
          </div>

          {/* Editor Area */}
          <div className="formula-editor-container">
            <div style={{ padding: '12px 20px', background: '#f8fafc', borderBottom: '1px solid #e2e8f0', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
              <span style={{ fontSize: '12px', fontWeight: 800, color: '#475569' }}>
                <i className="fas fa-code-branch"></i> LOGIC BUILDER
              </span>
              <div style={{ display: 'flex', gap: '8px' }}>
                <button className="choice-btn-v3" style={{ padding: '4px 8px', fontSize: '11px' }} onClick={() => onChange('')}>
                  <i className="fas fa-eraser"></i> 초기화
                </button>
              </div>
            </div>

            <textarea
              ref={textareaRef}
              className="formula-textarea-v3"
              placeholder="왼쪽의 변수와 상단의 연산자를 클릭하여 수식을 빌드하세요..."
              value={expression}
              onChange={(e) => onChange(e.target.value)}
              style={{ minHeight: '140px', borderBottom: validationResult && !validationResult.isValid ? '2px solid #ef4444' : undefined }}
            />

            {/* Validation Errors Display */}
            {validationResult && !validationResult.isValid && validationResult.errors.length > 0 && (
              <div style={{ padding: '8px 20px', background: '#fff1f2', borderTop: '1px solid #fecdd3', color: '#be123c', fontSize: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <i className="fas fa-exclamation-triangle"></i>
                <span>{validationResult.errors[0].message}</span>
              </div>
            )}

            {/* Live Logic Preview (Interpreted) */}
            <div style={{ padding: '12px 20px', background: '#fff', borderTop: '1px solid #f1f5f9', borderBottom: '1px solid #f1f5f9' }}>
              <div style={{ fontSize: '11px', fontWeight: 800, color: '#94a3b8', marginBottom: '4px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                <i className="fas fa-microchip"></i> 해석된 로직 (Natural View)
              </div>
              <div style={{ fontSize: '12px', color: '#475569', minHeight: '18px', fontWeight: 500, fontStyle: expression ? 'normal' : 'italic' }}>
                {expression ? (
                  expression
                    .replace(/SUM/g, '합계')
                    .replace(/AVG/g, '평균')
                    .replace(/MIN/g, '최솟값')
                    .replace(/MAX/g, '최댓값')
                    .replace(/IF/g, '만약 ~라면')
                    .replace(/\*/g, ' X ')
                    .replace(/\//g, ' ÷ ')
                    .replace(/\+/g, ' + ')
                    .replace(/-/g, ' - ')
                ) : (
                  '수식을 입력하면 이곳에 사람이 읽기 쉬운 형태로 요약됩니다.'
                )}
              </div>
            </div>

            <div className="formula-footer" style={{ justifyContent: 'space-between' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                <button className="btn btn-primary" onClick={onRunSimulation} disabled={isSimulating || !expression}>
                  {isSimulating ? '검증 중...' : <><i className="fas fa-play-circle"></i> 로직 테스트</>}
                </button>
                <div style={{ fontSize: '12px', color: (expression.match(/\(/g)?.length || 0) === (expression.match(/\)/g)?.length || 0) ? '#94a3b8' : '#e11d48', display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <i className="fas fa-bracket-curly"></i>
                  괄호 쌍: {(expression.match(/\(/g)?.length || 0)} / {(expression.match(/\)/g)?.length || 0)}
                  {(expression.match(/\(/g)?.length || 0) !== (expression.match(/\)/g)?.length || 0) && (
                    <span style={{ fontWeight: 800 }}> (불일치)</span>
                  )}
                </div>
              </div>
              {simulationResult !== null && (
                <div style={{ background: '#ecfdf5', color: '#059669', padding: '6px 12px', borderRadius: '6px', fontWeight: 800, fontSize: '13px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <i className="fas fa-check-circle"></i> 예측 결과값: {typeof simulationResult === 'number' ? simulationResult.toFixed(2) : String(simulationResult)}
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Column 3: Logic Guide */}
        <div className="guidance-panel">
          <div className="guidance-header">
            <i className="fas fa-book" style={{ color: 'var(--blueprint-500)' }}></i>
            함수 사용 가이드
          </div>

          <div className="guidance-step">
            <div className="guidance-text" style={{ width: '100%' }}>
              <div style={{ marginBottom: '16px' }}>
                <div style={{ fontSize: '12px', fontWeight: 800, color: '#334155', marginBottom: '4px' }}>사칙 연산</div>
                <p style={{ fontSize: '12px', color: '#64748b' }}><code>+ - * /</code> 기본 연산자와 괄호<code>( )</code>를 사용하여 계산 순서를 조절합니다.</p>
              </div>

              <div style={{ marginBottom: '16px' }}>
                <div style={{ fontSize: '12px', fontWeight: 800, color: '#334155', marginBottom: '4px' }}>표준 함수</div>
                <ul style={{ paddingLeft: '16px', margin: 0, fontSize: '12px', color: '#64748b' }}>
                  <li><code>SUM(v1, v2...)</code>: 합계</li>
                  <li><code>AVG(v1, v2...)</code>: 평균</li>
                  <li><code>IF(조건, 참, 거짓)</code>: 조건문</li>
                </ul>
              </div>
            </div>
          </div>

          <div style={{ marginTop: 'auto', padding: '16px', background: '#f0f9ff', borderRadius: '12px', border: '1px solid #e0f2fe' }}>
            <div style={{ fontSize: '12px', fontWeight: 800, color: '#0369a1', marginBottom: '4px' }}>PRO TIP</div>
            <div style={{ fontSize: '12px', color: '#0c4a6e', lineHeight: '1.4' }}>
              수식을 완성한 후 <strong>[로직 테스트]</strong>를 눌러보세요. 실제 계산 시 오류가 발생하지 않는지 미리 점검할 수 있습니다.
            </div>
          </div>
        </div>

      </div>
    </div>
  );
};

export default FormulaEditor;