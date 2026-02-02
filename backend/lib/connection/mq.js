const amqp = require('amqplib');
const axios = require('axios');
const ConfigManager = require('../config/ConfigManager');

class RabbitMQManager {
    constructor() {
        // 시스템 연결 (공통 알람, 명령용)
        this.systemConnection = null;
        this.systemChannel = null;
        this.isSystemConnected = false;

        // 인스턴스별 연결 저장소 (MQTT 브로커 격리 대응)
        this.instanceConnections = new Map();

        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 5000;

        // ConfigManager에서 시스템 기본 설정 읽기
        const config = ConfigManager.getInstance();
        this.baseConfig = {
            host: config.get('RABBITMQ_HOST', 'pulseone-rabbitmq'),
            port: config.getNumber('RABBITMQ_PORT', 5672),
            mgmtPort: config.getNumber('RABBITMQ_MGMT_PORT', 15672),
            user: config.get('RABBITMQ_USER', 'guest'),
            password: config.get('RABBITMQ_PASSWORD', 'guest'),
            vhost: config.get('RABBITMQ_VHOST', '/'),
            enabled: config.getBoolean('RABBITMQ_ENABLED', true)
        };

        this.systemUrl = `amqp://${this.baseConfig.user}:${this.baseConfig.password}@${this.baseConfig.host}:${this.baseConfig.port}${this.baseConfig.vhost}`;

        // 이벤트 핸들러 바인딩
        this.handleConnectionClose = this.handleConnectionClose.bind(this);
        this.handleConnectionError = this.handleConnectionError.bind(this);
    }

    /**
     * 메인 시스템 연결 (Backwards Compatibility 유지)
     */
    async connect() {
        if (!this.baseConfig.enabled) {
            console.log('RabbitMQ 비활성화됨 (RABBITMQ_ENABLED=false)');
            return;
        }

        try {
            console.log('RabbitMQ 시스템 연결 시도:', this.systemUrl.replace(/:[^@]+@/, ':***@'));

            this.systemConnection = await amqp.connect(this.systemUrl);
            this.systemChannel = await this.systemConnection.createChannel();

            // 연결 이벤트 핸들러 설정
            this.systemConnection.on('close', () => this.handleConnectionClose('system'));
            this.systemConnection.on('error', (err) => this.handleConnectionError('system', err));

            this.isSystemConnected = true;
            this.reconnectAttempts = 0;

            console.log('RabbitMQ 시스템 연결 성공');

            // 기본 인프라 설정
            await this.setupAlarmInfrastructure();
            await this.setupCollectorInfrastructure();

        } catch (error) {
            console.error('RabbitMQ 시스템 연결 실패:', error.message);
            await this.handleReconnect();
        }
    }

    /**
     * 특정 프로토콜 인스턴스 전용 연결 수립
     * @param {Object} instanceData { id, vhost, api_key, connection_params }
     */
    async connectInstance(instanceData) {
        if (!this.baseConfig.enabled) return;

        const { id, vhost, instance_name } = instanceData;

        // 이미 연결되어 있다면 기존 연결 반환 (또는 재연결 로직)
        if (this.instanceConnections.has(id)) {
            const existing = this.instanceConnections.get(id);
            if (existing.connected) return existing;
        }

        try {
            console.log(`[Instance:${id}] RabbitMQ 연결 시도 (${vhost})`);

            // 인스턴스 전용 URL (기본 계정 사용하되 Vhost만 격리)
            const instanceUrl = `amqp://${this.baseConfig.user}:${this.baseConfig.password}@${this.baseConfig.host}:${this.baseConfig.port}${vhost || '/'}`;

            const connection = await amqp.connect(instanceUrl);
            const channel = await connection.createChannel();

            const instanceInfo = {
                id,
                name: instance_name,
                connection,
                channel,
                connected: true,
                vhost
            };

            connection.on('close', () => {
                console.log(`[Instance:${id}] 연결 종료됨`);
                instanceInfo.connected = false;
                this.instanceConnections.delete(id);
            });

            connection.on('error', (err) => {
                console.error(`[Instance:${id}] 연결 오류:`, err.message);
                instanceInfo.connected = false;
            });

            this.instanceConnections.set(id, instanceInfo);
            console.log(`[Instance:${id}] 연결 성공 (Vhost: ${vhost})`);

            return instanceInfo;

        } catch (error) {
            console.error(`[Instance:${id}] 연결 실패:`, error.message);
            throw error;
        }
    }

    /**
     * 특정 인스턴스 연결 해제
     */
    async disconnectInstance(id) {
        const instance = this.instanceConnections.get(id);
        if (instance) {
            try {
                if (instance.channel) await instance.channel.close();
                if (instance.connection) await instance.connection.close();
                this.instanceConnections.delete(id);
                console.log(`[Instance:${id}] 연결 해제 완료`);
            } catch (error) {
                console.error(`[Instance:${id}] 연결 해제 실패:`, error.message);
            }
        }
    }

    async handleConnectionClose(type) {
        if (type === 'system') {
            console.log('RabbitMQ 시스템 연결 종료됨');
            this.isSystemConnected = false;
            await this.handleReconnect();
        }
    }

    handleConnectionError(type, error) {
        if (type === 'system') {
            console.error('RabbitMQ 시스템 연결 오류:', error.message);
            this.isSystemConnected = false;
        }
    }

    async handleReconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error(`RabbitMQ 시스템 재연결 최대 시도 횟수 초과 (${this.maxReconnectAttempts})`);
            return;
        }

        this.reconnectAttempts++;
        console.log(`RabbitMQ 시스템 재연결 시도 ${this.reconnectAttempts}/${this.maxReconnectAttempts} (${this.reconnectDelay}ms 후)`);

        setTimeout(() => {
            this.connect().catch(console.error);
        }, this.reconnectDelay);
    }

    // =========================================================================
    // 알람 및 명령 (시스템 채널 사용)
    // =========================================================================

    async setupAlarmInfrastructure() {
        if (!this.isSystemConnected || !this.systemChannel) return;

        try {
            await this.systemChannel.assertExchange('alarm.events', 'topic', { durable: true, autoDelete: false });

            const queues = ['alarm.critical', 'alarm.warning', 'alarm.history'];
            for (const q of queues) {
                await this.systemChannel.assertQueue(q, {
                    durable: true,
                    arguments: { 'x-message-ttl': q === 'alarm.history' ? 86400000 : 3600000 }
                });
            }

            await this.systemChannel.bindQueue('alarm.critical', 'alarm.events', 'alarm.critical.*');
            await this.systemChannel.bindQueue('alarm.warning', 'alarm.events', 'alarm.warning.*');
            await this.systemChannel.bindQueue('alarm.warning', 'alarm.events', 'alarm.high.*');
            await this.systemChannel.bindQueue('alarm.history', 'alarm.events', 'alarm.*');

        } catch (error) {
            console.error('알람 인프라 설정 실패:', error.message);
        }
    }

    async setupCollectorInfrastructure() {
        if (!this.isSystemConnected || !this.systemChannel) return;

        try {
            await this.systemChannel.assertExchange('collector.control', 'direct', { durable: true, autoDelete: false });
            await this.systemChannel.assertQueue('collector.commands', { durable: true });
            await this.systemChannel.assertQueue('collector.status', { durable: true });
            await this.systemChannel.bindQueue('collector.commands', 'collector.control', 'command');
            await this.systemChannel.bindQueue('collector.status', 'collector.control', 'status');
        } catch (error) {
            console.error('Collector 인프라 설정 실패:', error.message);
        }
    }

    async publishAlarm(alarmData) {
        if (!this.isSystemConnected || !this.systemChannel) return false;

        try {
            const { severity = 'medium', deviceId } = alarmData;
            const routingKey = `alarm.${severity}.device.${deviceId}`;
            const publishData = { ...alarmData, timestamp: new Date().toISOString(), source: 'pulseone-collector' };

            return this.systemChannel.publish(
                'alarm.events',
                routingKey,
                Buffer.from(JSON.stringify(publishData)),
                { persistent: true }
            );
        } catch (error) {
            console.error('알람 발행 중 오류:', error.message);
            return false;
        }
    }

    async publishCollectorCommand(command) {
        if (!this.isSystemConnected || !this.systemChannel) return false;

        try {
            const commandData = { ...command, timestamp: new Date().toISOString(), source: 'pulseone-backend' };
            return this.systemChannel.publish(
                'collector.control',
                'command',
                Buffer.from(JSON.stringify(commandData)),
                { persistent: true }
            );
        } catch (error) {
            console.error('Collector 명령 발행 중 오류:', error.message);
            return false;
        }
    }

    // =========================================================================
    // 상태 및 통계
    // =========================================================================

    async isHealthy() {
        if (!this.baseConfig.enabled) return true;
        const systemHealthy = this.isSystemConnected && this.systemConnection && !this.systemConnection.connection.destroyed;

        // 인스턴스 중 하나라도 끊겼으면 경고 수준으로 처리할 수 있으나, 일단 시스템 연결 기준
        return systemHealthy;
    }

    async getAdvancedStats() {
        if (!this.baseConfig.enabled) return null;

        try {
            const baseUrl = `http://${this.baseConfig.host}:${this.baseConfig.mgmtPort}/api`;
            const auth = { username: this.baseConfig.user, password: this.baseConfig.password };

            const [ov, nodes, conns, vhosts] = await Promise.all([
                axios.get(`${baseUrl}/overview`, { auth }),
                axios.get(`${baseUrl}/nodes`, { auth }),
                axios.get(`${baseUrl}/connections`, { auth }),
                axios.get(`${baseUrl}/vhosts`, { auth })
            ]);

            const nodeInfo = nodes.data[0] || {};

            return {
                timestamp: new Date().toISOString(),
                health: {
                    status: this.isSystemConnected ? 'healthy' : 'unhealthy',
                    instance_count: this.instanceConnections.size,
                    vhost_count: vhosts.data.length
                },
                metrics: {
                    connections: ov.data.object_totals.connections,
                    queues: ov.data.object_totals.queues,
                    mem_used: (nodeInfo.mem_used / 1024 / 1024).toFixed(2) + ' MB'
                },
                connection_list: conns.data.map(c => ({
                    name: c.name,
                    vhost: c.vhost,
                    user: c.user,
                    state: c.state,
                    connected_at: new Date(c.connected_at).toISOString()
                }))
            };
        } catch (error) {
            return { error: error.message, health: { status: 'error' } };
        }
    }

    async disconnect() {
        console.log('RabbitMQ 모든 연결 종료 중...');

        // 인스턴스 연결 모두 종료
        for (const [id, info] of this.instanceConnections) {
            await this.disconnectInstance(id);
        }

        if (this.systemChannel) await this.systemChannel.close();
        if (this.systemConnection) await this.systemConnection.close();

        this.isSystemConnected = false;
        console.log('RabbitMQ 전체 종료 완료');
    }
}

const manager = new RabbitMQManager();
module.exports = {
    manager,
    RabbitMQManager
};