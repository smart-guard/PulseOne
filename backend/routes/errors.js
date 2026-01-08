// =============================================================================
// backend/routes/errors.js
// 에러 모니터링 API 엔드포인트들
// =============================================================================

const express = require('express');
const router = express.Router();

// 에러 통계 저장소 (실제로는 데이터베이스 사용)
let errorStats = {
    totalErrors: 0,
    errorsByCategory: {},
    errorsByDevice: {},
    errorHistory: [],
    recoveryStats: {
        autoRecoveryAttempts: 0,
        autoRecoverySuccess: 0,
        manualInterventionRequired: 0
    }
};

/**
 * @brief 에러 통계 조회
 * GET /api/errors/statistics
 */
router.get('/statistics', async (req, res) => {
    try {
        // 실시간 에러 통계 계산
        const last24h = errorStats.errorHistory.filter(error => 
            Date.now() - new Date(error.timestamp).getTime() < 24 * 60 * 60 * 1000
        );
        
        const stats = {
            total_errors: errorStats.totalErrors,
            last_24h_errors: last24h.length,
            error_distribution: {
                connection_errors: (errorStats.errorsByCategory.connection || []).length,
                device_errors: (errorStats.errorsByCategory.device || []).length,
                protocol_errors: (errorStats.errorsByCategory.protocol || []).length,
                maintenance_events: (errorStats.errorsByCategory.maintenance || []).length,
                system_errors: (errorStats.errorsByCategory.system || []).length
            },
            top_error_codes: getTopErrorCodes(10),
            recovery_rates: {
                auto_recovery_success: calculateRecoveryRate('auto'),
                manual_intervention_required: calculateRecoveryRate('manual')
            },
            device_error_summary: getDeviceErrorSummary(),
            hourly_distribution: getHourlyErrorDistribution()
        };
        
        res.json({
            success: true,
            data: stats,
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * @brief 실시간 에러 목록 조회
 * GET /api/errors/recent?limit=50&category=connection
 */
router.get('/recent', async (req, res) => {
    try {
        const limit = parseInt(req.query.limit) || 50;
        const category = req.query.category;
        const deviceId = req.query.device_id;
        
        let filteredErrors = [...errorStats.errorHistory];
        
        // 카테고리 필터
        if (category) {
            filteredErrors = filteredErrors.filter(error => error.category === category);
        }
        
        // 디바이스 필터
        if (deviceId) {
            filteredErrors = filteredErrors.filter(error => error.deviceId === deviceId);
        }
        
        // 최신순 정렬 후 제한
        const recentErrors = filteredErrors
            .sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp))
            .slice(0, limit)
            .map(error => ({
                ...error,
                error_name: getErrorName(error.httpStatus),
                category_color: getCategoryColor(error.category),
                severity_level: getSeverityLevel(error.httpStatus)
            }));
        
        res.json({
            success: true,
            data: recentErrors,
            total: filteredErrors.length,
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * @brief 특정 에러코드 상세 정보 조회
 * GET /api/errors/codes/:code
 */
router.get('/codes/:code', async (req, res) => {
    try {
        const errorCode = parseInt(req.params.code);
        
        const errorInfo = getErrorCodeDetails(errorCode);
        
        res.json({
            success: true,
            data: errorInfo,
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        res.status(400).json({
            success: false,
            error: 'Invalid error code',
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * @brief 에러 발생 보고 (Collector에서 호출)
 * POST /api/errors/report
 */
router.post('/report', async (req, res) => {
    try {
        const { 
            error_code, 
            http_status, 
            device_id, 
            category, 
            message, 
            tech_details,
            context 
        } = req.body;
        
        // 에러 기록
        const errorRecord = {
            id: Date.now() + Math.random(),
            error_code,
            http_status,
            device_id,
            category,
            message,
            tech_details,
            context,
            timestamp: new Date().toISOString()
        };
        
        // 통계 업데이트
        recordError(errorRecord);
        
        res.json({
            success: true,
            message: 'Error recorded successfully',
            error_id: errorRecord.id
        });
        
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * @brief 디바이스별 에러 현황
 * GET /api/errors/devices/:deviceId
 */
router.get('/devices/:deviceId', async (req, res) => {
    try {
        const deviceId = req.params.deviceId;
        const deviceErrors = errorStats.errorsByDevice[deviceId] || [];
        
        const last24h = deviceErrors.filter(error => 
            Date.now() - new Date(error.timestamp).getTime() < 24 * 60 * 60 * 1000
        );
        
        const summary = {
            device_id: deviceId,
            total_errors: deviceErrors.length,
            last_24h_errors: last24h.length,
            most_common_error: getMostCommonErrorForDevice(deviceId),
            error_timeline: getErrorTimeline(deviceId),
            current_status: getCurrentDeviceErrorStatus(deviceId),
            recommended_actions: getRecommendedActions(deviceId)
        };
        
        res.json({
            success: true,
            data: summary,
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// =============================================================================
// 헬퍼 함수들
// =============================================================================

function recordError(errorRecord) {
    errorStats.totalErrors++;
    errorStats.errorHistory.unshift(errorRecord);
    
    // 카테고리별 분류
    if (!errorStats.errorsByCategory[errorRecord.category]) {
        errorStats.errorsByCategory[errorRecord.category] = [];
    }
    errorStats.errorsByCategory[errorRecord.category].push(errorRecord);
    
    // 디바이스별 분류
    if (errorRecord.device_id) {
        if (!errorStats.errorsByDevice[errorRecord.device_id]) {
            errorStats.errorsByDevice[errorRecord.device_id] = [];
        }
        errorStats.errorsByDevice[errorRecord.device_id].push(errorRecord);
    }
    
    // 히스토리 크기 제한 (최근 1000개만)
    if (errorStats.errorHistory.length > 1000) {
        errorStats.errorHistory = errorStats.errorHistory.slice(0, 1000);
    }
}

function getErrorName(httpStatus) {
    const names = {
        420: '연결 실패', 421: '연결 타임아웃', 422: '연결 거부', 423: '연결 끊어짐',
        430: '타임아웃', 431: '프로토콜 에러', 434: '체크섬 에러', 435: '프레임 에러',
        450: '잘못된 데이터', 451: '타입 불일치', 452: '범위 초과', 453: '포맷 에러',
        470: '응답 없음', 471: '사용 중', 472: '하드웨어 에러', 473: '디바이스 없음', 474: '오프라인',
        490: '설정 오류', 491: '설정 누락', 492: '설정 에러', 493: '매개변수 오류',
        510: '점검 중', 513: '원격 제어 차단', 514: '권한 부족',
        530: 'Modbus 예외', 540: 'MQTT 실패', 550: 'BACnet 에러'
    };
    return names[httpStatus] || `에러 ${httpStatus}`;
}

function getCategoryColor(category) {
    const colors = {
        connection: '#ef4444',    // 빨간색
        device: '#f97316',       // 주황색  
        protocol: '#eab308',     // 노란색
        maintenance: '#3b82f6',  // 파란색
        system: '#8b5cf6',       // 보라색
        modbus: '#06b6d4',       // 청록색
        mqtt: '#10b981',         // 녹색
        bacnet: '#f59e0b'        // 황색
    };
    return colors[category] || '#6b7280';
}

function getTopErrorCodes(limit) {
    const errorCounts = {};
    
    errorStats.errorHistory.forEach(error => {
        const key = error.http_status;
        errorCounts[key] = (errorCounts[key] || 0) + 1;
    });
    
    return Object.entries(errorCounts)
        .sort((a, b) => b[1] - a[1])
        .slice(0, limit)
        .map(([code, count]) => ({
            http_status: parseInt(code),
            count,
            description: getErrorName(parseInt(code))
        }));
}

function calculateRecoveryRate(type) {
    const total = errorStats.recoveryStats.autoRecoveryAttempts + 
                 errorStats.recoveryStats.manualInterventionRequired;
    
    if (total === 0) return 0;
    
    if (type === 'auto') {
        return (errorStats.recoveryStats.autoRecoverySuccess / total * 100).toFixed(1);
    } else {
        return (errorStats.recoveryStats.manualInterventionRequired / total * 100).toFixed(1);
    }
}

function getDeviceErrorSummary() {
    return Object.entries(errorStats.errorsByDevice)
        .map(([deviceId, errors]) => ({
            device_id: deviceId,
            error_count: errors.length,
            last_error: errors[0]?.timestamp,
            status: getCurrentDeviceErrorStatus(deviceId)
        }))
        .sort((a, b) => b.error_count - a.error_count)
        .slice(0, 10); // 상위 10개 디바이스
}

function getCurrentDeviceErrorStatus(deviceId) {
    const deviceErrors = errorStats.errorsByDevice[deviceId] || [];
    if (deviceErrors.length === 0) return 'normal';
    
    const latestError = deviceErrors[0];
    const timeSinceError = Date.now() - new Date(latestError.timestamp).getTime();
    
    // 5분 이내 에러면 active
    if (timeSinceError < 5 * 60 * 1000) {
        return 'error_active';
    }
    
    // 1시간 이내 에러면 warning
    if (timeSinceError < 60 * 60 * 1000) {
        return 'warning';
    }
    
    return 'recovered';
}

module.exports = router;