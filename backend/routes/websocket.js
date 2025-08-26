// =============================================================================
// backend/routes/websocket.js - WebSocket 상태 관리 라우트
// WebSocket 서버 상태 조회 및 관리 API
// =============================================================================

const express = require('express');
const router = express.Router();

// =============================================================================
// 🌐 WebSocket 상태 조회 API
// =============================================================================

/**
 * GET /api/websocket/status
 * WebSocket 서버 전체 상태 조회
 */
router.get('/status', (req, res) => {
    try {
        // app.js에서 전달받은 WebSocket 관련 객체들
        const { io, alarmSubscriber } = req.app.locals;
        
        const status = {
            websocket: {
                enabled: !!io,
                server_running: !!io,
                connected_clients: io ? io.engine.clientsCount : 0,
                socket_io_version: io ? getSocketIOVersion() : null,
                server_start_time: req.app.locals.serverStartTime || null
            },
            alarm_subscriber: {
                enabled: !!alarmSubscriber,
                running: alarmSubscriber ? alarmSubscriber.isRunning : false,
                status: alarmSubscriber ? alarmSubscriber.getStatus() : null
            },
            redis_connection: {
                available: !!alarmSubscriber,
                connected: alarmSubscriber ? 
                    (alarmSubscriber.subscriber?.status === 'ready') : false,
                host: process.env.REDIS_HOST || 'localhost',
                port: process.env.REDIS_PORT || 6379
            },
            environment: {
                cors_origins: process.env.CORS_ORIGINS?.split(',') || ['http://localhost:3000'],
                node_env: process.env.NODE_ENV || 'development'
            }
        };

        res.json({
            success: true,
            data: status,
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('❌ WebSocket 상태 조회 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * GET /api/websocket/clients
 * 연결된 WebSocket 클라이언트 목록 조회
 */
router.get('/clients', (req, res) => {
    const { io } = req.app.locals;
    
    if (!io) {
        return res.status(503).json({
            success: false,
            error: 'WebSocket 서버가 비활성화되어 있습니다.',
            suggestion: 'Socket.IO를 설치하고 서버를 재시작하세요: npm install socket.io'
        });
    }

    try {
        const clients = [];
        const rooms = new Map();
        const sockets = io.sockets.sockets;
        
        // 연결된 소켓 정보 수집
        sockets.forEach((socket, socketId) => {
            const socketRooms = Array.from(socket.rooms).filter(room => room !== socketId);
            
            clients.push({
                socket_id: socketId,
                connected_at: new Date(socket.handshake.time).toISOString(),
                address: socket.handshake.address,
                user_agent: socket.handshake.headers['user-agent']?.substring(0, 100) || 'unknown',
                rooms: socketRooms,
                transport: socket.conn.transport.name
            });
            
            // 룸 통계 수집
            socketRooms.forEach(room => {
                rooms.set(room, (rooms.get(room) || 0) + 1);
            });
        });

        // 전체 룸 정보 (어댑터에서)
        const allRooms = Array.from(io.sockets.adapter.rooms.keys());
        
        res.json({
            success: true,
            data: {
                total_clients: clients.length,
                clients: clients,
                rooms_summary: {
                    total_rooms: allRooms.length,
                    room_stats: Object.fromEntries(rooms),
                    all_rooms: allRooms
                },
                server_info: {
                    engine_version: io.engine.protocol,
                    transport_methods: ['polling', 'websocket']
                }
            },
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('❌ WebSocket 클라이언트 조회 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * GET /api/websocket/rooms
 * WebSocket 룸 정보 조회
 */
router.get('/rooms', (req, res) => {
    const { io } = req.app.locals;
    
    if (!io) {
        return res.status(503).json({
            success: false,
            error: 'WebSocket 서버가 비활성화되어 있습니다.'
        });
    }

    try {
        const roomsInfo = [];
        const adapter = io.sockets.adapter;
        
        adapter.rooms.forEach((sockets, roomName) => {
            // 개별 소켓 ID는 제외 (룸 이름과 동일한 경우)
            if (!adapter.sids.has(roomName)) {
                roomsInfo.push({
                    room_name: roomName,
                    client_count: sockets.size,
                    socket_ids: Array.from(sockets),
                    room_type: getRoomType(roomName)
                });
            }
        });

        res.json({
            success: true,
            data: {
                total_rooms: roomsInfo.length,
                rooms: roomsInfo,
                room_types: {
                    tenant: roomsInfo.filter(r => r.room_type === 'tenant').length,
                    device: roomsInfo.filter(r => r.room_type === 'device').length,
                    admin: roomsInfo.filter(r => r.room_type === 'admin').length,
                    custom: roomsInfo.filter(r => r.room_type === 'custom').length
                }
            },
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('❌ WebSocket 룸 조회 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// =============================================================================
// 🚨 실시간 알람 구독자 상태 API
// =============================================================================

/**
 * GET /api/websocket/alarm-subscriber
 * 실시간 알람 구독자 상태 조회
 */
router.get('/alarm-subscriber', (req, res) => {
    const { alarmSubscriber } = req.app.locals;
    
    if (!alarmSubscriber) {
        return res.status(503).json({
            success: false,
            error: 'AlarmEventSubscriber가 비활성화되어 있습니다.',
            suggestion: 'Redis 설정을 확인하고 AlarmEventSubscriber를 초기화하세요.',
            redis_config: {
                host: process.env.REDIS_HOST || 'localhost',
                port: process.env.REDIS_PORT || 6379,
                db: 0
            }
        });
    }

    try {
        const status = alarmSubscriber.getStatus();
        
        res.json({
            success: true,
            data: {
                alarm_subscriber: {
                    running: alarmSubscriber.isRunning,
                    redis_connected: status.connected,
                    subscriptions: status.subscriptions || [],
                    subscriber_status: alarmSubscriber.subscriber?.status || 'unknown'
                },
                redis_config: {
                    host: process.env.REDIS_HOST || 'localhost',
                    port: process.env.REDIS_PORT || 6379,
                    db: 0
                },
                subscription_channels: [
                    'alarms:all',
                    'alarms:critical', 
                    'alarms:high',
                    'tenant:*:alarms',
                    'device:*:alarms'
                ]
            },
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('❌ 알람 구독자 상태 조회 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * POST /api/websocket/alarm-subscriber/restart
 * 알람 구독자 재시작
 */
router.post('/alarm-subscriber/restart', async (req, res) => {
    const { alarmSubscriber } = req.app.locals;
    
    if (!alarmSubscriber) {
        return res.status(503).json({
            success: false,
            error: 'AlarmEventSubscriber를 사용할 수 없습니다.'
        });
    }

    try {
        console.log('🔄 알람 구독자 재시작 요청...');
        
        // 기존 구독 중지
        await alarmSubscriber.stop();
        console.log('✅ 알람 구독자 중지 완료');
        
        // 2초 대기 후 재시작
        setTimeout(async () => {
            try {
                await alarmSubscriber.start();
                console.log('✅ 알람 구독자 재시작 완료');
            } catch (error) {
                console.error('❌ 알람 구독자 재시작 실패:', error);
            }
        }, 2000);
        
        res.json({
            success: true,
            message: '알람 구독자가 재시작되었습니다.',
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('❌ 알람 구독자 재시작 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// =============================================================================
// 🧪 테스트 및 디버깅 API
// =============================================================================

/**
 * POST /api/websocket/test/broadcast
 * WebSocket 브로드캐스트 테스트
 */
router.post('/test/broadcast', (req, res) => {
    const { io } = req.app.locals;
    
    if (!io) {
        return res.status(503).json({
            success: false,
            error: 'WebSocket 서버가 비활성화되어 있습니다.'
        });
    }

    try {
        const { message = '테스트 메시지', room = null } = req.body;
        
        const testData = {
            type: 'test_broadcast',
            message: message,
            timestamp: new Date().toISOString(),
            from: 'backend_api'
        };

        let sentTo = 'all_clients';
        
        if (room) {
            // 특정 룸에만 전송
            io.to(room).emit('test_message', testData);
            sentTo = `room: ${room}`;
        } else {
            // 모든 클라이언트에게 전송
            io.emit('test_message', testData);
        }

        res.json({
            success: true,
            message: 'WebSocket 테스트 메시지가 전송되었습니다.',
            data: {
                sent_to: sentTo,
                test_data: testData,
                connected_clients: io.engine.clientsCount
            },
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('❌ WebSocket 브로드캐스트 테스트 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// =============================================================================
// 🔧 유틸리티 함수들
// =============================================================================

/**
 * Socket.IO 버전 조회
 */
function getSocketIOVersion() {
    try {
        return require('socket.io/package.json').version;
    } catch (error) {
        return 'unknown';
    }
}

/**
 * 룸 타입 판별
 */
function getRoomType(roomName) {
    if (roomName.startsWith('tenant:')) return 'tenant';
    if (roomName.startsWith('device:')) return 'device';
    if (roomName === 'admins') return 'admin';
    return 'custom';
}

module.exports = router;