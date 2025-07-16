// ===========================================================================
// lib/connection/mq.js
// ===========================================================================
const amqp = require('amqplib');
const env = require('../../../config/env');

class RabbitMQManager {
  constructor() {
    this.connection = null;
    this.channel = null;
    this.isConnected = false;
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 5;
    this.reconnectInterval = 5000;
  }

  async connect() {
    try {
      const host = env.RABBITMQ_HOST || 'localhost';
      const port = env.RABBITMQ_PORT || '5672';
      const user = process.env.RABBITMQ_DEFAULT_USER || 'admin';
      const pass = process.env.RABBITMQ_DEFAULT_PASS || 'admin';
      
      const url = `amqp://${user}:${pass}@${host}:${port}`;
      
      this.connection = await amqp.connect(url);
      this.channel = await this.connection.createChannel();
      this.isConnected = true;
      this.reconnectAttempts = 0;

      console.log('✅ RabbitMQ 연결 성공');

      // 연결 에러 핸들러
      this.connection.on('error', (err) => {
        console.error('❌ RabbitMQ 연결 오류:', err.message);
        this.isConnected = false;
        this.reconnect();
      });

      this.connection.on('close', () => {
        console.warn('⚠️ RabbitMQ 연결 종료됨');
        this.isConnected = false;
        this.reconnect();
      });

      return { conn: this.connection, channel: this.channel };
    } catch (error) {
      console.error('❌ RabbitMQ 연결 실패:', error.message);
      this.reconnect();
      return null;
    }
  }

  async reconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('❌ RabbitMQ 재연결 시도 횟수 초과');
      return;
    }

    this.reconnectAttempts++;
    console.log(`🔄 RabbitMQ 재연결 시도 ${this.reconnectAttempts}/${this.maxReconnectAttempts}`);

    setTimeout(() => {
      this.connect();
    }, this.reconnectInterval);
  }

  async publishMessage(exchange, routingKey, message, options = {}) {
    try {
      if (!this.isConnected || !this.channel) {
        await this.connect();
      }

      const messageBuffer = Buffer.from(JSON.stringify(message));
      await this.channel.publish(exchange, routingKey, messageBuffer, options);
      
      console.log(`📤 메시지 발송: ${exchange}/${routingKey}`);
      return true;
    } catch (error) {
      console.error('❌ 메시지 발송 실패:', error.message);
      return false;
    }
  }

  async sendToQueue(queueName, message, options = {}) {
    try {
      if (!this.isConnected || !this.channel) {
        await this.connect();
      }

      await this.channel.assertQueue(queueName, { durable: true });
      const messageBuffer = Buffer.from(JSON.stringify(message));
      await this.channel.sendToQueue(queueName, messageBuffer, options);
      
      console.log(`📤 큐 메시지 발송: ${queueName}`);
      return true;
    } catch (error) {
      console.error('❌ 큐 메시지 발송 실패:', error.message);
      return false;
    }
  }

  async close() {
    try {
      if (this.channel) {
        await this.channel.close();
      }
      if (this.connection) {
        await this.connection.close();
      }
      console.log('✅ RabbitMQ 연결 종료됨');
    } catch (error) {
      console.error('❌ RabbitMQ 종료 실패:', error.message);
    }
  }
}

const mqManager = new RabbitMQManager();

// 레거시 호환을 위한 함수
const connectMQ = async () => {
  return await mqManager.connect();
};

module.exports = connectMQ;
module.exports.RabbitMQManager = mqManager;
module.exports.publishMessage = mqManager.publishMessage.bind(mqManager);
module.exports.sendToQueue = mqManager.sendToQueue.bind(mqManager);