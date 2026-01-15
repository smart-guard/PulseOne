const express = require('express');
const router = express.Router();

// 응답 헬퍼
function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code,
        timestamp: new Date().toISOString()
    };
}

/**
 * GET /api/websocket/status
 * WebSocket 서버 전체 상태 조회
 */
router.get('/status', (req, res) => {
    try {
        const { io, alarmSubscriber } = req.app.locals;

        const status = {
            websocket: {
                enabled: !!io,
                server_running: !!io,
                connected_clients: io ? io.engine.clientsCount : 0
            },
            alarm_subscriber: {
                enabled: !!alarmSubscriber,
                running: alarmSubscriber ? alarmSubscriber.isRunning : false
            }
        };

        res.json(createResponse(true, status));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'WEBSOCKET_STATUS_ERROR'));
    }
});

/**
 * GET /api/websocket/clients
 * 연결된 WebSocket 클라이언트 목록 조회
 */
router.get('/clients', (req, res) => {
    const { io } = req.app.locals;
    if (!io) return res.status(503).json(createResponse(false, null, 'WebSocket server disabled'));

    try {
        const clients = [];
        io.sockets.sockets.forEach((socket, socketId) => {
            clients.push({
                socket_id: socketId,
                connected_at: new Date(socket.handshake.time).toISOString(),
                rooms: Array.from(socket.rooms).filter(room => room !== socketId)
            });
        });

        res.json(createResponse(true, { total_clients: clients.length, clients }));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'WEBSOCKET_CLIENTS_ERROR'));
    }
});

/**
 * POST /api/websocket/alarm-subscriber/restart
 * 알람 구독자 재시작
 */
router.post('/alarm-subscriber/restart', async (req, res) => {
    const { alarmSubscriber } = req.app.locals;
    if (!alarmSubscriber) return res.status(503).json(createResponse(false, null, 'Alarm subscriber disabled'));

    try {
        await alarmSubscriber.stop();
        setTimeout(async () => {
            try {
                await alarmSubscriber.start();
            } catch (error) {
                console.error('Alarm subscriber restart failed:', error);
            }
        }, 2000);

        res.json(createResponse(true, null, 'Alarm subscriber restarting...'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_SUBSCRIBER_RESTART_ERROR'));
    }
});

module.exports = router;