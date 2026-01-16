// backend/lib/services/DatabaseExplorerService.js
const DatabaseFactory = require('../database/DatabaseFactory');
const LogManager = require('../utils/LogManager');

class DatabaseExplorerService {
    constructor() {
        this.dbFactory = new DatabaseFactory();
        this.logger = LogManager.getInstance();
        this.dbType = this.dbFactory.config.database.type;
        this.adapter = this.dbFactory.getQueryAdapter();
    }

    /**
     * 모든 테이블 목록 조회
     */
    async listTables() {
        try {
            const dbPath = await this.getDatabasePath();
            this.logger.debug('DatabaseExplorer', `Listing tables from: ${dbPath} (Type: ${this.dbType})`);

            let query;
            if (this.dbType === 'sqlite' || this.dbType === 'SQLITE') {
                query = "SELECT name, 'table' as type FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'";
            } else if (this.dbType === 'postgres' || this.dbType === 'postgresql') {
                query = "SELECT table_name as name, 'table' as type FROM information_schema.tables WHERE table_schema = 'public'";
            } else {
                // MySQL/MariaDB
                query = "SELECT table_name as name, 'table' as type FROM information_schema.tables WHERE table_schema = DATABASE()";
            }

            const result = await this.dbFactory.executeQuery(query);
            this.logger.debug('DatabaseExplorer', `Found ${Array.isArray(result) ? result.length : (result.rows?.length || 0)} tables`);

            // SQLite returns { rows: [] }, others might return []
            const rows = Array.isArray(result) ? result : (result.rows || []);

            return rows.map(r => ({ name: r.name, type: r.type }));
        } catch (error) {
            this.logger.error('DatabaseExplorer', `List tables failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 테이블 스키마 조회 (컬럼 정보)
     */
    async getTableSchema(tableName) {
        try {
            // 테이블 존재 확인
            const exists = await this.tableExists(tableName);
            if (!exists) {
                throw new Error(`Table ${tableName} not found`);
            }

            let columns = [];

            if (this.dbType === 'sqlite' || this.dbType === 'SQLITE') {
                const info = await this.dbFactory.executeQuery(`PRAGMA table_info(${tableName})`);
                const rows = Array.isArray(info) ? info : (info.rows || []);

                columns = rows.map(col => ({
                    name: col.name,
                    type: col.type,
                    notNull: col.notnull === 1,
                    pk: col.pk === 1,
                    defaultValue: col.dflt_value
                }));
            } else if (this.dbType === 'postgres' || this.dbType === 'postgresql') {
                const query = `
                    SELECT column_name, data_type, is_nullable, column_default 
                    FROM information_schema.columns 
                    WHERE table_name = $1 AND table_schema = 'public'
                `;
                const info = await this.dbFactory.executeQuery(query, [tableName]);
                const rows = Array.isArray(info) ? info : (info.rows || []);

                columns = rows.map(col => ({
                    name: col.column_name,
                    type: col.data_type,
                    notNull: col.is_nullable === 'NO',
                    pk: false, // 별도 쿼리 필요하지만 간소화를 위해 일단 생략하거나 추후 보강
                    defaultValue: col.column_default
                }));
                // PostgreSQL PK 별도 조회 로직은 복잡도를 위해 생략, 필요시 추가
            } else {
                // MySQL
                const info = await this.dbFactory.executeQuery(`DESCRIBE ${tableName}`);
                const rows = Array.isArray(info) ? info : (info.rows || []);

                columns = rows.map(col => ({
                    name: col.Field,
                    type: col.Type,
                    notNull: col.Null === 'NO',
                    pk: col.Key === 'PRI',
                    defaultValue: col.Default
                }));
            }

            return columns;
        } catch (error) {
            this.logger.error('DatabaseExplorer', `Get schema failed for ${tableName}: ${error.message}`);
            throw error;
        }
    }

    /**
     * 테이블 데이터 조회 (페이징/정렬)
     */
    async getTableData(tableName, { page = 1, limit = 50, sortColumn = null, sortOrder = 'ASC' }) {
        try {
            const offset = (page - 1) * limit;

            // 안전한 쿼리 구성을 위해 식별자 검증/이스케이프가 이상적이지만, 
            // Admin 전용 기능이므로 간단한 검증만 수행
            if (!/^[a-zA-Z0-9_]+$/.test(tableName)) throw new Error('Invalid table name');
            if (sortColumn && !/^[a-zA-Z0-9_]+$/.test(sortColumn)) throw new Error('Invalid sort column');

            // 1. 전체 카운트 조회
            const countRes = await this.dbFactory.executeQuery(`SELECT COUNT(*) as count FROM ${tableName}`);
            const total = (Array.isArray(countRes) ? countRes[0] : (countRes.rows || [])[0]).count;

            // 2. 데이터 조회
            let query = `SELECT * FROM ${tableName}`;

            if (sortColumn) {
                query += ` ORDER BY ${sortColumn} ${sortOrder === 'DESC' ? 'DESC' : 'ASC'}`;
            } else {
                // 기본 PK 정렬을 시도하거나, 없으면 그대로
                // SQLite의 경우 rowid가 있을 수 있음
                if (this.dbType === 'sqlite' || this.dbType === 'SQLITE') {
                    // query += ' ORDER BY rowid DESC'; // 항상 rowid가 있는건 아니므로 생략
                }
            }

            // 페이징
            if (this.dbType === 'postgres' || this.dbType === 'postgresql' || this.dbType === 'sqlite' || this.dbType === 'SQLITE') {
                query += ` LIMIT ${limit} OFFSET ${offset}`;
            } else {
                query += ` LIMIT ${offset}, ${limit}`;
            }

            const result = await this.dbFactory.executeQuery(query);
            const items = Array.isArray(result) ? result : (result.rows || []);

            return {
                items,
                pagination: {
                    total: parseInt(total),
                    page: parseInt(page),
                    limit: parseInt(limit),
                    totalPages: Math.ceil(total / limit)
                }
            };
        } catch (error) {
            this.logger.error('DatabaseExplorer', `Get data failed for ${tableName}: ${error.message}`);
            throw error;
        }
    }

    /**
     * 데이터 수정 (Update)
     */
    async updateRow(tableName, pkColumn, pkValue, updates) {
        try {
            if (!/^[a-zA-Z0-9_]+$/.test(tableName)) throw new Error('Invalid table name');
            if (!/^[a-zA-Z0-9_]+$/.test(pkColumn)) throw new Error('Invalid PK column');
            if (Object.keys(updates).length === 0) return { changes: 0 };

            const setClauses = [];
            const params = [];

            Object.entries(updates).forEach(([key, value]) => {
                setClauses.push(`${key} = ?`);
                params.push(value);
            });

            // PostgreSQL용 파라미터 처리 ($1, $2...) 로직이 필요하다면 dbFactory.getQueryAdapter() 사용
            // 현재 dbFactory.executeQuery가 ?를 지원한다고 가정 (BaseRepository 참조)
            let query = `UPDATE ${tableName} SET ${setClauses.join(', ')} WHERE ${pkColumn} = ?`;
            params.push(pkValue);

            // DB 타입에 따른 쿼리 변환 (필요시)
            query = this.adapter.adaptQuery(query);

            const result = await this.dbFactory.executeQuery(query, params);

            // 결과 표준화
            let changes = 0;
            if (result && typeof result.changes === 'number') changes = result.changes; // sqlite
            else if (result && typeof result.affectedRows === 'number') changes = result.affectedRows; // mysql
            else if (result && typeof result.rowCount === 'number') changes = result.rowCount; // postgres

            return { changes };
        } catch (error) {
            this.logger.error('DatabaseExplorer', `Update failed for ${tableName}: ${error.message}`);
            throw error;
        }
    }

    /**
     * 데이터 삭제 (Delete)
     */
    async deleteRow(tableName, pkColumn, pkValue) {
        try {
            if (!/^[a-zA-Z0-9_]+$/.test(tableName)) throw new Error('Invalid table name');
            if (!/^[a-zA-Z0-9_]+$/.test(pkColumn)) throw new Error('Invalid PK column');

            let query = `DELETE FROM ${tableName} WHERE ${pkColumn} = ?`;
            query = this.adapter.adaptQuery(query);

            const result = await this.dbFactory.executeQuery(query, [pkValue]);

            let changes = 0;
            if (result && typeof result.changes === 'number') changes = result.changes;
            else if (result && typeof result.affectedRows === 'number') changes = result.affectedRows;
            else if (result && typeof result.rowCount === 'number') changes = result.rowCount;

            return { changes };
        } catch (error) {
            this.logger.error('DatabaseExplorer', `Delete failed for ${tableName}: ${error.message}`);
            throw error;
        }
    }

    async getDatabasePath() {
        const config = this.dbFactory.config;
        const type = this.dbType || 'sqlite';

        if (type.toLowerCase().includes('sqlite')) {
            return config.sqlite.path;
        } else {
            // RDBMS
            const typeConfig = config[type.toLowerCase()] || {};
            // Hide password
            return `${type}://${typeConfig.host || 'localhost'}:${typeConfig.port}/${typeConfig.database}`;
        }
    }

    async tableExists(tableName) {
        try {
            let query;
            if (this.dbType === 'sqlite' || this.dbType === 'SQLITE') {
                query = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
            } else if (this.dbType === 'postgres' || this.dbType === 'postgresql') {
                query = "SELECT table_name FROM information_schema.tables WHERE table_name=$1";
            } else {
                query = "SELECT table_name FROM information_schema.tables WHERE table_name=?";
            }

            const result = await this.dbFactory.executeQuery(query, [tableName]);
            const rows = Array.isArray(result) ? result : (result.rows || []);
            return rows.length > 0;
        } catch (error) {
            return false;
        }
    }
}

module.exports = DatabaseExplorerService;
