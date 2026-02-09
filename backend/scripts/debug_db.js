
const ConfigManager = require('../lib/config/ConfigManager');
const DatabaseFactory = require('../lib/database/DatabaseFactory');

async function debug() {
    try {
        console.log("--- Debugging DB ---");
        const configManager = ConfigManager.getInstance();
        console.log("SQLITE_PATH (Config):", configManager.get('SQLITE_PATH'));

        const dbConfig = configManager.getDatabaseConfig();
        console.log("SQLITE_PATH (Resolved):", dbConfig.sqlite.path);

        const dbFactory = new DatabaseFactory();

        // 1. Check Table Exists
        const checkTable = "SELECT name FROM sqlite_master WHERE type='table' AND name='export_targets'";
        const tableRes = await dbFactory.executeQuery(checkTable);
        const tableExists = Array.isArray(tableRes) ? tableRes.length > 0 : (tableRes.rows || []).length > 0;
        console.log("Table 'export_targets' exists:", tableExists);

        if (tableExists) {
            // 2. Count Rows
            const countRes = await dbFactory.executeQuery("SELECT COUNT(*) as count FROM export_targets");
            const count = (Array.isArray(countRes) ? countRes[0] : (countRes.rows || [])[0]).count;
            console.log("Row Count:", count);

            // 3. Select Rows
            const rows = await dbFactory.executeQuery("SELECT * FROM export_targets");
            console.log("Rows:", JSON.stringify(rows, null, 2));
        }

        await dbFactory.closeAllConnections();

    } catch (e) {
        console.error("Error:", e);
    }
}

debug();
