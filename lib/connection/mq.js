const amqp = require('amqplib');

const connectMQ = async () => {
  const conn = await amqp.connect(`amqp://${process.env.RABBITMQ_HOST || 'localhost'}`);
  const channel = await conn.createChannel();
  return { conn, channel };
};

module.exports = connectMQ;
