// =============================================================================
// lib/connection/db.js - PulseOne 데이터베이스 연결 매니저 (연결만!)
// =============================================================================
const path = require('path');
const fs = require('fs');
const dotenv = require('dotenv');

// =============================================================================
// 환경변수 로딩 (최초 1회만)
// =============================================================================

// 1. 메인 .env 파일 로드
const mainEnvPath = path.join(__dirname, '../../../config/.env');
if (fs.existsSync(mainEnvPath)) {
  dotenv.config({ path: mainEnvPath });
  console.log('✅ 메인 설정 로드: .env');
}

// 2. 추가 환경 파일들 자동 탐지 및 로드
const configDir = path.join(__dirname, '../../../config');
try {
  const configFiles = fs.readdirSync(configDir)
    .filter(file => file.endsWith('.env') && file !== '.env')
    .sort();

  configFiles.forEach(file => {
    const envPath = path.join(configDir, file);
    dotenv.config({ path: envPath });
    console.log(`✅ 추가 설정 로드: ${file}`);
  });
} catch (error) {
  console.warn('⚠️ config 디렉토리 읽기 실패:', error.message);
}

// =============================================================================
// 데이터베이스 연결 클래스 (연결만 담당!)
// =============================================================================

class DatabaseConnection {
  constructor() {
    // 환경변수 한 번만 읽기
    this.dbType = process.env.DATABASE_TYPE;
    this.config = this._loadConfig();
    this.connection = null;
    this.isConnected = false;

    console.log(`🔗 데이터베이스 타입: ${this.dbType}`);
  }

  _loadConfig() {
    const dbType = this.dbType?.toLowerCase();

    switch (dbType) {
      case 'sqlite':
      case 'sqlite3':
        return {
          type: 'sqlite',
          path: process.env.SQLITE_PATH,
          journalMode: process.env.SQLITE_JOURNAL_MODE,
          cacheSize: parseInt(process.env.SQLITE_CACHE_SIZE),
          busyTimeout: parseInt(process.env.SQLITE_BUSY_TIMEOUT),
          foreignKeys: process.env.SQLITE_FOREIGN_KEYS === 'true'
        };

      case 'postgres':
      case 'postgresql':
        return {
          type: 'postgres',
          host: process.env.POSTGRES_PRIMARY_HOST,
          port: parseInt(process.env.POSTGRES_PRIMARY_PORT),
          database: process.env.POSTGRES_PRIMARY_DB,
          user: process.env.POSTGRES_PRIMARY_USER,
          password: process.env.POSTGRES_PRIMARY_PASSWORD
        };

      case 'mysql':
      case 'mariadb':
        return {
          type: 'mysql',
          host: process.env.MYSQL_HOST || process.env.MARIADB_HOST,
          port: parseInt(process.env.MYSQL_PORT || process.env.MARIADB_PORT || '3306'),
          database: process.env.MYSQL_DATABASE || process.env.MARIADB_DATABASE,
          user: process.env.MYSQL_USER || process.env.MARIADB_USER,
          password: process.env.MYSQL_PASSWORD || process.env.MARIADB_PASSWORD,
          charset: process.env.MYSQL_CHARSET || 'utf8mb4',
          timezone: process.env.MYSQL_TIMEZONE || 'local'
        };

      case 'mssql':
      case 'sqlserver':
        return {
          type: 'mssql',
          server: process.env.MSSQL_SERVER,
          port: parseInt(process.env.MSSQL_PORT || '1433'),
          database: process.env.MSSQL_DATABASE,
          user: process.env.MSSQL_USER,
          password: process.env.MSSQL_PASSWORD,
          options: {
            encrypt: process.env.MSSQL_ENCRYPT === 'true',
            trustServerCertificate: process.env.MSSQL_TRUST_CERT === 'true'
          }
        };

      case 'oracle':
        return {
          type: 'oracle',
          connectString: process.env.ORACLE_CONNECT_STRING,
          user: process.env.ORACLE_USER,
          password: process.env.ORACLE_PASSWORD,
          poolMin: parseInt(process.env.ORACLE_POOL_MIN || '1'),
          poolMax: parseInt(process.env.ORACLE_POOL_MAX || '10')
        };

      default:
        throw new Error(`지원하지 않는 데이터베이스 타입: ${dbType} (지원: sqlite, postgres, mysql, mariadb, mssql, oracle)`);
    }
  }

  async connect() {
    if (this.isConnected) {
      return this.connection; // 이미 연결됨
    }

    switch (this.config.type) {
      case 'sqlite':
        return await this._connectSQLite();
      case 'postgres':
        return await this._connectPostgres();
      case 'mysql':
        return await this._connectMySQL();
      case 'mssql':
        return await this._connectMSSQL();
      case 'oracle':
        return await this._connectOracle();
      default:
        throw new Error(`연결 방법을 알 수 없는 타입: ${this.config.type}`);
    }
  }

  async _connectSQLite() {
    return new Promise((resolve, reject) => {
      // 파일 존재 여부 확인
      if (!fs.existsSync(this.config.path)) {
        const error = new Error(`SQLite 파일이 없습니다: ${this.config.path}`);
        console.error('❌', error.message);
        console.log('💡 해결방법: npm run db:init 실행');
        reject(error);
        return;
      }

      try {
        const sqlite3 = require('sqlite3').verbose();
        
        this.connection = new sqlite3.Database(this.config.path, sqlite3.OPEN_READWRITE, (err) => {
          if (err) {
            console.error('❌ SQLite 연결 실패:', err.message);
            reject(err);
          } else {
            console.log('✅ SQLite 데이터베이스 연결 성공');
            this.isConnected = true;
            this._applySQLitePragmas();
            resolve(this.connection);
          }
        });
      } catch (error) {
        console.error('❌ sqlite3 모듈 로드 실패. npm install sqlite3 필요');
        reject(error);
      }
    });
  }

  _applySQLitePragmas() {
    // PRAGMA 설정 적용
    if (this.config.journalMode) {
      this.connection.run(`PRAGMA journal_mode = ${this.config.journalMode}`);
    }
    if (this.config.cacheSize) {
      this.connection.run(`PRAGMA cache_size = ${this.config.cacheSize}`);
    }
    if (this.config.busyTimeout) {
      this.connection.run(`PRAGMA busy_timeout = ${this.config.busyTimeout}`);
    }
    if (this.config.foreignKeys) {
      this.connection.run(`PRAGMA foreign_keys = ON`);
    }
    console.log('✅ SQLite PRAGMA 설정 적용 완료');
  }

  async _connectPostgres() {
    try {
      const { Client } = require('pg');
      
      this.connection = new Client(this.config);
      await this.connection.connect();
      
      console.log('✅ PostgreSQL 연결 성공');
      this.isConnected = true;
      return this.connection;
      
    } catch (error) {
      console.error('❌ PostgreSQL 연결 실패:', error.message);
      throw error;
    }
  }

  async _connectMySQL() {
    try {
      const mysql = require('mysql2/promise');
      
      this.connection = await mysql.createConnection({
        host: this.config.host,
        port: this.config.port,
        database: this.config.database,
        user: this.config.user,
        password: this.config.password,
        charset: this.config.charset,
        timezone: this.config.timezone
      });
      
      console.log('✅ MySQL/MariaDB 연결 성공');
      this.isConnected = true;
      return this.connection;
      
    } catch (error) {
      console.error('❌ MySQL/MariaDB 연결 실패:', error.message);
      throw error;
    }
  }

  async _connectMSSQL() {
    try {
      const sql = require('mssql');
      
      this.connection = await sql.connect({
        server: this.config.server,
        port: this.config.port,
        database: this.config.database,
        user: this.config.user,
        password: this.config.password,
        options: this.config.options
      });
      
      console.log('✅ MS SQL Server 연결 성공');
      this.isConnected = true;
      return this.connection;
      
    } catch (error) {
      console.error('❌ MS SQL Server 연결 실패:', error.message);
      throw error;
    }
  }

  async _connectOracle() {
    try {
      const oracledb = require('oracledb');
      
      this.connection = await oracledb.getConnection({
        connectString: this.config.connectString,
        user: this.config.user,
        password: this.config.password
      });
      
      console.log('✅ Oracle 데이터베이스 연결 성공');
      this.isConnected = true;
      return this.connection;
      
    } catch (error) {
      console.error('❌ Oracle 연결 실패:', error.message);
      throw error;
    }
  }

  async close() {
    if (!this.isConnected || !this.connection) {
      return;
    }

    switch (this.config.type) {
      case 'sqlite':
        return new Promise((resolve, reject) => {
          this.connection.close((err) => {
            if (err) {
              reject(err);
            } else {
              console.log('📴 SQLite 연결 종료');
              this.isConnected = false;
              resolve();
            }
          });
        });

      case 'postgres':
        await this.connection.end();
        console.log('📴 PostgreSQL 연결 종료');
        this.isConnected = false;
        break;

      case 'mysql':
        await this.connection.end();
        console.log('📴 MySQL/MariaDB 연결 종료');
        this.isConnected = false;
        break;

      case 'mssql':
        await this.connection.close();
        console.log('📴 MS SQL Server 연결 종료');
        this.isConnected = false;
        break;

      case 'oracle':
        await this.connection.close();
        console.log('📴 Oracle 연결 종료');
        this.isConnected = false;
        break;
    }
  }

  // ==========================================================================
  // 연결 정보 조회 (디버깅용)
  // ==========================================================================

  getConnectionInfo() {
    return {
      type: this.config.type,
      isConnected: this.isConnected,
      config: { ...this.config, password: '***' } // 비밀번호 마스킹
    };
  }

  async testConnection() {
    try {
      await this.connect();
      console.log('✅ 데이터베이스 연결 테스트 성공');
      return true;
    } catch (error) {
      console.error('❌ 데이터베이스 연결 테스트 실패:', error.message);
      return false;
    }
  }
}

// =============================================================================
// 싱글톤 인스턴스 생성 및 내보내기
// =============================================================================

const dbConnection = new DatabaseConnection();

// 디버깅 정보 (개발 환경에서만)
if (process.env.NODE_ENV === 'development') {
  console.log('\n📋 데이터베이스 연결 설정:');
  const info = dbConnection.getConnectionInfo();
  console.log(`   타입: ${info.type}`);
  
  if (info.type === 'sqlite') {
    console.log(`   파일: ${info.config.path}`);
  } else if (info.type === 'postgres') {
    console.log(`   호스트: ${info.config.host}:${info.config.port}`);
    console.log(`   데이터베이스: ${info.config.database}`);
  }
  console.log('');
}

// 싱글톤 인스턴스 내보내기
module.exports = dbConnection;