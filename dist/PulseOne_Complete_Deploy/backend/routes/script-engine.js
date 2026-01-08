// ============================================================================
// backend/routes/script-engine.js
// 스크립트 엔진 API - ConfigManager/DatabaseFactory 패턴 준수
// ============================================================================

const express = require('express');
const router = express.Router();

// ConfigManager 기반 DB 연결 (기존 패턴 준수)
const ConfigManager = require('../lib/config/ConfigManager');
const DatabaseFactory = require('../lib/database/DatabaseFactory');
// const ScriptEngineQueries = require('../lib/database/queries/ScriptEngineQueries'); // 필요시 추가

// DB 팩토리 관리 (기존 패턴과 동일)
let dbFactory = null;

async function getDatabaseFactory() {
    if (!dbFactory) {
        const config = ConfigManager.getInstance();
        dbFactory = new DatabaseFactory();
    }
    return dbFactory;
}

// DatabaseFactory.executeQuery 사용 헬퍼 함수들 (기존 alarms.js 패턴)
async function dbAll(sql, params = []) {
    const factory = await getDatabaseFactory();
    try {
        const results = await factory.executeQuery(sql, params);
        
        console.log('🔍 DatabaseFactory 원시 결과:', {
            타입: typeof results,
            생성자: results?.constructor?.name,
            키들: results ? Object.keys(results) : '없음'
        });
        
        // 다양한 가능한 결과 구조 처리
        if (Array.isArray(results)) {
            return results;
        } else if (results && Array.isArray(results.rows)) {
            return results.rows;
        } else if (results && results.recordset) {
            return results.recordset;
        } else if (results && Array.isArray(results.results)) {
            return results.results;
        } else {
            console.warn('예상하지 못한 결과 구조:', results);
            return [];
        }
    } catch (error) {
        console.error('Database query error:', error);
        throw error;
    }
}

async function dbGet(sql, params = []) {
    const results = await dbAll(sql, params);
    return results.length > 0 ? results[0] : null;
}

// 공통 응답 함수 (기존 패턴 완전 준수)
const createResponse = (success, data, message, error_code = null) => ({
    success,
    data,
    message,
    error_code,
    timestamp: new Date().toISOString()
});

// ============================================================================
// 내장 함수 라이브러리 데이터 (DB 저장 없이 메모리에서 관리)
// ============================================================================

const BUILTIN_FUNCTIONS = [
    // 수학 함수
    { name: 'abs', displayName: 'abs', category: 'math', description: '절댓값을 계산합니다', signature: 'abs(x)', examples: ['abs(-5) // 5'] },
    { name: 'round', displayName: 'round', category: 'math', description: '반올림합니다', signature: 'round(x, digits?)', examples: ['round(3.14159, 2) // 3.14'] },
    { name: 'floor', displayName: 'floor', category: 'math', description: '내림합니다', signature: 'floor(x)', examples: ['floor(3.7) // 3'] },
    { name: 'ceil', displayName: 'ceil', category: 'math', description: '올림합니다', signature: 'ceil(x)', examples: ['ceil(3.2) // 4'] },
    { name: 'min', displayName: 'min', category: 'math', description: '최솟값을 찾습니다', signature: 'min(a, b, ...)', examples: ['min(1, 2, 3) // 1'] },
    { name: 'max', displayName: 'max', category: 'math', description: '최댓값을 찾습니다', signature: 'max(a, b, ...)', examples: ['max(1, 2, 3) // 3'] },
    { name: 'sqrt', displayName: 'sqrt', category: 'math', description: '제곱근을 계산합니다', signature: 'sqrt(x)', examples: ['sqrt(16) // 4'] },
    { name: 'pow', displayName: 'pow', category: 'math', description: '거듭제곱을 계산합니다', signature: 'pow(base, exponent)', examples: ['pow(2, 3) // 8'] },

    // 통계 함수
    { name: 'avg', displayName: 'avg', category: 'statistics', description: '평균을 계산합니다', signature: 'avg(...values)', examples: ['avg(1, 2, 3, 4) // 2.5'] },
    { name: 'sum', displayName: 'sum', category: 'statistics', description: '합계를 계산합니다', signature: 'sum(...values)', examples: ['sum(1, 2, 3) // 6'] },
    { name: 'count', displayName: 'count', category: 'statistics', description: '개수를 셉니다', signature: 'count(...values)', examples: ['count(1, 2, null, 4) // 3'] },

    // 문자열 함수
    { name: 'concat', displayName: 'concat', category: 'string', description: '문자열을 결합합니다', signature: 'concat(...strings)', examples: ['concat("Hello", " ", "World") // "Hello World"'] },
    { name: 'substring', displayName: 'substring', category: 'string', description: '부분 문자열을 추출합니다', signature: 'substring(str, start, end?)', examples: ['substring("Hello", 1, 4) // "ell"'] },
    { name: 'length', displayName: 'length', category: 'string', description: '문자열 길이를 반환합니다', signature: 'length(str)', examples: ['length("Hello") // 5'] },

    // 조건 함수
    { name: 'if', displayName: 'if', category: 'conditional', description: '조건에 따라 값을 반환합니다', signature: 'if(condition, trueValue, falseValue)', examples: ['if(temp > 30, "뜨거움", "적당함")'] },
    { name: 'isNull', displayName: 'isNull', category: 'conditional', description: '값이 null인지 확인합니다', signature: 'isNull(value)', examples: ['isNull(temp) // true/false'] },
    { name: 'isNumber', displayName: 'isNumber', category: 'conditional', description: '값이 숫자인지 확인합니다', signature: 'isNumber(value)', examples: ['isNumber(123) // true'] },

    // 시간 함수
    { name: 'now', displayName: 'now', category: 'datetime', description: '현재 시간을 반환합니다', signature: 'now()', examples: ['now() // 현재 타임스탬프'] },
    { name: 'formatDate', displayName: 'formatDate', category: 'datetime', description: '날짜를 포맷합니다', signature: 'formatDate(date, format)', examples: ['formatDate(now(), "YYYY-MM-DD")'] },

    // 변환 함수
    { name: 'toNumber', displayName: 'toNumber', category: 'conversion', description: '숫자로 변환합니다', signature: 'toNumber(value)', examples: ['toNumber("123") // 123'] },
    { name: 'toString', displayName: 'toString', category: 'conversion', description: '문자열로 변환합니다', signature: 'toString(value)', examples: ['toString(123) // "123"'] },
    { name: 'toBoolean', displayName: 'toBoolean', category: 'conversion', description: '불리언으로 변환합니다', signature: 'toBoolean(value)', examples: ['toBoolean(1) // true'] }
];

const SCRIPT_TEMPLATES = [
    {
        id: 'simple_average',
        name: '간단한 평균',
        category: 'math',
        description: '여러 센서의 평균값을 계산합니다',
        script: '(temp1 + temp2 + temp3) / 3',
        variables: ['temp1', 'temp2', 'temp3'],
        tags: ['평균', '온도', '기본']
    },
    {
        id: 'conditional_status',
        name: '조건부 상태',
        category: 'conditional',
        description: '임계값에 따른 상태를 반환합니다',
        script: 'if(pressure > 5.0, "위험", if(pressure > 2.0, "주의", "정상"))',
        variables: ['pressure'],
        tags: ['조건문', '상태', '압력']
    },
    {
        id: 'energy_efficiency',
        name: '에너지 효율',
        category: 'statistics',
        description: '에너지 효율을 계산합니다',
        script: '(output_power / input_power) * 100',
        variables: ['output_power', 'input_power'],
        tags: ['효율', '에너지', '비율']
    }
];

// ============================================================================
// 스크립트 엔진 유틸리티 클래스
// ============================================================================

class ScriptEngineUtils {
    /**
     * 스크립트 검증
     */
    static validateScript(script, context = {}, functions = []) {
        const errors = [];
        const warnings = [];

        try {
            // 기본 문법 체크
            if (!script || typeof script !== 'string') {
                errors.push({
                    line: 1,
                    column: 1,
                    message: '스크립트가 비어있습니다',
                    type: 'syntax',
                    severity: 'error'
                });
                return { isValid: false, errors, warnings, suggestions: [] };
            }

            // 괄호 매칭 체크
            const openParens = (script.match(/\(/g) || []).length;
            const closeParens = (script.match(/\)/g) || []).length;
            if (openParens !== closeParens) {
                errors.push({
                    line: 1,
                    column: script.length,
                    message: '괄호가 매칭되지 않습니다',
                    type: 'syntax',
                    severity: 'error'
                });
            }

            // 변수 사용 체크
            const usedVariables = this.extractVariables(script);
            const contextKeys = Object.keys(context);
            const undefinedVars = usedVariables.filter(v => !contextKeys.includes(v));
            
            undefinedVars.forEach(varName => {
                warnings.push({
                    line: 1,
                    column: script.indexOf(varName) + 1,
                    message: `변수 '${varName}'가 정의되지 않았습니다`,
                    type: 'variable',
                    severity: 'warning'
                });
            });

            // 함수 사용 체크
            const usedFunctions = this.extractFunctions(script);
            const availableFunctions = BUILTIN_FUNCTIONS.map(f => f.name);
            const undefinedFuncs = usedFunctions.filter(f => !availableFunctions.includes(f));
            
            undefinedFuncs.forEach(funcName => {
                errors.push({
                    line: 1,
                    column: script.indexOf(funcName) + 1,
                    message: `함수 '${funcName}'를 찾을 수 없습니다`,
                    type: 'function',
                    severity: 'error'
                });
            });

            return {
                isValid: errors.length === 0,
                errors,
                warnings,
                suggestions: this.generateSuggestions(script, usedVariables, usedFunctions)
            };

        } catch (error) {
            return {
                isValid: false,
                errors: [{
                    line: 1,
                    column: 1,
                    message: `검증 중 오류 발생: ${error.message}`,
                    type: 'runtime',
                    severity: 'error'
                }],
                warnings: [],
                suggestions: []
            };
        }
    }

    /**
     * 스크립트 테스트 실행
     */
    static async testScript(script, context = {}, timeout = 5000) {
        const startTime = Date.now();
        const logs = [];

        try {
            // 검증 먼저 수행
            const validation = this.validateScript(script, context);
            if (!validation.isValid) {
                return {
                    success: false,
                    result: null,
                    executionTime: Date.now() - startTime,
                    error: validation.errors[0],
                    logs
                };
            }

            // 안전한 스크립트 실행 환경 구성
            const safeContext = this.createSafeContext(context);
            const result = this.executeScript(script, safeContext);

            return {
                success: true,
                result,
                executionTime: Date.now() - startTime,
                error: null,
                logs
            };

        } catch (error) {
            return {
                success: false,
                result: null,
                executionTime: Date.now() - startTime,
                error: {
                    line: 1,
                    column: 1,
                    message: error.message,
                    type: 'runtime',
                    severity: 'error'
                },
                logs
            };
        }
    }

    /**
     * 스크립트에서 변수 추출
     */
    static extractVariables(script) {
        const variables = new Set();
        const regex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\b/g;
        let match;

        while ((match = regex.exec(script)) !== null) {
            const name = match[1];
            // 함수명과 예약어 제외
            if (!BUILTIN_FUNCTIONS.some(f => f.name === name) && 
                !['if', 'true', 'false', 'null', 'undefined'].includes(name)) {
                variables.add(name);
            }
        }

        return Array.from(variables);
    }

    /**
     * 스크립트에서 함수 추출
     */
    static extractFunctions(script) {
        const functions = new Set();
        const regex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\(/g;
        let match;

        while ((match = regex.exec(script)) !== null) {
            functions.add(match[1]);
        }

        return Array.from(functions);
    }

    /**
     * 제안사항 생성
     */
    static generateSuggestions(script, usedVariables, usedFunctions) {
        const suggestions = [];

        // 변수 제안
        if (usedVariables.length > 0) {
            suggestions.push({
                type: 'info',
                message: `사용된 변수: ${usedVariables.join(', ')}`,
                action: 'highlight_variables'
            });
        }

        // 함수 제안
        if (usedFunctions.length > 0) {
            suggestions.push({
                type: 'info',
                message: `사용된 함수: ${usedFunctions.join(', ')}`,
                action: 'highlight_functions'
            });
        }

        return suggestions;
    }

    /**
     * 안전한 실행 컨텍스트 생성
     */
    static createSafeContext(context) {
        const safeContext = { ...context };
        
        // 기본 수학 함수들 추가
        safeContext.abs = Math.abs;
        safeContext.round = (x, digits = 0) => Number(x.toFixed(digits));
        safeContext.floor = Math.floor;
        safeContext.ceil = Math.ceil;
        safeContext.min = Math.min;
        safeContext.max = Math.max;
        safeContext.sqrt = Math.sqrt;
        safeContext.pow = Math.pow;

        // 통계 함수들
        safeContext.avg = (...values) => {
            const nums = values.filter(v => typeof v === 'number');
            return nums.length > 0 ? nums.reduce((a, b) => a + b, 0) / nums.length : 0;
        };
        safeContext.sum = (...values) => values.filter(v => typeof v === 'number').reduce((a, b) => a + b, 0);
        safeContext.count = (...values) => values.filter(v => v != null).length;

        // 조건 함수들
        safeContext.if = (condition, trueValue, falseValue) => condition ? trueValue : falseValue;
        safeContext.isNull = (value) => value == null;
        safeContext.isNumber = (value) => typeof value === 'number' && !isNaN(value);

        // 시간 함수들
        safeContext.now = () => Date.now();

        return safeContext;
    }

    /**
     * 스크립트 실행 (간단한 수식 계산)
     */
    static executeScript(script, context) {
        try {
            // 변수 치환
            let processedScript = script;
            Object.entries(context).forEach(([key, value]) => {
                const regex = new RegExp(`\\b${key}\\b`, 'g');
                processedScript = processedScript.replace(regex, JSON.stringify(value));
            });

            // 안전한 eval 대신 간단한 계산만 수행
            if (processedScript.match(/^[\d\s+\-*/().]+$/)) {
                return Function('"use strict"; return (' + processedScript + ')')();
            } else {
                // 복잡한 스크립트는 기본값 반환
                return null;
            }
        } catch (error) {
            throw new Error(`실행 오류: ${error.message}`);
        }
    }
}

// ============================================================================
// API 엔드포인트 (기존 패턴 준수)
// ============================================================================

/**
 * GET /api/script-engine/functions
 * 사용 가능한 함수 목록 조회
 */
router.get('/functions', async (req, res) => {
    try {
        const { category, search } = req.query;
        let functions = [...BUILTIN_FUNCTIONS];

        // 카테고리 필터링
        if (category) {
            functions = functions.filter(f => f.category === category);
        }

        // 검색 필터링
        if (search) {
            const searchTerm = search.toLowerCase();
            functions = functions.filter(f =>
                f.name.toLowerCase().includes(searchTerm) ||
                f.displayName.toLowerCase().includes(searchTerm) ||
                f.description.toLowerCase().includes(searchTerm)
            );
        }

        console.log(`스크립트 함수 ${functions.length}개 반환`);
        res.json(createResponse(true, functions, 'Functions retrieved successfully'));
    } catch (error) {
        console.error('함수 목록 조회 실패:', error);
        res.status(500).json(createResponse(false, null, error.message, 'FUNCTIONS_ERROR'));
    }
});

/**
 * POST /api/script-engine/validate
 * 스크립트 검증
 */
router.post('/validate', async (req, res) => {
    try {
        const { script, context = {}, functions = [], strictMode = false } = req.body;

        if (!script) {
            return res.status(400).json(createResponse(false, null, 'Script is required', 'VALIDATION_ERROR'));
        }

        const result = ScriptEngineUtils.validateScript(script, context, functions);

        console.log(`스크립트 검증 완료: ${result.isValid ? '성공' : '실패'}`);
        res.json(createResponse(true, result, 'Script validation completed'));
    } catch (error) {
        console.error('스크립트 검증 실패:', error);
        res.status(500).json(createResponse(false, null, error.message, 'VALIDATION_ERROR'));
    }
});

/**
 * POST /api/script-engine/test
 * 스크립트 테스트 실행
 */
router.post('/test', async (req, res) => {
    try {
        const { script, context = {}, timeout = 5000, includeDebugInfo = false } = req.body;

        if (!script) {
            return res.status(400).json(createResponse(false, null, 'Script is required', 'TEST_ERROR'));
        }

        const result = await ScriptEngineUtils.testScript(script, context, timeout);

        console.log(`스크립트 테스트 완료: ${result.success ? '성공' : '실패'}`);
        res.json(createResponse(true, result, 'Script test completed'));
    } catch (error) {
        console.error('스크립트 테스트 실패:', error);
        res.status(500).json(createResponse(false, null, error.message, 'TEST_ERROR'));
    }
});

/**
 * POST /api/script-engine/parse
 * 스크립트 파싱 (변수, 함수 추출)
 */
router.post('/parse', async (req, res) => {
    try {
        const { script } = req.body;

        if (!script) {
            return res.status(400).json(createResponse(false, null, 'Script is required', 'PARSE_ERROR'));
        }

        const variables = ScriptEngineUtils.extractVariables(script);
        const functions = ScriptEngineUtils.extractFunctions(script);
        const constants = script.match(/\b\d+\.?\d*\b/g) || [];

        // 복잡도 계산 (간단한 기준)
        let complexity = 'low';
        if (script.length > 100 || functions.length > 3 || variables.length > 5) {
            complexity = 'medium';
        }
        if (script.length > 300 || functions.length > 8 || variables.length > 10) {
            complexity = 'high';
        }

        const result = {
            variables,
            functions,
            constants,
            complexity
        };

        console.log(`스크립트 파싱 완료: ${variables.length}개 변수, ${functions.length}개 함수`);
        res.json(createResponse(true, result, 'Script parsing completed'));
    } catch (error) {
        console.error('스크립트 파싱 실패:', error);
        res.status(500).json(createResponse(false, null, error.message, 'PARSE_ERROR'));
    }
});

/**
 * GET /api/script-engine/templates
 * 스크립트 템플릿 목록 조회
 */
router.get('/templates', async (req, res) => {
    try {
        const { category } = req.query;
        let templates = [...SCRIPT_TEMPLATES];

        if (category) {
            templates = templates.filter(t => t.category === category);
        }

        console.log(`스크립트 템플릿 ${templates.length}개 반환`);
        res.json(createResponse(true, templates, 'Templates retrieved successfully'));
    } catch (error) {
        console.error('템플릿 조회 실패:', error);
        res.status(500).json(createResponse(false, null, error.message, 'TEMPLATES_ERROR'));
    }
});

/**
 * GET /api/script-engine/test
 * 스크립트 엔진 API 테스트 엔드포인트
 */
router.get('/test', async (req, res) => {
    try {
        // DatabaseFactory 연결 테스트
        const factory = await getDatabaseFactory();
        const testResult = await factory.executeQuery('SELECT 1 as test', []);
        
        res.json(createResponse(true, { 
            message: 'Script Engine API is working!',
            database_test: testResult,
            architecture: [
                'ConfigManager-based database configuration',
                'DatabaseFactory.executeQuery for unified database access', 
                'Built-in function library with 23 functions',
                'Script validation and execution engine',
                'Template system for common calculations'
            ],
            available_endpoints: [
                'GET /api/script-engine/functions',
                'POST /api/script-engine/validate',
                'POST /api/script-engine/test', 
                'POST /api/script-engine/parse',
                'GET /api/script-engine/templates',
                'GET /api/script-engine/test'
            ],
            builtin_functions_count: BUILTIN_FUNCTIONS.length,
            template_count: SCRIPT_TEMPLATES.length
        }, 'Script Engine API test successful'));

    } catch (error) {
        console.error('테스트 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TEST_ERROR'));
    }
});

module.exports = router;