// backend/lib/database/queryAdapter.js
// DB별 쿼리 문법 변환 어댑터

class QueryAdapter {
    constructor() {
        // DB별 어댑터 정의
        this.adapters = {
            'sqlite': new SQLiteAdapter(),
            'sqlite3': new SQLiteAdapter(),
            'postgresql': new PostgreSQLAdapter(),
            'postgres': new PostgreSQLAdapter(),
            'mssql': new MSSQLAdapter(),
            'mariadb': new MariaDBAdapter(),
            'mysql': new MariaDBAdapter()
        };
    }

    /**
     * 특정 DB 타입의 어댑터 반환
     */
    getAdapter(dbType) {
        const adapter = this.adapters[dbType.toLowerCase()];
        if (!adapter) {
            throw new Error(`지원하지 않는 데이터베이스 타입: ${dbType}`);
        }
        return adapter;
    }

    /**
     * 쿼리 변환 (DB 타입별 자동 변환)
     */
    adaptQuery(query, dbType, params = []) {
        const adapter = this.getAdapter(dbType);
        return adapter.adaptQuery(query, params);
    }
}

/**
 * 기본 어댑터 클래스
 */
class BaseAdapter {
    constructor() {
        this.autoIncrement = 'SERIAL';
        this.uuid = 'VARCHAR(36)';
        this.timestamp = 'TIMESTAMP';
        this.boolean = 'BOOLEAN';
        this.text = 'TEXT';
        this.json = 'JSON';
        this.parameterPlaceholder = '?';
    }

    /**
     * 쿼리 변환 (기본 구현)
     */
    adaptQuery(query, params = []) {
        return { query, params };
    }

    /**
     * 테이블 존재 여부 확인 쿼리
     */
    getTableExistsQuery(tableName) {
        return `SELECT name FROM sqlite_master WHERE type='table' AND name=?`;
    }

    /**
     * 컬럼 존재 여부 확인 쿼리
     */
    getColumnExistsQuery(tableName, columnName) {
        return `PRAGMA table_info(${tableName})`;
    }

    /**
     * 인덱스 생성 쿼리
     */
    createIndexQuery(indexName, tableName, columns) {
        const columnList = Array.isArray(columns) ? columns.join(', ') : columns;
        return `CREATE INDEX IF NOT EXISTS ${indexName} ON ${tableName} (${columnList})`;
    }

    /**
     * UPSERT 쿼리 (INSERT OR UPDATE)
     */
    upsertQuery(tableName, data, conflictColumns) {
        throw new Error('UPSERT는 각 DB별로 구현이 필요합니다');
    }

    /**
     * 페이징 쿼리
     */
    paginateQuery(baseQuery, limit, offset) {
        return `${baseQuery} LIMIT ${limit} OFFSET ${offset}`;
    }
}

/**
 * SQLite 어댑터
 */
class SQLiteAdapter extends BaseAdapter {
    constructor() {
        super();
        this.autoIncrement = 'INTEGER PRIMARY KEY AUTOINCREMENT';
        this.uuid = 'TEXT';
        this.timestamp = 'DATETIME DEFAULT CURRENT_TIMESTAMP';
        this.boolean = 'INTEGER';
        this.text = 'TEXT';
        this.json = 'TEXT';
        this.parameterPlaceholder = '?';
    }

    getTableExistsQuery(tableName) {
        return {
            query: `SELECT name FROM sqlite_master WHERE type='table' AND name=?`,
            params: [tableName]
        };
    }

    getColumnExistsQuery(tableName, columnName) {
        return {
            query: `PRAGMA table_info(${tableName})`,
            params: []
        };
    }

    upsertQuery(tableName, data, conflictColumns) {
        const columns = Object.keys(data);
        const values = Object.values(data);
        const placeholders = values.map(() => '?').join(', ');
        
        const updateSet = columns
            .filter(col => !conflictColumns.includes(col))
            .map(col => `${col} = excluded.${col}`)
            .join(', ');

        const query = `
            INSERT INTO ${tableName} (${columns.join(', ')}) 
            VALUES (${placeholders})
            ON CONFLICT(${conflictColumns.join(', ')}) 
            DO UPDATE SET ${updateSet}
        `;

        return { query, params: values };
    }

    adaptQuery(query, params = []) {
        // SQLite 특화 변환
        let adaptedQuery = query
            .replace(/SERIAL/g, 'INTEGER PRIMARY KEY AUTOINCREMENT')
            .replace(/BOOLEAN/g, 'INTEGER')
            .replace(/TIMESTAMP WITH TIME ZONE/g, 'DATETIME')
            .replace(/UUID/g, 'TEXT')
            .replace(/JSONB/g, 'TEXT');

        return { query: adaptedQuery, params };
    }
}

/**
 * PostgreSQL 어댑터  
 */
class PostgreSQLAdapter extends BaseAdapter {
    constructor() {
        super();
        this.autoIncrement = 'SERIAL PRIMARY KEY';
        this.uuid = 'UUID';
        this.timestamp = 'TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP';
        this.boolean = 'BOOLEAN';
        this.text = 'TEXT';
        this.json = 'JSONB';
        this.parameterPlaceholder = '$';
    }

    getTableExistsQuery(tableName) {
        return {
            query: `SELECT table_name FROM information_schema.tables WHERE table_name=$1`,
            params: [tableName]
        };
    }

    getColumnExistsQuery(tableName, columnName) {
        return {
            query: `SELECT column_name FROM information_schema.columns WHERE table_name=$1 AND column_name=$2`,
            params: [tableName, columnName]
        };
    }

    upsertQuery(tableName, data, conflictColumns) {
        const columns = Object.keys(data);
        const values = Object.values(data);
        const placeholders = values.map((_, i) => `$${i + 1}`).join(', ');
        
        const updateSet = columns
            .filter(col => !conflictColumns.includes(col))
            .map(col => `${col} = EXCLUDED.${col}`)
            .join(', ');

        const query = `
            INSERT INTO ${tableName} (${columns.join(', ')}) 
            VALUES (${placeholders})
            ON CONFLICT(${conflictColumns.join(', ')}) 
            DO UPDATE SET ${updateSet}
        `;

        return { query, params: values };
    }

    adaptQuery(query, params = []) {
        // PostgreSQL 파라미터 변환 (? → $1, $2, ...)
        let paramIndex = 1;
        const adaptedQuery = query.replace(/\?/g, () => `$${paramIndex++}`);
        
        return { query: adaptedQuery, params };
    }
}

/**
 * MS SQL Server 어댑터
 */
class MSSQLAdapter extends BaseAdapter {
    constructor() {
        super();
        this.autoIncrement = 'IDENTITY(1,1) PRIMARY KEY';
        this.uuid = 'UNIQUEIDENTIFIER';
        this.timestamp = 'DATETIME2 DEFAULT GETDATE()';
        this.boolean = 'BIT';
        this.text = 'NVARCHAR(MAX)';
        this.json = 'NVARCHAR(MAX)';
        this.parameterPlaceholder = '@param';
    }

    getTableExistsQuery(tableName) {
        return {
            query: `SELECT table_name FROM information_schema.tables WHERE table_name=@tableName`,
            params: { tableName }
        };
    }

    getColumnExistsQuery(tableName, columnName) {
        return {
            query: `SELECT column_name FROM information_schema.columns WHERE table_name=@tableName AND column_name=@columnName`,
            params: { tableName, columnName }
        };
    }

    upsertQuery(tableName, data, conflictColumns) {
        const columns = Object.keys(data);
        const updateSet = columns
            .filter(col => !conflictColumns.includes(col))
            .map(col => `${col} = @${col}`)
            .join(', ');

        const whereClause = conflictColumns
            .map(col => `${col} = @${col}`)
            .join(' AND ');

        const insertColumns = columns.join(', ');
        const insertValues = columns.map(col => `@${col}`).join(', ');

        const query = `
            MERGE ${tableName} AS target
            USING (SELECT ${insertValues}) AS source (${insertColumns})
            ON ${whereClause}
            WHEN MATCHED THEN
                UPDATE SET ${updateSet}
            WHEN NOT MATCHED THEN
                INSERT (${insertColumns}) VALUES (${insertValues});
        `;

        // 파라미터를 객체 형태로 변환
        const params = {};
        columns.forEach((col, i) => {
            params[col] = Object.values(data)[i];
        });

        return { query, params };
    }

    adaptQuery(query, params = []) {
        // MSSQL 특화 변환
        let adaptedQuery = query
            .replace(/SERIAL/g, 'IDENTITY(1,1)')
            .replace(/BOOLEAN/g, 'BIT')
            .replace(/TIMESTAMP WITH TIME ZONE/g, 'DATETIME2')
            .replace(/UUID/g, 'UNIQUEIDENTIFIER')
            .replace(/JSONB/g, 'NVARCHAR(MAX)')
            .replace(/TEXT/g, 'NVARCHAR(MAX)');

        // 파라미터 변환 (? → @param1, @param2, ...)
        if (Array.isArray(params)) {
            let paramIndex = 1;
            adaptedQuery = adaptedQuery.replace(/\?/g, () => `@param${paramIndex++}`);
            
            // 배열을 객체로 변환
            const paramObj = {};
            params.forEach((value, i) => {
                paramObj[`param${i + 1}`] = value;
            });
            
            return { query: adaptedQuery, params: paramObj };
        }

        return { query: adaptedQuery, params };
    }

    paginateQuery(baseQuery, limit, offset) {
        return `${baseQuery} ORDER BY (SELECT NULL) OFFSET ${offset} ROWS FETCH NEXT ${limit} ROWS ONLY`;
    }
}

/**
 * MariaDB/MySQL 어댑터
 */
class MariaDBAdapter extends BaseAdapter {
    constructor() {
        super();
        this.autoIncrement = 'INT AUTO_INCREMENT PRIMARY KEY';
        this.uuid = 'CHAR(36)';
        this.timestamp = 'TIMESTAMP DEFAULT CURRENT_TIMESTAMP';
        this.boolean = 'BOOLEAN';
        this.text = 'TEXT';
        this.json = 'JSON';
        this.parameterPlaceholder = '?';
    }

    getTableExistsQuery(tableName) {
        return {
            query: `SELECT table_name FROM information_schema.tables WHERE table_name=?`,
            params: [tableName]
        };
    }

    getColumnExistsQuery(tableName, columnName) {
        return {
            query: `SELECT column_name FROM information_schema.columns WHERE table_name=? AND column_name=?`,
            params: [tableName, columnName]
        };
    }

    upsertQuery(tableName, data, conflictColumns) {
        const columns = Object.keys(data);
        const values = Object.values(data);
        const placeholders = values.map(() => '?').join(', ');
        
        const updateSet = columns
            .filter(col => !conflictColumns.includes(col))
            .map(col => `${col} = VALUES(${col})`)
            .join(', ');

        const query = `
            INSERT INTO ${tableName} (${columns.join(', ')}) 
            VALUES (${placeholders})
            ON DUPLICATE KEY UPDATE ${updateSet}
        `;

        return { query, params: values };
    }

    adaptQuery(query, params = []) {
        // MariaDB/MySQL 특화 변환
        let adaptedQuery = query
            .replace(/SERIAL/g, 'INT AUTO_INCREMENT')
            .replace(/TIMESTAMP WITH TIME ZONE/g, 'TIMESTAMP')
            .replace(/UUID/g, 'CHAR(36)')
            .replace(/JSONB/g, 'JSON');

        return { query: adaptedQuery, params };
    }
}

module.exports = QueryAdapter;