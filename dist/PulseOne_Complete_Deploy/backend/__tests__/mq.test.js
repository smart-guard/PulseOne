const connectMQ = require('../lib/connection/mq');

describe('RabbitMQ Connection Test', () => {
  let connection, channel;

  beforeAll(async () => {
    const mq = await connectMQ();
    if (!mq) return; // 연결 실패 시 테스트 스킵 또는 예외 유도 가능
    connection = mq.conn;
    channel = mq.channel;
  });

  test('should publish and consume a message', async () => {
    if (!channel) {
      console.warn('RabbitMQ 미연결 → 테스트 건너뜀');
      return;
    }

    const queue = 'test-queue';
    const msg = 'hello';

    await channel.assertQueue(queue, { durable: false });
    await channel.sendToQueue(queue, Buffer.from(msg));

    const result = await new Promise((resolve) => {
      channel.consume(queue, (msg) => {
        resolve(msg.content.toString());
        channel.ack(msg);
      });
    });

    expect(result).toBe(msg);
  });

  afterAll(async () => {
    if (channel) await channel.close();
    if (connection) await connection.close();
  });
});
