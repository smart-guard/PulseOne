// backend/lib/connection/rabbitmq.js
const amqp = require('amqplib');
const ConfigManager = require('../config/ConfigManager');

class RabbitMQManager {
    constructor() {
        this.connection = null;
        this.channel = null;
        this.isConnected = false;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 5000; // 5초
        
        // ConfigManager에서 설정 읽기
        const config = ConfigManager.getInstance();
        this.config = {
            host: config.get('RABBITMQ_HOST', 'pulseone-rabbitmq'),
            port: config.getNumber('RABBITMQ_PORT', 5672),
            user: config.get('RABBITMQ_USER', 'guest'),
            password: config.get('RABBITMQ_PASSWORD', 'guest'),
            vhost: config.get('RABBITMQ_VHOST', '/'),
            enabled: config.getBoolean('RABBITMQ_ENABLED', true)
        };
        
        this.connectionUrl = `amqp://${this.config.user}:${this.config.password}@${this.config.host}:${this.config.port}${this.config.vhost}`;
        
        // 이벤트 핸들러 바인딩
        this.setupConnectionHandlers = this.setupConnectionHandlers.bind(this);
        this.handleConnectionClose = this.handleConnectionClose.bind(this);
        this.handleConnectionError = this.handleConnectionError.bind(this);
    }

    async connect() {
        if (!this.config.enabled) {
            console.log('RabbitMQ 비활성화됨 (RABBITMQ_ENABLED=false)');
            return;
        }

        try {
            console.log('RabbitMQ 연결 시도:', this.connectionUrl.replace(/:[^@]+@/, ':***@'));
            
            this.connection = await amqp.connect(this.connectionUrl);
            this.channel = await this.connection.createChannel();
            
            // 연결 이벤트 핸들러 설정
            this.setupConnectionHandlers();
            
            this.isConnected = true;
            this.reconnectAttempts = 0;
            
            console.log('RabbitMQ 연결 성공');
            
        } catch (error) {
            console.error('RabbitMQ 연결 실패:', error.message);
            await this.handleReconnect();
        }
    }

    setupConnectionHandlers() {
        if (!this.connection) return;

        this.connection.on('close', this.handleConnectionClose);
        this.connection.on('error', this.handleConnectionError);
    }

    async handleConnectionClose() {
        console.log('RabbitMQ 연결 종료됨');
        this.isConnected = false;
        await this.handleReconnect();
    }

    handleConnectionError(error) {
        console.error('RabbitMQ 연결 오류:', error.message);
        this.isConnected = false;
    }

    async handleReconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error(`RabbitMQ 재연결 최대 시도 횟수 초과 (${this.maxReconnectAttempts})`);
            return;
        }

        this.reconnectAttempts++;
        console.log(`RabbitMQ 재연결 시도 ${this.reconnectAttempts}/${this.maxReconnectAttempts} (${this.reconnectDelay}ms 후)`);

        setTimeout(() => {
            this.connect().catch(console.error);
        }, this.reconnectDelay);
    }

    async setupAlarmInfrastructure() {
        if (!this.isConnected || !this.channel) {
            throw new Error('RabbitMQ 연결이 필요합니다');
        }

        try {
            console.log('알람 시스템 인프라 설정 시작...');

            // Exchange 생성
            await this.channel.assertExchange('alarm.events', 'topic', { 
                durable: true,
                autoDelete: false
            });

            console.log('  - Exchange "alarm.events" 생성됨');

            // 중요 알람 큐
            const criticalQueue = await this.channel.assertQueue('alarm.critical', { 
                durable: true,
                arguments: {
                    'x-message-ttl': 3600000, // 1시간
                    'x-max-length': 10000
                }
            });

            // 일반 알람 큐
            const warningQueue = await this.channel.assertQueue('alarm.warning', { 
                durable: true,
                arguments: {
                    'x-message-ttl': 3600000,
                    'x-max-length': 50000
                }
            });

            // 알람 이력 큐
            const historyQueue = await this.channel.assertQueue('alarm.history', { 
                durable: true,
                arguments: {
                    'x-message-ttl': 86400000, // 24시간
                    'x-max-length': 100000
                }
            });

            console.log('  - 알람 큐들 생성됨:', {
                critical: criticalQueue.queue,
                warning: warningQueue.queue,
                history: historyQueue.queue
            });

            // 바인딩 설정
            await this.channel.bindQueue('alarm.critical', 'alarm.events', 'alarm.critical.*');
            await this.channel.bindQueue('alarm.warning', 'alarm.events', 'alarm.warning.*');
            await this.channel.bindQueue('alarm.warning', 'alarm.events', 'alarm.high.*');
            await this.channel.bindQueue('alarm.warning', 'alarm.events', 'alarm.medium.*');
            await this.channel.bindQueue('alarm.warning', 'alarm.events', 'alarm.low.*');
            await this.channel.bindQueue('alarm.history', 'alarm.events', 'alarm.*');

            console.log('  - 라우팅 바인딩 설정됨');

            console.log('알람 시스템 인프라 설정 완료');

        } catch (error) {
            console.error('알람 인프라 설정 실패:', error.message);
            throw error;
        }
    }

    async setupCollectorInfrastructure() {
        if (!this.isConnected || !this.channel) {
            throw new Error('RabbitMQ 연결이 필요합니다');
        }

        try {
            console.log('Collector 제어 인프라 설정 시작...');

            // Collector 제어용 Exchange
            await this.channel.assertExchange('collector.control', 'direct', { 
                durable: true,
                autoDelete: false
            });

            // Collector 명령 큐
            const commandQueue = await this.channel.assertQueue('collector.commands', { 
                durable: true,
                arguments: {
                    'x-message-ttl': 300000, // 5분
                    'x-max-length': 1000
                }
            });

            // Collector 상태 큐
            const statusQueue = await this.channel.assertQueue('collector.status', { 
                durable: true,
                arguments: {
                    'x-message-ttl': 600000, // 10분
                    'x-max-length': 5000
                }
            });

            console.log('  - Collector 큐들 생성됨:', {
                commands: commandQueue.queue,
                status: statusQueue.queue
            });

            // 바인딩 설정
            await this.channel.bindQueue('collector.commands', 'collector.control', 'command');
            await this.channel.bindQueue('collector.status', 'collector.control', 'status');

            console.log('  - Collector 바인딩 설정됨');

            console.log('Collector 제어 인프라 설정 완료');

        } catch (error) {
            console.error('Collector 인프라 설정 실패:', error.message);
            throw error;
        }
    }

    // 알람 발행
    async publishAlarm(alarmData) {
        if (!this.isConnected || !this.channel) {
            console.error('RabbitMQ 연결 없음 - 알람 발행 실패');
            return false;
        }

        try {
            const { severity = 'medium', deviceId, message } = alarmData;
            const routingKey = `alarm.${severity}.device.${deviceId}`;

            const publishData = {
                ...alarmData,
                timestamp: new Date().toISOString(),
                source: 'pulseone-collector'
            };

            const published = this.channel.publish(
                'alarm.events',
                routingKey,
                Buffer.from(JSON.stringify(publishData)),
                { 
                    persistent: true,
                    messageId: `alarm_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`,
                    timestamp: Date.now()
                }
            );

            if (published) {
                console.log(`알람 발행됨: ${routingKey}`);
                return true;
            } else {
                console.warn('알람 발행 실패 - 큐 가득참');
                return false;
            }

        } catch (error) {
            console.error('알람 발행 중 오류:', error.message);
            return false;
        }
    }

    // Collector 명령 발행
    async publishCollectorCommand(command) {
        if (!this.isConnected || !this.channel) {
            console.error('RabbitMQ 연결 없음 - 명령 발행 실패');
            return false;
        }

        try {
            const commandData = {
                ...command,
                timestamp: new Date().toISOString(),
                source: 'pulseone-backend'
            };

            const published = this.channel.publish(
                'collector.control',
                'command',
                Buffer.from(JSON.stringify(commandData)),
                { 
                    persistent: true,
                    messageId: `cmd_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`,
                    timestamp: Date.now()
                }
            );

            if (published) {
                console.log(`Collector 명령 발행됨: ${command.action}`);
                return true;
            } else {
                console.warn('Collector 명령 발행 실패');
                return false;
            }

        } catch (error) {
            console.error('Collector 명령 발행 중 오류:', error.message);
            return false;
        }
    }

    // 메시지 구독
    async subscribeToAlarms(callback) {
        if (!this.isConnected || !this.channel) {
            throw new Error('RabbitMQ 연결이 필요합니다');
        }

        try {
            // 중요 알람 구독
            await this.channel.consume('alarm.critical', (msg) => {
                if (msg) {
                    try {
                        const alarmData = JSON.parse(msg.content.toString());
                        callback(alarmData, 'critical');
                        this.channel.ack(msg);
                    } catch (error) {
                        console.error('중요 알람 처리 오류:', error.message);
                        this.channel.nack(msg, false, false);
                    }
                }
            });

            // 일반 알람 구독
            await this.channel.consume('alarm.warning', (msg) => {
                if (msg) {
                    try {
                        const alarmData = JSON.parse(msg.content.toString());
                        callback(alarmData, 'warning');
                        this.channel.ack(msg);
                    } catch (error) {
                        console.error('일반 알람 처리 오류:', error.message);
                        this.channel.nack(msg, false, false);
                    }
                }
            });

            console.log('알람 구독 설정됨');

        } catch (error) {
            console.error('알람 구독 설정 실패:', error.message);
            throw error;
        }
    }

    // Collector 상태 구독
    async subscribeToCollectorStatus(callback) {
        if (!this.isConnected || !this.channel) {
            throw new Error('RabbitMQ 연결이 필요합니다');
        }

        try {
            await this.channel.consume('collector.status', (msg) => {
                if (msg) {
                    try {
                        const statusData = JSON.parse(msg.content.toString());
                        callback(statusData);
                        this.channel.ack(msg);
                    } catch (error) {
                        console.error('Collector 상태 처리 오류:', error.message);
                        this.channel.nack(msg, false, false);
                    }
                }
            });

            console.log('Collector 상태 구독 설정됨');

        } catch (error) {
            console.error('Collector 상태 구독 실패:', error.message);
            throw error;
        }
    }

    // 연결 상태 확인
    async isHealthy() {
        try {
            if (!this.config.enabled) {
                return true; // 비활성화 상태면 정상으로 간주
            }

            return this.isConnected && this.connection && !this.connection.connection.destroyed;
        } catch {
            return false;
        }
    }

    // 통계 정보
    async getStats() {
        if (!this.isConnected || !this.channel) {
            return null;
        }

        try {
            const queues = ['alarm.critical', 'alarm.warning', 'alarm.history', 'collector.commands', 'collector.status'];
            const stats = {};

            for (const queueName of queues) {
                try {
                    const queueInfo = await this.channel.checkQueue(queueName);
                    stats[queueName] = {
                        messageCount: queueInfo.messageCount,
                        consumerCount: queueInfo.consumerCount
                    };
                } catch (error) {
                    stats[queueName] = { error: error.message };
                }
            }

            return {
                isConnected: this.isConnected,
                reconnectAttempts: this.reconnectAttempts,
                queues: stats
            };

        } catch (error) {
            console.error('RabbitMQ 통계 조회 실패:', error.message);
            return null;
        }
    }

    // 연결 종료
    async disconnect() {
        try {
            console.log('RabbitMQ 연결 종료 중...');

            if (this.channel) {
                await this.channel.close();
                this.channel = null;
            }

            if (this.connection) {
                await this.connection.close();
                this.connection = null;
            }

            this.isConnected = false;
            console.log('RabbitMQ 연결 종료됨');

        } catch (error) {
            console.error('RabbitMQ 연결 종료 중 오류:', error.message);
        }
    }
}

module.exports = RabbitMQManager;