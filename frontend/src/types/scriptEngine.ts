// ============================================================================
// frontend/src/types/scriptEngine.ts
// 스크립트 엔진 공통 타입 정의 (가상포인트 + 알람 공용)
// ============================================================================

import { ApiResponse } from './common';

// ============================================================================
// 스크립트 함수 및 타입 정의
// ============================================================================

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

export type ScriptDataType = 'number' | 'boolean' | 'string' | 'any' | 'array' | 'object';

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

export interface ScriptExample {
  title: string;
  code: string;
  description: string;
  expectedResult: any;
  variables?: Record<string, any>; // 예제에 필요한 변수들
}

// ============================================================================
// 스크립트 검증 및 테스트
// ============================================================================

export interface ScriptValidationResult {
  isValid: boolean;
  errors: ScriptError[];
  warnings: ScriptWarning[];
  usedVariables: string[];
  usedFunctions: string[];
  estimatedComplexity: 'low' | 'medium' | 'high';
  estimatedExecutionTime?: number; // ms
}

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

export interface ScriptWarning {
  line: number;
  column: number;
  message: string;
  type: 'performance' | 'style' | 'compatibility' | 'security';
  suggestion?: string;
}

export interface ScriptTestResult {
  success: boolean;
  result: any;
  executionTime: number; // ms
  memoryUsage?: number; // bytes
  error?: ScriptError;
  logs: ScriptLog[];
  variableValues?: Record<string, any>; // 실행 후 변수 상태
}

export interface ScriptLog {
  timestamp: number;
  level: 'debug' | 'info' | 'warn' | 'error';
  message: string;
  data?: any;
}

// ============================================================================
// 스크립트 컨텍스트 및 메타데이터
// ============================================================================

export interface ScriptContext {
  variables: Record<string, any>;
  functions?: ScriptFunction[];
  timeout?: number; // ms
  maxMemory?: number; // bytes
  allowedModules?: string[];
  strictMode?: boolean;
}

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

// ============================================================================
// 스크립트 템플릿
// ============================================================================

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

export interface ScriptTemplateVariable {
  name: string;
  type: ScriptDataType;
  description: string;
  placeholder: string;
  required: boolean;
  defaultValue?: any;
}

// ============================================================================
// API 요청/응답 타입
// ============================================================================

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

export type ScriptFunctionsApiResponse = ApiResponse<ScriptFunction[]>;
export type ScriptValidationApiResponse = ApiResponse<ScriptValidationResult>;
export type ScriptTestApiResponse = ApiResponse<ScriptTestResult>;
export type ScriptTemplatesApiResponse = ApiResponse<ScriptTemplate[]>;

// ============================================================================
// 내장 함수 라이브러리 정의 (수정됨)
// ============================================================================

export const BUILTIN_FUNCTIONS: Record<ScriptFunctionCategory, ScriptFunction[]> = {
  // 수학 함수
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
        { title: '기본 사용법', code: 'abs(-5)', description: '음수의 절댓값', expectedResult: 5 },
        { title: '양수 입력', code: 'abs(3.14)', description: '양수의 절댓값', expectedResult: 3.14 }
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
        { title: '기본 사용법', code: 'sqrt(16)', description: '16의 제곱근', expectedResult: 4 },
        { title: '소수점', code: 'sqrt(2)', description: '2의 제곱근', expectedResult: 1.41421 }
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
        { title: '기본 사용법', code: 'pow(2, 3)', description: '2의 3제곱', expectedResult: 8 },
        { title: '제곱근', code: 'pow(9, 0.5)', description: '9의 제곱근', expectedResult: 3 }
      ],
      isBuiltIn: true
    },
    {
      id: 'round',
      name: 'round',
      displayName: '반올림',
      category: 'math',
      description: '숫자를 가장 가까운 정수로 반올림합니다.',
      syntax: 'round(x)',
      parameters: [
        { name: 'x', type: 'number', required: true, description: '입력 숫자' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'round(3.7)', description: '반올림', expectedResult: 4 },
        { title: '내림', code: 'round(3.2)', description: '내림', expectedResult: 3 }
      ],
      isBuiltIn: true
    },
    {
      id: 'floor',
      name: 'floor',
      displayName: '내림',
      category: 'math',
      description: '숫자를 가장 가까운 작은 정수로 내림합니다.',
      syntax: 'floor(x)',
      parameters: [
        { name: 'x', type: 'number', required: true, description: '입력 숫자' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'floor(3.9)', description: '내림', expectedResult: 3 }
      ],
      isBuiltIn: true
    },
    {
      id: 'ceil',
      name: 'ceil',
      displayName: '올림',
      category: 'math',
      description: '숫자를 가장 가까운 큰 정수로 올림합니다.',
      syntax: 'ceil(x)',
      parameters: [
        { name: 'x', type: 'number', required: true, description: '입력 숫자' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'ceil(3.1)', description: '올림', expectedResult: 4 }
      ],
      isBuiltIn: true
    },
    {
      id: 'max',
      name: 'max',
      displayName: '최댓값',
      category: 'math',
      description: '여러 숫자 중 가장 큰 값을 반환합니다.',
      syntax: 'max(...numbers)',
      parameters: [
        { name: 'numbers', type: 'number', required: true, description: '비교할 숫자들' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'max(10, 20, 5)', description: '최댓값 찾기', expectedResult: 20 }
      ],
      isBuiltIn: true
    },
    {
      id: 'min',
      name: 'min',
      displayName: '최솟값',
      category: 'math',
      description: '여러 숫자 중 가장 작은 값을 반환합니다.',
      syntax: 'min(...numbers)',
      parameters: [
        { name: 'numbers', type: 'number', required: true, description: '비교할 숫자들' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'min(10, 20, 5)', description: '최솟값 찾기', expectedResult: 5 }
      ],
      isBuiltIn: true
    }
  ],

  // 논리 함수 (수정됨 - JavaScript 문법 사용)
  logic: [
    {
      id: 'and',
      name: 'and',
      displayName: '논리곱',
      category: 'logic',
      description: '모든 조건이 참일 때 참을 반환합니다.',
      syntax: 'condition1 && condition2',
      parameters: [
        { name: 'condition1', type: 'boolean', required: true, description: '첫 번째 조건' },
        { name: 'condition2', type: 'boolean', required: true, description: '두 번째 조건' }
      ],
      returnType: 'boolean',
      examples: [
        { title: '기본 사용법', code: 'temp > 20 && humidity < 80', description: '온도와 습도 조건', expectedResult: true, variables: { temp: 25, humidity: 70 } }
      ],
      isBuiltIn: true
    },
    {
      id: 'or',
      name: 'or',
      displayName: '논리합',
      category: 'logic',
      description: '하나 이상의 조건이 참일 때 참을 반환합니다.',
      syntax: 'condition1 || condition2',
      parameters: [
        { name: 'condition1', type: 'boolean', required: true, description: '첫 번째 조건' },
        { name: 'condition2', type: 'boolean', required: true, description: '두 번째 조건' }
      ],
      returnType: 'boolean',
      examples: [
        { title: '기본 사용법', code: 'temp > 30 || humidity > 90', description: '고온 또는 고습', expectedResult: false, variables: { temp: 25, humidity: 70 } }
      ],
      isBuiltIn: true
    },
    {
      id: 'not',
      name: 'not',
      displayName: '논리부정',
      category: 'logic',
      description: '조건의 반대 값을 반환합니다.',
      syntax: '!condition',
      parameters: [
        { name: 'condition', type: 'boolean', required: true, description: '조건' }
      ],
      returnType: 'boolean',
      examples: [
        { title: '기본 사용법', code: '!isActive', description: '활성 상태의 반대', expectedResult: true, variables: { isActive: false } }
      ],
      isBuiltIn: true
    }
  ],

  // 조건 함수 (수정됨)
  conditional: [
    {
      id: 'ternary',
      name: 'ternary',
      displayName: '삼항연산자',
      category: 'conditional',
      description: '조건에 따라 다른 값을 반환합니다.',
      syntax: 'condition ? trueValue : falseValue',
      parameters: [
        { name: 'condition', type: 'boolean', required: true, description: '조건식' },
        { name: 'trueValue', type: 'any', required: true, description: '조건이 참일 때 반환값' },
        { name: 'falseValue', type: 'any', required: true, description: '조건이 거짓일 때 반환값' }
      ],
      returnType: 'any',
      examples: [
        { title: '온도 상태', code: 'temp > 25 ? "높음" : "정상"', description: '온도에 따른 상태', expectedResult: '높음', variables: { temp: 30 } },
        { title: '숫자 비교', code: 'value > 100 ? value : 100', description: '최소값 보장', expectedResult: 100, variables: { value: 50 } }
      ],
      isBuiltIn: true
    },
    {
      id: 'switch_case',
      name: 'switch_case',
      displayName: '다중조건',
      category: 'conditional',
      description: '여러 조건을 순차적으로 검사합니다.',
      syntax: 'condition1 ? value1 : condition2 ? value2 : defaultValue',
      parameters: [
        { name: 'conditions', type: 'boolean', required: true, description: '조건들' },
        { name: 'values', type: 'any', required: true, description: '반환값들' }
      ],
      returnType: 'any',
      examples: [
        { title: '등급 분류', code: 'score >= 90 ? "A" : score >= 80 ? "B" : score >= 70 ? "C" : "D"', description: '점수별 등급', expectedResult: 'B', variables: { score: 85 } }
      ],
      isBuiltIn: true
    }
  ],

  // 시간 함수
  time: [
    {
      id: 'now',
      name: 'now',
      displayName: '현재 시간',
      category: 'time',
      description: '현재 시간을 타임스탬프로 반환합니다.',
      syntax: 'Date.now()',
      parameters: [],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'Date.now()', description: '현재 타임스탬프', expectedResult: 1640995200000 }
      ],
      isBuiltIn: true
    },
    {
      id: 'timestamp',
      name: 'timestamp',
      displayName: '타임스탬프 생성',
      category: 'time',
      description: '지정된 날짜의 타임스탬프를 반환합니다.',
      syntax: 'new Date(year, month, day).getTime()',
      parameters: [
        { name: 'year', type: 'number', required: true, description: '연도' },
        { name: 'month', type: 'number', required: true, description: '월 (0-11)' },
        { name: 'day', type: 'number', required: true, description: '일' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'new Date(2024, 0, 1).getTime()', description: '2024년 1월 1일', expectedResult: 1704067200000 }
      ],
      isBuiltIn: true
    }
  ],

  // 문자열 함수
  string: [
    {
      id: 'concat',
      name: 'concat',
      displayName: '문자열 연결',
      category: 'string',
      description: '여러 문자열을 연결합니다.',
      syntax: 'str1 + str2',
      parameters: [
        { name: 'str1', type: 'string', required: true, description: '첫 번째 문자열' },
        { name: 'str2', type: 'string', required: true, description: '두 번째 문자열' }
      ],
      returnType: 'string',
      examples: [
        { title: '기본 사용법', code: '"Hello" + " " + "World"', description: '문자열 연결', expectedResult: 'Hello World' }
      ],
      isBuiltIn: true
    },
    {
      id: 'length',
      name: 'length',
      displayName: '문자열 길이',
      category: 'string',
      description: '문자열의 길이를 반환합니다.',
      syntax: 'str.length',
      parameters: [
        { name: 'str', type: 'string', required: true, description: '입력 문자열' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: '"Hello".length', description: '문자열 길이', expectedResult: 5 }
      ],
      isBuiltIn: true
    }
  ],

  // 변환 함수
  conversion: [
    {
      id: 'celsius2fahrenheit',
      name: 'celsius2fahrenheit',
      displayName: '섭씨→화씨',
      category: 'conversion',
      description: '섭씨 온도를 화씨 온도로 변환합니다.',
      syntax: '(celsius * 9/5) + 32',
      parameters: [
        { name: 'celsius', type: 'number', required: true, description: '섭씨 온도' }
      ],
      returnType: 'number',
      examples: [
        { title: '물의 어는점', code: '(0 * 9/5) + 32', description: '0°C를 화씨로', expectedResult: 32 },
        { title: '물의 끓는점', code: '(100 * 9/5) + 32', description: '100°C를 화씨로', expectedResult: 212 }
      ],
      isBuiltIn: true
    },
    {
      id: 'fahrenheit2celsius',
      name: 'fahrenheit2celsius',
      displayName: '화씨→섭씨',
      category: 'conversion',
      description: '화씨 온도를 섭씨 온도로 변환합니다.',
      syntax: '(fahrenheit - 32) * 5/9',
      parameters: [
        { name: 'fahrenheit', type: 'number', required: true, description: '화씨 온도' }
      ],
      returnType: 'number',
      examples: [
        { title: '물의 어는점', code: '(32 - 32) * 5/9', description: '32°F를 섭씨로', expectedResult: 0 }
      ],
      isBuiltIn: true
    },
    {
      id: 'toNumber',
      name: 'toNumber',
      displayName: '숫자 변환',
      category: 'conversion',
      description: '문자열을 숫자로 변환합니다.',
      syntax: 'Number(str)',
      parameters: [
        { name: 'str', type: 'string', required: true, description: '변환할 문자열' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'Number("123")', description: '문자열을 숫자로', expectedResult: 123 }
      ],
      isBuiltIn: true
    },
    {
      id: 'toString',
      name: 'toString',
      displayName: '문자열 변환',
      category: 'conversion',
      description: '숫자를 문자열로 변환합니다.',
      syntax: 'String(num)',
      parameters: [
        { name: 'num', type: 'number', required: true, description: '변환할 숫자' }
      ],
      returnType: 'string',
      examples: [
        { title: '기본 사용법', code: 'String(123)', description: '숫자를 문자열로', expectedResult: '123' }
      ],
      isBuiltIn: true
    }
  ],

  // 통계 함수
  statistical: [
    {
      id: 'average',
      name: 'average',
      displayName: '평균값',
      category: 'statistical',
      description: '여러 숫자의 평균을 계산합니다.',
      syntax: '(num1 + num2 + num3) / 3',
      parameters: [
        { name: 'numbers', type: 'number', required: true, description: '평균을 구할 숫자들' }
      ],
      returnType: 'number',
      examples: [
        { title: '3개 값 평균', code: '(temp1 + temp2 + temp3) / 3', description: '온도 평균', expectedResult: 25, variables: { temp1: 20, temp2: 25, temp3: 30 } }
      ],
      isBuiltIn: true
    }
  ],

  // 집계 함수
  aggregate: [
    {
      id: 'sum',
      name: 'sum',
      displayName: '합계',
      category: 'aggregate',
      description: '여러 숫자의 합계를 계산합니다.',
      syntax: 'num1 + num2 + num3',
      parameters: [
        { name: 'numbers', type: 'number', required: true, description: '합계를 구할 숫자들' }
      ],
      returnType: 'number',
      examples: [
        { title: '기본 사용법', code: 'input1 + input2 + input3', description: '3개 값의 합', expectedResult: 60, variables: { input1: 10, input2: 20, input3: 30 } }
      ],
      isBuiltIn: true
    }
  ],

  // 사용자 정의 함수 (빈 배열)
  custom: []
};

// ============================================================================
// 스크립트 상수
// ============================================================================

export const SCRIPT_CONSTANTS = {
  // 수학 상수
  PI: Math.PI,
  E: Math.E,
  
  // 실행 제한
  MAX_EXECUTION_TIME: 10000, // ms
  MAX_MEMORY_USAGE: 1024 * 1024, // 1MB
  MAX_SCRIPT_LENGTH: 10000, // characters
  
  // 공통 변수명
  COMMON_VARIABLES: [
    'temp', 'temperature', 'humidity', 'pressure', 'voltage', 'current',
    'value', 'input', 'output', 'status', 'state', 'level', 'flow',
    'speed', 'count', 'time', 'timestamp', 'duration'
  ]
} as const;

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * 카테고리별 함수 개수 반환
 */
export function getFunctionCountByCategory(): Record<ScriptFunctionCategory, number> {
  const counts = {} as Record<ScriptFunctionCategory, number>;
  
  Object.entries(BUILTIN_FUNCTIONS).forEach(([category, functions]) => {
    counts[category as ScriptFunctionCategory] = functions.length;
  });
  
  return counts;
}

/**
 * 모든 함수를 평면 배열로 반환
 */
export function getAllFunctions(): ScriptFunction[] {
  return Object.values(BUILTIN_FUNCTIONS).flat();
}

/**
 * 함수 ID로 함수 찾기
 */
export function getFunctionById(id: string): ScriptFunction | undefined {
  return getAllFunctions().find(func => func.id === id);
}

/**
 * 카테고리별 함수 반환
 */
export function getFunctionsByCategory(category: ScriptFunctionCategory): ScriptFunction[] {
  return BUILTIN_FUNCTIONS[category] || [];
}

/**
 * 검색어로 함수 필터링
 */
export function searchFunctions(
  query: string, 
  category?: ScriptFunctionCategory
): ScriptFunction[] {
  const functions = category ? getFunctionsByCategory(category) : getAllFunctions();
  const searchTerm = query.toLowerCase();
  
  return functions.filter(func => 
    func.name.toLowerCase().includes(searchTerm) ||
    func.displayName.toLowerCase().includes(searchTerm) ||
    func.description.toLowerCase().includes(searchTerm) ||
    func.syntax.toLowerCase().includes(searchTerm)
  );
}