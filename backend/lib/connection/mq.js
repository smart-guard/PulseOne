const amqp = require('amqplib');
const env = require('../../../config/env'); // 필요 시 환경설정 통일

const connectMQ = async () => {
  const host = env?.RABBITMQ_HOST || process.env.RABBITMQ_HOST || 'localhost';
  const port = env?.RABBITMQ_PORT || process.env.RABBITMQ_PORT || '5672';

  try {
    const conn = await amqp.connect(`amqp://${host}:${port}`);
    const channel = await conn.createChannel();
    return { conn, channel };
  } catch (error) {
    console.error('🐇 RabbitMQ 연결 실패:', error.message || error);
    return null;  // ⛔ 실패 시 undefined 방지
  }
};

module.exports = connectMQ;
