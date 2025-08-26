import { defineConfig } from 'vite'

export default defineConfig({
  server: {
    host: '0.0.0.0',
    port: 5173,
    proxy: {
      // HTTP API 프록시 (기존)
      '/api': {
        target: 'http://backend:3000', // Docker 서비스명 사용
        changeOrigin: true,
        secure: false,
        configure: (proxy, _options) => {
          proxy.on('error', (err, _req, _res) => {
            console.log('🚨 API Proxy error:', err);
          });
          proxy.on('proxyReq', (proxyReq, req, _res) => {
            console.log(`🔄 API Proxying: ${req.method} ${req.url} -> http://backend:3000${req.url}`);
          });
          proxy.on('proxyRes', (proxyRes, req, _res) => {
            console.log(`✅ API Response: ${proxyRes.statusCode} ${req.url}`);
          });
        }
      },
      
      // WebSocket 프록시 추가 (Socket.IO용)
      '/socket.io': {
        target: 'http://backend:3000',
        changeOrigin: true,
        ws: true, // WebSocket 지원 활성화
        secure: false,
        configure: (proxy, _options) => {
          proxy.on('error', (err, _req, _res) => {
            console.log('🚨 WebSocket Proxy error:', err);
          });
          proxy.on('proxyReq', (proxyReq, req, _res) => {
            console.log(`🔄 WS Proxying: ${req.method} ${req.url} -> http://backend:3000${req.url}`);
          });
          proxy.on('proxyRes', (proxyRes, req, _res) => {
            console.log(`✅ WS Response: ${proxyRes.statusCode} ${req.url}`);
          });
          // WebSocket 업그레이드 핸들링
          proxy.on('proxyReqWs', (proxyReq, req, socket, options, head) => {
            console.log('🔄 WebSocket upgrade proxying to backend:3000');
          });
          proxy.on('open', (proxySocket) => {
            console.log('✅ WebSocket proxy connection opened');
          });
          proxy.on('close', (res, socket, head) => {
            console.log('🔌 WebSocket proxy connection closed');
          });
        }
      }
    }
  },
  
  // 빌드 설정
  build: {
    outDir: 'dist',
    sourcemap: true,
    rollupOptions: {
      output: {
        manualChunks: {
          vendor: ['socket.io-client']
        }
      }
    }
  },
  
  // 환경변수 설정 (Docker용)
  define: {
    __API_URL__: JSON.stringify(process.env.VITE_API_URL || 'http://localhost:3000'),
    __WS_URL__: JSON.stringify(process.env.VITE_WEBSOCKET_URL || 'ws://localhost:3000')
  },
  
  // 개발 서버 최적화
  optimizeDeps: {
    include: ['socket.io-client']
  }
})