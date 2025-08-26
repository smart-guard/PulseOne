// ============================================================================
// frontend/src/services/AlarmWebSocketService.ts
// 완전 디버깅 버전 - WebSocket 연결 문제 해결
// ============================================================================

import { io, Socket } from 'socket.io-client';

export interface AlarmEvent {
  occurrence_id: number;
  rule_id: number;
  tenant_id: number;
  device_id: string;
  point_id: number;
  message: string;
  severity: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  severity_level: number;
  state: number;
  timestamp: number;
  source_name: string;
  location: string;
  trigger_value: any;
  formatted_time: string;
}

export interface WebSocketAlarmEvent {
  type: 'alarm_triggered' | 'critical_alarm' | 'alarm_state_change';
  data: AlarmEvent;
  timestamp: string;
}

export interface AlarmAcknowledgment {
  occurrence_id: number;
  acknowledged_by: number;
  comment?: string;
  timestamp: string;
}

export interface ConnectionStatus {
  status: 'connected' | 'disconnected' | 'connecting' | 'error';
  tenant_id?: number;
  socket_id?: string;
  timestamp: string;
  error?: string;
  reason?: string;
}

type AlarmEventHandler = (event: WebSocketAlarmEvent) => void;
type AcknowledgmentHandler = (ack: AlarmAcknowledgment) => void;
type ConnectionHandler = (status: ConnectionStatus) => void;
type ErrorHandler = (error: string) => void;

export class AlarmWebSocketService {
  private socket: Socket | null = null;
  private isConnecting = false;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 5;
  private reconnectDelay = 1000;
  private connectionStartTime: number = 0;

  private alarmEventHandlers: Set<AlarmEventHandler> = new Set();
  private acknowledgmentHandlers: Set<AcknowledgmentHandler> = new Set();
  private connectionHandlers: Set<ConnectionHandler> = new Set();
  private errorHandlers: Set<ErrorHandler> = new Set();

  constructor(private tenantId: number = 1) {
    this.enableDebugMode();
  }

  // =========================================================================
  // 디버깅 모드 활성화
  // =========================================================================
  private enableDebugMode(): void {
    if (process.env.NODE_ENV === 'development') {
      // Socket.IO 클라이언트 디버깅 활성화
      (window as any).localStorage.debug = 'socket.io-client:*';
      console.log('🐛 Socket.IO 클라이언트 디버깅 모드 활성화');
    }
  }

  // =========================================================================
  // 백엔드 URL 결정 로직 (완전 디버깅 버전)
  // =========================================================================
    private getBackendUrl(): string {
        // 1단계: 환경변수 직접 확인
        const apiUrl = import.meta.env?.VITE_API_URL;
        if (apiUrl) {
        console.log('✅ VITE_API_URL 사용:', apiUrl);
        return apiUrl;
        }

        // 2단계: 개발 환경 자동 감지
        const currentHost = window.location.hostname;
        const currentPort = window.location.port;
        
        if (currentPort === '5173' && (currentHost === 'localhost' || currentHost === '127.0.0.1')) {
        const backendUrl = 'http://localhost:3000';
        console.log('✅ Vite 개발서버 감지 -> Backend URL:', backendUrl);
        return backendUrl;
        }

        // 3단계: 기본값
        const defaultUrl = 'http://localhost:3000';
        console.log('✅ 기본 Backend URL 사용:', defaultUrl);
        return defaultUrl;
  }

  // =========================================================================
  // WebSocket 연결 (완전 디버깅 버전)
  // =========================================================================
  connect(): Promise<void> {
    if (this.socket && this.socket.connected) {
      console.log('✅ 이미 연결됨 - Socket ID:', this.socket.id);
      return Promise.resolve();
    }

    if (this.isConnecting) {
      console.log('⏳ 연결이 이미 진행 중');
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      try {
        this.isConnecting = true;
        this.connectionStartTime = Date.now();
        
        console.log('🚀 WebSocket 연결 시작...');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');

        const backendUrl = this.getBackendUrl();
        console.log('🎯 최종 Backend URL:', backendUrl);

        // 🔧 최적화된 Socket.IO 옵션
        const socketOptions = {
          // 기본 설정
          path: '/socket.io/',
          autoConnect: true,
          forceNew: true,
          
          // 🎯 Transport 설정 - polling 우선으로 안정성 확보
          transports: ['polling', 'websocket'],
          upgrade: true,
          rememberUpgrade: false,
          
          // 🎯 타임아웃 설정 - 더 관대하게
          timeout: 30000,           // 30초
          connectTimeout: 60000,    // 60초
          
          // 재연결 설정
          reconnection: true,
          reconnectionDelay: this.reconnectDelay,
          reconnectionAttempts: this.maxReconnectAttempts,
          reconnectionDelayMax: 10000,
          randomizationFactor: 0.5,
          
          // 🎯 CORS 및 보안 설정 최적화
          withCredentials: false,
          secure: false,
          rejectUnauthorized: false,
          
          // 🎯 추가 헤더 최소화
          extraHeaders: {
            'Accept': 'application/json'
          },
          
          // 🎯 쿼리 파라미터
          query: {
            client: 'AlarmWebSocketService',
            version: '2.0.0',
            tenant_id: this.tenantId,
            // 타임스탬프 추가로 캐시 방지
            t: Date.now()
          },
          
          // 성능 설정
          httpCompression: false,
          perMessageDeflate: false,
          maxHttpBufferSize: 1e6
        };

        console.log('📋 Socket.IO 연결 설정:');
        console.log('   URL:', backendUrl);
        console.log('   Path:', socketOptions.path);
        console.log('   Transports:', socketOptions.transports);
        console.log('   Timeout:', socketOptions.timeout + 'ms');
        console.log('   Query:', JSON.stringify(socketOptions.query, null, 2));

        // Socket 생성
        this.socket = io(backendUrl, socketOptions);
        console.log('🔌 Socket.IO 클라이언트 생성 완료');

        // 🎯 이벤트 핸들러 설정 (connect 전에)
        this.setupEventHandlers();

        // 🎯 타임아웃 설정 (Promise 레벨에서)
        const connectionTimeout = setTimeout(() => {
          if (this.isConnecting) {
            console.error('❌ 연결 타임아웃 (60초)');
            this.isConnecting = false;
            
            this.notifyConnectionChange({
              status: 'error',
              timestamp: new Date().toISOString(),
              error: '연결 타임아웃'
            });
            
            reject(new Error('연결 타임아웃'));
          }
        }, 60000);

        // ✅ 연결 성공 이벤트
        this.socket.once('connect', () => {
          clearTimeout(connectionTimeout);
          const connectionTime = Date.now() - this.connectionStartTime;
          
          console.log('🎉 WebSocket 연결 성공!');
          console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
          console.log('   Socket ID:', this.socket?.id);
          console.log('   Transport:', this.socket?.io.engine.transport.name);
          console.log('   연결 시간:', connectionTime + 'ms');
          console.log('   재시도 횟수:', this.reconnectAttempts);
          console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
          
          this.isConnecting = false;
          this.reconnectAttempts = 0;
          
          // 🎯 즉시 연결 테스트
          this.sendConnectionTest();
          
          // 룸 조인
          this.joinTenantRoom();
          this.joinAdminRoom();
          
          // 연결 상태 알림
          this.notifyConnectionChange({
            status: 'connected',
            tenant_id: this.tenantId,
            socket_id: this.socket?.id,
            timestamp: new Date().toISOString()
          });
          
          resolve();
        });

        // ❌ 연결 실패 이벤트
        this.socket.once('connect_error', (error) => {
          clearTimeout(connectionTimeout);
          const connectionTime = Date.now() - this.connectionStartTime;
          
          console.error('❌ WebSocket 연결 실패!');
          console.error('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
          console.error('   에러 타입:', error.type || 'unknown');
          console.error('   에러 메시지:', error.message || 'unknown');
          console.error('   시도 시간:', connectionTime + 'ms');
          console.error('   시도 횟수:', this.reconnectAttempts + 1);
          console.error('   Target URL:', backendUrl);
          console.error('   Full Error:', error);
          console.error('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
          
          this.isConnecting = false;
          this.reconnectAttempts++;
          
          // 🎯 사용자 친화적 에러 메시지
          let userMessage = '연결 실패';
          if (error.message?.includes('ECONNREFUSED')) {
            userMessage = 'Backend 서버가 실행되지 않음 (localhost:3000)';
          } else if (error.message?.includes('timeout')) {
            userMessage = '연결 타임아웃 - 네트워크 확인 필요';
          } else if (error.message?.includes('CORS')) {
            userMessage = 'CORS 정책 위반 - 서버 설정 확인 필요';
          } else {
            userMessage = `연결 실패: ${error.message}`;
          }
          
          this.notifyConnectionChange({
            status: 'error',
            timestamp: new Date().toISOString(),
            error: userMessage
          });
          
          this.notifyError(userMessage);
          reject(error);
        });

        console.log('⏳ 연결 대기 중... (최대 60초)');

      } catch (error) {
        this.isConnecting = false;
        console.error('❌ Socket 초기화 중 예외:', error);
        reject(error);
      }
    });
  }

  // =========================================================================
  // 연결 테스트 메시지 전송
  // =========================================================================
  async diagnoseConnection(): Promise<void> {
    console.log('🔍 WebSocket 연결 진단 시작...');
    console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');

    // 1. 기본 정보
    const backendUrl = this.getBackendUrl();
    console.log('📊 진단 정보:');
    console.log('   Backend URL:', backendUrl);
    console.log('   현재 Socket 상태:', this.socket?.connected ? 'connected' : 'disconnected');
    console.log('   Socket ID:', this.socket?.id || 'none');
    console.log('   Transport:', this.socket?.io?.engine?.transport?.name || 'none');

    // 2. Backend 서버 상태 확인
    try {
      console.log('🔍 Backend 서버 응답 확인 중...');
      const response = await fetch(`${backendUrl}/api/health`, {
        method: 'GET',
        headers: { 'Accept': 'application/json' },
        signal: AbortSignal.timeout(10000)
      });

      if (response.ok) {
        const health = await response.json();
        console.log('✅ Backend 서버 응답 정상:', health.status);
        console.log('   서버 가동 시간:', health.uptime + '초');
      } else {
        console.error('❌ Backend 서버 응답 비정상:', response.status);
      }
    } catch (error) {
      console.error('❌ Backend 서버 접근 실패:', error);
      console.log('💡 해결 방법:');
      console.log('   1. Backend 서버가 실행 중인지 확인 (localhost:3000)');
      console.log('   2. 방화벽 설정 확인');
      console.log('   3. CORS 설정 확인');
    }

    // 3. WebSocket 엔드포인트 확인
    try {
      console.log('🔍 Socket.IO 엔드포인트 확인 중...');
      const socketResponse = await fetch(`${backendUrl}/socket.io/?EIO=4&transport=polling&t=${Date.now()}`, {
        method: 'GET',
        signal: AbortSignal.timeout(10000)
      });

      if (socketResponse.ok) {
        console.log('✅ Socket.IO 엔드포인트 응답 정상');
      } else {
        console.error('❌ Socket.IO 엔드포인트 응답 비정상:', socketResponse.status);
      }
    } catch (error) {
      console.error('❌ Socket.IO 엔드포인트 접근 실패:', error);
    }

    console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
    console.log('🔍 진단 완료');
  }

  // =========================================================================
  // 🎯 강제 재연결 (디버깅용)
  // =========================================================================
  async forceReconnect(): Promise<void> {
    console.log('🔄 강제 재연결 시작...');
    
    // 기존 연결 정리
    if (this.socket) {
      this.socket.disconnect();
      this.socket = null;
    }
    
    this.isConnecting = false;
    this.reconnectAttempts = 0;
    
    // 잠시 대기
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    // 연결 진단 후 재연결
    await this.diagnoseConnection();
    return this.connect();
  }

  // =========================================================================
  // 연결 해제
  // =========================================================================
  disconnect(): void {
    if (this.socket) {
      console.log('🔌 WebSocket 연결 해제 시도');
      console.log('   Socket ID:', this.socket.id);
      console.log('   연결 상태:', this.socket.connected ? 'connected' : 'disconnected');
      
      this.socket.disconnect();
      this.socket = null;
      
      console.log('✅ WebSocket 연결 해제 완료');
    }
    
    this.notifyConnectionChange({
      status: 'disconnected',
      timestamp: new Date().toISOString()
    });
  }

  // =========================================================================
  // 연결 상태 확인
  // =========================================================================
  isConnected(): boolean {
    const connected = this.socket?.connected ?? false;
    if (process.env.NODE_ENV === 'development') {
      console.log('🔍 연결 상태 확인:', {
        socket_exists: !!this.socket,
        connected: connected,
        socket_id: this.socket?.id || 'none'
      });
    }
    return connected;
  }

  // =========================================================================
  // 이벤트 핸들러 설정 (완전 디버깅 버전)
  // =========================================================================
  private setupEventHandlers(): void {
    if (!this.socket) return;

    console.log('📡 이벤트 핸들러 설정 중...');

    // 기본 연결 상태 이벤트들
    this.socket.on('connection_status', (data) => {
      console.log('📊 서버 연결 상태 알림:', data);
    });

    this.socket.on('disconnect', (reason) => {
      console.warn('⚠️ WebSocket 연결 해제:', reason);
      this.notifyConnectionChange({
        status: 'disconnected',
        timestamp: new Date().toISOString(),
        reason: reason
      });
    });

    this.socket.on('reconnect', (attemptNumber) => {
      console.log('🔄 WebSocket 재연결 성공 (시도 #' + attemptNumber + ')');
      this.joinTenantRoom();
      this.joinAdminRoom();
      
      this.notifyConnectionChange({
        status: 'connected',
        socket_id: this.socket?.id,
        timestamp: new Date().toISOString()
      });
    });

    this.socket.on('reconnecting', (attemptNumber) => {
      console.log('🔄 WebSocket 재연결 시도 #' + attemptNumber);
      this.notifyConnectionChange({
        status: 'connecting',
        timestamp: new Date().toISOString()
      });
    });

    this.socket.on('reconnect_error', (error) => {
      console.error('❌ WebSocket 재연결 에러:', error);
    });

    this.socket.on('reconnect_failed', () => {
      console.error('❌ WebSocket 재연결 완전 실패');
      this.notifyConnectionChange({
        status: 'error',
        timestamp: new Date().toISOString(),
        error: '재연결 실패'
      });
    });

    // 테스트 메시지 핸들러
    this.socket.on('test-response', (data) => {
      console.log('✅ 서버 테스트 응답 수신:', data);
    });

    // 룸 조인 확인
    this.socket.on('room_joined', (data) => {
      console.log('👥 룸 조인 확인:', data);
    });

    // 알람 이벤트들
    this.socket.on('alarm:new', (data: WebSocketAlarmEvent) => {
      console.log('🚨 새 알람 수신:', data);
      this.notifyAlarmEvent(data);
    });

    this.socket.on('alarm:critical', (data: WebSocketAlarmEvent) => {
      console.log('🚨🚨 긴급 알람 수신:', data);
      this.notifyAlarmEvent(data);
    });

    this.socket.on('alarm:acknowledged', (data: AlarmAcknowledgment) => {
      console.log('✅ 알람 확인 알림:', data);
      this.notifyAcknowledgment(data);
    });

    this.socket.on('alarm_acknowledged', (data) => {
      console.log('✅ 알람 확인 응답:', data);
    });

    // 범용 에러 핸들러
    this.socket.on('error', (error) => {
      console.error('❌ Socket 에러:', error);
      this.notifyError(`Socket 에러: ${error.message || error}`);
    });

    // Transport 관련 이벤트
    this.socket.io.engine.on('upgrade', () => {
      console.log('🔄 Transport 업그레이드됨:', this.socket?.io.engine.transport.name);
    });

    this.socket.io.engine.on('upgradeError', (error) => {
      console.error('❌ Transport 업그레이드 에러:', error);
    });

    // Ping/Pong 이벤트 (선택적)
    this.socket.on('ping', () => {
      console.log('🏓 서버로부터 Ping 수신');
    });

    this.socket.on('pong', (latency) => {
      console.log(`🏓 서버로부터 Pong 수신, 지연시간: ${latency}ms`);
    });

    console.log('✅ 이벤트 핸들러 설정 완료');
  }

  // =========================================================================
  // 룸 관리
  // =========================================================================
  private joinTenantRoom(): void {
    if (this.socket?.connected) {
      console.log(`👥 테넌트 룸 조인 시도: tenant:${this.tenantId}`);
      this.socket.emit('join_tenant', this.tenantId);
    }
  }

  private joinAdminRoom(): void {
    if (this.socket?.connected) {
      console.log('👑 관리자 룸 조인 시도');
      this.socket.emit('join_admin');
    }
  }

  // =========================================================================
  // 알람 확인
  // =========================================================================
  acknowledgeAlarm(occurrenceId: number, userId: number, comment: string = ''): void {
    if (!this.socket || !this.socket.connected) {
      const error = 'WebSocket이 연결되지 않음 - 알람 확인 불가';
      console.error('❌', error);
      this.notifyError(error);
      return;
    }

    const ackData = {
      occurrence_id: occurrenceId,
      user_id: userId,
      comment: comment,
      timestamp: new Date().toISOString()
    };

    console.log('📝 알람 확인 요청 전송:', ackData);
    this.socket.emit('acknowledge_alarm', ackData);
  }

  // =========================================================================
  // 이벤트 구독/해제
  // =========================================================================
  onAlarmEvent(handler: AlarmEventHandler): () => void {
    this.alarmEventHandlers.add(handler);
    return () => this.alarmEventHandlers.delete(handler);
  }

  onAcknowledgment(handler: AcknowledgmentHandler): () => void {
    this.acknowledgmentHandlers.add(handler);
    return () => this.acknowledgmentHandlers.delete(handler);
  }

  onConnectionChange(handler: ConnectionHandler): () => void {
    this.connectionHandlers.add(handler);
    return () => this.connectionHandlers.delete(handler);
  }

  onError(handler: ErrorHandler): () => void {
    this.errorHandlers.add(handler);
    return () => this.errorHandlers.delete(handler);
  }

  // =========================================================================
  // 내부 알림 메서드들
  // =========================================================================
  private notifyAlarmEvent(event: WebSocketAlarmEvent): void {
    this.alarmEventHandlers.forEach(handler => {
      try {
        handler(event);
      } catch (error) {
        console.error('❌ 알람 이벤트 핸들러 에러:', error);
      }
    });
  }

  private notifyAcknowledgment(ack: AlarmAcknowledgment): void {
    this.acknowledgmentHandlers.forEach(handler => {
      try {
        handler(ack);
      } catch (error) {
        console.error('❌ 확인 이벤트 핸들러 에러:', error);
      }
    });
  }

  private notifyConnectionChange(status: ConnectionStatus): void {
    this.connectionHandlers.forEach(handler => {
      try {
        handler(status);
      } catch (error) {
        console.error('❌ 연결 상태 핸들러 에러:', error);
      }
    });
  }

  private notifyError(error: string): void {
    this.errorHandlers.forEach(handler => {
      try {
        handler(error);
      } catch (error) {
        console.error('❌ 에러 핸들러 에러:', error);
      }
    });
  }

  // =========================================================================
  // 상태 정보 조회
  // =========================================================================
  getConnectionInfo() {
    const info = {
      connected: this.isConnected(),
      socketId: this.socket?.id || null,
      tenantId: this.tenantId,
      reconnectAttempts: this.reconnectAttempts,
      maxReconnectAttempts: this.maxReconnectAttempts,
      isConnecting: this.isConnecting,
      backendUrl: this.getBackendUrl(),
      transport: this.socket?.io.engine.transport.name || null,
      environment: {
        hostname: window.location.hostname,
        port: window.location.port,
        protocol: window.location.protocol,
        origin: window.location.origin
      },
      socketInfo: this.socket ? {
        id: this.socket.id,
        connected: this.socket.connected,
        disconnected: this.socket.disconnected
      } : null
    };

    console.log('📊 연결 정보 조회:', info);
    return info;
  }

  // =========================================================================
  // 테스트 알람 전송
  // =========================================================================
  async sendTestAlarm(): Promise<void> {
    try {
      console.log('🧪 테스트 알람 전송 시작...');
      
      const response = await fetch('/api/test/alarm', {
        method: 'POST',
        headers: { 
          'Content-Type': 'application/json',
          'Accept': 'application/json'
        },
        body: JSON.stringify({
          test: true,
          timestamp: new Date().toISOString()
        })
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log('✅ 테스트 알람 전송 완료:', result);
      
    } catch (error) {
      console.error('❌ 테스트 알람 전송 실패:', error);
      this.notifyError(`테스트 알람 전송 실패: ${error}`);
    }
  }

  // =========================================================================
  // 수동 재연결
  // =========================================================================
  async reconnect(): Promise<void> {
    console.log('🔄 수동 재연결 시도...');
    
    if (this.socket) {
      this.disconnect();
    }
    
    // 잠시 대기 후 재연결
    await new Promise(resolve => setTimeout(resolve, 1000));
    return this.connect();
  }
}

// 싱글톤 인스턴스 생성
export const alarmWebSocketService = new AlarmWebSocketService();