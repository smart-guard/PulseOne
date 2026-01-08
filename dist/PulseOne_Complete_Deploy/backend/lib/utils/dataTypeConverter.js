// ============================================================================
// backend/lib/utils/dataTypeConverter.js
// C++ 데이터 타입을 웹 표준 타입으로 변환하는 유틸리티
// ============================================================================

/**
 * C++ 데이터 타입을 웹 표준 타입으로 변환
 * @param {string} cppType - DB에 저장된 C++ 타입 (FLOAT32, uint32, bool 등)
 * @returns {string} 웹 표준 타입 (number, boolean, string)
 */
function convertDataType(cppType) {
    if (!cppType || typeof cppType !== 'string') {
        return 'string'; // 기본값
    }
    
    const type = cppType.toLowerCase();
    
    // 숫자 타입들
    if (type.includes('float') ||
        type.includes('double') ||
        type.includes('int') ||
        type.includes('uint') ||
        type.includes('number') ||
        type.includes('real') ||
        type.includes('decimal')) {
        return 'number';
    }
    
    // 불린 타입들
    if (type.includes('bool')) {
        return 'boolean';
    }
    
    // 나머지는 문자열
    return 'string';
}

/**
 * 데이터포인트 객체의 data_type을 변환
 * @param {Object} dataPoint - 데이터포인트 객체
 * @returns {Object} 변환된 데이터포인트 객체
 */
function transformDataPoint(dataPoint) {
    if (!dataPoint) return dataPoint;
    
    // 이미 변환된 경우 스킵
    if (dataPoint.data_type === 'number' || 
        dataPoint.data_type === 'boolean' || 
        dataPoint.data_type === 'string') {
        return dataPoint;
    }
    
    return {
        ...dataPoint,
        data_type: convertDataType(dataPoint.data_type),
        original_data_type: dataPoint.data_type // 원본 타입 보존
    };
}

/**
 * 데이터포인트 배열의 data_type들을 변환
 * @param {Array} dataPoints - 데이터포인트 배열
 * @returns {Array} 변환된 데이터포인트 배열
 */
function transformDataPoints(dataPoints) {
    if (!Array.isArray(dataPoints)) return dataPoints;
    return dataPoints.map(transformDataPoint);
}

module.exports = {
    convertDataType,
    transformDataPoint,
    transformDataPoints
};