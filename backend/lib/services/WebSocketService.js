// =============================================================================
// backend/lib/services/WebSocketService.js - 프로덕션 최적화 버전
// =============================================================================

class WebSocketService {
    constructor(server) {
        this.server = server;
        this.io = null;
        this.clients = new Map();
        this.rooms = new Map();
        this.isInitialized = false;
        this.connectionCount = 0;
        this.isDevelopment = process.env.NODE_ENV === 'development';

        this.statusMonitorInterval = null;

        this.log('WebSocketService 초기화 시작...');
        this.initialize();
    }

    // =========================================================================
    // 로깅 유틸리티 - 환경별 분리
    // =========================================================================
    log(message, level = 'info') {
        if (!this.isDevelopment && level === 'debug') return;

        const timestamp = new Date().toISOString();
        const prefix = level === 'error' ? '❌' : level === 'warn' ? '⚠️' : '✅';
        console.log(`${prefix} [${timestamp}] ${message}`);
    }

    logDebug(message) {
        this.log(message, 'debug');
    }

    // =========================================================================
    // 초기화 - 단순화 및 안정화
    // =========================================================================
    initialize() {
        try {
            const socketIo = require('socket.io');
            this.log('Socket.IO 모듈 로드 성공');

            // 단계별 초기화
            this.setupServer(socketIo);
            this.setupConnectionHandling();

            if (this.isDevelopment) {
                this.startStatusMonitoring();
            }

            this.isInitialized = true;
            this.log('WebSocket 서비스 초기화 완료');

        } catch (error) {
            this.log('Socket.IO 모듈이 없습니다. npm install socket.io', 'warn');
            this.isInitialized = false;
        }
    }

    // =========================================================================
    // 서버 설정 - 프로덕션 최적화
    // =========================================================================
    setupServer(socketIo) {
        const corsOrigins = this.getCorsOrigins();

        this.io = socketIo(this.server, {
            cors: {
                origin: corsOrigins,
                methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
                credentials: true
            },

            // 연결 설정
            transports: ['polling', 'websocket'],
            allowEIO3: true,

            // 타임아웃 - 프로덕션 친화적
            pingTimeout: this.isDevelopment ? 60000 : 30000,
            pingInterval: 25000,
            connectTimeout: this.isDevelopment ? 45000 : 30000,
            upgradeTimeout: 20000,

            // 성능 최적화
            maxHttpBufferSize: 1e6,
            httpCompression: true,
            perMessageDeflate: false, // CPU 사용량 감소

            path: '/socket.io/',
            serveClient: false,

            // 요청 검증 - 간소화
            allowRequest: (req, callback) => {
                if (this.isDevelopment) {
                    this.logDebug(`연결 요청: ${req.headers.origin || 'no-origin'}`);
                }
                callback(null, true);
            }
        });

        this.log(`Socket.IO 서버 설정 완료 (개발모드: ${this.isDevelopment})`);
    }

    getCorsOrigins() {
        if (process.env.CORS_ORIGINS) {
            return process.env.CORS_ORIGINS.split(',');
        }

        return this.isDevelopment ? [
            'http://localhost:3000',
            'http://localhost:5173',
            'http://localhost:8080',
            'http://127.0.0.1:5173',
            'http://127.0.0.1:3000'
        ] : ['https://your-production-domain.com'];
    }

    // =========================================================================
    // 연결 처리 - 단일 통합 방식
    // =========================================================================
    setupConnectionHandling() {
        if (!this.io) {
            this.log('Socket.IO 인스턴스가 없습니다', 'error');
            return;
        }

        // Engine 이벤트 (선택적 디버깅)
        if (this.isDevelopment) {
            this.setupEngineDebugEvents();
        }

        // Socket 연결 이벤트 (핵심)
        this.io.on('connection', (socket) => {
            this.handleNewConnection(socket);
        });

        // 에러 처리
        this.io.on('connect_error', (error) => {
            this.log(`Socket.IO 연결 에러: ${error.message}`, 'error');
        });

        this.log('Socket.IO 이벤트 핸들러 등록 완료');
    }

    setupEngineDebugEvents() {
        if (!this.io.engine) return;

        this.io.engine.on('connection', (socket) => {
            this.logDebug(`Engine 연결: ${socket.id} (${socket.transport.name})`);
        });

        this.io.engine.on('connection_error', (err) => {
            this.log(`Engine 연결 에러: ${err.message}`, 'error');
        });
    }

    // =========================================================================
    // 새 연결 처리 - 최적화
    // =========================================================================
    handleNewConnection(socket) {
        const connectionTime = Date.now();
        this.connectionCount++;

        // 개발 모드에서만 상세 로깅
        if (this.isDevelopment) {
            this.log(`Socket 연결: ${socket.id} (#${this.connectionCount})`);
            this.logDebug(`Transport: ${socket.conn.transport.name}`);
            this.logDebug(`Client IP: ${socket.handshake.address}`);
        } else {
            this.log(`Socket 연결: ${socket.id}`);
        }

        // 클라이언트 정보 저장 - 메모리 효율적
        this.clients.set(socket.id, {
            socket,
            connectedAt: Date.now(),
            rooms: new Set(),
            metadata: this.extractBasicMetadata(socket)
        });

        // 즉시 응답
        this.sendConnectionConfirmation(socket);

        // 이벤트 핸들러 설정
        this.setupSocketEvents(socket);

        // 연결 해제 핸들러
        socket.on('disconnect', (reason) => {
            this.handleDisconnection(socket, reason, connectionTime);
        });

        // 에러 핸들러
        socket.on('error', (error) => {
            this.log(`Socket 에러 (${socket.id}): ${error.message}`, 'error');
        });

        // Transport 업그레이드 추적 (개발 모드)
        if (this.isDevelopment) {
            socket.conn.on('upgrade', () => {
                this.logDebug(`Transport 업그레이드: ${socket.id} → ${socket.conn.transport.name}`);
            });
        }
    }

    sendConnectionConfirmation(socket) {
        const clientCount = this.clients.size;

        socket.emit('connection_status', {
            status: 'connected',
            socket_id: socket.id,
            server_time: new Date().toISOString(),
            transport: socket.conn.transport.name,
            client_count: clientCount,
            connection_number: this.connectionCount
        });

        socket.emit('welcome', {
            message: 'PulseOne WebSocket에 오신 것을 환영합니다!',
            server_version: '3.0.0',
            timestamp: new Date().toISOString()
        });
    }

    // =========================================================================
    // Socket 이벤트 핸들러 - 입력 검증 강화
    // =========================================================================
    setupSocketEvents(socket) {
        // 테스트 메시지
        socket.on('test-message', (data) => {
            try {
                const validData = this.validateTestMessage(data);
                socket.emit('test-response', {
                    message: '서버 응답 성공!',
                    received_data: validData,
                    server_time: new Date().toISOString(),
                    socket_id: socket.id,
                    transport: socket.conn.transport.name
                });
            } catch (error) {
                this.log(`테스트 메시지 처리 실패: ${error.message}`, 'error');
                socket.emit('error', { message: '잘못된 메시지 형식' });
            }
        });

        // Ping/Pong
        socket.on('ping', (callback) => {
            if (typeof callback === 'function') {
                callback('pong');
            }
            socket.emit('pong', { timestamp: Date.now() });
        });

        // 룸 조인 - 입력 검증
        socket.on('join_tenant', (tenantId) => {
            if (this.validateTenantId(tenantId)) {
                this.joinRoom(socket, `tenant:${tenantId}`, { tenantId });
            } else {
                socket.emit('error', { message: '유효하지 않은 테넌트 ID' });
            }
        });

        socket.on('join_admin', () => {
            this.joinRoom(socket, 'admins', { role: 'admin' });
        });

        socket.on('monitor_device', (deviceId) => {
            if (this.validateDeviceId(deviceId)) {
                this.joinRoom(socket, `device:${deviceId}`, { deviceId });
            } else {
                socket.emit('error', { message: '유효하지 않은 디바이스 ID' });
            }
        });

        // 알람 확인 - 검증 강화
        socket.on('acknowledge_alarm', (data) => {
            try {
                const validData = this.validateAlarmAck(data);
                this.handleAlarmAcknowledgment(socket, validData);
            } catch (error) {
                this.log(`알람 확인 처리 실패: ${error.message}`, 'error');
                socket.emit('error', { message: '알람 확인 데이터가 유효하지 않습니다' });
            }
        });

        // 개발 모드에서만 모든 이벤트 로깅
        if (this.isDevelopment) {
            socket.onAny((eventName, ...args) => {
                if (!['ping', 'pong', 'heartbeat'].includes(eventName)) {
                    this.logDebug(`Socket ${socket.id} 이벤트: ${eventName}`);
                }
            });
        }
    }

    // =========================================================================
    // 입력 검증 메서드들
    // =========================================================================
    validateTestMessage(data) {
        if (!data || typeof data !== 'object') {
            throw new Error('데이터가 객체가 아닙니다');
        }

        return {
            message: String(data.message || '').substring(0, 1000),
            timestamp: data.timestamp || new Date().toISOString(),
            tenant_id: Number(data.tenant_id) || 1
        };
    }

    validateTenantId(tenantId) {
        const id = Number(tenantId);
        return Number.isInteger(id) && id > 0 && id < 10000;
    }

    validateDeviceId(deviceId) {
        const id = Number(deviceId);
        return Number.isInteger(id) && id > 0 && id < 100000;
    }

    validateAlarmAck(data) {
        if (!data || typeof data !== 'object') {
            throw new Error('알람 확인 데이터가 객체가 아닙니다');
        }

        const occurrenceId = Number(data.occurrence_id);
        if (!Number.isInteger(occurrenceId) || occurrenceId <= 0) {
            throw new Error('유효하지 않은 occurrence_id');
        }

        return {
            occurrence_id: occurrenceId,
            user_id: Number(data.user_id) || 1,
            comment: String(data.comment || '').substring(0, 500),
            timestamp: new Date().toISOString()
        };
    }

    // =========================================================================
    // 룸 관리 - 메모리 최적화
    // =========================================================================
    joinRoom(socket, roomName, metadata = {}) {
        socket.join(roomName);

        // 클라이언트 룸 추가
        const client = this.clients.get(socket.id);
        if (client) {
            client.rooms.add(roomName);
        }

        // 룸 통계 업데이트
        if (!this.rooms.has(roomName)) {
            this.rooms.set(roomName, new Set());
        }
        this.rooms.get(roomName).add(socket.id);

        if (this.isDevelopment) {
            this.logDebug(`Socket ${socket.id} joined room: ${roomName}`);
        }

        socket.emit('room_joined', {
            room: roomName,
            metadata,
            timestamp: new Date().toISOString(),
            success: true
        });
    }

    // =========================================================================
    // 연결 해제 처리 - 메모리 정리 강화
    // =========================================================================
    handleDisconnection(socket, reason, connectionTime) {
        const duration = Date.now() - connectionTime;

        this.log(`Socket 해제: ${socket.id} (${Math.round(duration / 1000)}초, ${reason})`);

        // 클라이언트 정보 가져오기
        const client = this.clients.get(socket.id);

        // 룸에서 제거
        if (client && client.rooms) {
            client.rooms.forEach(roomName => {
                const roomClients = this.rooms.get(roomName);
                if (roomClients) {
                    roomClients.delete(socket.id);
                    if (roomClients.size === 0) {
                        this.rooms.delete(roomName);
                    }
                }
            });
        }

        // 클라이언트 제거
        this.clients.delete(socket.id);

        if (this.isDevelopment) {
            this.logDebug(`남은 연결: ${this.clients.size}명`);
        }
    }

    // =========================================================================
    // 알람 처리
    // =========================================================================
    handleAlarmAcknowledgment(socket, data) {
        this.log(`알람 확인: ${data.occurrence_id}`);

        socket.emit('alarm_acknowledged', {
            ...data,
            success: true
        });

        // 다른 클라이언트들에게 브로드캐스트
        socket.broadcast.emit('alarm:acknowledged', {
            occurrence_id: data.occurrence_id,
            acknowledged_by: data.user_id,
            comment: data.comment,
            timestamp: data.timestamp
        });
    }

    sendAlarm(alarmData) {
        if (!this.io || !this.validateAlarmData(alarmData)) {
            return false;
        }

        const event = {
            type: 'alarm_triggered',
            data: alarmData,
            timestamp: new Date().toISOString()
        };

        // 테넌트 브로드캐스트
        this.broadcastToTenant(alarmData.tenant_id, 'alarm:new', event);

        // 긴급 알람
        if (alarmData.severity_level >= 3) {
            this.broadcastToAdmins('alarm:critical', {
                ...event,
                type: 'critical_alarm'
            });
        }

        this.log(`알람 전송: ${alarmData.id} (심각도: ${alarmData.severity_level})`);
        return true;
    }

    validateAlarmData(data) {
        return data &&
            typeof data.id !== 'undefined' &&
            typeof data.tenant_id === 'number' &&
            typeof data.severity_level === 'number';
    }

    // =========================================================================
    // 브로드캐스트 메서드들
    // =========================================================================
    broadcastToAll(event, data) {
        if (!this.io) return false;

        this.io.emit(event, data);
        this.logDebug(`전체 브로드캐스트 (${this.clients.size}명): ${event}`);
        return true;
    }

    broadcastToRoom(roomName, event, data) {
        if (!this.io) return false;

        this.io.to(roomName).emit(event, data);
        const roomSize = this.rooms.get(roomName)?.size || 0;
        this.logDebug(`룸 브로드캐스트 ${roomName} (${roomSize}명): ${event}`);
        return true;
    }

    broadcastToTenant(tenantId, event, data) {
        return this.broadcastToRoom(`tenant:${tenantId}`, event, data);
    }

    broadcastToAdmins(event, data) {
        return this.broadcastToRoom('admins', event, data);
    }

    // =========================================================================
    // 상태 조회 - 간소화
    // =========================================================================
    getStatus() {
        if (!this.io) {
            return { enabled: false, reason: 'Socket.IO not initialized' };
        }

        const engineCount = this.io.engine?.clientsCount || 0;
        const socketCount = this.clients.size;

        return {
            enabled: true,
            initialized: this.isInitialized,
            stats: {
                engine_clients: engineCount,
                socket_clients: socketCount,
                rooms: this.rooms.size,
                uptime: Math.floor(process.uptime()),
                connection_count: this.connectionCount
            },
            health: engineCount === socketCount ? 'healthy' : 'warning'
        };
    }

    getClients() {
        const clients = [];

        this.clients.forEach((client, socketId) => {
            clients.push({
                socket_id: socketId,
                connected_at: new Date(client.connectedAt).toISOString(),
                duration_seconds: Math.floor((Date.now() - client.connectedAt) / 1000),
                rooms: Array.from(client.rooms),
                transport: client.socket.conn.transport.name
            });
        });

        return { total: clients.length, clients };
    }

    // =========================================================================
    // 유틸리티
    // =========================================================================
    extractBasicMetadata(socket) {
        return {
            address: socket.handshake.address,
            user_agent: socket.handshake.headers['user-agent']?.substring(0, 100),
            origin: socket.handshake.headers.origin,
            query: socket.handshake.query
        };
    }

    // =========================================================================
    // 모니터링 - 개발 모드 전용
    // =========================================================================
    startStatusMonitoring() {
        this.statusMonitorInterval = setInterval(() => {
            if (!this.io) return;

            const engineCount = this.io.engine.clientsCount;
            const socketCount = this.clients.size;

            if (engineCount > 0 || socketCount > 0) {
                this.logDebug(`상태: Engine(${engineCount}) Socket(${socketCount}) Rooms(${this.rooms.size})`);

                if (engineCount !== socketCount) {
                    this.log(`Engine/Socket 불일치: ${engineCount}/${socketCount}`, 'warn');
                }
            }
        }, 30000);
    }

    // =========================================================================
    // 테스트 및 정리
    // =========================================================================
    testConnection() {
        if (!this.io) return { success: false, reason: 'Not initialized' };

        this.broadcastToAll('server:test', {
            type: 'connection_test',
            timestamp: new Date().toISOString(),
            clients: this.clients.size
        });

        return { success: true, clients: this.clients.size };
    }

    // 정리 메서드
    cleanup() {
        if (this.statusMonitorInterval) {
            clearInterval(this.statusMonitorInterval);
            this.statusMonitorInterval = null;
        }

        if (this.io) {
            this.io.close();
            this.io = null;
        }

        this.clients.clear();
        this.rooms.clear();
        this.isInitialized = false;

        this.log('WebSocket 서비스 정리 완료');
    }
}

module.exports = WebSocketService;