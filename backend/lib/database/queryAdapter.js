// ============================================================================
// backend/lib/database/queryAdapter.js  
// DB별 쿼리 문법 자동 변환 (SQLite ↔ PostgreSQL ↔ MariaDB)
// ============================================================================

class QueryAdapter {
    constructor() {
        this.adapters = {
            'sqlite': new SqliteAdapter(),
            'sqlite3': new SqliteAdapter(),
            'postgresql': new PostgresAdapter(),
            'postgres': new PostgresAdapter(),
            'mariadb': new MariadbAdapter(),
            'mysql': new MariadbAdapter(),
            'mssql': new MssqlAdapter(),
            'sqlserver': new MssqlAdapter()
        };
    }

    /**
     * DB 타입별 어댑터 반환
     */
    getAdapter(dbType) {
        const adapter = this.adapters[dbType.toLowerCase()];
        if (!adapter) {
            throw new Error(`지원하지 않는 DB 타입: ${dbType}`);
        }
        return adapter;
    }
}

/**
 * 기본 어댑터 클래스
 */
class BaseAdapter {
    constructor(dbType) {
        this.dbType = dbType;
    }

    /**
     * 쿼리 변환
     */
    adaptQuery(query) {
        let adaptedQuery = query;
        
        // 기본 변환들
        adaptedQuery = this.convertDataTypes(adaptedQuery);
        adaptedQuery = this.convertFunctions(adaptedQuery);
        adaptedQuery = this.convertSyntax(adaptedQuery);
        
        return adaptedQuery;
    }

    /**
     * 데이터 타입 변환 (기본)
     */
    convertDataTypes(query) {
        return query;
    }

    /**
     * 함수 변환 (기본)
     */
    convertFunctions(query) {
        return query;
    }

    /**
     * 문법 변환 (기본)
     */
    convertSyntax(query) {
        return query;
    }

    /**
     * 자동 증가 필드
     */
    get autoIncrement() {
        return 'INTEGER PRIMARY KEY';
    }

    /**
     * 현재 시간
     */
    get timestamp() {
        return 'DATETIME';
    }

    /**
     * 불린 타입
     */
    get boolean() {
        return 'INTEGER';
    }
}

/**
 * SQLite 어댑터
 */
class SqliteAdapter extends BaseAdapter {
    constructor() {
        super('sqlite');
    }

    get autoIncrement() {
        return 'INTEGER PRIMARY KEY AUTOINCREMENT';
    }

    get timestamp() {
        return 'DATETIME DEFAULT (datetime(\'now\'))';
    }

    get boolean() {
        return 'INTEGER'; // SQLite는 BOOLEAN이 없음
    }

    convertDataTypes(query) {
        let adaptedQuery = query;
        
        // PostgreSQL → SQLite 변환
        adaptedQuery = adaptedQuery.replace(/SERIAL PRIMARY KEY/gi, 'INTEGER PRIMARY KEY AUTOINCREMENT');
        adaptedQuery = adaptedQuery.replace(/\bTIMESTAMP\b/gi, 'DATETIME');
        adaptedQuery = adaptedQuery.replace(/BOOLEAN/gi, 'INTEGER');
        adaptedQuery = adaptedQuery.replace(/VARCHAR\(\d+\)/gi, 'TEXT');
        
        // MariaDB → SQLite 변환
        adaptedQuery = adaptedQuery.replace(/INT AUTO_INCREMENT PRIMARY KEY/gi, 'INTEGER PRIMARY KEY AUTOINCREMENT');
        adaptedQuery = adaptedQuery.replace(/TINYINT\(1\)/gi, 'INTEGER');
        adaptedQuery = adaptedQuery.replace(/AUTO_INCREMENT/gi, 'AUTOINCREMENT');
        
        return adaptedQuery;
    }

    convertFunctions(query) {
        let adaptedQuery = query;
        
        // PostgreSQL → SQLite 함수 변환
        adaptedQuery = adaptedQuery.replace(/NOW\(\)/gi, 'datetime(\'now\')');
        adaptedQuery = adaptedQuery.replace(/CURRENT_TIMESTAMP/gi, 'datetime(\'now\')');
        
        // LIMIT/OFFSET 변환 (MariaDB → SQLite)
        adaptedQuery = adaptedQuery.replace(/LIMIT\s+(\d+)\s*,\s*(\d+)/gi, 'LIMIT $2 OFFSET $1');
        
        return adaptedQuery;
    }

    convertSyntax(query) {
        let adaptedQuery = query;
        
        // PostgreSQL 파라미터 → SQLite
        adaptedQuery = adaptedQuery.replace(/\$(\d+)/g, '?');
        
        // IF NOT EXISTS 처리
        adaptedQuery = adaptedQuery.replace(/CREATE TABLE IF NOT EXISTS/gi, 'CREATE TABLE IF NOT EXISTS');
        
        return adaptedQuery;
    }
}

/**
 * PostgreSQL 어댑터
 */
class PostgresAdapter extends BaseAdapter {
    constructor() {
        super('postgresql');
    }

    get autoIncrement() {
        return 'SERIAL PRIMARY KEY';
    }

    get timestamp() {
        return 'TIMESTAMP DEFAULT CURRENT_TIMESTAMP';
    }

    get boolean() {
        return 'BOOLEAN';
    }

    convertDataTypes(query) {
        let adaptedQuery = query;
        
        // SQLite → PostgreSQL 변환
        adaptedQuery = adaptedQuery.replace(/INTEGER PRIMARY KEY AUTOINCREMENT/gi, 'SERIAL PRIMARY KEY');
        adaptedQuery = adaptedQuery.replace(/DATETIME/gi, 'TIMESTAMP');
        adaptedQuery = adaptedQuery.replace(/TEXT/gi, 'VARCHAR(255)');
        
        // MariaDB → PostgreSQL 변환
        adaptedQuery = adaptedQuery.replace(/INT AUTO_INCREMENT PRIMARY KEY/gi, 'SERIAL PRIMARY KEY');
        adaptedQuery = adaptedQuery.replace(/TINYINT\(1\)/gi, 'BOOLEAN');
        
        return adaptedQuery;
    }

    convertFunctions(query) {
        let adaptedQuery = query;
        
        // SQLite → PostgreSQL 함수 변환
        adaptedQuery = adaptedQuery.replace(/datetime\('now'\)/gi, 'NOW()');
        
        // LIMIT/OFFSET 변환 (MariaDB → PostgreSQL)
        adaptedQuery = adaptedQuery.replace(/LIMIT\s+(\d+)\s*,\s*(\d+)/gi, 'LIMIT $2 OFFSET $1');
        
        return adaptedQuery;
    }

    convertSyntax(query) {
        let adaptedQuery = query;
        
        // SQLite 파라미터 → PostgreSQL
        let paramCounter = 1;
        adaptedQuery = adaptedQuery.replace(/\?/g, () => `${paramCounter++}`);
        
        // 대소문자 구분 문자열 비교
        adaptedQuery = adaptedQuery.replace(/LIKE\s+'([^']+)'/gi, 'ILIKE \'$1\'');
        
        return adaptedQuery;
    }
}

/**
 * MariaDB/MySQL 어댑터
 */
class MariadbAdapter extends BaseAdapter {
    constructor() {
        super('mariadb');
    }

    get autoIncrement() {
        return 'INT AUTO_INCREMENT PRIMARY KEY';
    }

    get timestamp() {
        return 'TIMESTAMP DEFAULT CURRENT_TIMESTAMP';
    }

    get boolean() {
        return 'TINYINT(1)';
    }

    convertDataTypes(query) {
        let adaptedQuery = query;
        
        // SQLite → MariaDB 변환
        adaptedQuery = adaptedQuery.replace(/INTEGER PRIMARY KEY AUTOINCREMENT/gi, 'INT AUTO_INCREMENT PRIMARY KEY');
        adaptedQuery = adaptedQuery.replace(/DATETIME/gi, 'TIMESTAMP');
        adaptedQuery = adaptedQuery.replace(/TEXT/gi, 'VARCHAR(255)');
        
        // PostgreSQL → MariaDB 변환
        adaptedQuery = adaptedQuery.replace(/SERIAL PRIMARY KEY/gi, 'INT AUTO_INCREMENT PRIMARY KEY');
        adaptedQuery = adaptedQuery.replace(/BOOLEAN/gi, 'TINYINT(1)');
        
        return adaptedQuery;
    }

    convertFunctions(query) {
        let adaptedQuery = query;
        
        // SQLite → MariaDB 함수 변환
        adaptedQuery = adaptedQuery.replace(/datetime\('now'\)/gi, 'NOW()');
        
        // PostgreSQL LIMIT/OFFSET → MariaDB LIMIT
        adaptedQuery = adaptedQuery.replace(/LIMIT\s+(\d+)\s+OFFSET\s+(\d+)/gi, 'LIMIT $2, $1');
        
        return adaptedQuery;
    }

    convertSyntax(query) {
        let adaptedQuery = query;
        
        // PostgreSQL 파라미터 → MariaDB
        adaptedQuery = adaptedQuery.replace(/\$(\d+)/g, '?');
        
        // 백틱으로 테이블/컬럼명 감싸기
        adaptedQuery = adaptedQuery.replace(/CREATE TABLE\s+([a-zA-Z_][a-zA-Z0-9_]*)/gi, 'CREATE TABLE `$1`');
        
        return adaptedQuery;
    }
}

/**
 * MSSQL (SQL Server) 어댑터
 */
class MssqlAdapter extends BaseAdapter {
    constructor() {
        super('mssql');
    }

    get autoIncrement() {
        return 'INT IDENTITY(1,1) PRIMARY KEY';
    }

    get timestamp() {
        return 'DATETIME2 DEFAULT GETDATE()';
    }

    get boolean() {
        return 'BIT';
    }

    convertDataTypes(query) {
        let adaptedQuery = query;
        
        // SQLite → MSSQL 변환
        adaptedQuery = adaptedQuery.replace(/INTEGER PRIMARY KEY AUTOINCREMENT/gi, 'INT IDENTITY(1,1) PRIMARY KEY');
        adaptedQuery = adaptedQuery.replace(/DATETIME/gi, 'DATETIME2');
        adaptedQuery = adaptedQuery.replace(/TEXT/gi, 'NVARCHAR(MAX)');
        
        // PostgreSQL → MSSQL 변환
        adaptedQuery = adaptedQuery.replace(/SERIAL PRIMARY KEY/gi, 'INT IDENTITY(1,1) PRIMARY KEY');
        adaptedQuery = adaptedQuery.replace(/BOOLEAN/gi, 'BIT');
        adaptedQuery = adaptedQuery.replace(/VARCHAR\(/gi, 'NVARCHAR(');
        
        // MariaDB → MSSQL 변환
        adaptedQuery = adaptedQuery.replace(/INT AUTO_INCREMENT PRIMARY KEY/gi, 'INT IDENTITY(1,1) PRIMARY KEY');
        adaptedQuery = adaptedQuery.replace(/TINYINT\(1\)/gi, 'BIT');
        adaptedQuery = adaptedQuery.replace(/TIMESTAMP/gi, 'DATETIME2');
        
        return adaptedQuery;
    }

    convertFunctions(query) {
        let adaptedQuery = query;
        
        // 다른 DB → MSSQL 함수 변환
        adaptedQuery = adaptedQuery.replace(/datetime\('now'\)/gi, 'GETDATE()');
        adaptedQuery = adaptedQuery.replace(/NOW\(\)/gi, 'GETDATE()');
        adaptedQuery = adaptedQuery.replace(/CURRENT_TIMESTAMP/gi, 'GETDATE()');
        
        // LIMIT/OFFSET → TOP 및 OFFSET/FETCH
        adaptedQuery = this.convertLimitToTopOrOffset(adaptedQuery);
        
        return adaptedQuery;
    }

    convertSyntax(query) {
        let adaptedQuery = query;
        
        // PostgreSQL 파라미터 → MSSQL
        adaptedQuery = adaptedQuery.replace(/\$(\d+)/g, '@param$1');
        
        // SQLite 파라미터 → MSSQL  
        let paramCounter = 1;
        adaptedQuery = adaptedQuery.replace(/\?/g, () => `@param${paramCounter++}`);
        
        // IF NOT EXISTS → IF NOT EXISTS (MSSQL 2016+)
        adaptedQuery = adaptedQuery.replace(/CREATE TABLE IF NOT EXISTS\s+([a-zA-Z_][a-zA-Z0-9_]*)/gi, 
            'IF NOT EXISTS (SELECT * FROM sysobjects WHERE name=\'$1\' AND xtype=\'U\') CREATE TABLE $1');
        
        // 대괄호로 테이블/컬럼명 감싸기
        adaptedQuery = adaptedQuery.replace(/CREATE TABLE\s+([a-zA-Z_][a-zA-Z0-9_]*)/gi, 'CREATE TABLE [$1]');
        
        return adaptedQuery;
    }

    /**
     * LIMIT/OFFSET을 MSSQL TOP이나 OFFSET/FETCH로 변환
     */
    convertLimitToTopOrOffset(query) {
        let adaptedQuery = query;
        
        // LIMIT만 있는 경우 → TOP 사용
        const limitOnlyRegex = /LIMIT\s+(\d+)(?!\s+OFFSET)/gi;
        adaptedQuery = adaptedQuery.replace(limitOnlyRegex, (match, limit) => {
            // SELECT 다음에 TOP 추가
            return adaptedQuery.replace(/SELECT/gi, `SELECT TOP ${limit}`);
        });
        
        // LIMIT + OFFSET → OFFSET/FETCH 사용 (SQL Server 2012+)
        const limitOffsetRegex = /LIMIT\s+(\d+)\s+OFFSET\s+(\d+)/gi;
        adaptedQuery = adaptedQuery.replace(limitOffsetRegex, 'OFFSET $2 ROWS FETCH NEXT $1 ROWS ONLY');
        
        // MariaDB 스타일 LIMIT → OFFSET/FETCH
        const mariaLimitRegex = /LIMIT\s+(\d+)\s*,\s*(\d+)/gi;
        adaptedQuery = adaptedQuery.replace(mariaLimitRegex, 'OFFSET $1 ROWS FETCH NEXT $2 ROWS ONLY');
        
        return adaptedQuery;
    }
}

module.exports = QueryAdapter;