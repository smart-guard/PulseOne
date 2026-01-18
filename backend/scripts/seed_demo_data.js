
const path = require('path');
const knex = require('knex');

// HARDCODED PATH for reliability in this script
const DB_PATH = path.join(__dirname, '../../data/db/pulseone.db');

async function seed() {
    console.log('🌱 Seeding demo data for Virtual Points feature verification...');
    console.log(`📂 Using Database: ${DB_PATH}`);

    const db = knex({
        client: 'sqlite3',
        connection: {
            filename: DB_PATH
        },
        useNullAsDefault: true
    });

    try {
        // 1. Find a target virtual point (prefer named "평균" or just the first one)
        let vp = await db('virtual_points').where('name', 'like', '%평균%').first();
        if (!vp) {
            vp = await db('virtual_points').first();
        }

        if (!vp) {
            console.error('❌ No virtual points found. Please create one first via the UI.');
            // Creating a dummy one if none exists
            const [newId] = await db('virtual_points').insert({
                name: 'Test Virtual Point',
                description: 'Created by seed script',
                formula: 'return 0;',
                params_json: '[]',
                variable_mapping_json: '[]',
                tenant_id: 1,
                is_enabled: 1,
                created_at: new Date(),
                updated_at: new Date()
            });
            vp = { id: newId, name: 'Test Virtual Point' };
            console.log('✨ Created a new mock virtual point.');
        }

        console.log(`🎯 Target Virtual Point: [${vp.id}] ${vp.name}`);

        // 2. Update Formula for Dependency Map
        // We want to show some inputs like inputs.temp_A, inputs.temp_B
        const newFormula = "return (inputs.temp_A + inputs.temp_B) / 2;";
        // Also update expression which is what logic studio uses often
        await db('virtual_points')
            .where('id', vp.id)
            .update({ formula: newFormula, expression: newFormula });
        console.log(`✅ Updated formula to: ${newFormula}`);

        // 3. Clear existing logs for this point
        await db('virtual_point_logs').where('point_id', vp.id).delete();

        // 4. Insert Mock Audit Logs
        const logs = [
            {
                point_id: vp.id,
                action: 'CREATE',
                details: '가상포인트 생성됨 (Initial creation)',
                created_at: new Date(Date.now() - 86400000 * 2).toISOString() // 2 days ago
            },
            {
                point_id: vp.id,
                action: 'UPDATE',
                details: '실행 주기 변경: 500ms -> 1000ms',
                created_at: new Date(Date.now() - 86400000).toISOString() // 1 day ago
            },
            {
                point_id: vp.id,
                action: 'DISABLE',
                details: '연산 일시 정지 (Maintenance)',
                created_at: new Date(Date.now() - 18000000).toISOString() // 5 hours ago
            },
            {
                point_id: vp.id,
                action: 'ENABLE',
                details: '연산 재개 (Resumed)',
                created_at: new Date(Date.now() - 3600000).toISOString() // 1 hour ago
            },
            {
                point_id: vp.id,
                action: 'UPDATE',
                details: '수식 변경: (temp_A + temp_B) / 2',
                created_at: new Date().toISOString() // Now
            }
        ];

        await db('virtual_point_logs').insert(logs);
        console.log(`✅ Inserted ${logs.length} audit logs.`);

    } catch (error) {
        console.error('❌ Error during seeding:', error);
    } finally {
        await db.destroy();
        process.exit(0);
    }
}

seed();
