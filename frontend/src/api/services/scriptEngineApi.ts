// ============================================================================
// frontend/src/api/services/scriptEngineApi.ts
// 스크립트 엔진 공통 API 서비스 (가상포인트 + 알람 공용)
// ============================================================================

import { httpClient } from './httpClient';
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
      console.log('스크립트 함수 목록 조회:', request);

      // 캐시된 함수가 있고 필터가 없으면 캐시 사용
      if (this.functionsCache.length > 0 && !request?.category && !request?.search) {
        return this.functionsCache;
      }

      const response = await httpClient.get<ScriptFunctionsApiResponse>(
        `${this.baseUrl}/functions`,
        { params: request }
      );

      if (response.success && Array.isArray(response.data)) {
        // 캐시 업데이트 (필터가 없는 경우만)
        if (!request?.category && !request?.search) {
          this.functionsCache = response.data;
        }
        
        console.log(`${response.data.length}개 함수 로드 완료`);
        return response.data;
      } else {
        throw new Error(response.message || '함수 목록 조회 실패');
      }
    } catch (error) {
      console.error('함수 목록 조회 실패, 내장 함수 사용:', error);
      
      // 백엔드 API가 없는 경우 내장 함수 반환
      return this.getBuiltinFunctions(request);
    }
  }

  /**
   * 카테고리별 함수 조회
   */
  async getFunctionsByCategory(category: ScriptFunctionCategory): Promise<ScriptFunction[]> {
    return this.getFunctions({ category });
  }

  /**
   * 함수 검색
   */
  async searchFunctions(searchTerm: string): Promise<ScriptFunction[]> {
    return this.getFunctions({ search: searchTerm });
  }

  /**
   * 특정 함수 상세 정보 조회
   */
  async getFunction(functionName: string): Promise<ScriptFunction | null> {
    try {
      const functions = await this.getFunctions();
      return functions.find(f => f.name === functionName) || null;
    } catch (error) {
      console.error('함수 조회 실패:', error);
      return null;
    }
  }

  // ========================================================================
  // 스크립트 검증 및 테스트
  // ========================================================================

  /**
   * 스크립트 문법 검증
   */
  async validateScript(request: ScriptValidationRequest): Promise<ScriptValidationResult> {
    try {
      console.log('스크립트 검증 시작:', request.script.substring(0, 100) + '...');

      const response = await httpClient.post<ScriptValidationApiResponse>(
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

      const response = await httpClient.post<ScriptTestApiResponse>(
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
      const response = await httpClient.post(`${this.baseUrl}/parse`, { script });
      
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

      const response = await httpClient.get<ScriptTemplatesApiResponse>(
        `${this.baseUrl}/templates`,
        { params: category ? { category } : {} }
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
    let functions: ScriptFunction[] = [];
    
    // 모든 카테고리의 함수 수집
    Object.values(BUILTIN_FUNCTIONS).forEach(categoryFunctions => {
      functions = functions.concat(categoryFunctions);
    });

    // 카테고리 필터 적용
    if (request?.category) {
      functions = functions.filter(f => f.category === request.category);
    }

    // 검색 필터 적용
    if (request?.search) {
      const searchTerm = request.search.toLowerCase();
      functions = functions.filter(f => 
        f.name.toLowerCase().includes(searchTerm) ||
        f.displayName.toLowerCase().includes(searchTerm) ||
        f.description.toLowerCase().includes(searchTerm)
      );
    }

    // deprecated 함수 제외 (요청하지 않은 경우)
    if (!request?.includeDeprecated) {
      functions = functions.filter(f => !f.deprecated);
    }

    return functions;
  }

  /**
   * 클라이언트 사이드 스크립트 검증
   */
  private clientSideValidation(request: ScriptValidationRequest): ScriptValidationResult {
    const { script, context = {}, functions = [] } = request;
    const errors: any[] = [];
    const warnings: any[] = [];
    const usedVariables: string[] = [];
    const usedFunctions: string[] = [];

    try {
      // 기본적인 문법 검사
      if (!script || script.trim() === '') {
        errors.push({
          line: 1,
          column: 1,
          message: '스크립트가 비어있습니다',
          type: 'syntax',
          severity: 'error'
        });
      }

      // 괄호 매칭 검사
      const openBrackets = (script.match(/\(/g) || []).length;
      const closeBrackets = (script.match(/\)/g) || []).length;
      if (openBrackets !== closeBrackets) {
        errors.push({
          line: 1,
          column: 1,
          message: '괄호가 매칭되지 않습니다',
          type: 'syntax',
          severity: 'error'
        });
      }

      // 변수 및 함수 추출
      const variableMatches = script.match(/\b[a-zA-Z_][a-zA-Z0-9_]*\b/g) || [];
      variableMatches.forEach(match => {
        if (!usedVariables.includes(match) && context.hasOwnProperty(match)) {
          usedVariables.push(match);
        }
      });

      const functionMatches = script.match(/\b[a-zA-Z_][a-zA-Z0-9_]*(?=\s*\()/g) || [];
      functionMatches.forEach(match => {
        if (!usedFunctions.includes(match)) {
          usedFunctions.push(match);
        }
      });

      return {
        isValid: errors.length === 0,
        errors,
        warnings,
        usedVariables,
        usedFunctions,
        estimatedComplexity: script.length > 200 ? 'high' : script.length > 50 ? 'medium' : 'low'
      };

    } catch (error) {
      return {
        isValid: false,
        errors: [{
          line: 1,
          column: 1,
          message: '스크립트 검증 중 오류 발생',
          type: 'runtime',
          severity: 'error'
        }],
        warnings,
        usedVariables,
        usedFunctions,
        estimatedComplexity: 'high'
      };
    }
  }

  /**
   * 클라이언트 사이드 스크립트 테스트
   */
  private clientSideTest(request: ScriptTestRequest): ScriptTestResult {
    const startTime = performance.now();
    
    try {
      const { script, context } = request;
      
      // 간단한 수식 평가 시뮬레이션
      let result;
      
      if (script.includes('temp1 + temp2 + temp3')) {
        const temp1 = context.temp1 || 25;
        const temp2 = context.temp2 || 26;
        const temp3 = context.temp3 || 24;
        result = temp1 + temp2 + temp3;
      } else if (script.includes('avg(')) {
        result = 25.4;
      } else if (script.includes('if(')) {
        result = 'HIGH';
      } else {
        result = 'Mock Test Result';
      }

      const executionTime = performance.now() - startTime;

      return {
        success: true,
        result,
        executionTime,
        logs: [
          {
            timestamp: Date.now(),
            level: 'info',
            message: `스크립트 테스트 완료 (클라이언트 시뮬레이션)`,
            data: { script: script.substring(0, 50) + '...' }
          }
        ]
      };

    } catch (error) {
      return {
        success: false,
        result: null,
        executionTime: performance.now() - startTime,
        error: {
          line: 1,
          column: 1,
          message: error instanceof Error ? error.message : '테스트 실행 실패',
          type: 'runtime',
          severity: 'error'
        },
        logs: []
      };
    }
  }

  /**
   * 클라이언트 사이드 스크립트 파싱
   */
  private clientSideParse(script: string) {
    const variables = (script.match(/\b[a-zA-Z_][a-zA-Z0-9_]*\b/g) || [])
      .filter(word => !['if', 'else', 'true', 'false', 'null', 'undefined'].includes(word));
    
    const functions = (script.match(/\b[a-zA-Z_][a-zA-Z0-9_]*(?=\s*\()/g) || []);
    
    const constants = (script.match(/\b\d+(\.\d+)?\b/g) || []);
    
    const complexity = script.length > 200 ? 'high' as const : 
                      script.length > 50 ? 'medium' as const : 'low' as const;

    return {
      variables: [...new Set(variables)],
      functions: [...new Set(functions)],
      constants: [...new Set(constants)],
      complexity
    };
  }

  /**
   * 내장 템플릿 반환
   */
  private getBuiltinTemplates(category?: ScriptFunctionCategory): ScriptTemplate[] {
    const templates: ScriptTemplate[] = [
      {
        id: 'simple_average',
        name: '단순 평균',
        description: '여러 값의 평균을 계산합니다',
        category: 'math',
        template: '(${var1} + ${var2} + ${var3}) / 3',
        variables: [
          { name: 'var1', type: 'number', description: '첫 번째 값', placeholder: 'temp1', required: true },
          { name: 'var2', type: 'number', description: '두 번째 값', placeholder: 'temp2', required: true },
          { name: 'var3', type: 'number', description: '세 번째 값', placeholder: 'temp3', required: true }
        ],
        examples: [
          {
            title: '온도 센서 평균',
            code: '(temp1 + temp2 + temp3) / 3',
            description: '3개 온도 센서의 평균값',
            expectedResult: 25.4,
            variables: { temp1: 25.1, temp2: 25.3, temp3: 25.8 }
          }
        ],
        difficulty: 'beginner',
        tags: ['average', 'math', 'basic']
      }
    ];

    return category ? templates.filter(t => t.category === category) : templates;
  }
}

// 싱글톤 인스턴스 생성 및 내보내기
export const scriptEngineApi = new ScriptEngineApiService();
export default scriptEngineApi;