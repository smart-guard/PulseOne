
const axios = require('axios');

async function run() {
    try {
        console.log('1. Creating...');
        const createRes = await axios.post('http://localhost:3000/api/virtual-points',
            { virtualPoint: { name: `SIMPLE_TEST_${Date.now()}`, formula: 'return 1;' } },
            { headers: { 'x-tenant-id': 1 } }
        );
        console.log('Create Status:', createRes.status);
        if (!createRes.data.success) {
            console.log('Create Failed logic:', createRes.data);
            process.exit(1);
        }
        const id = createRes.data.data.id;
        console.log('Created ID:', id);

        console.log('2. Listing...');
        const listRes = await axios.get('http://localhost:3000/api/virtual-points', { headers: { 'x-tenant-id': 1 } });
        console.log('List Status:', listRes.status);
        // console.log('List Data Keys:', Object.keys(listRes.data)); 
        // console.log('List Data.Data Keys:', listRes.data.data ? Object.keys(listRes.data.data) : 'null');

        let items = [];
        if (Array.isArray(listRes.data.data)) items = listRes.data.data;
        else if (listRes.data.data && Array.isArray(listRes.data.data.items)) items = listRes.data.data.items;

        console.log('Items Count:', items.length);
        const found = items.find(p => p.id === id);
        if (!found) {
            console.error('Item not found in list!');
            console.log('First 3 items:', items.slice(0, 3));
            process.exit(1);
        }
        console.log('Item found:', found.name);

        console.log('3. Deleting...');
        await axios.delete(`http://localhost:3000/api/virtual-points/${id}`, { headers: { 'x-tenant-id': 1 } });
        console.log('Deleted.');

        console.log('SUCCESS');
    } catch (e) {
        console.error('ERROR:', e.message);
        if (e.response) {
            console.error('Response Status:', e.response.status);
            console.error('Response Data:', JSON.stringify(e.response.data).substring(0, 1000)); // Limit output
        }
    }
}

run();
