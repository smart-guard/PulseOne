/**
 * backend/lib/services/ProtocolValidationService.js
 * 프로토콜별 설정 및 데이터 포인트 유효성 검증 서비스
 */

const logger = require('../utils/LogManager');

class ProtocolValidationService {
    constructor() {
        this.logger = logger;
    }

    /**
     * 템플릿의 전체 설정을 검증합니다.
     */
    validateTemplate(protocolType, config, dataPoints) {
        const errors = [];

        // 1. 프로토콜별 기본 설정 검증 (config)
        const configErrors = this.validateConfig(protocolType, config);
        errors.push(...configErrors);

        // 2. 데이터 포인트 개별 검증
        const pointErrors = this.validateDataPoints(protocolType, dataPoints);
        errors.push(...pointErrors);

        // 3. 데이터 포인트 간 상호 충돌 검증 (중복 등)
        const conflictErrors = this.checkConflicts(protocolType, dataPoints);
        errors.push(...conflictErrors);

        return {
            isValid: errors.length === 0,
            errors: errors
        };
    }

    /**
     * 프로토콜 설정을 검증합니다.
     */
    validateConfig(protocolType, config) {
        const errors = [];
        if (!config) return errors;

        if (protocolType && protocolType.includes('MODBUS')) {
            // Modbus Slave ID 검증 (보통 1-247)
            if (config.slave_id !== undefined) {
                const sid = parseInt(config.slave_id);
                if (isNaN(sid) || sid < 1 || sid > 247) {
                    errors.push({ field: 'slave_id', message: 'Modbus Slave ID는 1에서 247 사이여야 합니다.' });
                }
            }
        }

        // 공통 정책 검증
        if (config.polling_interval !== undefined) {
            if (config.polling_interval < 50) {
                errors.push({ field: 'polling_interval', message: '수집 주기는 최소 50ms 이상이어야 합니다.' });
            }
        }

        return errors;
    }

    /**
     * 데이터 포인트 개별 검증
     */
    validateDataPoints(protocolType, dataPoints) {
        const errors = [];
        if (!Array.isArray(dataPoints)) return errors;

        dataPoints.forEach((point, index) => {
            // 데이터 타입 정규화 (UI 입력 호환성)
            if (point.data_type) {
                point.data_type = this.normalizeDataType(point.data_type);
            }

            if (!point.name) {
                errors.push({ index, field: 'name', message: '포인트 이름은 필수입니다.' });
            }

            if (point.address === undefined || point.address === '') {
                errors.push({ index, field: 'address', message: '주소는 필수입니다.' });
            }

            if (protocolType && protocolType.includes('MODBUS')) {
                const addr = parseInt(point.address);
                if (isNaN(addr) || addr < 1 || addr > 65535) {
                    errors.push({ index, field: 'address', message: 'Modbus 주소는 1에서 65535 사이여야 합니다.' });
                }
            }
        });

        return errors;
    }

    /**
     * 데이터 타입을 산업 표준 형식으로 정규화합니다.
     */
    normalizeDataType(type) {
        if (!type || typeof type !== 'string') return type;

        const upper = type.toUpperCase();

        // Alias mapping
        const aliases = {
            'BOOLEAN': 'BOOL',
            'FLOAT': 'FLOAT32',
            'DOUBLE': 'FLOAT64',
            'INT': 'INT32',
            'SHORT': 'INT16',
            'USHORT': 'UINT16',
            'LONG': 'INT64',
            'ULONG': 'UINT64',
            'BYTE': 'UINT8'
        };

        return aliases[upper] || upper;
    }

    /**
     * 데이터 포인트 간 충돌을 확인합니다.
     */
    checkConflicts(protocolType, dataPoints) {
        const errors = [];
        if (!Array.isArray(dataPoints)) return errors;

        const intervals = [];
        dataPoints.forEach((p, index) => {
            const addr = parseInt(p.address);
            if (!isNaN(addr)) {
                // 정규화된 데이터 타입 사용
                const dataType = this.normalizeDataType(p.data_type);

                // Modbus 등 워드 단위 주소를 사용하는 경우 (32비트 이상은 2워드 이상 점유)
                let size = 1;
                if (['UINT32', 'INT32', 'FLOAT32'].includes(dataType)) {
                    size = 2;
                } else if (['UINT64', 'INT64', 'FLOAT64'].includes(dataType)) {
                    size = 4;
                }

                intervals.push({ start: addr, end: addr + size - 1, index, name: p.name });
            }
        });

        // 겹치는 구간 확인
        for (let i = 0; i < intervals.length; i++) {
            for (let j = i + 1; j < intervals.length; j++) {
                if (intervals[i].start <= intervals[j].end && intervals[j].start <= intervals[i].end) {
                    errors.push({
                        type: 'CONFLICT',
                        indices: [intervals[i].index, intervals[j].index],
                        message: `주소 충돌 감지: '${intervals[i].name}'(#${intervals[i].start})와 '${intervals[j].name}'(#${intervals[j].start})의 범위가 겹칩니다.`
                    });
                }
            }
        }

        return errors;
    }
}

module.exports = new ProtocolValidationService();
