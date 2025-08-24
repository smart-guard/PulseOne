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
        type.includes('number')) {
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

// ============================================================================
// backend/routes/data.js 수정 부분
// 기존 라우터에서 이 함수를 사용하도록 수정
// ============================================================================

// 파일 맨 위에 추가
const { transformDataPoints } = require('../lib/utils/dataTypeConverter');

// GET /api/data/points 라우터 수정
router.get('/points', async (req, res) => {
    try {
        // ... 기존 코드 ...
        
        // 🔥 데이터포인트 조회 후 데이터 타입 변환
        let dataPoints = await getDataPoints(filters);
        dataPoints = transformDataPoints(dataPoints); // 타입 변환 추가

        const response = {
            success: true,
            data: {
                points: dataPoints,
                total_items: dataPoints.length,
                pagination: {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    has_next: dataPoints.length === parseInt(limit),
                    has_prev: parseInt(page) > 1
                }
            },
            message: `Found ${dataPoints.length} data points`,
            timestamp: new Date().toISOString()
        };

        res.json(response);
    } catch (error) {
        console.error('데이터포인트 조회 오류:', error);
        res.status(500).json({
            success: false,
            data: null,
            message: 'Failed to fetch data points',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// ============================================================================
// 테스트용 SQL 쿼리 (확인용)
// ============================================================================

/*
-- 변환 전후 비교 쿼리
SELECT 
    dp.id,
    dp.name,
    dp.data_type as '원본_타입',
    CASE 
        WHEN lower(dp.data_type) LIKE '%float%' OR 
             lower(dp.data_type) LIKE '%double%' OR 
             lower(dp.data_type) LIKE '%int%' OR
             lower(dp.data_type) LIKE '%uint%' OR
             lower(dp.data_type) LIKE '%number%' THEN 'number'
        WHEN lower(dp.data_type) LIKE '%bool%' THEN 'boolean'
        ELSE 'string'
    END as '변환된_타입'
FROM data_points dp
WHERE dp.device_id = 1
LIMIT 10;
*/