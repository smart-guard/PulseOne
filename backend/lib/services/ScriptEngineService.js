const BaseService = require('./BaseService');
const DatabaseFactory = require('../database/DatabaseFactory');
const vm = require('vm');

/**
 * Service class for Script Engine logic.
 * Enhanced to provide robust validation and simulation mirroring the Collector engine.
 */
class ScriptEngineService extends BaseService {
    constructor() {
        super(null);
        this.dbFactory = new DatabaseFactory();

        this.BUILTIN_FUNCTIONS = [
            // Math
            { name: 'abs', displayName: 'abs', category: 'math', description: '절댓값을 계산합니다', signature: 'abs(x)', examples: ['abs(-5) // 5'] },
            { name: 'round', displayName: 'round', category: 'math', description: '반올림합니다', signature: 'round(x, digits?)', examples: ['round(3.14159, 2) // 3.14'] },
            { name: 'floor', displayName: 'floor', category: 'math', description: '내림합니다', signature: 'floor(x)', examples: ['floor(3.7) // 3'] },
            { name: 'ceil', displayName: 'ceil', category: 'math', description: '올림합니다', signature: 'ceil(x)', examples: ['ceil(3.2) // 4'] },
            { name: 'sqrt', displayName: 'sqrt', category: 'math', description: '제곱근을 계산합니다', signature: 'sqrt(x)', examples: ['sqrt(16) // 4'] },
            { name: 'pow', displayName: 'pow', category: 'math', description: '거듭제곱을 계산합니다', signature: 'pow(base, exponent)', examples: ['pow(2, 3) // 8'] },

            // Statistics
            { name: 'SUM', displayName: 'SUM', category: 'statistics', description: '합계를 계산합니다', signature: 'SUM(...values)', examples: ['SUM(1, 2, 3) // 6'] },
            { name: 'AVG', displayName: 'AVG', category: 'statistics', description: '평균을 계산합니다', signature: 'AVG(...values)', examples: ['AVG(1, 2, 3, 4) // 2.5'] },
            { name: 'MIN', displayName: 'MIN', category: 'statistics', description: '최솟값을 찾습니다', signature: 'MIN(a, b, ...)', examples: ['MIN(1, 2, 3) // 1'] },
            { name: 'MAX', displayName: 'MAX', category: 'statistics', description: '최댓값을 찾습니다', signature: 'MAX(a, b, ...)', examples: ['MAX(1, 2, 3) // 3'] },

            // Conditional
            { name: 'IF', displayName: 'IF', category: 'conditional', description: '조건에 따라 값을 반환합니다', signature: 'IF(condition, trueValue, falseValue)', examples: ['IF(temp > 30, "뜨거움", "적당함")'] },
            { name: 'if', displayName: 'if', category: 'conditional', description: 'IF의 소문자 별칭입니다', signature: 'if(condition, trueValue, falseValue)', examples: ['if(temp > 30, 1, 0)'] },

            // Internal/Legacy Utility
            { name: 'toNumber', displayName: 'toNumber', category: 'conversion', description: '숫자로 변환합니다', signature: 'toNumber(value)', examples: ['toNumber("123") // 123'] },
            { name: 'toString', displayName: 'toString', category: 'conversion', description: '문자열로 변환합니다', signature: 'toString(value)', examples: ['toString(123) // "123"'] }
        ];

        this.SCRIPT_TEMPLATES = [
            { id: 'simple_average', name: '간단한 평균', category: 'math', description: '여러 센서의 평균값을 계산합니다', script: 'AVG(temp1, temp2, temp3)', variables: ['temp1', 'temp2', 'temp3'], tags: ['평균', '온도', '기본'] },
            { id: 'conditional_status', name: '조건부 상태', category: 'conditional', description: '임계값에 따른 상태를 반환합니다', script: 'IF(pressure > 5.0, "위험", IF(pressure > 2.0, "주의", "정상"))', variables: ['pressure'], tags: ['조건문', '상태', '압력'] }
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
     * Validate a script by attempting a dry run in a sandbox.
     */
    async validateScript(script, context = {}) {
        return await this.handleRequest(async () => {
            const errors = [];
            const warnings = [];

            if (!script || !script.trim()) {
                return { isValid: false, errors: [{ message: '수식이 비어있습니다.' }], warnings: [] };
            }

            try {
                const sandbox = this._createSafeSandbox(context);
                vm.createContext(sandbox);

                // Dry run check
                vm.runInContext(script, sandbox, { timeout: 100 });

                // Variable extraction and check for missing ones in the provided context
                const vars = this._extractVariables(script);
                const funcs = this._extractFunctions(script);

                vars.forEach(v => {
                    if (!(v in context) && !(v in sandbox)) {
                        warnings.push({ message: `변수 '${v}'가 컨텍스트에 정의되어 있지 않습니다. 실제 센서 데이터일 수 있습니다.` });
                    }
                });

                return {
                    isValid: errors.length === 0,
                    errors,
                    warnings,
                    usedVariables: vars,
                    usedFunctions: funcs
                };

            } catch (e) {
                errors.push({
                    message: `문법 또는 실행 오류: ${e.message}`,
                    stack: e.stack
                });
            }

            return {
                isValid: false,
                errors,
                warnings,
                usedVariables: [],
                usedFunctions: []
            };
        }, 'ScriptEngineService.validateScript');
    }

    /**
     * Test execution of a script with simulation.
     */
    async testScript(script, context = {}) {
        return await this.handleRequest(async () => {
            const start = Date.now();
            try {
                const sandbox = this._createSafeSandbox(context);
                vm.createContext(sandbox);

                const result = vm.runInContext(script, sandbox, { timeout: 500 });
                const duration = Date.now() - start;

                return {
                    success: true,
                    result,
                    duration,
                    output: result !== undefined ? String(result) : 'null'
                };
            } catch (e) {
                return {
                    success: false,
                    error: e.message,
                    duration: Date.now() - start
                };
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

    async verifyDatabase() {
        return await this.handleRequest(async () => {
            return await this.dbFactory.executeQuery('SELECT 1 as test', []);
        }, 'ScriptEngineService.verifyDatabase');
    }

    // -- Internal Helpers --

    _createSafeSandbox(context) {
        // Core Math functions & Statistics
        const sandbox = {
            ...context,
            // Math aliases
            abs: Math.abs,
            round: (x, d = 0) => Number(x.toFixed(d)),
            floor: Math.floor,
            ceil: Math.ceil,
            sqrt: Math.sqrt,
            pow: Math.pow,

            // Collector equivalents
            SUM: (...args) => args.flat().reduce((a, b) => a + (Number(b) || 0), 0),
            AVG: (...args) => {
                const vals = args.flat();
                return vals.length === 0 ? 0 : vals.reduce((a, b) => a + (Number(b) || 0), 0) / vals.length;
            },
            MIN: (...args) => Math.min(...args.flat().map(v => Number(v) || 0)),
            MAX: (...args) => Math.max(...args.flat().map(v => Number(v) || 0)),

            // Conditional
            IF: (cond, t, f) => cond ? t : f,
            if: (cond, t, f) => cond ? t : f,

            // Conversion
            toNumber: (v) => Number(v),
            toString: (v) => String(v)
        };
        return sandbox;
    }

    _extractVariables(script) {
        const vars = new Set();
        // Strip out strings first to avoid matching words inside quotes
        const stripped = script.replace(/(['"])(?:(?!\1|\\).|\\.)*\1/g, ' ');

        // Match words that are not part of a function call (not followed by '(')
        // and are not reserved words or known functions
        const regex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\b(?!\s*\()/g;
        let match;
        const reserved = ['if', 'true', 'false', 'null', 'undefined', 'return', 'var', 'let', 'const'];
        const knownFuncs = this.BUILTIN_FUNCTIONS.map(f => f.name);

        while ((match = regex.exec(stripped)) !== null) {
            const name = match[1];
            if (!reserved.includes(name) && !knownFuncs.includes(name)) {
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
}

module.exports = new ScriptEngineService();
