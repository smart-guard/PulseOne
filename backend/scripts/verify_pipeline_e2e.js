
const axios = require('axios');

async function run() {
    try {
        const TENANT_ID = 1;
        const BASE_URL = 'http://localhost:3000';

        console.log('--- Pipeline E2E Verification Start ---');

        // 1. Create Virtual Point
        console.log('1. Creating Virtual Point (dependent on Point 3)...');
        const vpName = `E2E_Test_VP_${Date.now()}`;
        const vpRes = await axios.post(`${BASE_URL}/api/virtual-points`, {
            virtualPoint: {
                name: vpName,
                formula: 'motor_current + 10',
                calculation_trigger: 'onchange',
                scope_type: 'device',
                site_id: 1,
                device_id: 1,
                category: 'test'
            },
            inputs: [
                {
                    variable_name: 'motor_current',
                    source_type: 'data_point',
                    source_id: 3
                }
            ]
        }, { headers: { 'x-tenant-id': TENANT_ID } });

        if (!vpRes.data.success) throw new Error('VP Creation Failed: ' + JSON.stringify(vpRes.data));
        const vpId = vpRes.data.data.id;
        console.log(`✓ Virtual Point created. ID: ${vpId}`);

        // 2. Create Alarm Rule
        console.log('2. Creating Alarm Rule for Virtual Point...');
        const ruleName = `E2E_Test_Rule_${Date.now()}`;
        const ruleRes = await axios.post(`${BASE_URL}/api/alarms/rules`, {
            name: ruleName,
            target_type: 'virtual_point',
            target_id: vpId,
            alarm_type: 'analog',
            high_limit: 40,
            severity: 'high',
            is_enabled: 1
        }, { headers: { 'x-tenant-id': TENANT_ID } });

        if (!ruleRes.data.success) throw new Error('Rule Creation Failed: ' + JSON.stringify(ruleRes.data));
        const ruleId = ruleRes.data.data.id;
        console.log(`✓ Alarm Rule created. ID: ${ruleId}`);

        // 3. Trigger Collector Reload
        console.log('3. Triggering Collector Config Reload...');
        const reloadRes = await axios.post(`${BASE_URL}/api/collector/config/reload`, {}, { headers: { 'x-tenant-id': TENANT_ID } });
        if (!reloadRes.data.success) throw new Error('Reload Failed: ' + JSON.stringify(reloadRes.data));
        console.log('✓ Collector reload triggered.');

        console.log('\n--- Waiting for 10 seconds for calculation and alarm evaluation ---');
        await new Promise(resolve => setTimeout(resolve, 10000));

        console.log('\n--- Verification Commands ---');
        console.log(`Redis Value: docker exec docker-redis-1 redis-cli GET "virtualpoint:${vpId}"`);
        console.log(`Alarm Check: docker exec docker-collector-1 cat /app/core/collector/logs/$(date +%Y%m%d)/alarm.log | grep -A 5 "Evaluating Point ${vpId}"`);

    } catch (e) {
        console.error('❌ ERROR:', e.message);
        if (e.response) {
            console.error('Response Status:', e.response.status);
            console.error('Response Data:', JSON.stringify(e.response.data));
        }
    }
}

run();
