const client = require('../lib/connection/rpc');

describe('RPC Client', () => {
  test('ping 메서드 호출 시 정상 응답을 반환해야 한다', async () => {
    const result = await new Promise((resolve, reject) => {
      client.request('ping', [], (err, error, response) => {
        if (err) return reject(err);
        if (error) return reject(error);
        resolve(response);
      });
    });

    expect(result).toBe('pong'); // or whatever your RPC returns
  });
});
