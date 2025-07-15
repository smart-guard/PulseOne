const connectMQ = require('../lib/connection/mq');

describe('RabbitMQ Connection Test', () => {
  let connection, channel;

  beforeAll(async () => {
    ({ conn: connection, channel } = await connectMQ());
  });

  afterAll(async () => {
    await channel.close();
    await connection.close();
  });

  it('should publish and consume a message', async () => {
    const queue = 'test-queue';
    const testMessage = 'hello world';

    await channel.assertQueue(queue);
    await channel.sendToQueue(queue, Buffer.from(testMessage));

    const consumed = await new Promise((resolve) => {
      channel.consume(queue, (msg) => {
        if (msg !== null) {
          channel.ack(msg);
          resolve(msg.content.toString());
        }
      }, { noAck: false });
    });

    expect(consumed).toBe(testMessage);
  });
});
