// ============================================================================
// frontend/src/types/scriptEngine.ts
// 스크립트 엔진 공통 타입 정의 (가상포인트 + 알람 공용)
// ============================================================================

import { ApiResponse } from './common';

// 스크립트 함수 정의
export interface ScriptFunction {
  id: string;
  name: string;
  displayName: string;
  category: ScriptFunctionCategory;
  description: string;
  syntax: string;
  parameters: ScriptParameter[];
  returnType: ScriptDataType;
  examples: ScriptExample[];
  isBuiltIn: boolean;
  version?: string;
  deprecated?: boolean;
  replacedBy?: string;
}

// 스크립트 함수 카테고리
export type ScriptFunctionCategory = 
  | 'math'          // 수학 함수
  | 'logic'         // 논리 함수  
  | 'time'          // 시간 함수
  | 'string'        // 문자열 함수
  | 'conversion'    // 변환 함수
  | 'statistical'   // 통계 함수
  | 'conditional'   // 조건 함수
  | 'aggregate'     // 집계 함수
  | 'custom';       // 사용자 정의

// 스크립트 데이터 타입
export type ScriptDataType = 'number' | 'boolean' | 'string' | 'any' | 'array' | 'object';

// 함수 파라미터 정의
export interface ScriptParameter {
  name: string;
  type: ScriptDataType;
  required: boolean;
  description: string;
  defaultValue?: any;
  minValue?: number;
  maxValue?: number;
  options?: string[]; // enum 값들
}

// 함수 사용 예제
export interface ScriptExample {
  title: string;
  code: string;
  description: string;
  expectedResult: any;
  variables?: Record<string, any>; // 예제에 필요한 변수들
}

// 스크립트 검증 결과
export interface ScriptValidationResult {
  isValid: boolean;
  errors: ScriptError[];
  warnings: ScriptWarning[];
  usedVariables: string[];
  usedFunctions: string[];
  estimatedComplexity: 'low' | 'medium' | 'high';
  estimatedExecutionTime?: number; // ms
}

// 스크립트 에러
export interface ScriptError {
  line: number;
  column: number;
  message: string;
  type: ScriptErrorType;
  severity: 'error' | 'warning';
  suggestion?: string;
}

export type ScriptErrorType = 
  | 'syntax'        // 문법 오류
  | 'runtime'       // 실행 오류  
  | 'reference'     // 참조 오류 (변수/함수 미정의)
  | 'type'          // 타입 오류
  | 'logic'         // 논리 오류
  | 'performance';  // 성능 경고

// 스크립트 경고
export interface ScriptWarning {
  line: number;
  column: number;
  message: string;
  type: 'performance' | 'style' | 'compatibility' | 'security';
  suggestion?: string;
}

// 스크립트 테스트 결과
export interface ScriptTestResult {
  success: boolean;
  result: any;
  executionTime: number; // ms
  memoryUsage?: number; // bytes
  error?: ScriptError;
  logs: ScriptLog[];
  variableValues?: Record<string, any>; // 실행 후 변수 상태
}

// 스크립트 로그
export interface ScriptLog {
  timestamp: number;
  level: 'debug' | 'info' | 'warn' | 'error';
  message: string;
  data?: any;
}

// 스크립트 컨텍스트 (실행 환경)
export interface ScriptContext {
  variables: Record<string, any>;
  functions?: ScriptFunction[];
  timeout?: number; // ms
  maxMemory?: number; // bytes
  allowedModules?: string[];
  strictMode?: boolean;
}

// 스크립트 메타데이터
export interface ScriptMetadata {
  id?: string;
  name: string;
  description: string;
  author: string;
  version: string;
  category: string;
  tags: string[];
  createdAt: Date;
  updatedAt: Date;
  usageCount?: number;
  avgExecutionTime?: number;
  dependencies: string[];
}

// 스크립트 템플릿
export interface ScriptTemplate {
  id: string;
  name: string;
  description: string;
  category: ScriptFunctionCategory;
  template: string;
  variables: ScriptTemplateVariable[];
  examples: ScriptExample[];
  difficulty: 'beginner' | 'intermediate' | 'advanced';
  tags: string[];
}

// 템플릿 변수
export interface ScriptTemplateVariable {
  name: string;
  type: ScriptDataType;
  description: string;
  placeholder: string;
  required: boolean;
  defaultValue?: any;
}

// API 요청 타입들
export interface ScriptValidationRequest {
  script: string;
  context?: Record<string, any>;
  functions?: string[];
  strictMode?: boolean;
}

export interface ScriptTestRequest {
  script: string;
  context: Record<string, any>;
  timeout?: number;
  includeDebugInfo?: boolean;
}

export interface ScriptFunctionSearchRequest {
  category?: ScriptFunctionCategory;
  search?: string;
  includeDeprecated?: boolean;
}

// API 응답 타입들
export type ScriptFunctionsApiResponse = ApiResponse<ScriptFunction[]>;
export type ScriptValidationApiResponse = ApiResponse<ScriptValidationResult>;
export type ScriptTestApiResponse = ApiResponse<ScriptTestResult>;
export type ScriptTemplatesApiResponse = ApiResponse<ScriptTemplate[]>;

// 내장 함수 라이브러리 정의
export const BUILTIN_FUNCTIONS: Record<ScriptFunctionCategory, ScriptFunction[]> = {
  math: [
    {
      id: 'abs',
      name: 'abs',
      displayName: '절댓값',
      category: 'math',
      description: '숫자의 절댓값을 반환합니다.',
      syntax: 'abs(x)',
      parameters: [
        { name: 'x', type: 'number', required: true, description: '입력 숫자' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'abs(-5)', description: '음수의 절댓값', expectedResult: 5 }
      ],
      isBuiltIn: true
    },
    {
      id: 'sqrt',
      name: 'sqrt',
      displayName: '제곱근',
      category: 'math',
      description: '숫자의 제곱근을 반환합니다.',
      syntax: 'sqrt(x)',
      parameters: [
        { name: 'x', type: 'number', required: true, description: '입력 숫자 (0 이상)', minValue: 0 }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'sqrt(16)', description: '16의 제곱근', expectedResult: 4 }
      ],
      isBuiltIn: true
    },
    {
      id: 'pow',
      name: 'pow',
      displayName: '거듭제곱',
      category: 'math',
      description: '첫 번째 숫자를 두 번째 숫자만큼 거듭제곱합니다.',
      syntax: 'pow(base, exponent)',
      parameters: [
        { name: 'base', type: 'number', required: true, description: '밑' },
        { name: 'exponent', type: 'number', required: true, description: '지수' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'pow(2, 3)', description: '2의 3제곱', expectedResult: 8 }
      ],
      isBuiltIn: true
    }
  ],
  logic: [
    {
      id: 'if',
      name: 'if',
      displayName: '조건문',
      category: 'logic',
      description: '조건에 따라 다른 값을 반환합니다.',
      syntax: 'if(condition, trueValue, falseValue)',
      parameters: [
        { name: 'condition', type: 'boolean', required: true, description: '조건식' },
        { name: 'trueValue', type: 'any', required: true, description: '조건이 참일 때 반환값' },
        { name: 'falseValue', type: 'any', required: true, description: '조건이 거짓일 때 반환값' }
      ],
      returnType: 'any',
      examples: [
        { title: '기본 사용법', code: 'if(temp > 25, "HIGH", "NORMAL")', description: '온도에 따른 상태', expectedResult: 'HIGH', variables: { temp: 30 } }
      ],
      isBuiltIn: true
    }
  ],
  time: [
    {
      id: 'now',
      name: 'now',
      displayName: '현재 시간',
      category: 'time',
      description: '현재 시간을 반환합니다.',
      syntax: 'now()',
      parameters: [],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'now()', description: '현재 타임스탬프', expectedResult: 1640995200000 }
      ],
      isBuiltIn: true
    }
  ],
  string: [],
  conversion: [
    {
      id: 'celsius2fahrenheit',
      name: 'celsius2fahrenheit',
      displayName: '섭씨→화씨',
      category: 'conversion',
      description: '섭씨 온도를 화씨 온도로 변환합니다.',
      syntax: 'celsius2fahrenheit(celsius)',
      parameters: [
        { name: 'celsius', type: 'number', required: true, description: '섭씨 온도' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'celsius2fahrenheit(0)', description: '물의 어는점', expectedResult: 32 }
      ],
      isBuiltIn: true
    }
  ],
  statistical: [],
  conditional: [],
  aggregate: [],
  custom: []
};

// 스크립트 상수
export const SCRIPT_CONSTANTS = {
  PI: Math.PI,
  E: Math.E,
  MAX_EXECUTION_TIME: 10000, // ms
  MAX_MEMORY_USAGE: 1024 * 1024, // 1MB
  MAX_SCRIPT_LENGTH: 10000 // characters
} as const;