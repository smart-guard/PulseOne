// __tests__/app.test.js

jest.mock('redis'); // __mocks__/redis.js 자동 사용


const request = require('supertest');
const app = require('../app');

describe('GET /health', () => {
  it('should return status ok', async () => {
    const res = await request(app).get('/health');
    expect(res.statusCode).toBe(200);
    expect(res.body.status).toBe('ok');
  });
});
