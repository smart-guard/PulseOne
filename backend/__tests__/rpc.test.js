// __tests__/rpc.test.js

const client = require('../lib/connection/rpc');

(async () => {
  try {
    const result = await new Promise((resolve, reject) => {
      client.request('ping', [], (err, error, response) => {
        if (err) return reject(err);
        if (error) return reject(error);
        resolve(response);
      });
    });
    console.log('RPC 서버 응답:', result);
  } catch (err) {
    console.error('RPC 호출 실패:', err.message || err);
    process.exit(1);
  }
})();
