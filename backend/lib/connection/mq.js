// ===========================================================================
// backend/lib/connection/mq.js - ConfigManager 사용하도록 수정
// ===========================================================================
const amqp = require('amqplib');
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

class RabbitMQManager {
  constructor() {
    this.connection = null;
    this.channel = null;
    this.isConnected = false;
    
    // ConfigManager에서 설정 로드
    this.host = config.get('RABBITMQ_HOST', 'localhost');
    this.port = config.get('RABBITMQ_PORT', '5672');
    this.username = config.get('RABBITMQ_USERNAME', 'guest');
    this.password = config.get('RABBITMQ_PASSWORD', 'guest');
    this.vhost = config.get('RABBITMQ_VHOST', '/');
    
    this.url = `amqp://${this.username}:${this.password}@${this.host}:${this.port}${this.vhost}`;
    
    console.log(`🔗 RabbitMQ 연결 설정:
   호스트: ${this.host}:${this.port}
   사용자: ${this.username}
   가상호스트: ${this.vhost}`);
  }

  async connect() {
    try {
      console.log('🔄 RabbitMQ 연결 시도 중...');
      
      this.connection = await amqp.connect(this.url);
      this.channel = await this.connection.createChannel();
      
      // 연결 이벤트 처리
      this.connection.on('error', (err) => {
        console.error('❌ RabbitMQ 연결 오류:', err.message);
        this.isConnected = false;
      });

      this.connection.on('close', () => {
        console.warn('⚠️ RabbitMQ 연결 종료됨');
        this.isConnected = false;
      });

      this.isConnected = true;
      console.log('✅ RabbitMQ 연결 성공');
      
      return { conn: this.connection, channel: this.channel };
      
    } catch (error) {
      console.error('❌ RabbitMQ 연결 실패:', error.message);
      console.log('⚠️  RabbitMQ 없이 계속 진행합니다.');
      return null;
    }
  }

  async sendToQueue(queueName, message) {
    try {
      if (!this.isConnected) {
        await this.connect();
      }

      if (!this.channel) {
        throw new Error('RabbitMQ 채널이 없습니다');
      }

      // 큐 존재 확인/생성
      await this.channel.assertQueue(queueName, { durable: true });
      
      // 메시지 전송
      const messageBuffer = Buffer.from(JSON.stringify(message));
      const result = this.channel.sendToQueue(queueName, messageBuffer, { persistent: true });
      
      console.log(`📤 메시지 전송: ${queueName}`);
      return result;
      
    } catch (error) {
      console.error('❌ RabbitMQ 메시지 전송 실패:', error.message);
      return false;
    }
  }

  async consume(queueName, callback) {
    try {
      if (!this.isConnected) {
        await this.connect();
      }

      if (!this.channel) {
        throw new Error('RabbitMQ 채널이 없습니다');
      }

      // 큐 존재 확인/생성
      await this.channel.assertQueue(queueName, { durable: true });
      
      // 메시지 소비
      await this.channel.consume(queueName, (msg) => {
        if (msg) {
          try {
            const content = JSON.parse(msg.content.toString());
            callback(content);
            this.channel.ack(msg);
          } catch (error) {
            console.error('❌ 메시지 처리 오류:', error.message);
            this.channel.nack(msg, false, false); // 재큐잉 없이 거부
          }
        }
      });
      
      console.log(`📥 큐 소비 시작: ${queueName}`);
      
    } catch (error) {
      console.error('❌ RabbitMQ 메시지 소비 실패:', error.message);
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
      this.isConnected = false;
      console.log('📴 RabbitMQ 연결 종료');
    } catch (error) {
      console.error('❌ RabbitMQ 종료 오류:', error.message);
    }
  }
}

// 싱글톤 인스턴스 생성
const rabbitMQManager = new RabbitMQManager();

// 기존 인터페이스 호환성 유지
async function connectMQ() {
  return await rabbitMQManager.connect();
}

async function sendToQueue(queueName, message) {
  return await rabbitMQManager.sendToQueue(queueName, message);
}

async function consume(queueName, callback) {
  return await rabbitMQManager.consume(queueName, callback);
}

module.exports = {
  connectMQ,
  sendToQueue,
  consume,
  manager: rabbitMQManager
};