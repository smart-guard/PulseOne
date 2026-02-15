/**
 * ROS Bridge Simulator for PulseOne
 * Mimics a ROS Bridge server (TCP) sending JSON messages.
 */
const net = require('net');

const PORT = 9090;
const HOST = '0.0.0.0';

const server = net.createServer((socket) => {
    console.log(`[ROS-Sim] Client connected: ${socket.remoteAddress}:${socket.remotePort}`);

    const interval = setInterval(() => {
        // Mock Robot Telemetry
        const payload = {
            op: "publish",
            topic: "/robot/telemetry",
            msg: {
                battery_voltage: Number((24 + Math.random() * 2).toFixed(2)),
                linear_velocity: Number((Math.random() * 1.5).toFixed(2)),
                angular_velocity: Number((Math.random() * 0.5).toFixed(2)),
                status: "normal",
                timestamp: Date.now()
            }
        };

        const jsonStr = JSON.stringify(payload);
        socket.write(jsonStr);
        console.log(`[ROS-Sim] Published to /robot/telemetry: ${jsonStr}`);
    }, 2000);

    socket.on('data', (data) => {
        console.log(`[ROS-Sim] Received: ${data.toString()}`);
        try {
            const cmd = JSON.parse(data.toString());
            if (cmd.op === 'subscribe') {
                console.log(`[ROS-Sim] Client subscribed to: ${cmd.topic}`);
            }
        } catch (e) {
            // Noise or partial chunk
        }
    });

    socket.on('end', () => {
        clearInterval(interval);
        console.log('[ROS-Sim] Client disconnected');
    });

    socket.on('error', (err) => {
        clearInterval(interval);
        console.error(`[ROS-Sim] Socket Error: ${err.message}`);
    });
});

server.listen(PORT, HOST, () => {
    console.log(`🚀 ROS Simulator running on tcp://${HOST}:${PORT}`);
});
