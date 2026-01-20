const axios = require('axios');

async function verifyApi() {
    const baseUrl = 'http://localhost:3000/api';
    const token = 'dev-dummy-token'; // Authenticates as system_admin via mock in middleware

    try {
        console.log('--- Verifying Data Points API Response Structure ---');
        const response = await axios.get(`${baseUrl}/data/points`, {
            headers: { 'Authorization': `Bearer ${token}` },
            params: { limit: 5 }
        });

        console.log('Status:', response.status);
        console.log('Response Structure:', JSON.stringify(Object.keys(response.data), null, 2));

        if (response.data.success) {
            console.log('Success: true');
            if (response.data.data && response.data.data.items) {
                console.log('Items found:', response.data.data.items.length);
                console.log('First item keys:', Object.keys(response.data.data.items[0] || {}));
            } else {
                console.log('WARNING: data.items not found in response.data');
                console.log('response.data length:', JSON.stringify(response.data.data).length);
            }
        } else {
            console.log('API Error:', response.data.message);
        }

        console.log('\n--- Verifying Devices API Response Structure ---');
        const devResponse = await axios.get(`${baseUrl}/devices`, {
            headers: { 'Authorization': `Bearer ${token}` },
            params: { limit: 5 }
        });

        console.log('Status:', devResponse.status);
        if (devResponse.data.success && devResponse.data.data) {
            console.log('Devices found:', devResponse.data.data.items?.length || 0);
        }

    } catch (error) {
        console.error('Test failed:', error.message);
        if (error.response) {
            console.error('Response data:', error.response.data);
        }
    }
}

verifyApi();
