// ============================================================================
// frontend/src/components/VirtualPoints/FormulaHelper/FormulaHelper.tsx
// 수식 도우미 컴포넌트 - 간단한 버전
// ============================================================================

import React, { useState } from 'react';
import { ScriptFunction } from '../../../types/scriptEngine';

interface FormulaHelperProps {
  isOpen: boolean;
  onClose: () => void;
  onInsertFunction: (functionCode: string) => void;
  functions?: ScriptFunction[];
}

export const FormulaHelper: React.FC<FormulaHelperProps> = ({
  isOpen,
  onClose,
  onInsertFunction,
  functions = []
}) => {
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [searchTerm, setSearchTerm] = useState('');

  if (!isOpen) return null;

  // 기본 함수들 (functions prop이 비어있을 때 사용)
  const defaultFunctions: ScriptFunction[] = [
    {
      id: '1',
      name: 'Math.max',
      displayName: 'Maximum',
      category: 'math',
      description: 'Returns the maximum value among multiple values',
      syntax: 'Math.max(a, b, ...)',
      parameters: [
        { name: 'a', type: 'number', required: true, description: 'First value' },
        { name: 'b', type: 'number', required: true, description: 'Second value' }
      ],
      returnType: 'number',
      examples: [
        { code: 'Math.max(10, 20, 5)', description: 'Returns 20', expectedResult: 20, title: 'Math.max Example' }
      ],
      isBuiltIn: true
    },
    {
      id: '2',
      name: 'Math.min',
      displayName: 'Minimum',
      category: 'math',
      description: 'Returns the minimum value among multiple values',
      syntax: 'Math.min(a, b, ...)',
      parameters: [
        { name: 'a', type: 'number', required: true, description: 'First value' },
        { name: 'b', type: 'number', required: true, description: 'Second value' }
      ],
      returnType: 'number',
      examples: [
        { code: 'Math.min(10, 20, 5)', description: 'Returns 5', expectedResult: 5, title: 'Math.min Example' }
      ],
      isBuiltIn: true
    },
    {
      id: '3',
      name: 'Math.round',
      displayName: 'Round',
      category: 'math',
      description: 'Rounds to the nearest integer',
      syntax: 'Math.round(value)',
      parameters: [
        { name: 'value', type: 'number', required: true, description: 'Value to round' }
      ],
      returnType: 'number',
      examples: [
        { code: 'Math.round(4.7)', description: 'Returns 5', expectedResult: 5, title: 'Math.round Example' }
      ],
      isBuiltIn: true
    },
    {
      id: '4',
      name: 'Math.abs',
      displayName: 'Absolute Value',
      category: 'math',
      description: 'Returns the absolute value',
      syntax: 'Math.abs(value)',
      parameters: [
        { name: 'value', type: 'number', required: true, description: 'Value to get absolute of' }
      ],
      returnType: 'number',
      examples: [
        { code: 'Math.abs(-5)', description: 'Returns 5', expectedResult: 5, title: 'Math.abs Example' }
      ],
      isBuiltIn: true
    }
  ];

  const availableFunctions = functions.length > 0 ? functions : defaultFunctions;

  const filteredFunctions = availableFunctions.filter(func => {
    const matchesCategory = selectedCategory === 'all' || func.category === selectedCategory;
    const matchesSearch = searchTerm === '' ||
      func.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      func.displayName.toLowerCase().includes(searchTerm.toLowerCase());

    return matchesCategory && matchesSearch;
  });

  const categories = [
    { value: 'all', label: 'All Functions' },
    { value: 'math', label: 'Math' },
    { value: 'logic', label: 'Logic' },
    { value: 'time', label: 'Time' },
    { value: 'string', label: 'String' }
  ];

  return (
    <div className="formula-helper-overlay" onClick={onClose}>
      <div className="formula-helper-modal" onClick={(e) => e.stopPropagation()}>
        <div className="helper-header">
          <h3>
            <i className="fas fa-function"></i>
            함수 도우미
          </h3>
          <button onClick={onClose} className="close-btn">
            <i className="fas fa-times"></i>
          </button>
        </div>

        <div className="helper-filters">
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
            {categories.map(cat => (
              <option key={cat.value} value={cat.value}>{cat.label}</option>
            ))}
          </select>
        </div>

        <div className="functions-list">
          {filteredFunctions.length === 0 ? (
            <div className="no-functions">
              <i className="fas fa-info-circle"></i>
              검색 결과가 없습니다
            </div>
          ) : (
            filteredFunctions.map(func => (
              <div
                key={func.id}
                className="function-item"
                onClick={() => {
                  onInsertFunction(func.syntax);
                  onClose();
                }}
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
                  <div className="function-example">
                    <strong>예제:</strong> <code>{func.examples[0].code}</code>
                  </div>
                )}
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
};