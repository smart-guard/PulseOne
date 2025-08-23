// ============================================================================
// frontend/src/hooks/shared/useScriptEngine.ts
// 스크립트 엔진 공통 훅 (가상포인트 + 알람 공용)
// ============================================================================

import { useState, useCallback, useEffect } from 'react';
import { scriptEngineApi } from '../../api/services/scriptEngineApi';
import {
  ScriptFunction,
  ScriptValidationResult,
  ScriptTestResult,
  ScriptFunctionCategory,
  ScriptTemplate
} from '../../types/scriptEngine';

interface UseScriptEngineOptions {
  autoLoadFunctions?: boolean;
  initialVariables?: Record<string, any>;
  defaultTimeout?: number;
}

export const useScriptEngine = (options: UseScriptEngineOptions = {}) => {
  const { 
    autoLoadFunctions = true, 
    initialVariables = {},
    defaultTimeout = 5000
  } = options;

  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [functions, setFunctions] = useState<ScriptFunction[]>([]);
  const [templates, setTemplates] = useState<ScriptTemplate[]>([]);
  const [variables, setVariables] = useState<Record<string, any>>(initialVariables);
  const [validationResult, setValidationResult] = useState<ScriptValidationResult | null>(null);
  const [testResult, setTestResult] = useState<ScriptTestResult | null>(null);
  
  const [isLoadingFunctions, setIsLoadingFunctions] = useState(false);
  const [isValidating, setIsValidating] = useState(false);
  const [isTesting, setIsTesting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // ========================================================================
  // 함수 라이브러리 관리
  // ========================================================================

  /**
   * 함수 라이브러리 로드
   */
  const loadFunctions = useCallback(async (category?: ScriptFunctionCategory) => {
    setIsLoadingFunctions(true);
    setError(null);
    
    try {
      console.log('스크립트 함수 라이브러리 로딩:', category || 'all');
      
      const loadedFunctions = await scriptEngineApi.getFunctions({ 
        category,
        includeDeprecated: false 
      });
      
      setFunctions(loadedFunctions);
      console.log(`${loadedFunctions.length}개 함수 로드 완료`);
      
    } catch (error) {
      console.error('함수 라이브러리 로딩 실패:', error);
      setError('함수 라이브러리를 불러오는데 실패했습니다.');
    } finally {
      setIsLoadingFunctions(false);
    }
  }, []);

  /**
   * 함수 검색
   */
  const searchFunctions = useCallback(async (searchTerm: string) => {
    if (!searchTerm.trim()) {
      return functions;
    }
    
    try {
      return await scriptEngineApi.searchFunctions(searchTerm);
    } catch (error) {
      console.error('함수 검색 실패:', error);
      
      // 로컬 검색으로 폴백
      return functions.filter(func =>
        func.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        func.displayName.toLowerCase().includes(searchTerm.toLowerCase()) ||
        func.description.toLowerCase().includes(searchTerm.toLowerCase())
      );
    }
  }, [functions]);

  /**
   * 카테고리별 함수 조회
   */
  const getFunctionsByCategory = useCallback((category: ScriptFunctionCategory) => {
    return functions.filter(func => func.category === category);
  }, [functions]);

  /**
   * 특정 함수 정보 조회
   */
  const getFunction = useCallback((functionName: string) => {
    return functions.find(func => func.name === functionName) || null;
  }, [functions]);

  // ========================================================================
  // 스크립트 검증 및 테스트
  // ========================================================================

  /**
   * 스크립트 검증
   */
  const validateScript = useCallback(async (
    script: string, 
    context?: Record<string, any>,
    strictMode = false
  ) => {
    if (!script.trim()) {
      setValidationResult(null);
      return null;
    }
    
    setIsValidating(true);
    setError(null);
    
    try {
      console.log('스크립트 검증 시작');
      
      const result = await scriptEngineApi.validateScript({
        script,
        context: { ...variables, ...context },
        functions: functions.map(f => f.name),
        strictMode
      });
      
      setValidationResult(result);
      console.log('스크립트 검증 완료:', result.isValid ? '성공' : '실패');
      
      return result;
      
    } catch (error) {
      console.error('스크립트 검증 실패:', error);
      setError('스크립트 검증 중 오류가 발생했습니다.');
      return null;
    } finally {
      setIsValidating(false);
    }
  }, [functions, variables]);

  /**
   * 스크립트 테스트 실행
   */
  const testScript = useCallback(async (
    script: string,
    context?: Record<string, any>,
    timeout = defaultTimeout
  ) => {
    if (!script.trim()) {
      setTestResult(null);
      return null;
    }
    
    setIsTesting(true);
    setError(null);
    
    try {
      console.log('스크립트 테스트 실행 시작');
      
      const result = await scriptEngineApi.testScript({
        script,
        context: { ...variables, ...context },
        timeout,
        includeDebugInfo: true
      });
      
      setTestResult(result);
      console.log('스크립트 테스트 완료:', result.success ? '성공' : '실패');
      
      return result;
      
    } catch (error) {
      console.error('스크립트 테스트 실패:', error);
      setError('스크립트 테스트 중 오류가 발생했습니다.');
      
      // 실패한 결과 객체 생성
      const failedResult: ScriptTestResult = {
        success: false,
        result: null,
        executionTime: 0,
        error: {
          line: 1,
          column: 1,
          message: error instanceof Error ? error.message : '테스트 실행 실패',
          type: 'runtime',
          severity: 'error'
        },
        logs: []
      };
      
      setTestResult(failedResult);
      return failedResult;
    } finally {
      setIsTesting(false);
    }
  }, [variables, defaultTimeout]);

  /**
   * 스크립트 파싱 (변수, 함수 추출)
   */
  const parseScript = useCallback(async (script: string) => {
    try {
      return await scriptEngineApi.parseScript(script);
    } catch (error) {
      console.error('스크립트 파싱 실패:', error);
      return {
        variables: [],
        functions: [],
        constants: [],
        complexity: 'low' as const
      };
    }
  }, []);

  // ========================================================================
  // 템플릿 관리
  // ========================================================================

  /**
   * 템플릿 로드
   */
  const loadTemplates = useCallback(async (category?: ScriptFunctionCategory) => {
    try {
      console.log('스크립트 템플릿 로딩:', category || 'all');
      
      const loadedTemplates = await scriptEngineApi.getTemplates(category);
      setTemplates(loadedTemplates);
      
      console.log(`${loadedTemplates.length}개 템플릿 로드 완료`);
      
    } catch (error) {
      console.error('템플릿 로딩 실패:', error);
      setError('템플릿을 불러오는데 실패했습니다.');
    }
  }, []);

  /**
   * 템플릿 적용
   */
  const applyTemplate = useCallback((template: ScriptTemplate, templateVariables: Record<string, any>) => {
    let script = template.template;
    
    // 템플릿 변수 치환
    template.variables.forEach(variable => {
      const value = templateVariables[variable.name] || variable.defaultValue || variable.placeholder;
      const pattern = new RegExp(`\\$\\{${variable.name}\\}`, 'g');
      script = script.replace(pattern, String(value));
    });
    
    return script;
  }, []);

  // ========================================================================
  // 변수 관리
  // ========================================================================

  /**
   * 변수 업데이트
   */
  const updateVariables = useCallback((newVariables: Record<string, any>) => {
    setVariables(prev => ({ ...prev, ...newVariables }));
  }, []);

  /**
   * 변수 초기화
   */
  const resetVariables = useCallback(() => {
    setVariables(initialVariables);
  }, [initialVariables]);

  /**
   * 단일 변수 설정
   */
  const setVariable = useCallback((name: string, value: any) => {
    setVariables(prev => ({ ...prev, [name]: value }));
  }, []);

  /**
   * 변수 제거
   */
  const removeVariable = useCallback((name: string) => {
    setVariables(prev => {
      const newVars = { ...prev };
      delete newVars[name];
      return newVars;
    });
  }, []);

  // ========================================================================
  // 유틸리티 함수들
  // ========================================================================

  /**
   * 함수 자동 완성 제안
   */
  const getSuggestions = useCallback((input: string, cursorPosition: number) => {
    const beforeCursor = input.substring(0, cursorPosition);
    const lastWord = beforeCursor.split(/[^a-zA-Z0-9_]/).pop() || '';
    
    if (lastWord.length < 2) return [];
    
    const suggestions = [];
    
    // 함수 제안
    functions.forEach(func => {
      if (func.name.toLowerCase().startsWith(lastWord.toLowerCase())) {
        suggestions.push({
          type: 'function' as const,
          text: func.name,
          displayText: `${func.name}() - ${func.displayName}`,
          description: func.description,
          insertText: `${func.name}(${func.parameters.map(p => p.name).join(', ')})`
        });
      }
    });
    
    // 변수 제안
    Object.keys(variables).forEach(varName => {
      if (varName.toLowerCase().startsWith(lastWord.toLowerCase())) {
        suggestions.push({
          type: 'variable' as const,
          text: varName,
          displayText: `${varName} = ${variables[varName]}`,
          description: `변수 (현재값: ${variables[varName]})`,
          insertText: varName
        });
      }
    });
    
    return suggestions.sort((a, b) => a.text.localeCompare(b.text));
  }, [functions, variables]);

  /**
   * 스크립트 포맷팅
   */
  const formatScript = useCallback((script: string) => {
    try {
      // 기본적인 포맷팅 (들여쓰기, 줄바꿈 등)
      return script
        .replace(/\s*\(\s*/g, '(')
        .replace(/\s*\)\s*/g, ')')
        .replace(/\s*,\s*/g, ', ')
        .replace(/\s*\+\s*/g, ' + ')
        .replace(/\s*-\s*/g, ' - ')
        .replace(/\s*\*\s*/g, ' * ')
        .replace(/\s*\/\s*/g, ' / ')
        .replace(/\s*=\s*/g, ' = ')
        .replace(/\s*>\s*/g, ' > ')
        .replace(/\s*<\s*/g, ' < ')
        .replace(/\s*>=\s*/g, ' >= ')
        .replace(/\s*<=\s*/g, ' <= ')
        .replace(/\s*==\s*/g, ' == ')
        .replace(/\s*!=\s*/g, ' != ')
        .trim();
    } catch (error) {
      console.error('스크립트 포맷팅 실패:', error);
      return script;
    }
  }, []);

  /**
   * 스크립트 복잡도 계산
   */
  const calculateComplexity = useCallback((script: string) => {
    const length = script.length;
    const functionCalls = (script.match(/\w+\(/g) || []).length;
    const operators = (script.match(/[+\-*\/=<>!&|]/g) || []).length;
    const conditionals = (script.match(/\bif\b|\?/g) || []).length;
    
    const score = length * 0.1 + functionCalls * 2 + operators * 1 + conditionals * 3;
    
    if (score < 10) return 'low';
    if (score < 30) return 'medium';
    return 'high';
  }, []);

  /**
   * 에러 정리
   */
  const clearResults = useCallback(() => {
    setValidationResult(null);
    setTestResult(null);
    setError(null);
  }, []);

  // ========================================================================
  // 초기화
  // ========================================================================

  useEffect(() => {
    if (autoLoadFunctions) {
      loadFunctions();
    }
  }, [autoLoadFunctions, loadFunctions]);

  // ========================================================================
  // 반환값
  // ========================================================================

  return {
    // 상태
    functions,
    templates,
    variables,
    validationResult,
    testResult,
    error,
    
    // 로딩 상태
    isLoadingFunctions,
    isValidating,
    isTesting,
    
    // 함수 라이브러리 관리
    loadFunctions,
    searchFunctions,
    getFunctionsByCategory,
    getFunction,
    
    // 스크립트 검증 및 테스트
    validateScript,
    testScript,
    parseScript,
    
    // 템플릿 관리
    loadTemplates,
    applyTemplate,
    
    // 변수 관리
    updateVariables,
    resetVariables,
    setVariable,
    removeVariable,
    
    // 유틸리티
    getSuggestions,
    formatScript,
    calculateComplexity,
    clearResults,
    
    // 상수
    categories: ['math', 'logic', 'time', 'string', 'conversion', 'statistical', 'conditional', 'aggregate'] as ScriptFunctionCategory[],
    dataTypes: ['number', 'boolean', 'string', 'any'] as const
  };
};