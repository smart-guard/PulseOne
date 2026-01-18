
const axios = require('axios');

const API_URL = 'http://localhost:3000/api/virtual-points';
const TENANT_ID = 1; // Assuming default tenant

// Helper for formatted logging
const log = (step, msg, data = '') => {
    console.log(`[${step}] ${msg}`, data ? JSON.stringify(data, null, 2) : '');
};

const assert = (condition, msg) => {
    if (!condition) {
        console.error(`❌ ASSERTION FAILED: ${msg}`);
        // Log only the data part if available, safely
        const debugData = arguments[2];
        if (debugData) {
            // Check if it looks like an axios response
            if (debugData.data && debugData.status) {
                console.error('Response Status:', debugData.status);
                console.error('Response Data:', JSON.stringify(debugData.data, null, 2));
            } else {
                try {
                    console.error('Debug Data:', JSON.stringify(debugData, null, 2));
                } catch (e) { console.error('Debug Data: [Unstringifiable]'); }
            }
        }
        process.exit(1);
    }
    console.log(`✅ ${msg}`);
};

async function runVerification() {
    console.log('🚀 Starting Full Lifecycle Verification for Virtual Points...\n');
    let pointId = null;

    try {
        // 1. CREATE
        const createPayload = {
            name: `AUTO_TEST_${Date.now()}`,
            description: 'Created by verification script',
            formula: 'return 100;',
            script_library_id: null,
            tags: [],
            input_variables: [],
            log_enabled: true
        };

        log('1. CREATE', 'Creating new Virtual Point...');
        const payloadWrapper = { virtualPoint: createPayload };
        const createRes = await axios.post(API_URL, payloadWrapper, { headers: { 'x-tenant-id': TENANT_ID } });
        assert(createRes.status === 201, 'Create status is 201');
        assert(createRes.data.success === true, 'Create response success is true');
        pointId = createRes.data.data.id;
        assert(pointId, `Created Point ID: ${pointId}`);


        // 2. READ (List)
        log('2. READ', 'Fetching list to verify creation...');
        const listRes = await axios.get(API_URL, { headers: { 'x-tenant-id': TENANT_ID } });
        // API returns data as array directly
        const listItems = Array.isArray(listRes.data.data) ? listRes.data.data : listRes.data.data.items;
        assert(listItems, 'List items found');
        const foundInList = listItems.find(p => p.id === pointId);
        assert(foundInList, 'Newly created point found in list');
        assert(foundInList.name === createPayload.name, 'Name matches in list');


        // 3. READ (Detail)
        log('3. READ', 'Fetching detail view...');
        const detailRes = await axios.get(`${API_URL}/${pointId}`, { headers: { 'x-tenant-id': TENANT_ID } });
        const detail = detailRes.data;
        assert(detail.formula === createPayload.formula, 'Formula matches in detail');


        // 4. UPDATE
        log('4. UPDATE', 'Updating name and formula...');
        const updatePayload = {
            name: `${createPayload.name}_UPDATED`,
            formula: 'return 200;'
        };
        const updateRes = await axios.put(`${API_URL}/${pointId}`, updatePayload, { headers: { 'x-tenant-id': TENANT_ID } });
        assert(updateRes.data.success === true, 'Update successful');

        // Check if update persisted
        const verifyUpdate = await axios.get(`${API_URL}/${pointId}`, { headers: { 'x-tenant-id': TENANT_ID } });
        assert(verifyUpdate.data.name === updatePayload.name, 'Deep check: Name updated');
        assert(verifyUpdate.data.formula === updatePayload.formula, 'Deep check: Formula updated');


        // 5. TOGGLE (Enable/Disable)
        log('5. TOGGLE', 'Toggling enabled status...');
        const toggleRes = await axios.patch(`${API_URL}/${pointId}/toggle`, {}, { headers: { 'x-tenant-id': TENANT_ID } });
        assert(toggleRes.data.success === true, 'Toggle successful');

        const verifyToggle = await axios.get(`${API_URL}/${pointId}`, { headers: { 'x-tenant-id': TENANT_ID } });
        assert(verifyToggle.data.is_enabled !== true, 'Deep check: Status toggled (should be false/0)');


        // 6. AUDIT LOGS
        log('6. AUDIT', 'Verifying audit logs generated...');
        // Logs endpoint wrapper removal verification
        const logsRes = await axios.get(`${API_URL}/${pointId}/logs`, { headers: { 'x-tenant-id': TENANT_ID } });

        // Check structure (should be array directly in .data)
        const logs = logsRes.data.success ? logsRes.data.data : [];
        assert(Array.isArray(logs), 'Logs data is an array');
        assert(logs.length >= 2, `Logs found: ${logs.length} (Expected at least Create & Update)`);

        const hasCreateLog = logs.some(l => l.action === 'CREATE');
        const hasUpdateLog = logs.some(l => l.action === 'UPDATE');
        assert(hasCreateLog, 'Found CREATE log entry');
        assert(hasUpdateLog, 'Found UPDATE log entry');


        // 7. DELETE (Soft Delete)
        log('7. DELETE', 'Soft deleting the point...');
        const deleteRes = await axios.delete(`${API_URL}/${pointId}`, { headers: { 'x-tenant-id': TENANT_ID } });
        assert(deleteRes.data.success === true, 'Delete successful');

        // Verify it's gone from main list
        const listAfterDelete = await axios.get(API_URL, { headers: { 'x-tenant-id': TENANT_ID } });
        const foundAfterDelete = listAfterDelete.data.data.items.find(p => p.id === pointId);
        assert(!foundAfterDelete, 'Point removed from main list');

        // Verify it exists in trash (show_deleted=true)
        const trashList = await axios.get(`${API_URL}?show_deleted=true`, { headers: { 'x-tenant-id': TENANT_ID } });
        const trashItems = Array.isArray(trashList.data.data) ? trashList.data.data : trashList.data.data.items;
        const foundInTrash = trashItems.find(p => p.id === pointId);
        assert(foundInTrash, 'Point found in trash (Soft Delete verified)');


        console.log('\n✨ ALL CHECKS PASSED SUCCESSFULLY! The system is solid. ✨');

    } catch (error) {
        console.error('\n❌ VERIFICATION FAILED');
        if (error.response) {
            console.error('Status:', error.response.status);
            console.error('Status:', error.response.status);
            // Sanitize response data logging
            try {
                const dataStr = JSON.stringify(error.response.data, null, 2);
                if (dataStr.length > 5000) console.error('Data (truncated):', dataStr.substring(0, 5000) + '...');
                else console.error('Data:', dataStr);
            } catch (e) { console.error('Data: [Unstringifiable]'); }
        } else {
            console.error(error.message);
        }
        process.exit(1);
    }
}

runVerification();
