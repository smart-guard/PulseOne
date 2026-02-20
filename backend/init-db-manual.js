const SchemaManager = require('./lib/database/schemaManager');
const { createSQLiteConnection } = require('./lib/connection/db');

async function run() {
    try {
        console.log('🚀 Starting Manual Schema Initialization...');
        const db = createSQLiteConnection();
        const schemaManager = new SchemaManager(db);

        // initializeDatabase() 가 내부적으로 테이블을 생성함
        await schemaManager.initializeDatabase();

        console.log('✅ Schema Initialization Complete!');
        process.exit(0);
    } catch (error) {
        console.error('❌ Schema Initialization Failed:', error);
        process.exit(1);
    }
}

run();
