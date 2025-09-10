// =============================================================================
// backend/lib/database/DatabaseAbstractionLayer.js
// 🔥 SQLite 연결 문제 해결 - 무한 대기 수정
// =============================================================================

/**
 * @brief DB별 SQL 방언(Dialect) 처리 인터페이스
 * C++ ISQLDialect와 동일한 구조
 */
class ISQLDialect {
    // 기본 타입 변환
    getBooleanType() { throw new Error('Not implemented'); }
    getTimestampType() { throw new Error('Not implemented'); }
    getAutoIncrementType() { throw new Error('Not implemented'); }
    getCurrentTimestamp() { throw new Error('Not implemented'); }
    
    // UPSERT 쿼리 생성
    buildUpsertQuery(tableName, columns, primaryKeys) { throw new Error('Not implemented'); }
    
    // CREATE TABLE 구문
    adaptCreateTableQuery(baseQuery) { throw new Error('Not implemented'); }
    
    // BOOLEAN 값 변환
    formatBooleanValue(value) { throw new Error('Not implemented'); }
    parseBooleanValue(value) { throw new Error('Not implemented'); }
    
    // 테이블 존재 확인 쿼리
    getTableExistsQuery() { throw new Error('Not implemented'); }
    
    // 파라미터 플레이스홀더 (?, $1, 등)
    getParameterPlaceholder(index) { throw new Error('Not implemented'); }
}

/**
 * @brief SQLite 방언 구현
 */
class SQLiteDialect extends ISQLDialect {
    getBooleanType() { return 'BOOLEAN'; }
    getTimestampType() { return 'DATETIME'; }
    getAutoIncrementType() { return 'INTEGER PRIMARY KEY AUTOINCREMENT'; }
    getCurrentTimestamp() { return 'CURRENT_TIMESTAMP'; }
    
    buildUpsertQuery(tableName, columns, primaryKeys) {
        const placeholders = columns.map(() => '?').join(', ');
        const updateSet = columns
            .filter(col => !primaryKeys.includes(col))
            .map(col => `${col} = excluded.${col}`)
            .join(', ');
        
        return `INSERT INTO ${tableName} (${columns.join(', ')}) VALUES (${placeholders}) 
                ON CONFLICT (${primaryKeys.join(', ')}) DO UPDATE SET ${updateSet}`;
    }
    
    adaptCreateTableQuery(baseQuery) {
        return baseQuery
            .replace(/SERIAL PRIMARY KEY/g, 'INTEGER PRIMARY KEY AUTOINCREMENT')
            .replace(/BOOLEAN DEFAULT true/g, 'BOOLEAN DEFAULT 1')
            .replace(/BOOLEAN DEFAULT false/g, 'BOOLEAN DEFAULT 0')
            .replace(/TIMESTAMP/g, 'DATETIME');
    }
    
    formatBooleanValue(value) { return value ? '1' : '0'; }
    parseBooleanValue(value) { return value === '1' || value === 'true'; }
    getTableExistsQuery() { return `SELECT name FROM sqlite_master WHERE type='table' AND name = ?`; }
    getParameterPlaceholder(index) { return '?'; }
}

/**
 * @brief PostgreSQL 방언 구현
 */
class PostgreSQLDialect extends ISQLDialect {
    getBooleanType() { return 'BOOLEAN'; }
    getTimestampType() { return 'TIMESTAMP'; }
    getAutoIncrementType() { return 'SERIAL PRIMARY KEY'; }
    getCurrentTimestamp() { return 'CURRENT_TIMESTAMP'; }
    
    buildUpsertQuery(tableName, columns, primaryKeys) {
        const placeholders = columns.map((_, i) => `$${i + 1}`).join(', ');
        const updateSet = columns
            .filter(col => !primaryKeys.includes(col))
            .map(col => `${col} = EXCLUDED.${col}`)
            .join(', ');
        
        return `INSERT INTO ${tableName} (${columns.join(', ')}) VALUES (${placeholders}) 
                ON CONFLICT (${primaryKeys.join(', ')}) DO UPDATE SET ${updateSet}`;
    }
    
    adaptCreateTableQuery(baseQuery) {
        return baseQuery; // PostgreSQL은 표준 SQL이므로 변경 불필요
    }
    
    formatBooleanValue(value) { return value ? 'true' : 'false'; }
    parseBooleanValue(value) { return value === 'true' || value === true; }
    getTableExistsQuery() { return `SELECT table_name FROM information_schema.tables WHERE table_schema = 'public' AND table_name = $1`; }
    getParameterPlaceholder(index) { return `$${index + 1}`; }
}

/**
 * @brief MySQL/MariaDB 방언 구현
 */
class MySQLDialect extends ISQLDialect {
    getBooleanType() { return 'BOOLEAN'; }
    getTimestampType() { return 'TIMESTAMP'; }
    getAutoIncrementType() { return 'INT AUTO_INCREMENT PRIMARY KEY'; }
    getCurrentTimestamp() { return 'CURRENT_TIMESTAMP'; }
    
    buildUpsertQuery(tableName, columns, primaryKeys) {
        const placeholders = columns.map(() => '?').join(', ');
        const updateSet = columns
            .filter(col => !primaryKeys.includes(col))
            .map(col => `${col} = VALUES(${col})`)
            .join(', ');
        
        return `INSERT INTO ${tableName} (${columns.join(', ')}) VALUES (${placeholders}) 
                ON DUPLICATE KEY UPDATE ${updateSet}`;
    }
    
    adaptCreateTableQuery(baseQuery) {
        return baseQuery
            .replace(/SERIAL PRIMARY KEY/g, 'INT AUTO_INCREMENT PRIMARY KEY')
            .replace(/DATETIME DEFAULT CURRENT_TIMESTAMP/g, 'TIMESTAMP DEFAULT CURRENT_TIMESTAMP');
    }
    
    formatBooleanValue(value) { return value ? 'true' : 'false'; }
    parseBooleanValue(value) { return value === 'true' || value === true || value === 1; }
    getTableExistsQuery() { return `SELECT table_name FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = ?`; }
    getParameterPlaceholder(index) { return '?'; }
}

/**
 * @brief MSSQL/SQL Server 방언 구현
 */
class MSSQLDialect extends ISQLDialect {
    getBooleanType() { return 'BIT'; }
    getTimestampType() { return 'DATETIME2'; }
    getAutoIncrementType() { return 'INT IDENTITY(1,1) PRIMARY KEY'; }
    getCurrentTimestamp() { return 'GETDATE()'; }
    
    buildUpsertQuery(tableName, columns, primaryKeys) {
        const placeholders = columns.map(() => '?').join(', ');
        const updateSet = columns
            .filter(col => !primaryKeys.includes(col))
            .map(col => `${col} = SOURCE.${col}`)
            .join(', ');
        
        const insertColumns = columns.join(', ');
        const sourceColumns = columns.map(col => `SOURCE.${col}`).join(', ');
        const matchCondition = primaryKeys.map(key => `TARGET.${key} = SOURCE.${key}`).join(' AND ');
        
        return `MERGE ${tableName} AS TARGET
                USING (VALUES (${placeholders})) AS SOURCE (${insertColumns})
                ON ${matchCondition}
                WHEN MATCHED THEN UPDATE SET ${updateSet}
                WHEN NOT MATCHED THEN INSERT (${insertColumns}) VALUES (${sourceColumns});`;
    }
    
    adaptCreateTableQuery(baseQuery) {
        return baseQuery
            .replace(/SERIAL PRIMARY KEY/g, 'INT IDENTITY(1,1) PRIMARY KEY')
            .replace(/BOOLEAN DEFAULT true/g, 'BIT DEFAULT 1')
            .replace(/BOOLEAN DEFAULT false/g, 'BIT DEFAULT 0')
            .replace(/BOOLEAN/g, 'BIT')
            .replace(/DATETIME/g, 'DATETIME2')
            .replace(/CURRENT_TIMESTAMP/g, 'GETDATE()')
            .replace(/TIMESTAMP/g, 'DATETIME2');
    }
    
    formatBooleanValue(value) { return value ? '1' : '0'; }
    parseBooleanValue(value) { return value === '1' || value === 'true' || value === true || value === 1; }
    getTableExistsQuery() { return `SELECT table_name FROM information_schema.tables WHERE table_schema = 'dbo' AND table_name = ?`; }
    getParameterPlaceholder(index) { return '?'; }
}

/**
 * @brief 통합 데이터베이스 추상화 레이어
 * C++의 DatabaseAbstractionLayer와 동일한 구조
 */
class DatabaseAbstractionLayer {
    constructor(connections = null) {
        this.connections = connections;
        this.currentDbType = process.env.DATABASE_TYPE || 'SQLITE';
        this.dialect = this.createDialect(this.currentDbType);
        
        console.log(`🔧 DatabaseAbstractionLayer: ${this.currentDbType} dialect 초기화됨`);
    }
    
    /**
     * 연결 객체 설정
     */
    setConnections(connections) {
        this.connections = connections;
        console.log('✅ DatabaseAbstractionLayer connections 설정됨');
    }
    
    /**
     * DB별 Dialect 생성
     */
    createDialect(dbType) {
        switch (dbType.toUpperCase()) {
            case 'SQLITE':
                return new SQLiteDialect();
            case 'POSTGRESQL':
            case 'POSTGRES':
                return new PostgreSQLDialect();
            case 'MYSQL':
            case 'MARIADB':
                return new MySQLDialect();
            case 'MSSQL':
            case 'SQLSERVER':
                return new MSSQLDialect();
            default:
                throw new Error(`Unsupported database type: ${dbType}`);
        }
    }
    
    /**
     * 현재 활성 데이터베이스 연결 가져오기
     */
    getCurrentDatabase() {
        if (!this.connections) {
            throw new Error('Database connections not initialized');
        }

        switch (this.currentDbType.toUpperCase()) {
            case 'POSTGRESQL':
            case 'POSTGRES':
                if (!this.connections.postgres) {
                    throw new Error('PostgreSQL connection not available');
                }
                return { db: this.connections.postgres, type: 'postgresql' };
                
            case 'SQLITE':
                if (!this.connections.sqlite) {
                    throw new Error('SQLite connection not available');
                }
                return { db: this.connections.sqlite, type: 'sqlite' };
                
            case 'MYSQL':
            case 'MARIADB':
                if (!this.connections.mysql) {
                    throw new Error('MySQL connection not available');
                }
                return { db: this.connections.mysql, type: 'mysql' };
                
            case 'MSSQL':
            case 'SQLSERVER':
                if (!this.connections.mssql) {
                    throw new Error('MSSQL connection not available');
                }
                return { db: this.connections.mssql, type: 'mssql' };
                
            default:
                throw new Error(`Unsupported database type: ${this.currentDbType}`);
        }
    }
    
    // =========================================================================
    // 🎯 Repository가 사용할 간단한 인터페이스 (C++과 동일)
    // =========================================================================
    
    /**
     * SELECT 쿼리 실행 (표준 SQL → DB별 방언 변환)
     */
    async executeQuery(query, params = []) {
        try {
            const { db, type } = this.getCurrentDatabase();
            
            // 쿼리를 DB별로 적응
            const adaptedQuery = this.preprocessQuery(query);
            
            switch (type) {
                case 'postgresql':
                    const pgResult = await db.query(adaptedQuery, params);
                    return pgResult.rows;
                    
                case 'sqlite':
                    // 🔥 핵심 수정: SQLite 연결 객체의 실제 메서드 사용
                    if (typeof db.all === 'function') {
                        // SQLite 연결 객체가 all 메서드를 지원하는 경우
                        return await db.all(adaptedQuery, params);
                    } else if (typeof db.query === 'function') {
                        // SQLite 연결 객체가 query 메서드를 지원하는 경우
                        const result = await db.query(adaptedQuery, params);
                        return result.rows || result;
                    } else {
                        throw new Error('SQLite connection does not support query methods');
                    }
                    
                case 'mysql':
                    const [mysqlRows] = await db.execute(adaptedQuery, params);
                    return mysqlRows;
                    
                case 'mssql':
                    const mssqlResult = await db.request();
                    // MSSQL 파라미터 바인딩
                    params.forEach((param, index) => {
                        mssqlResult.input(`param${index}`, param);
                    });
                    const result = await mssqlResult.query(adaptedQuery);
                    return result.recordset;
                    
                default:
                    throw new Error(`Unsupported database type: ${type}`);
            }
            
        } catch (error) {
            console.error('DatabaseAbstractionLayer::executeQuery failed:', error.message);
            console.error('  Query:', query);
            console.error('  Params:', params);
            throw error;
        }
    }
    
    /**
     * INSERT/UPDATE/DELETE 실행 (표준 SQL → DB별 방언 변환)
     */
    async executeNonQuery(query, params = []) {
        try {
            const { db, type } = this.getCurrentDatabase();
            
            // 쿼리를 DB별로 적응
            const adaptedQuery = this.preprocessQuery(query);
            
            switch (type) {
                case 'postgresql':
                    const pgResult = await db.query(adaptedQuery, params);
                    return pgResult.rowCount > 0;
                    
                case 'sqlite':
                    // 🔥 핵심 수정: SQLite 연결 객체의 실제 메서드 사용
                    if (typeof db.run === 'function') {
                        // SQLite 연결 객체가 run 메서드를 지원하는 경우
                        const result = await db.run(adaptedQuery, params);
                        return result.changes > 0;
                    } else if (typeof db.query === 'function') {
                        // SQLite 연결 객체가 query 메서드를 지원하는 경우
                        const result = await db.query(adaptedQuery, params);
                        return result.rowCount > 0 || result.changes > 0;
                    } else {
                        throw new Error('SQLite connection does not support non-query methods');
                    }
                    
                case 'mysql':
                    const [mysqlResult] = await db.execute(adaptedQuery, params);
                    return mysqlResult.affectedRows > 0;
                    
                case 'mssql':
                    const mssqlResult = await db.request();
                    params.forEach((param, index) => {
                        mssqlResult.input(`param${index}`, param);
                    });
                    const result = await mssqlResult.query(adaptedQuery);
                    return result.rowsAffected[0] > 0;
                    
                default:
                    throw new Error(`Unsupported database type: ${type}`);
            }
            
        } catch (error) {
            console.error('DatabaseAbstractionLayer::executeNonQuery failed:', error.message);
            console.error('  Query:', query);
            console.error('  Params:', params);
            throw error;
        }
    }
    
    /**
     * UPSERT 쿼리 생성 및 실행
     */
    async executeUpsert(tableName, data, primaryKeys) {
        try {
            const columns = Object.keys(data);
            const values = Object.values(data);
            
            const upsertQuery = this.dialect.buildUpsertQuery(tableName, columns, primaryKeys);
            return await this.executeNonQuery(upsertQuery, values);
            
        } catch (error) {
            console.error('DatabaseAbstractionLayer::executeUpsert failed:', error.message);
            return false;
        }
    }
    
    /**
     * CREATE TABLE 실행 (DB별 자동 적응)
     */
    async executeCreateTable(createSql) {
        try {
            // 1. 테이블 이름 추출
            const tableName = this.extractTableNameFromCreateSQL(createSql);
            if (!tableName) {
                throw new Error('테이블 이름을 추출할 수 없음');
            }
            
            // 2. 테이블 존재 여부 확인
            if (await this.doesTableExist(tableName)) {
                console.log(`✅ 테이블 이미 존재: ${tableName}`);
                return true;
            }
            
            // 3. DB별로 쿼리 적응
            const adaptedQuery = this.dialect.adaptCreateTableQuery(createSql);
            
            // 4. 테이블 생성 실행
            console.log(`📋 테이블 생성 시도: ${tableName}`);
            return await this.executeNonQuery(adaptedQuery);
            
        } catch (error) {
            console.error('executeCreateTable failed:', error.message);
            return false;
        }
    }
    
    /**
     * 테이블 존재 여부 확인
     */
    async doesTableExist(tableName) {
        try {
            const query = this.dialect.getTableExistsQuery();
            const result = await this.executeQuery(query, [tableName]);
            return result.length > 0;
            
        } catch (error) {
            console.error('doesTableExist failed:', error.message);
            return false;
        }
    }
    
    /**
     * CREATE TABLE SQL에서 테이블명 추출
     */
    extractTableNameFromCreateSQL(createSql) {
        const match = createSql.match(/CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+)/i);
        return match ? match[1] : null;
    }
    
    /**
     * 쿼리 전처리 (주석 제거, 공백 정리 등)
     */
    preprocessQuery(query) {
        return query
            .replace(/--[^\r\n]*/g, '')  // SQL 주석 제거
            .replace(/\s+/g, ' ')        // 여러 공백을 하나로
            .trim();                     // 앞뒤 공백 제거
    }
    
    /**
     * BOOLEAN 값 포맷팅 (DB별)
     */
    formatBoolean(value) {
        return this.dialect.formatBooleanValue(value);
    }
    
    /**
     * BOOLEAN 값 파싱 (DB별)
     */
    parseBoolean(value) {
        return this.dialect.parseBooleanValue(value);
    }
    
    /**
     * 현재 타임스탬프 가져오기 (DB별)
     */
    getCurrentTimestamp() {
        return this.dialect.getCurrentTimestamp();
    }
    
    /**
     * 현재 DB 타입 반환
     */
    getCurrentDbType() {
        return this.currentDbType;
    }

    /**
     * 🔥 디버깅을 위한 연결 정보 출력
     */
    debugConnectionInfo() {
        try {
            const { db, type } = this.getCurrentDatabase();
            console.log(`🔍 연결 디버그 정보:
  타입: ${type}
  연결 객체 존재: ${!!db}
  지원 메서드: ${Object.getOwnPropertyNames(Object.getPrototypeOf(db)).filter(name => typeof db[name] === 'function').join(', ')}`);
        } catch (error) {
            console.error('🔍 연결 디버그 실패:', error.message);
        }
    }
}

module.exports = {
    DatabaseAbstractionLayer,
    SQLiteDialect,
    PostgreSQLDialect, 
    MySQLDialect,
    MSSQLDialect
};