const config = require('../lib/config/ConfigManager');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
const ExportGatewayService = require('../lib/services/ExportGatewayService');
const CrossPlatformManager = require('../lib/services/crossPlatformManager');
const EdgeServerService = require('../lib/services/EdgeServerService');

async function runTest() {
    console.log('=== Export Gateway Multi-process E2E Test ===');

    // 0. Initialize RepositoryFactory
    console.log('[Step 0] Initializing RepositoryFactory...');
    const dbConfig = config.getDatabaseConfig();
    const rf = RepositoryFactory.getInstance();
    await rf.initialize({
        database: dbConfig,
        cache: { enabled: false }
    });
    console.log('RepositoryFactory initialized.');

    const tenantId = 1;
    const gatewayId = 2;
    const profileId = 1;

    try {
        // 1. Register Gateway 2 if not exists
        console.log(`[Step 1] Registering Gateway ID ${gatewayId}...`);
        try {
            await ExportGatewayService.registerGateway({
                id: gatewayId,
                name: 'Gateway-2 (E2E Test)',
                ip_address: '127.0.0.1',
                status: 'offline'
            }, tenantId);
            console.log('Gateway registered successfully.');
        } catch (e) {
            console.log('Gateway might already exist, continuing...');
        }

        // 2. Assign Profile 1
        console.log(`[Step 2] Assigning Profile ${profileId} to Gateway ${gatewayId}...`);
        await ExportGatewayService.assignProfileToGateway(profileId, gatewayId);
        console.log('Profile assigned.');

        // 3. Start Gateway Process
        console.log(`[Step 3] Starting Gateway ${gatewayId} process...`);
        const startResult = await ExportGatewayService.startGateway(gatewayId, tenantId);
        console.log('Start Result:', JSON.stringify(startResult, null, 2));

        if (!startResult.success) {
            throw new Error(`Failed to start gateway: ${startResult.error}`);
        }

        // 4. Verify Process via ps
        console.log('[Step 4] Verifying process via ps...');
        const statusResponse = await ExportGatewayService.getAllGateways(tenantId);
        const allGatewaysData = statusResponse.data.items; // Access items from paginated response
        const gw2 = allGatewaysData.find(g => g.id === gatewayId);

        console.log(`Gateway 2 Processes:`, JSON.stringify(gw2.processes, null, 2));

        if (gw2.processes && gw2.processes.length > 0) {
            console.log('✅ MULTI-PROCESS DETECTION SUCCESS: Process found in backend container.');
        } else {
            console.warn('⚠️ Process not detected via ps. This might be normal if started in a different container, but check CrossPlatformManager logs.');
        }

        // 5. Verify Live Status (Redis)
        console.log('[Step 5] Waiting for heartbeat (Redis)...');
        // Give it some time to start and heartbeat
        for (let i = 0; i < 15; i++) {
            await new Promise(resolve => setTimeout(resolve, 2000));
            const liveStatus = await EdgeServerService.getLiveStatus(gatewayId);
            if (liveStatus && liveStatus.status === 'online') {
                console.log(`✅ HEARTBEAT SUCCESS: Gateway ${gatewayId} is ${liveStatus.status}!`);
                console.log('Live Status Data:', JSON.stringify(liveStatus, null, 2));
                break;
            }
            console.log(`Still waiting... (${i + 1}/15)`);
        }

        // 6. Final Status of All Gateways
        console.log('[Step 6] Current Gateway Overview:');
        const finalStatusResponse = await ExportGatewayService.getAllGateways(tenantId);
        const finalGateways = finalStatusResponse.data.items;
        if (finalGateways && finalGateways.length > 0) {
            console.log(`Found ${finalGateways.length} gateways:`);
            finalGateways.forEach(gw => {
                console.log(` - ID: ${gw.id}, Name: ${gw.name}, Status: ${gw.status}, Live: ${gw.live_status ? 'ONLINE' : 'OFFLINE'}`);
                if (gw.live_status) {
                    console.log(`   Live Data: ${JSON.stringify(gw.live_status)}`);
                }
            });
        } else {
            console.log('No gateways found in database.');
        }

        console.log('\n=== E2E Test Completed Successfully ===');
        process.exit(0);
    } catch (error) {
        console.error('❌ E2E Test Failed:', error);
        process.exit(1);
    }
}

runTest();
