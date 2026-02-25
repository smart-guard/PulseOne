//=============================================================================
// backend/lib/services/AlarmEventSubscriber.js
// Redis에서 C++ Collector가 발행하는 알람 이벤트를 구독하여 웹소켓으로 전달
// ConfigManager 사용 버전 (환경변수 문제 해결)
//=============================================================================

const Redis = require('ioredis');
const configManager = require('../config/ConfigManager');
const logger = console;

class AlarmEventSubscriber {
    constructor(io) {
        this.io = io;  // Socket.IO 서버 인스턴스
        
        // ConfigManager를 통해 Redis 설정 가져오기
        const redisConfig = configManager.getRedisConfig();
        
        
        this.subscriber = new Redis({
            host: redisConfig.host,
            port: redisConfig.port,
            password: redisConfig.password || undefined,
            db: redisConfig.db || 0,
            retryDelayOnFailover: 100,
            maxRetriesPerRequest: null,
            lazyConnect: true,
            connectTimeout: redisConfig.connectTimeout || 5000,
            commandTimeout: configManager.getNumber('REDIS_COMMAND_TIMEOUT_MS', 10000)
        });
        
        this.isRunning = false;
        this.setupEventHandlers();
    }
    
    // =======================================================================
    // Redis 이벤트 핸들러 설정
    // =======================================================================
    setupEventHandlers() {
        this.subscriber.on('connect', () => {
            console.log('🔔 Redis 알람 구독자 연결됨');
            if (logger && logger.info) {
                logger.info('🔔 Redis 알람 구독자 연결됨');
            }
        });
        
        this.subscriber.on('error', (error) => {
            console.error('❌ Redis 알람 구독자 오류:', error.message);
            if (logger && logger.error) {
                logger.error('❌ Redis 알람 구독자 오류:', error);
            }
        });
        
        this.subscriber.on('message', (channel, message) => {
            this.handleAlarmMessage(channel, message);
        });
        
        this.subscriber.on('pmessage', (pattern, channel, message) => {
            this.handleAlarmMessage(channel, message);
        });
        
        this.subscriber.on('ready', () => {
            console.log('✅ Redis 알람 구독자 준비됨');
        });
        
        this.subscriber.on('close', () => {
            console.log('🔌 Redis 알람 구독자 연결 종료됨');
        });
    }
    
    // =======================================================================
    // 구독 시작
    // =======================================================================
    async start() {
        try {
            console.log('🚀 AlarmEventSubscriber 시작 중...');
            
            await this.subscriber.connect();
            console.log('🔗 Redis 연결 성공');
            
            // C++ DataProcessingService에서 발행하는 채널들 구독
            await this.subscriber.subscribe([
                'alarms:all',           // 모든 알람
                'alarms:critical',      // 긴급 알람
                'alarms:high'           // 높은 우선순위 알람
            ]);
            console.log('📡 기본 알람 채널 구독 완료');
            
            // 테넌트별 알람 구독 (패턴 구독)
            await this.subscriber.psubscribe([
                'tenant:*:alarms',      // tenant:1:alarms, tenant:2:alarms
                'device:*:alarms'       // device:device_001:alarms
            ]);
            console.log('📡 패턴 알람 채널 구독 완료');
            
            this.isRunning = true;
            
            if (logger && logger.info) {
                logger.info('✅ 알람 이벤트 구독 시작');
            } else {
                console.log('✅ 알람 이벤트 구독 시작');
            }
            
        } catch (error) {
            console.error('❌ 알람 구독자 시작 실패:', error.message);
            if (logger && logger.error) {
                logger.error('❌ 알람 구독자 시작 실패:', error);
            }
            throw error;
        }
    }
    
    // =======================================================================
    // 구독 중지
    // =======================================================================
    async stop() {
        try {
            this.isRunning = false;
            await this.subscriber.unsubscribe();
            await this.subscriber.punsubscribe();
            await this.subscriber.disconnect();
            
            console.log('🛑 알람 이벤트 구독 중지');
            if (logger && logger.info) {
                logger.info('🛑 알람 이벤트 구독 중지');
            }
        } catch (error) {
            console.error('❌ 알람 구독자 중지 실패:', error.message);
            if (logger && logger.error) {
                logger.error('❌ 알람 구독자 중지 실패:', error);
            }
        }
    }
    
    // =======================================================================
    // 알람 메시지 처리 및 WebSocket 전송
    // =======================================================================
    handleAlarmMessage(channel, message) {
        try {
            const alarmData = JSON.parse(message);
            
            // 메시지 타입 확인
            if (alarmData.type !== 'alarm_event') {
                return;
            }
            
            const logMessage = `🚨 알람 이벤트 수신: ${channel}`;
            console.log(logMessage, {
                occurrence_id: alarmData.occurrence_id,
                rule_id: alarmData.rule_id,
                severity: alarmData.severity,
                message: alarmData.message
            });
            
            if (logger && logger.info) {
                logger.info(logMessage, {
                    occurrence_id: alarmData.occurrence_id,
                    rule_id: alarmData.rule_id,
                    severity: alarmData.severity,
                    message: alarmData.message
                });
            }
            
            // WebSocket으로 실시간 전송
            this.broadcastAlarmToClients(alarmData, channel);
            
            // 특별 처리가 필요한 알람들
            this.handleSpecialAlarms(alarmData);
            
        } catch (error) {
            console.error('❌ 알람 메시지 처리 실패:', error.message);
            if (logger && logger.error) {
                logger.error('❌ 알람 메시지 처리 실패:', error);
            }
        }
    }
    
    // =======================================================================
    // WebSocket 브로드캐스팅
    // =======================================================================
    broadcastAlarmToClients(alarmData, channel) {
        try {
            // 📡 전체 클라이언트에게 새 알람 알림
            if (channel === 'alarms:all') {
                this.io.emit('alarm:new', {
                    type: 'alarm_triggered',
                    data: this.formatAlarmForUI(alarmData),
                    timestamp: new Date().toISOString(),
                    channel: channel
                });
            }
            
            // 📡 테넌트별 클라이언트에게 전송
            if (channel.startsWith('tenant:')) {
                const tenantId = this.extractTenantId(channel);
                if (tenantId) {
                    this.io.to(`tenant:${tenantId}`).emit('alarm:new', {
                        type: 'tenant_alarm',
                        data: this.formatAlarmForUI(alarmData),
                        timestamp: new Date().toISOString(),
                        tenant_id: tenantId
                    });
                }
            }
            
            // 📡 디바이스별 클라이언트에게 전송
            if (channel.startsWith('device:')) {
                const deviceId = this.extractDeviceId(channel);
                if (deviceId) {
                    this.io.to(`device:${deviceId}`).emit('alarm:device', {
                        type: 'device_alarm',
                        data: this.formatAlarmForUI(alarmData),
                        timestamp: new Date().toISOString(),
                        device_id: deviceId
                    });
                }
            }
            
            // 🚨 긴급 알람 - 모든 관리자에게 즉시 알림
            if (alarmData.severity >= 3) { // CRITICAL or HIGH
                this.io.to('admins').emit('alarm:critical', {
                    type: 'critical_alarm',
                    data: this.formatAlarmForUI(alarmData),
                    timestamp: new Date().toISOString(),
                    requires_action: true
                });
            }
            
        } catch (error) {
            console.error('❌ WebSocket 브로드캐스팅 실패:', error.message);
            if (logger && logger.error) {
                logger.error('❌ WebSocket 브로드캐스팅 실패:', error);
            }
        }
    }
    
    // =======================================================================
    // 특별 알람 처리 (이메일, SMS 등)
    // =======================================================================
    handleSpecialAlarms(alarmData) {
        try {
            // 긴급 알람 처리
            if (alarmData.severity >= 3) {
                const message = `🚨 긴급 알람 발생: ${alarmData.message}`;
                console.warn(message);
                if (logger && logger.warn) {
                    logger.warn(message);
                }
                
                // TODO: 이메일/SMS 발송 서비스 호출
                // await this.sendEmergencyNotification(alarmData);
            }
            
            // 디바이스 연결 끊김 알람
            if (alarmData.message && alarmData.message.includes('연결 끊김')) {
                const message = `📶 디바이스 연결 문제: ${alarmData.device_id}`;
                console.warn(message);
                if (logger && logger.warn) {
                    logger.warn(message);
                }
                
                // TODO: 자동 재연결 시도 또는 기술팀 알림
            }
            
        } catch (error) {
            console.error('❌ 특별 알람 처리 실패:', error.message);
            if (logger && logger.error) {
                logger.error('❌ 특별 알람 처리 실패:', error);
            }
        }
    }
    
    // =======================================================================
    // 헬퍼 함수들
    // =======================================================================
    formatAlarmForUI(alarmData) {
        return {
            id: alarmData.occurrence_id,
            rule_id: alarmData.rule_id,
            tenant_id: alarmData.tenant_id,
            device_id: alarmData.device_id,
            point_id: alarmData.point_id,
            message: alarmData.message,
            severity: this.getSeverityText(alarmData.severity),
            severity_level: alarmData.severity,
            state: this.getStateText(alarmData.state),
            trigger_value: alarmData.trigger_value,
            timestamp: alarmData.timestamp,
            source_name: alarmData.source_name,
            location: alarmData.location,
            formatted_time: new Date(alarmData.timestamp).toLocaleString('ko-KR')
        };
    }
    
    extractTenantId(channel) {
        // "tenant:123:alarms" -> "123"
        const match = channel.match(/tenant:(\d+):/);
        return match ? match[1] : null;
    }
    
    extractDeviceId(channel) {
        // "device:device_001:alarms" -> "device_001"
        const match = channel.match(/device:([^:]+):/);
        return match ? match[1] : null;
    }
    
    getSeverityText(severity) {
        const levels = {
            0: 'INFO',
            1: 'LOW',
            2: 'MEDIUM', 
            3: 'HIGH',
            4: 'CRITICAL'
        };
        return levels[severity] || 'UNKNOWN';
    }
    
    getStateText(state) {
        const states = {
            0: 'INACTIVE',
            1: 'ACTIVE',
            2: 'ACKNOWLEDGED',
            3: 'CLEARED'
        };
        return states[state] || 'UNKNOWN';
    }
    
    // =======================================================================
    // 상태 확인 (향상된 버전)
    // =======================================================================
    getStatus() {
        const redisConfig = configManager.getRedisConfig();
        
        return {
            isRunning: this.isRunning,
            connected: this.subscriber.status === 'ready',
            redis_config: {
                host: redisConfig.host,
                port: redisConfig.port,
                enabled: redisConfig.enabled
            },
            subscriber_status: this.subscriber.status,
            subscriptions: this.subscriber.options.lazyConnect ? [] : 
                (this.subscriber.subscription ? Object.keys(this.subscriber.subscription) : [])
        };
    }

    // =======================================================================
    // Redis 연결 테스트
    // =======================================================================
    async testConnection() {
        try {
            const result = await this.subscriber.ping();
            return result === 'PONG';
        } catch (error) {
            console.error('❌ Redis ping 실패:', error.message);
            return false;
        }
    }
}

module.exports = AlarmEventSubscriber;

//C++ Collector → Redis Pub/Sub → Backend Subscriber → WebSocket → Frontend UI