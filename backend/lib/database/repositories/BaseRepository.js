// ============================================================================
// backend/lib/database/repositories/BaseRepository.js
// DatabaseFactory를 활용한 멀티 DB 지원 BaseRepository
// ============================================================================

const DatabaseFactory = require('../DatabaseFactory');

/**
 * 기본 Repository 클래스 (C++ IRepository<T> 역할)
 * DatabaseFactory를 통해 설정에 따라 다른 DB 사용 (SQLite, PostgreSQL, MariaDB 등)
 */
class BaseRepository {
    constructor(tableName) {
        this.tableName = tableName;
        this.logger = console; // TODO: 실제 LogManager로 교체 예정
        
        // DatabaseFactory 인스턴스 생성
        this.dbFactory = new DatabaseFactory();
        this.dbType = this.dbFactory.config.database.type;
        this.queryAdapter = this.dbFactory.getQueryAdapter();
        
        // 캐시 설정 (C++ Repository 패턴과 동일)
        this.cacheEnabled = process.env.REPOSITORY_CACHE === 'true';
        this.cache = new Map();
        this.cacheTimeout = 5 * 60 * 1000; // 5분
        
        this.logger.log(`✅ ${tableName}Repository initialized - DB: ${this.dbType.toUpperCase()}, Cache: ${this.cacheEnabled ? 'ON' : 'OFF'}`);
    }

    // ========================================================================
    // 데이터베이스 연결 관리 (DatabaseFactory 활용)
    // ========================================================================

    /**
     * 쿼리 실행 (단일 결과) - DatabaseFactory를 통한 자동 DB 타입 처리
     */
    async executeQuerySingle(query, params = []) {
        try {
            // DB별 쿼리 변환
            const adaptedQuery = this.adaptQuery(query);
            
            const results = await this.dbFactory.executeQuery(adaptedQuery, params);
            
            // DB별 결과 처리
            switch (this.dbType) {
                case 'sqlite':
                case 'sqlite3':
                    return results.length > 0 ? results[0] : null;
                
                case 'postgresql':
                case 'postgres':
                    return results.rows && results.rows.length > 0 ? results.rows[0] : null;
                
                case 'mariadb':
                case 'mysql':
                    return Array.isArray(results) && results.length > 0 ? results[0] : null;
                
                case 'mssql':
                case 'sqlserver':
                    return results.recordset && results.recordset.length > 0 ? results.recordset[0] : null;
                
                default:
                    return results.length > 0 ? results[0] : null;
            }
        } catch (error) {
            this.logger.error(`Query single error in ${this.tableName}:`, error.message);
            throw error;
        }
    }

    /**
     * 쿼리 실행 (다중 결과) - DatabaseFactory를 통한 자동 DB 타입 처리
     */
    async executeQuery(query, params = []) {
        try {
            // DB별 쿼리 변환
            const adaptedQuery = this.adaptQuery(query);
            
            const results = await this.dbFactory.executeQuery(adaptedQuery, params);
            
            // DB별 결과 처리
            switch (this.dbType) {
                case 'sqlite':
                case 'sqlite3':
                    return results || [];
                
                case 'postgresql':
                case 'postgres':
                    return results.rows || [];
                
                case 'mariadb':
                case 'mysql':
                    return Array.isArray(results) ? results : [];
                
                case 'mssql':
                case 'sqlserver':
                    return results.recordset || [];
                
                default:
                    return results || [];
            }
        } catch (error) {
            this.logger.error(`Query error in ${this.tableName}:`, error.message);
            throw error;
        }
    }

    /**
     * 비SELECT 쿼리 실행 (INSERT, UPDATE, DELETE) - DatabaseFactory 활용
     */
    async executeNonQuery(query, params = []) {
        try {
            // DB별 쿼리 변환
            const adaptedQuery = this.adaptQuery(query);
            
            const result = await this.dbFactory.executeQuery(adaptedQuery, params);
            
            // DB별 결과 처리
            switch (this.dbType) {
                case 'sqlite':
                case 'sqlite3':
                    return {
                        lastID: result.lastID || result.insertId,
                        changes: result.changes || result.affectedRows || (result.length || 0)
                    };
                
                case 'postgresql':
                case 'postgres':
                    return {
                        lastID: result.insertId || (result.rows && result.rows[0] ? result.rows[0].id : null),
                        changes: result.rowCount || 0
                    };
                
                case 'mariadb':
                case 'mysql':
                    return {
                        lastID: result.insertId,
                        changes: result.affectedRows || 0
                    };
                
                case 'mssql':
                case 'sqlserver':
                    return {
                        lastID: result.recordset && result.recordset[0] ? result.recordset[0].id : null,
                        changes: result.rowsAffected ? result.rowsAffected[0] : 0
                    };
                
                default:
                    return {
                        lastID: null,
                        changes: 0
                    };
            }
        } catch (error) {
            this.logger.error(`Non-query error in ${this.tableName}:`, error.message);
            throw error;
        }
    }

    // ========================================================================
    // DB별 쿼리 어댑터 (DatabaseFactory의 QueryAdapter 활용)
    // ========================================================================

    /**
     * DB 타입에 따른 쿼리 변환
     */
    adaptQuery(query) {
        // DatabaseFactory의 QueryAdapter 사용
        return this.queryAdapter.adaptQuery(query);
    }

    /**
     * DB별 데이터 타입 변환
     */
    getDbDataType(fieldType) {
        switch (this.dbType) {
            case 'sqlite':
            case 'sqlite3':
                return this.getSqliteDataType(fieldType);
            
            case 'postgresql':
            case 'postgres':
                return this.getPostgresDataType(fieldType);
            
            case 'mariadb':
            case 'mysql':
                return this.getMysqlDataType(fieldType);
            
            case 'mssql':
            case 'sqlserver':
                return this.getMssqlDataType(fieldType);
            
            default:
                return fieldType;
        }
    }

    getSqliteDataType(fieldType) {
        const typeMap = {
            'autoIncrement': 'INTEGER PRIMARY KEY AUTOINCREMENT',
            'varchar': 'TEXT',
            'text': 'TEXT',
            'int': 'INTEGER',
            'boolean': 'INTEGER',
            'datetime': 'DATETIME',
            'timestamp': 'DATETIME DEFAULT CURRENT_TIMESTAMP'
        };
        return typeMap[fieldType] || fieldType;
    }

    getPostgresDataType(fieldType) {
        const typeMap = {
            'autoIncrement': 'SERIAL PRIMARY KEY',
            'varchar': 'VARCHAR',
            'text': 'TEXT',
            'int': 'INTEGER',
            'boolean': 'BOOLEAN',
            'datetime': 'TIMESTAMP',
            'timestamp': 'TIMESTAMP DEFAULT CURRENT_TIMESTAMP'
        };
        return typeMap[fieldType] || fieldType;
    }

    getMysqlDataType(fieldType) {
        const typeMap = {
            'autoIncrement': 'INT AUTO_INCREMENT PRIMARY KEY',
            'varchar': 'VARCHAR',
            'text': 'TEXT',
            'int': 'INT',
            'boolean': 'TINYINT(1)',
            'datetime': 'DATETIME',
            'timestamp': 'TIMESTAMP DEFAULT CURRENT_TIMESTAMP'
        };
        return typeMap[fieldType] || fieldType;
    }

    getMssqlDataType(fieldType) {
        const typeMap = {
            'autoIncrement': 'INT IDENTITY(1,1) PRIMARY KEY',
            'varchar': 'NVARCHAR',
            'text': 'NVARCHAR(MAX)',
            'int': 'INT',
            'boolean': 'BIT',
            'datetime': 'DATETIME2',
            'timestamp': 'DATETIME2 DEFAULT GETDATE()'
        };
        return typeMap[fieldType] || fieldType;
    }

    // ========================================================================
    // 캐시 관리 (C++ Repository 패턴)
    // ========================================================================

    /**
     * 캐시에서 조회
     */
    getFromCache(key) {
        if (!this.cacheEnabled) return null;
        
        const cached = this.cache.get(key);
        if (!cached) return null;
        
        // 캐시 만료 확인
        if (Date.now() - cached.timestamp > this.cacheTimeout) {
            this.cache.delete(key);
            return null;
        }
        
        return cached.data;
    }

    /**
     * 캐시에 저장
     */
    setCache(key, data) {
        if (!this.cacheEnabled) return;
        
        this.cache.set(key, {
            data: data,
            timestamp: Date.now()
        });
    }

    /**
     * 캐시 무효화
     */
    invalidateCache(pattern = null) {
        if (!this.cacheEnabled) return;
        
        if (pattern) {
            // 패턴에 맞는 키만 삭제
            for (const key of this.cache.keys()) {
                if (key.includes(pattern)) {
                    this.cache.delete(key);
                }
            }
        } else {
            // 전체 캐시 클리어
            this.cache.clear();
        }
    }

    // ========================================================================
    // 공통 유틸리티 메서드들 (DB별 처리)
    // ========================================================================

    /**
     * WHERE 조건 빌더 (DB별 문법 지원)
     */
    buildWhereClause(conditions, tenantId = null) {
        const clauses = [];
        const params = [];

        // 테넌트 필터 자동 추가
        if (tenantId) {
            clauses.push(this.getParameterPlaceholder(params.length + 1) + ' = ?');
            if (this.dbType === 'postgresql') {
                clauses[clauses.length - 1] = clauses[clauses.length - 1].replace('?', `$${params.length + 1}`);
            }
            clauses[clauses.length - 1] = clauses[clauses.length - 1].replace('?', 'tenant_id');
            clauses[clauses.length - 1] = `tenant_id = ${this.getParameterPlaceholder(params.length + 1)}`;
            params.push(tenantId);
        }

        // 추가 조건들
        for (const condition of conditions) {
            const placeholder = this.getParameterPlaceholder(params.length + 1);
            clauses.push(`${condition.field} ${condition.operator} ${placeholder}`);
            params.push(condition.value);
        }

        const whereClause = clauses.length > 0 ? ` WHERE ${clauses.join(' AND ')}` : '';
        return { whereClause, params };
    }

    /**
     * DB별 파라미터 플레이스홀더 반환
     */
    getParameterPlaceholder(index) {
        switch (this.dbType) {
            case 'postgresql':
            case 'postgres':
                return `${index}`;
            
            case 'mssql':
            case 'sqlserver':
                return `@param${index}`;
            
            case 'sqlite':
            case 'sqlite3':
            case 'mariadb':
            case 'mysql':
            default:
                return '?';
        }
    }

    /**
     * ORDER BY 절 빌더 (DB별 키워드 지원)
     */
    buildOrderClause(sortBy = 'id', sortOrder = 'ASC') {
        const validOrders = ['ASC', 'DESC'];
        const order = validOrders.includes(sortOrder.toUpperCase()) ? sortOrder.toUpperCase() : 'ASC';
        
        // DB별 키워드 이스케이프
        let escapedSortBy = sortBy;
        if (this.dbType === 'postgresql' && ['order', 'user', 'group'].includes(sortBy.toLowerCase())) {
            escapedSortBy = `"${sortBy}"`;
        } else if (this.dbType === 'mssql' && ['order', 'user', 'group'].includes(sortBy.toLowerCase())) {
            escapedSortBy = `[${sortBy}]`;
        } else if ((this.dbType === 'mariadb' || this.dbType === 'mysql') && ['order', 'user', 'group'].includes(sortBy.toLowerCase())) {
            escapedSortBy = `\`${sortBy}\``;
        }
        
        return ` ORDER BY ${escapedSortBy} ${order}`;
    }

    /**
     * LIMIT/OFFSET 절 빌더 (DB별 문법)
     */
    buildLimitClause(page = 1, limit = 20) {
        const offset = (page - 1) * limit;
        
        switch (this.dbType) {
            case 'postgresql':
            case 'postgres':
            case 'sqlite':
            case 'sqlite3':
                return ` LIMIT ${limit} OFFSET ${offset}`;
            
            case 'mariadb':
            case 'mysql':
                return ` LIMIT ${offset}, ${limit}`;
            
            case 'mssql':
            case 'sqlserver':
                // SQL Server 2012+ OFFSET/FETCH 문법
                return ` OFFSET ${offset} ROWS FETCH NEXT ${limit} ROWS ONLY`;
            
            default:
                return ` LIMIT ${limit} OFFSET ${offset}`;
        }
    }

    /**
     * 현재 시간 함수 (DB별)
     */
    getCurrentTimestamp() {
        switch (this.dbType) {
            case 'postgresql':
            case 'postgres':
                return 'NOW()';
            
            case 'sqlite':
            case 'sqlite3':
                return "datetime('now')";
            
            case 'mariadb':
            case 'mysql':
                return 'NOW()';
            
            case 'mssql':
            case 'sqlserver':
                return 'GETDATE()';
            
            default:
                return 'NOW()';
        }
    }

    /**
     * 페이징 정보 계산
     */
    calculatePagination(total, page, limit) {
        const totalPages = Math.ceil(total / limit);
        return {
            current_page: parseInt(page),
            total_pages: totalPages,
            total_items: total,
            items_per_page: parseInt(limit),
            has_next: page < totalPages,
            has_prev: page > 1
        };
    }

    // ========================================================================
    // 표준 CRUD 메서드들 (C++ IRepository 인터페이스)
    // ========================================================================

    /**
     * 전체 조회 (자식 클래스에서 오버라이드)
     */
    async findAll(tenantId = null) {
        throw new Error(`findAll not implemented in ${this.tableName}Repository`);
    }

    /**
     * ID로 조회 (자식 클래스에서 오버라이드)
     */
    async findById(id, tenantId = null) {
        throw new Error(`findById not implemented in ${this.tableName}Repository`);
    }

    /**
     * 저장 (자식 클래스에서 오버라이드)
     */
    async save(entity, tenantId = null) {
        throw new Error(`save not implemented in ${this.tableName}Repository`);
    }

    /**
     * 수정 (자식 클래스에서 오버라이드)
     */
    async update(id, entity, tenantId = null) {
        throw new Error(`update not implemented in ${this.tableName}Repository`);
    }

    /**
     * 삭제 (자식 클래스에서 오버라이드)
     */
    async deleteById(id, tenantId = null) {
        throw new Error(`deleteById not implemented in ${this.tableName}Repository`);
    }

    /**
     * 존재 확인 (자식 클래스에서 오버라이드)
     */
    async exists(id, tenantId = null) {
        throw new Error(`exists not implemented in ${this.tableName}Repository`);
    }

    // ========================================================================
    // 트랜잭션 지원 (DatabaseFactory 활용)
    // ========================================================================

    /**
     * 트랜잭션 실행
     */
    async executeTransaction(operations) {
        try {
            // DatabaseFactory의 트랜잭션 기능 활용
            const adaptedOperations = operations.map(op => ({
                query: this.adaptQuery(op.query),
                params: op.params
            }));
            
            return await this.dbFactory.executeTransaction(adaptedOperations);
        } catch (error) {
            this.logger.error(`Transaction error in ${this.tableName}:`, error);
            throw error;
        }
    }

    // ========================================================================
    // 헬스체크 및 통계
    // ========================================================================

    /**
     * Repository 상태 확인
     */
    async healthCheck() {
        try {
            const testQuery = 'SELECT 1 as test';
            const result = await this.executeQuerySingle(testQuery);
            
            return {
                status: 'healthy',
                tableName: this.tableName,
                dbType: this.dbType,
                cacheEnabled: this.cacheEnabled,
                cacheSize: this.cache.size,
                testQuery: result ? 'passed' : 'failed',
                dbFactory: this.dbFactory.getConnectionStatus()
            };
        } catch (error) {
            return {
                status: 'unhealthy',
                tableName: this.tableName,
                dbType: this.dbType,
                error: error.message
            };
        }
    }

    /**
     * 캐시 통계
     */
    getCacheStats() {
        return {
            enabled: this.cacheEnabled,
            size: this.cache.size,
            timeout: this.cacheTimeout,
            tableName: this.tableName,
            dbType: this.dbType
        };
    }

    /**
     * 연결 정리
     */
    async cleanup() {
        this.cache.clear();
        await this.dbFactory.closeAllConnections();
    }
}

module.exports = BaseRepository;