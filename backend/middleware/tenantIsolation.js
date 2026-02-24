// ============================================================================
// backend/middleware/tenantIsolation.js
// 멀티테넌트 격리 및 권한 제어 미들웨어
// ============================================================================

const jwt = require('jsonwebtoken');
const sqlite3 = require('sqlite3').verbose();
const ConfigManager = require('../lib/config/ConfigManager');
const config = ConfigManager.getInstance();

// 데이터베이스 연결
const dbPath = config.getDatabaseConfig().sqlite.path;
const db = new sqlite3.Database(dbPath);

/**
 * JWT 토큰 검증 및 사용자 정보 추출
 */
function authenticateToken(req, res, next) {
    const authHeader = req.headers['authorization'];
    const token = authHeader && authHeader.split(' ')[1]; // Bearer TOKEN

    // 🛠️ 개발 환경 또는 no-auth 모드에서는 인증 스킵
    // ALLOW_NO_AUTH=true: 로그인 미구현 베어메탈 배포용 (Windows 설치 패키지)
    const allowNoAuth = process.env.ALLOW_NO_AUTH === 'true';
    if (process.env.NODE_ENV === 'development' || token === 'dev-dummy-token' || allowNoAuth) {
        req.user = {
            id: 1,
            email: 'admin@pulseone.com',
            role: 'system_admin',
            tenant_id: null, // System Admin sees all tenants
            permissions: JSON.stringify(['*']),
            site_access: JSON.stringify([]),
            device_access: JSON.stringify([])
        };
        return next();
    }

    if (!token) {
        return res.status(401).json({
            success: false,
            error: 'Access token required',
            error_code: 'TOKEN_REQUIRED'
        });
    }

    jwt.verify(token, process.env.JWT_SECRET || 'pulseone-secret-key', (err, user) => {
        if (err) {
            return res.status(403).json({
                success: false,
                error: 'Invalid or expired token',
                error_code: 'TOKEN_INVALID'
            });
        }

        req.user = user;
        next();
    });
}

/**
 * 테넌트 격리 미들웨어
 * 모든 DB 쿼리에 자동으로 tenant_id 필터 적용
 */
function tenantIsolation(req, res, next) {
    const user = req.user;

    if (!user) {
        return res.status(401).json({
            success: false,
            error: 'Authentication required',
            error_code: 'AUTH_REQUIRED'
        });
    }

    // 시스템 관리자는 테넌트 제한 없음
    if (user.role === 'system_admin') {
        req.tenantId = null; // 모든 테넌트 접근 가능
        req.isSystemAdmin = true;
        req.siteAccess = null; // 모든 사이트 접근 가능
        req.deviceAccess = null; // 모든 디바이스 접근 가능
        return next();
    }

    // 일반 사용자는 테넌트 격리 적용
    if (!user.tenant_id) {
        return res.status(403).json({
            success: false,
            error: 'Tenant access required',
            error_code: 'TENANT_REQUIRED'
        });
    }

    req.tenantId = user.tenant_id;
    req.isSystemAdmin = false;

    // 사이트 접근 권한 파싱 (JSON 배열)
    try {
        req.siteAccess = user.site_access ? JSON.parse(user.site_access) : null;
        req.deviceAccess = user.device_access ? JSON.parse(user.device_access) : null;
    } catch (e) {
        console.warn('Failed to parse access permissions:', e);
        req.siteAccess = null;
        req.deviceAccess = null;
    }

    next();
}

/**
 * 사이트 접근 권한 검증
 */
function checkSiteAccess(req, res, next) {
    const { tenantId, siteAccess, isSystemAdmin } = req;
    const requestedSiteId = req.params.site_id || req.query.site_id || req.body.site_id;

    // 시스템 관리자는 모든 사이트 접근 가능
    if (isSystemAdmin) {
        return next();
    }

    // 특정 사이트 요청이 없으면 통과 (목록 조회 등)
    if (!requestedSiteId) {
        return next();
    }

    // 사이트 접근 제한이 없으면 테넌트 내 모든 사이트 접근 가능
    if (!siteAccess || siteAccess.length === 0) {
        return next();
    }

    // 요청된 사이트가 접근 허용 목록에 있는지 확인
    const siteId = parseInt(requestedSiteId);
    if (!siteAccess.includes(siteId)) {
        return res.status(403).json({
            success: false,
            error: `Access denied to site ${siteId}`,
            error_code: 'SITE_ACCESS_DENIED'
        });
    }

    next();
}

/**
 * 디바이스 접근 권한 검증
 */
function checkDeviceAccess(req, res, next) {
    const { tenantId, deviceAccess, isSystemAdmin } = req;
    const requestedDeviceId = req.params.device_id || req.params.id || req.query.device_id || req.body.device_id;

    // 시스템 관리자는 모든 디바이스 접근 가능
    if (isSystemAdmin) {
        return next();
    }

    // 특정 디바이스 요청이 없으면 통과
    if (!requestedDeviceId) {
        return next();
    }

    // 디바이스 접근 제한이 없으면 테넌트 내 모든 디바이스 접근 가능
    if (!deviceAccess || deviceAccess.length === 0) {
        return next();
    }

    // 요청된 디바이스가 접근 허용 목록에 있는지 확인
    const deviceId = parseInt(requestedDeviceId);
    if (!deviceAccess.includes(deviceId)) {
        return res.status(403).json({
            success: false,
            error: `Access denied to device ${deviceId}`,
            error_code: 'DEVICE_ACCESS_DENIED'
        });
    }

    next();
}

/**
 * 역할 기반 권한 검증
 */
function requireRole(...allowedRoles) {
    return (req, res, next) => {
        const user = req.user;

        if (!user) {
            return res.status(401).json({
                success: false,
                error: 'Authentication required',
                error_code: 'AUTH_REQUIRED'
            });
        }

        if (!allowedRoles.includes(user.role)) {
            return res.status(403).json({
                success: false,
                error: `Access denied. Required roles: ${allowedRoles.join(', ')}`,
                error_code: 'INSUFFICIENT_ROLE'
            });
        }

        next();
    };
}

/**
 * 권한 기반 접근 제어
 */
function requirePermission(permission) {
    return (req, res, next) => {
        const user = req.user;

        if (!user) {
            return res.status(401).json({
                success: false,
                error: 'Authentication required',
                error_code: 'AUTH_REQUIRED'
            });
        }

        // 시스템 관리자는 모든 권한 보유
        if (user.role === 'system_admin') {
            return next();
        }

        // 사용자 권한 확인
        let userPermissions = [];
        try {
            userPermissions = user.permissions ? JSON.parse(user.permissions) : [];
        } catch (e) {
            console.warn('Failed to parse user permissions:', e);
        }

        if (!userPermissions.includes(permission)) {
            return res.status(403).json({
                success: false,
                error: `Permission '${permission}' required`,
                error_code: 'INSUFFICIENT_PERMISSION'
            });
        }

        next();
    };
}

/**
 * 테넌트 상태 검증 (활성화, 구독 상태 등)
 */
async function validateTenantStatus(req, res, next) {
    const { tenantId, isSystemAdmin } = req;

    // 시스템 관리자는 테넌트 상태 검증 스킵
    if (isSystemAdmin) {
        return next();
    }

    try {
        const tenant = await new Promise((resolve, reject) => {
            db.get(
                'SELECT * FROM tenants WHERE id = ? AND is_active = 1',
                [tenantId],
                (err, row) => {
                    if (err) reject(err);
                    else resolve(row);
                }
            );
        });

        if (!tenant) {
            return res.status(403).json({
                success: false,
                error: 'Tenant not found or inactive',
                error_code: 'TENANT_INACTIVE'
            });
        }

        // 구독 상태 확인
        if (tenant.subscription_status !== 'active') {
            return res.status(402).json({
                success: false,
                error: `Subscription status: ${tenant.subscription_status}`,
                error_code: 'SUBSCRIPTION_REQUIRED'
            });
        }

        // 사용량 제한 확인 (예: 최대 디바이스 수)
        req.tenantLimits = {
            maxEdgeServers: tenant.max_edge_servers,
            maxDataPoints: tenant.max_data_points,
            maxUsers: tenant.max_users
        };

        next();
    } catch (error) {
        console.error('Tenant validation error:', error);
        res.status(500).json({
            success: false,
            error: 'Tenant validation failed',
            error_code: 'TENANT_VALIDATION_ERROR'
        });
    }
}

/**
 * SQL 쿼리에 테넌트 필터 자동 추가 헬퍼
 */
function addTenantFilter(baseQuery, tenantId, tableName = 'devices') {
    if (!tenantId) {
        return baseQuery; // 시스템 관리자는 필터 없음
    }

    // WHERE 절이 이미 있는지 확인
    const hasWhere = baseQuery.toLowerCase().includes('where');
    const connector = hasWhere ? ' AND' : ' WHERE';

    return `${baseQuery}${connector} ${tableName}.tenant_id = ${tenantId}`;
}

/**
 * 사이트 필터 추가 헬퍼
 */
function addSiteFilter(baseQuery, siteAccess, tableName = 'devices') {
    if (!siteAccess || siteAccess.length === 0) {
        return baseQuery; // 사이트 제한 없음
    }

    const hasWhere = baseQuery.toLowerCase().includes('where');
    const connector = hasWhere ? ' AND' : ' WHERE';
    const siteIds = siteAccess.join(',');

    return `${baseQuery}${connector} ${tableName}.site_id IN (${siteIds})`;
}

module.exports = {
    authenticateToken,
    tenantIsolation,
    checkSiteAccess,
    checkDeviceAccess,
    requireRole,
    requirePermission,
    validateTenantStatus,
    addTenantFilter,
    addSiteFilter
};