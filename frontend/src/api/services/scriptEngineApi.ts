// ============================================================================
// frontend/src/api/services/scriptEngineApi.ts
// 스크립트 엔진 공통 API 서비스 (수정됨 - 통합 클라이언트 사용)
// ============================================================================

import { apiClient } from '../client';
import {
  ScriptFunction,
  ScriptValidationRequest,
  ScriptValidationResult,
  ScriptTestRequest,
  ScriptTestResult,
  ScriptFunctionSearchRequest,
  ScriptTemplate,
  ScriptFunctionsApiResponse,
  ScriptValidationApiResponse,
  ScriptTestApiResponse,
  ScriptTemplatesApiResponse,
  BUILTIN_FUNCTIONS,
  ScriptFunctionCategory
} from '../../types/scriptEngine';

class ScriptEngineApiService {
  private readonly baseUrl = '/api/script-engine';
  private functionsCache: ScriptFunction[] = [];
  private templatesCache: ScriptTemplate[] = [];

  // ========================================================================
  // 함수 라이브러리 관리
  // ========================================================================

  /**
   * 사용 가능한 함수 목록 조회
   */
  async getFunctions(request?: ScriptFunctionSearchRequest): Promise<ScriptFunction[]> {
    try {
      console.log('스크립트 함수 라이브러리 조회');

      // 캐시가 있고 필터가 없으면 캐시 반환
      if (this.functionsCache.length > 0 && !request) {
        return this.functionsCache;
      }

      const response = await apiClient.get<ScriptFunctionsApiResponse>(
        `${this.baseUrl}/functions`,
        request
      );

      if (response.success && Array.isArray(response.data)) {
        // 캐시 업데이트 (필터가 없는 경우만)
        if (!request) {
          this.functionsCache = response.data;
        }
        return response.data;
      } else {
        throw new Error(response.message || '함수 조회 실패');
      }
    } catch (error) {
      console.error('함수 조회 실패:', error);
      // 백엔드 API가 없는 경우 내장 함수 반환
      return this.getBuiltinFunctions(request);
    }
  }

  /**
   * 스크립트 문법 검증
   */
  async validateScript(request: ScriptValidationRequest): Promise<ScriptValidationResult> {
    try {
      console.log('스크립트 검증 시작');

      const response = await apiClient.post<ScriptValidationApiResponse>(
        `${this.baseUrl}/validate`,
        request
      );

      if (response.success && response.data) {
        console.log('스크립트 검증 완료:', response.data.isValid ? '성공' : '실패');
        return response.data;
      } else {
        throw new Error(response.message || '스크립트 검증 실패');
      }
    } catch (error) {
      console.error('스크립트 검증 실패:', error);
      
      // 백엔드 API가 없는 경우 클라이언트 검증
      return this.clientSideValidation(request);
    }
  }

  /**
   * 스크립트 테스트 실행
   */
  async testScript(request: ScriptTestRequest): Promise<ScriptTestResult> {
    try {
      console.log('스크립트 테스트 시작');

      const response = await apiClient.post<ScriptTestApiResponse>(
        `${this.baseUrl}/test`,
        {
          ...request,
          timeout: request.timeout || 5000
        }
      );

      if (response.success && response.data) {
        console.log('스크립트 테스트 완료');
        return response.data;
      } else {
        throw new Error(response.message || '스크립트 테스트 실패');
      }
    } catch (error) {
      console.error('스크립트 테스트 실패:', error);
      
      // 백엔드 API가 없는 경우 클라이언트 시뮬레이션
      return this.clientSideTest(request);
    }
  }

  /**
   * 스크립트 구문 분석
   */
  async parseScript(script: string): Promise<{
    variables: string[];
    functions: string[];
    constants: string[];
    complexity: 'low' | 'medium' | 'high';
  }> {
    try {
      const response = await apiClient.post(`${this.baseUrl}/parse`, { script });
      
      if (response.success) {
        return response.data;
      } else {
        throw new Error(response.message);
      }
    } catch (error) {
      console.error('스크립트 파싱 실패:', error);
      
      // 클라이언트 사이드 파싱
      return this.clientSideParse(script);
    }
  }

  // ========================================================================
  // 템플릿 관리
  // ========================================================================

  /**
   * 스크립트 템플릿 목록 조회
   */
  async getTemplates(category?: ScriptFunctionCategory): Promise<ScriptTemplate[]> {
    try {
      console.log('스크립트 템플릿 조회');

      if (this.templatesCache.length > 0 && !category) {
        return this.templatesCache;
      }

      const response = await apiClient.get<ScriptTemplatesApiResponse>(
        `${this.baseUrl}/templates`,
        category ? { category } : {}
      );

      if (response.success && Array.isArray(response.data)) {
        if (!category) {
          this.templatesCache = response.data;
        }
        return response.data;
      } else {
        throw new Error(response.message || '템플릿 조회 실패');
      }
    } catch (error) {
      console.error('템플릿 조회 실패:', error);
      return this.getBuiltinTemplates(category);
    }
  }

  // ========================================================================
  // 클라이언트 사이드 구현 (백업용)
  // ========================================================================

  /**
   * 내장 함수 반환
   */
  private getBuiltinFunctions(request?: ScriptFunctionSearchRequest): ScriptFunction[] {
    console.log('내장 함수 라이브러리 사용');
    
    let functions: ScriptFunction[] = [];
    
    if (request?.category) {
      functions = BUILTIN_FUNCTIONS[request.category] || [];
    } else {
      functions = Object.values(BUILTIN_FUNCTIONS).flat();
    }

    // 검색 필터 적용
    if (request?.search) {
      const searchTerm = request.search.toLowerCase();
      functions = functions.filter(func => 
        func.name.toLowerCase().includes(searchTerm) ||
        func.displayName.toLowerCase().includes(searchTerm) ||
        func.description.toLowerCase().includes(searchTerm)
      );
    }

    return functions;
  }

  /**
   * 클라이언트 사이드 검증 (백업)
   */
  private clientSideValidation(request: ScriptValidationRequest): ScriptValidationResult {
    console.log('클라이언트 사이드 스크립트 검증 사용');
    
    const errors = [];
    const warnings = [];
    
    // 기본적인 문법 체크
    try {
      // JavaScript 문법 검사
      new Function(request.script);
      
      return {
        isValid: true,
        errors: [],
        warnings: [],
        usedVariables: this.extractVariables(request.script),
        usedFunctions: this.extractFunctions(request.script)
      };
    } catch (error) {
      return {
        isValid: false,
        errors: [{
          line: 1,
          column: 1,
          message: error instanceof Error ? error.message : '문법 오류',
          type: 'syntax' as const,
          severity: 'error' as const
        }],
        warnings: [],
        usedVariables: [],
        usedFunctions: []
      };
    }
  }

  /**
   * 클라이언트 사이드 테스트 (백업)
   */
  private clientSideTest(request: ScriptTestRequest): ScriptTestResult {
    console.log('클라이언트 사이드 스크립트 테스트 사용');
    
    const startTime = performance.now();
    
    try {
      // 안전한 실행 환경 생성
      const func = new Function(
        ...Object.keys(request.context),
        `return (${request.script})`
      );
      
      const result = func(...Object.values(request.context));
      const executionTime = performance.now() - startTime;
      
      return {
        success: true,
        result: result,
        executionTime: executionTime,
        logs: [`실행 시간: ${executionTime.toFixed(2)}ms`]
      };
    } catch (error) {
      const executionTime = performance.now() - startTime;
      
      return {
        success: false,
        result: null,
        executionTime: executionTime,
        error: {
          line: 1,
          column: 1,
          message: error instanceof Error ? error.message : '실행 오류',
          type: 'runtime' as const,
          severity: 'error' as const
        },
        logs: [`실행 실패: ${executionTime.toFixed(2)}ms`]
      };
    }
  }

  /**
   * 스크립트에서 변수 추출
   */
  private extractVariables(script: string): string[] {
    const variables = new Set<string>();
    const regex = /\b([a-zA-Z_$][a-zA-Z0-9_$]*)\b/g;
    let match;
    
    while ((match = regex.exec(script)) !== null) {
      const variable = match[1];
      // JavaScript 예약어가 아닌 경우만 추가
      if (!this.isReservedWord(variable)) {
        variables.add(variable);
      }
    }
    
    return Array.from(variables);
  }

  /**
   * 스크립트에서 함수 추출
   */
  private extractFunctions(script: string): string[] {
    const functions = new Set<string>();
    const regex = /\b([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(/g;
    let match;
    
    while ((match = regex.exec(script)) !== null) {
      functions.add(match[1]);
    }
    
    return Array.from(functions);
  }

  /**
   * JavaScript 예약어 체크
   */
  private isReservedWord(word: string): boolean {
    const reserved = [
      'break', 'case', 'catch', 'class', 'const', 'continue', 'debugger', 'default',
      'delete', 'do', 'else', 'export', 'extends', 'finally', 'for', 'function',
      'if', 'import', 'in', 'instanceof', 'let', 'new', 'return', 'super', 'switch',
      'this', 'throw', 'try', 'typeof', 'var', 'void', 'while', 'with', 'yield'
    ];
    return reserved.includes(word);
  }

  /**
   * 클라이언트 사이드 파싱 (백업)
   */
  private clientSideParse(script: string): {
    variables: string[];
    functions: string[];
    constants: string[];
    complexity: 'low' | 'medium' | 'high';
  } {
    const variables = this.extractVariables(script);
    const functions = this.extractFunctions(script);
    
    // 상수 추출 (숫자, 문자열 리터럴)
    const constants: string[] = [];
    const numberMatches = script.match(/\b\d+\.?\d*\b/g) || [];
    const stringMatches = script.match(/["'`][^"'`]*["'`]/g) || [];
    constants.push(...numberMatches, ...stringMatches);
    
    // 복잡도 계산
    const complexity = this.calculateComplexity(script);
    
    return {
      variables: Array.from(new Set(variables)),
      functions: Array.from(new Set(functions)),
      constants: Array.from(new Set(constants)),
      complexity
    };
  }

  /**
   * 복잡도 계산
   */
  private calculateComplexity(script: string): 'low' | 'medium' | 'high' {
    const lines = script.split('\n').length;
    const operators = (script.match(/[+\-*/=<>!&|]/g) || []).length;
    const controlFlow = (script.match(/\b(if|else|for|while|switch|case)\b/g) || []).length;
    
    const score = lines + operators + (controlFlow * 2);
    
    if (score < 10) return 'low';
    if (score < 25) return 'medium';
    return 'high';
  }

  /**
   * 내장 템플릿 반환 (백업)
   */
  private getBuiltinTemplates(category?: ScriptFunctionCategory): ScriptTemplate[] {
    console.log('내장 템플릿 사용');
    
    const templates: ScriptTemplate[] = [
      {
        id: 'average',
        name: '평균값 계산',
        description: '여러 입력값의 평균을 계산합니다',
        category: 'math',
        template: '({{input1}} + {{input2}} + {{input3}}) / 3',
        variables: [
          { name: 'input1', type: 'number', description: '첫 번째 입력값', placeholder: '온도1', required: true },
          { name: 'input2', type: 'number', description: '두 번째 입력값', placeholder: '온도2', required: true },
          { name: 'input3', type: 'number', description: '세 번째 입력값', placeholder: '온도3', required: true }
        ],
        examples: [],
        difficulty: 'beginner',
        tags: ['평균', '계산', '기본']
      },
      {
        id: 'conditional',
        name: '조건부 값',
        description: '조건에 따라 다른 값을 반환합니다',
        category: 'logic',
        template: '{{condition}} ? {{trueValue}} : {{falseValue}}',
        variables: [
          { name: 'condition', type: 'boolean', description: '조건식', placeholder: 'temperature > 30', required: true },
          { name: 'trueValue', type: 'any', description: '참일 때 값', placeholder: '"높음"', required: true },
          { name: 'falseValue', type: 'any', description: '거짓일 때 값', placeholder: '"정상"', required: true }
        ],
        examples: [],
        difficulty: 'beginner',
        tags: ['조건', '분기', '삼항연산자']
      }
    ];
    
    return category ? 
      templates.filter(t => t.category === category) : 
      templates;
  }
}

// 싱글톤 인스턴스 생성 및 export
export const scriptEngineApi = new ScriptEngineApiService();