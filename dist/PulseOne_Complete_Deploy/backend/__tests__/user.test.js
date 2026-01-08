const request = require('supertest');
const express = require('express');
const userRoutes = require('../routes/user');

const app = express();


app.use(userRoutes);

describe('GET /user', () => {
  it('should return a user object', async () => {
    const res = await request(app).get('/user');
    expect(res.statusCode).toEqual(200);
    expect(res.body).toHaveProperty('name', 'John Doe');
  });
});
