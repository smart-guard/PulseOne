// =============================================================================
// backend/lib/services/WebSocketService.js - 연결 문제 해결 버전
// =============================================================================

class WebSocketService {
    constructor(server) {
        this.server = server;
        this.io = null;
        this.clients = new Map();
        this.rooms = new Map();
        this.isInitialized = false;
        
        this.initialize();
    }

    // =========================================================================
    // 초기화
    // =========================================================================
    initialize() {
        try {
            const socketIo = require('socket.io');
            console.log('✅ Socket.IO 모듈 로드 성공');
            
            this.setupServer(socketIo);
            this.setupEngineEvents();
            this.setupConnectionHandlers(); // 🎯 핵심: 연결 핸들러 설정
            this.startStatusMonitoring();
            
            this.isInitialized = true;
            console.log('✅ WebSocket 서비스 초기화 완료');
            
        } catch (error) {
            console.warn('⚠️ Socket.IO 모듈이 없습니다. 설치하려면: npm install socket.io');
            console.warn('   WebSocket 기능이 비활성화됩니다.');
            this.isInitialized = false;
        }
    }

    // =========================================================================
    // Socket.IO 서버 설정
    // =========================================================================
    setupServer(socketIo) {
        const corsOrigins = process.env.CORS_ORIGINS?.split(',') || [
            "http://localhost:3000", 
            "http://localhost:5173",
            "http://localhost:8080",
            "http://127.0.0.1:5173",
            "http://127.0.0.1:3000"
        ];

        this.io = socketIo(this.server, {
            cors: {
                origin: corsOrigins,
                methods: ["GET", "POST", "PUT", "DELETE", "OPTIONS"],
                credentials: false,
                allowedHeaders: ["Content-Type", "Authorization", "Accept"]
            },
            
            // 🎯 연결 설정 최적화
            allowEIO3: true,
            transports: ['polling', 'websocket'], // polling 우선
            
            // 🎯 타임아웃 설정
            pingTimeout: 60000,      // 1분
            pingInterval: 25000,     // 25초  
            connectTimeout: 45000,   // 45초
            upgradeTimeout: 10000,   // 업그레이드 타임아웃
            
            // 성능 설정
            maxHttpBufferSize: 1e6,
            httpCompression: true,
            perMessageDeflate: true,
            
            path: '/socket.io/',
            serveClient: false
        });

        console.log('📋 Socket.IO 서버 설정:', {
            corsOrigins,
            transports: ['polling', 'websocket'],
            pingTimeout: '60초',
            connectTimeout: '45초'
        });
    }

    // =========================================================================
    // Engine 이벤트 설정
    // =========================================================================
    setupEngineEvents() {
        if (!this.io) return;

        this.io.engine.on('initial_headers', (headers, req) => {
            console.log('📋 Socket.IO Initial Headers:', {
                url: req.url,
                method: req.method,
                origin: req.headers.origin
            });
            
            // CORS 헤더 설정
            headers['Access-Control-Allow-Origin'] = req.headers.origin || '*';
            headers['Access-Control-Allow-Methods'] = 'GET,HEAD,PUT,PATCH,POST,DELETE';
            headers['Access-Control-Allow-Headers'] = 'Content-Type, Authorization, Accept';
        });

        this.io.engine.on('connection_error', (err) => {
            console.error('❌ Socket.IO Engine 연결 에러:', {
                message: err.message,
                code: err.code,
                type: err.type,
                context: err.context
            });
        });

        this.io.engine.on('connection', (socket) => {
            console.log('🔧 Engine 레벨 연결 성공:', socket.id);
        });

        this.io.engine.on('disconnect', (socket) => {
            console.log('🔧 Engine 레벨 연결 해제:', socket.id);
        });
    }

    // =========================================================================
    // 🎯 핵심 수정: 연결 핸들러 설정
    // =========================================================================
    setupConnectionHandlers() {
        if (!this.io) return;

        console.log('🔗 Socket.IO connection 이벤트 핸들러 설정 중...');

        // 🎯 핵심: connection 이벤트 핸들러
        this.io.on('connection', (socket) => {
            const connectionTime = Date.now();
            
            console.log('🎉 새로운 클라이언트 연결됨!');
            console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
            console.log('   Socket ID:', socket.id);
            console.log('   Client IP:', socket.handshake.address);
            console.log('   Transport:', socket.conn.transport.name);
            console.log('   연결 시간:', new Date().toISOString());
            console.log('   Query Params:', JSON.stringify(socket.handshake.query, null, 2));
            console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');

            // 클라이언트 정보 저장
            this.clients.set(socket.id, {
                socket,
                connectedAt: new Date(),
                rooms: new Set(),
                metadata: this.extractClientMetadata(socket)
            });

            const connectedCount = this.clients.size;
            console.log(`📊 현재 연결된 클라이언트: ${connectedCount}명`);

            // 🎯 즉시 연결 확인 메시지 전송
            socket.emit('connection_status', {
                status: 'connected',
                socket_id: socket.id,
                server_time: new Date().toISOString(),
                transport: socket.conn.transport.name,
                client_count: connectedCount
            });

            // 🎯 이벤트 핸들러 설정
            this.setupSocketEvents(socket);

            // 🎯 연결 해제 핸들러
            socket.on('disconnect', (reason) => {
                this.handleDisconnection(socket, reason, connectionTime);
            });

            // 🎯 에러 핸들러
            socket.on('error', (error) => {
                console.error(`❌ Socket 에러 (${socket.id}):`, error);
            });
        });

        console.log('✅ Socket.IO connection 이벤트 핸들러 설정 완료');
    }

    // =========================================================================
    // Socket 이벤트 설정
    // =========================================================================
    setupSocketEvents(socket) {
        // 🎯 테스트 메시지 핸들러
        socket.on('test-message', (data) => {
            console.log('📨 테스트 메시지 수신:', data);
            socket.emit('test-response', {
                message: '서버 응답',
                received_data: data,
                server_time: new Date().toISOString(),
                socket_id: socket.id,
                transport: socket.conn.transport.name
            });
        });

        // 🎯 룸 관리
        socket.on('join_tenant', (tenantId) => {
            this.joinRoom(socket, `tenant:${tenantId}`, { tenantId });
        });

        socket.on('join_admin', () => {
            this.joinRoom(socket, 'admins', { role: 'admin' });
        });

        // 🎯 알람 처리
        socket.on('acknowledge_alarm', (data) => {
            console.log('📝 알람 확인:', data);
            socket.emit('alarm_acknowledged', {
                ...data,
                timestamp: new Date().toISOString(),
                success: true
            });
        });

        // 🎯 개발 모드에서 모든 이벤트 로깅
        if (process.env.NODE_ENV === 'development') {
            socket.onAny((eventName, ...args) => {
                if (!['ping', 'pong'].includes(eventName)) {
                    console.log(`📡 Socket ${socket.id} 이벤트 수신: ${eventName}`, 
                               args.length > 0 ? args : '(no args)');
                }
            });
        }
    }

    // =========================================================================
    // 룸 관리
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

        console.log(`👥 Socket ${socket.id} joined room: ${roomName}`);
        
        socket.emit('room_joined', {
            room: roomName,
            metadata,
            timestamp: new Date().toISOString(),
            success: true
        });
    }

    // =========================================================================
    // 연결 해제 처리
    // =========================================================================
    handleDisconnection(socket, reason, connectionTime) {
        const duration = Date.now() - connectionTime;
        
        console.log('👋 클라이언트 연결 해제:', {
            socketId: socket.id,
            reason,
            duration: Math.round(duration / 1000) + '초',
            remaining: Math.max(0, this.clients.size - 1)
        });

        // 클라이언트 정보에서 룸 제거
        const client = this.clients.get(socket.id);
        if (client) {
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
    }

    // =========================================================================
    // 브로드캐스트 메서드들
    // =========================================================================
    broadcastToAll(event, data) {
        if (!this.io) return false;
        
        this.io.emit(event, data);
        console.log(`📡 전체 브로드캐스트: ${event}`);
        return true;
    }

    broadcastToRoom(roomName, event, data) {
        if (!this.io) return false;
        
        this.io.to(roomName).emit(event, data);
        console.log(`📡 룸 브로드캐스트 (${roomName}): ${event}`);
        return true;
    }

    broadcastToTenant(tenantId, event, data) {
        return this.broadcastToRoom(`tenant:${tenantId}`, event, data);
    }

    broadcastToAdmins(event, data) {
        return this.broadcastToRoom('admins', event, data);
    }

    // =========================================================================
    // 알람 전용 메서드들
    // =========================================================================
    sendAlarm(alarmData) {
        if (!this.io) return false;

        const event = {
            type: 'alarm_triggered',
            data: alarmData,
            timestamp: new Date().toISOString()
        };

        // 해당 테넌트에게 전송
        this.broadcastToTenant(alarmData.tenant_id, 'alarm:new', event);

        // 긴급 알람은 관리자에게도
        if (alarmData.severity_level >= 3) {
            this.broadcastToAdmins('alarm:critical', {
                ...event,
                type: 'critical_alarm',
                requires_action: true
            });
        }

        return true;
    }

    // =========================================================================
    // 상태 조회 메서드들
    // =========================================================================
    getStatus() {
        if (!this.io) {
            return {
                enabled: false,
                reason: 'Socket.IO not initialized'
            };
        }

        return {
            enabled: true,
            stats: {
                engine_clients: this.io.engine.clientsCount,
                socket_clients: this.clients.size,
                rooms: this.rooms.size,
                uptime: process.uptime(),
                initialized: this.isInitialized
            }
        };
    }

    getClients() {
        const clients = [];
        
        this.clients.forEach((client, socketId) => {
            clients.push({
                socket_id: socketId,
                connected_at: client.connectedAt.toISOString(),
                rooms: Array.from(client.rooms),
                transport: client.socket.conn.transport.name,
                ...client.metadata
            });
        });

        return clients;
    }

    getRooms() {
        const rooms = {};
        
        this.rooms.forEach((clients, roomName) => {
            rooms[roomName] = {
                client_count: clients.size,
                clients: Array.from(clients),
                type: this.getRoomType(roomName)
            };
        });

        return rooms;
    }

    // =========================================================================
    // 유틸리티 메서드들
    // =========================================================================
    extractClientMetadata(socket) {
        return {
            address: socket.handshake.address,
            user_agent: socket.handshake.headers['user-agent']?.substring(0, 100),
            referer: socket.handshake.headers.referer,
            query: socket.handshake.query
        };
    }

    getRoomType(roomName) {
        if (roomName.startsWith('tenant:')) return 'tenant';
        if (roomName.startsWith('device:')) return 'device';
        if (roomName === 'admins') return 'admin';
        return 'custom';
    }

    startStatusMonitoring() {
        if (process.env.NODE_ENV !== 'development') return;

        setInterval(() => {
            if (!this.io) return;

            const engineClients = this.io.engine.clientsCount;
            const socketClients = this.clients.size;
            const rooms = this.rooms.size;

            if (engineClients > 0 || socketClients > 0) {
                console.log('📊 WebSocket 상태:', {
                    engine_clients: engineClients,
                    socket_clients: socketClients,
                    rooms: rooms,
                    timestamp: new Date().toISOString()
                });
                
                if (engineClients !== socketClients) {
                    console.warn('⚠️ Engine과 Socket 클라이언트 수 불일치!');
                    console.warn(`   Engine: ${engineClients}, Socket: ${socketClients}`);
                }
            }
        }, 30000);
    }
}

module.exports = WebSocketService;