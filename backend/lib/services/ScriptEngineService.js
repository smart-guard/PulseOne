// =============================================================================
// backend/lib/services/ScriptEngineService.js
// =============================================================================

const BaseService = require('./BaseService');
const DatabaseFactory = require('../database/DatabaseFactory');

/**
 * Service class for Script Engine logic.
 */
class ScriptEngineService extends BaseService {
    constructor() {
        super(null);
        this.dbFactory = new DatabaseFactory();

        // Internal data moved from route
        this.BUILTIN_FUNCTIONS = [
            { name: 'abs', displayName: 'abs', category: 'math', description: '절댓값을 계산합니다', signature: 'abs(x)', examples: ['abs(-5) // 5'] },
            { name: 'round', displayName: 'round', category: 'math', description: '반올림합니다', signature: 'round(x, digits?)', examples: ['round(3.14159, 2) // 3.14'] },
            { name: 'floor', displayName: 'floor', category: 'math', description: '내림합니다', signature: 'floor(x)', examples: ['floor(3.7) // 3'] },
            { name: 'ceil', displayName: 'ceil', category: 'math', description: '올림합니다', signature: 'ceil(x)', examples: ['ceil(3.2) // 4'] },
            { name: 'min', displayName: 'min', category: 'math', description: '최솟값을 찾습니다', signature: 'min(a, b, ...)', examples: ['min(1, 2, 3) // 1'] },
            { name: 'max', displayName: 'max', category: 'math', description: '최댓값을 찾습니다', signature: 'max(a, b, ...)', examples: ['max(1, 2, 3) // 3'] },
            { name: 'sqrt', displayName: 'sqrt', category: 'math', description: '제곱근을 계산합니다', signature: 'sqrt(x)', examples: ['sqrt(16) // 4'] },
            { name: 'pow', displayName: 'pow', category: 'math', description: '거듭제곱을 계산합니다', signature: 'pow(base, exponent)', examples: ['pow(2, 3) // 8'] },
            { name: 'avg', displayName: 'avg', category: 'statistics', description: '평균을 계산합니다', signature: 'avg(...values)', examples: ['avg(1, 2, 3, 4) // 2.5'] },
            { name: 'sum', displayName: 'sum', category: 'statistics', description: '합계를 계산합니다', signature: 'sum(...values)', examples: ['sum(1, 2, 3) // 6'] },
            { name: 'count', displayName: 'count', category: 'statistics', description: '개수를 셉니다', signature: 'count(...values)', examples: ['count(1, 2, null, 4) // 3'] },
            { name: 'concat', displayName: 'concat', category: 'string', description: '문자열을 결합합니다', signature: 'concat(...strings)', examples: ['concat("Hello", " ", "World") // "Hello World"'] },
            { name: 'substring', displayName: 'substring', category: 'string', description: '부분 문자열을 추출합니다', signature: 'substring(str, start, end?)', examples: ['substring("Hello", 1, 4) // "ell"'] },
            { name: 'length', displayName: 'length', category: 'string', description: '문자열 길이를 반환합니다', signature: 'length(str)', examples: ['length("Hello") // 5'] },
            { name: 'if', displayName: 'if', category: 'conditional', description: '조건에 따라 값을 반환합니다', signature: 'if(condition, trueValue, falseValue)', examples: ['if(temp > 30, "뜨거움", "적당함")'] },
            { name: 'isNull', displayName: 'isNull', category: 'conditional', description: '값이 null인지 확인합니다', signature: 'isNull(value)', examples: ['isNull(temp) // true/false'] },
            { name: 'isNumber', displayName: 'isNumber', category: 'conditional', description: '값이 숫자인지 확인합니다', signature: 'isNumber(value)', examples: ['isNumber(123) // true'] },
            { name: 'now', displayName: 'now', category: 'datetime', description: '현재 시간을 반환합니다', signature: 'now()', examples: ['now() // 현재 타임스탬프'] },
            { name: 'formatDate', displayName: 'formatDate', category: 'datetime', description: '날짜를 포맷합니다', signature: 'formatDate(date, format)', examples: ['formatDate(now(), "YYYY-MM-DD")'] },
            { name: 'toNumber', displayName: 'toNumber', category: 'conversion', description: '숫자로 변환합니다', signature: 'toNumber(value)', examples: ['toNumber("123") // 123'] },
            { name: 'toString', displayName: 'toString', category: 'conversion', description: '문자열로 변환합니다', signature: 'toString(value)', examples: ['toString(123) // "123"'] },
            { name: 'toBoolean', displayName: 'toBoolean', category: 'conversion', description: '불리언으로 변환합니다', signature: 'toBoolean(value)', examples: ['toBoolean(1) // true'] }
        ];

        this.SCRIPT_TEMPLATES = [
            { id: 'simple_average', name: '간단한 평균', category: 'math', description: '여러 센서의 평균값을 계산합니다', script: '(temp1 + temp2 + temp3) / 3', variables: ['temp1', 'temp2', 'temp3'], tags: ['평균', '온도', '기본'] },
            { id: 'conditional_status', name: '조건부 상태', category: 'conditional', description: '임계값에 따른 상태를 반환합니다', script: 'if(pressure > 5.0, "위험", if(pressure > 2.0, "주의", "정상"))', variables: ['pressure'], tags: ['조건문', '상태', '압력'] },
            { id: 'energy_efficiency', name: '에너지 효율', category: 'statistics', description: '에너지 효율을 계산합니다', script: '(output_power / input_power) * 100', variables: ['output_power', 'input_power'], tags: ['효율', '에너지', '비율'] }
        ];
    }

    /**
     * Get available functions.
     */
    async getFunctions(category, search) {
        return await this.handleRequest(async () => {
            let functions = [...this.BUILTIN_FUNCTIONS];
            if (category) functions = functions.filter(f => f.category === category);
            if (search) {
                const term = search.toLowerCase();
                functions = functions.filter(f =>
                    f.name.toLowerCase().includes(term) ||
                    f.description.toLowerCase().includes(term)
                );
            }
            return functions;
        }, 'ScriptEngineService.getFunctions');
    }

    /**
     * Validate a script.
     */
    async validateScript(script, context = {}) {
        return await this.handleRequest(async () => {
            const errors = [];
            const warnings = [];

            if (!script) {
                return { isValid: false, errors: [{ message: 'Script is empty' }], warnings: [] };
            }

            // Simple Bracket matching
            const openParens = (script.match(/\(/g) || []).length;
            const closeParens = (script.match(/\)/g) || []).length;
            if (openParens !== closeParens) {
                errors.push({ message: 'Unmatched parentheses' });
            }

            // Variable extraction and check
            const vars = this._extractVariables(script);
            vars.forEach(v => {
                if (!(v in context)) {
                    warnings.push({ message: `Variable '${v}' is undefined in context` });
                }
            });

            // Function check
            const funcs = this._extractFunctions(script);
            funcs.forEach(f => {
                if (!this.BUILTIN_FUNCTIONS.some(bf => bf.name === f)) {
                    errors.push({ message: `Function '${f}' is unknown` });
                }
            });

            return { isValid: errors.length === 0, errors, warnings };
        }, 'ScriptEngineService.validateScript');
    }

    /**
     * Test execution of a script.
     */
    async testScript(script, context = {}) {
        return await this.handleRequest(async () => {
            const validation = await this.validateScript(script, context);
            if (!validation.isValid) {
                return { success: false, error: validation.errors[0] };
            }

            try {
                // Simplified execution logic
                const safeContext = this._createSafeContext(context);
                const result = this._executeInContext(script, safeContext);
                return { success: true, result };
            } catch (e) {
                return { success: false, error: e.message };
            }
        }, 'ScriptEngineService.testScript');
    }

    /**
     * Parse script to extract variables and functions.
     */
    async parseScript(script) {
        return await this.handleRequest(async () => {
            return {
                variables: this._extractVariables(script),
                functions: this._extractFunctions(script)
            };
        }, 'ScriptEngineService.parseScript');
    }

    /**
     * Get script templates.
     */
    async getTemplates(category) {
        return await this.handleRequest(async () => {
            let templates = [...this.SCRIPT_TEMPLATES];
            if (category) templates = templates.filter(t => t.category === category);
            return templates;
        }, 'ScriptEngineService.getTemplates');
    }

    /**
     * Database verification endpoint.
     */
    async verifyDatabase() {
        return await this.handleRequest(async () => {
            return await this.dbFactory.executeQuery('SELECT 1 as test', []);
        }, 'ScriptEngineService.verifyDatabase');
    }

    // -- Internal Helpers --

    _extractVariables(script) {
        const vars = new Set();
        const regex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\b/g;
        let match;
        const reserved = ['if', 'true', 'false', 'null', 'undefined'];
        while ((match = regex.exec(script)) !== null) {
            const name = match[1];
            if (!this.BUILTIN_FUNCTIONS.some(f => f.name === name) && !reserved.includes(name)) {
                vars.add(name);
            }
        }
        return Array.from(vars);
    }

    _extractFunctions(script) {
        const funcs = new Set();
        const regex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\(/g;
        let match;
        while ((match = regex.exec(script)) !== null) {
            funcs.add(match[1]);
        }
        return Array.from(funcs);
    }

    _createSafeContext(context) {
        const ctx = { ...context };
        ctx.abs = Math.abs;
        ctx.round = (x, d = 0) => Number(x.toFixed(d));
        ctx.floor = Math.floor;
        ctx.ceil = Math.ceil;
        ctx.min = Math.min;
        ctx.max = Math.max;
        ctx.if = (c, t, f) => c ? t : f;
        ctx.now = () => Date.now();
        return ctx;
    }

    _executeInContext(script, context) {
        // Very limited execution for security
        let exp = script;
        Object.entries(context).forEach(([k, v]) => {
            const regex = new RegExp(`\\b${k}\\b`, 'g');
            exp = exp.replace(regex, JSON.stringify(v));
        });

        // Only allow simple arithmetic for now as in the original
        if (exp.match(/^[\d\s+\-*/().]+$/)) {
            return Function('"use strict"; return (' + exp + ')')();
        }
        return null;
    }
}

module.exports = new ScriptEngineService();
