// ============================================================================
// frontend/src/services/AlarmWebSocketService.ts
// Hybrid 연결 방식 (Polling → WebSocket 업그레이드) + 개선된 에러 처리
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

export interface ConnectionStatus {
  status: 'connected' | 'disconnected' | 'connecting' | 'error';
  tenant_id?: number;
  socket_id?: string;
  timestamp: string;
  error?: string;
}

export interface AlarmAcknowledgment {
  occurrence_id: number;
  timestamp: string;
  comment?: string;
}

type AlarmEventHandler = (event: WebSocketAlarmEvent) => void;
type ConnectionHandler = (status: ConnectionStatus) => void;
type ErrorHandler = (error: string) => void;

export class AlarmWebSocketService {
  private socket: Socket | null = null;
  private isConnecting = false;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 3;
  private reconnectDelay = 2000;
  private connectionPromise: Promise<void> | null = null;

  private alarmEventHandlers: Set<AlarmEventHandler> = new Set();
  private connectionHandlers: Set<ConnectionHandler> = new Set();
  private errorHandlers: Set<ErrorHandler> = new Set();

  constructor(private tenantId: number = 1) { }

  // =========================================================================
  // Hybrid 연결 방식 (Polling → WebSocket 자동 업그레이드)
  // =========================================================================
  connect(): Promise<void> {
    // 이미 연결된 경우
    if (this.socket && this.socket.connected) {
      console.log('이미 WebSocket에 연결됨');
      return Promise.resolve();
    }

    // 연결 진행 중인 경우 - 기존 Promise 반환
    if (this.isConnecting && this.connectionPromise) {
      console.log('WebSocket 연결 진행 중');
      return this.connectionPromise;
    }

    // 새로운 연결 시작
    this.connectionPromise = this.performConnection();
    return this.connectionPromise;
  }

  private performConnection(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.isConnecting = true;

      console.log('🚀 WebSocket 연결 시작 (Hybrid 모드: Polling → WebSocket)...');

      const backendUrl = 'http://localhost:3000';
      const socketOptions = {
        // Hybrid 방식: Polling으로 시작 후 WebSocket 업그레이드
        transports: ['polling', 'websocket'],

        // 연결 설정
        timeout: 20000,
        autoConnect: true,
        forceNew: true,
        upgrade: true,
        rememberUpgrade: false,

        // 쿼리 파라미터
        query: {
          tenant_id: this.tenantId
        },

        // 재연결 설정 (수동 관리)
        reconnection: false
      };

      console.log('📋 Socket.IO 연결 설정:');
      console.log('   URL:', backendUrl);
      console.log('   Strategy: Polling → WebSocket 업그레이드');
      console.log('   Timeout:', socketOptions.timeout + 'ms');
      console.log('   Tenant ID:', this.tenantId);

      try {
        this.socket = io(backendUrl, socketOptions);

        console.log('🔧 Socket.IO 클라이언트 생성 완료, 이벤트 핸들러 설정 중...');

        // 연결 성공 핸들러
        const onConnect = () => {
          console.log('✅ WebSocket 연결 성공!');
          console.log('   Socket ID:', this.socket?.id);
          console.log('   Transport:', this.socket?.io.engine.transport.name);
          console.log('   연결 시간:', new Date().toISOString());

          this.isConnecting = false;
          this.reconnectAttempts = 0;
          this.connectionPromise = null;

          // Transport 업그레이드 감지
          this.socket?.io.engine.on('upgrade', () => {
            console.log('🚀 Transport 업그레이드:', this.socket?.io.engine.transport.name);
          });

          // 즉시 테넌트 룸 조인
          this.socket?.emit('join_tenant', this.tenantId);

          this.notifyConnectionChange({
            status: 'connected',
            tenant_id: this.tenantId,
            socket_id: this.socket?.id,
            timestamp: new Date().toISOString()
          });

          // 연결 확인 테스트
          this.socket?.emit('test-message', {
            message: 'Frontend WebSocket 연결 확인',
            timestamp: new Date().toISOString(),
            tenant_id: this.tenantId
          });

          resolve();
        };

        // 연결 실패 핸들러
        const onConnectError = (error: Error) => {
          console.error('❌ WebSocket 연결 실패:');
          console.error('   에러:', error.message);
          console.error('   URL:', backendUrl);
          console.error('   재시도 횟수:', this.reconnectAttempts);

          this.isConnecting = false;
          this.connectionPromise = null;
          this.reconnectAttempts++;

          this.notifyConnectionChange({
            status: 'error',
            timestamp: new Date().toISOString(),
            error: error.message
          });

          this.notifyError(`WebSocket 연결 실패: ${error.message}`);

          // 초기 연결인 경우에만 reject
          if (this.reconnectAttempts === 1) {
            reject(error);
          }

          // 제한적 재연결 시도
          if (this.reconnectAttempts < this.maxReconnectAttempts) {
            console.log(`🔄 WebSocket 재연결 시도 ${this.reconnectAttempts}/${this.maxReconnectAttempts} (${this.reconnectDelay}ms 후)...`);
            setTimeout(() => {
              this.reconnect();
            }, this.reconnectDelay * this.reconnectAttempts);
          } else {
            console.error('❌ WebSocket 최대 재연결 시도 초과');
          }
        };

        // 연결 해제 핸들러
        const onDisconnect = (reason: string) => {
          console.warn('⚠️ WebSocket 연결 해제:', reason);
          this.notifyConnectionChange({
            status: 'disconnected',
            timestamp: new Date().toISOString()
          });

          // 서버에서 의도적으로 끊은 경우가 아니면 재연결 시도
          if (reason !== 'io server disconnect' && reason !== 'io client disconnect') {
            if (this.reconnectAttempts < this.maxReconnectAttempts) {
              console.log('예상치 못한 연결 해제, 재연결 시도 중...');
              setTimeout(() => {
                this.reconnect();
              }, this.reconnectDelay);
            }
          }
        };

        // 이벤트 핸들러 등록
        this.socket.once('connect', onConnect);
        this.socket.on('connect_error', onConnectError);
        this.socket.on('disconnect', onDisconnect);

        // 알람 이벤트 핸들러들
        this.socket.on('alarm:new', (data: WebSocketAlarmEvent) => {
          console.log('🚨 새 알람 수신:', data);
          this.notifyAlarmEvent(data);
        });

        this.socket.on('alarm:critical', (data: WebSocketAlarmEvent) => {
          console.log('🚨🚨 긴급 알람 수신:', data);
          this.notifyAlarmEvent(data);
        });

        this.socket.on('alarm:acknowledged', (data: AlarmAcknowledgment) => {
          console.log('✅ 알람 확인됨:', data);
        });

        // 서버 응답 핸들러들
        this.socket.on('test-response', (data) => {
          console.log('✅ 서버 테스트 응답:', data);
        });

        this.socket.on('room_joined', (data) => {
          console.log('👥 룸 조인 완료:', data);
        });

        this.socket.on('connection_status', (data) => {
          console.log('📡 서버 연결 상태 확인:', data);
        });

        this.socket.on('welcome', (data) => {
          console.log('👋 서버 환영 메시지:', data.message);
        });

        // 에러 핸들러
        this.socket.on('error', (error) => {
          console.error('❌ WebSocket 에러:', error);
          this.notifyError(`WebSocket 에러: ${error.message || '알 수 없는 에러'}`);
        });

        // 연결 타임아웃 처리
        const timeoutId = setTimeout(() => {
          if (this.isConnecting) {
            console.error('❌ Socket.IO 연결 타임아웃 (20초)');
            this.cleanup();
            reject(new Error('Socket.IO 연결 타임아웃'));
          }
        }, socketOptions.timeout);

        // 연결 성공 시 타임아웃 해제
        this.socket.once('connect', () => {
          clearTimeout(timeoutId);
        });

      } catch (error) {
        console.error('❌ WebSocket 초기화 에러:', error);
        this.cleanup();
        reject(error);
      }
    });
  }

  // =========================================================================
  // 연결 관리 메서드들
  // =========================================================================
  disconnect(): void {
    if (this.socket) {
      console.log('🔌 WebSocket 연결 해제');
      this.socket.disconnect();
      this.socket = null;
    }

    this.isConnecting = false;
    this.connectionPromise = null;

    this.notifyConnectionChange({
      status: 'disconnected',
      timestamp: new Date().toISOString()
    });
  }

  private cleanup(): void {
    this.isConnecting = false;
    this.connectionPromise = null;
    if (this.socket) {
      this.socket.disconnect();
      this.socket = null;
    }
  }

  private async reconnect(): Promise<void> {
    this.cleanup();
    await new Promise(resolve => setTimeout(resolve, 1000));
    return this.connect();
  }

  isConnected(): boolean {
    return this.socket?.connected ?? false;
  }

  getConnectionStatus(): ConnectionStatus {
    if (this.socket && this.socket.connected) {
      return {
        status: 'connected',
        tenant_id: this.tenantId,
        socket_id: this.socket.id,
        timestamp: new Date().toISOString()
      };
    }
    if (this.isConnecting) {
      return {
        status: 'connecting',
        timestamp: new Date().toISOString()
      };
    }
    return {
      status: 'disconnected',
      timestamp: new Date().toISOString()
    };
  }

  // =========================================================================
  // 알람 관련 메서드들
  // =========================================================================
  acknowledgeAlarm(occurrenceId: number, userId: number, comment: string = ''): void {
    if (!this.socket || !this.socket.connected) {
      console.error('❌ WebSocket 연결 없음 - 알람 확인 실패');
      this.notifyError('WebSocket 연결 없음');
      return;
    }

    console.log('📝 알람 확인 전송:', { occurrenceId, userId, comment });

    this.socket.emit('acknowledge_alarm', {
      occurrence_id: occurrenceId,
      user_id: userId,
      comment: comment,
      timestamp: new Date().toISOString()
    });
  }

  async sendTestAlarm(): Promise<void> {
    try {
      const response = await fetch('http://localhost:3000/api/test/alarm', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          test_source: 'frontend_websocket_service',
          timestamp: new Date().toISOString()
        })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const result = await response.json();
      console.log('📧 테스트 알람 전송 성공:', result);
    } catch (error) {
      console.error('❌ 테스트 알람 전송 실패:', error);
      this.notifyError(`테스트 알람 전송 실패: ${error}`);
    }
  }

  // =========================================================================
  // 이벤트 핸들러 등록
  // =========================================================================
  onAlarmEvent(handler: AlarmEventHandler): () => void {
    this.alarmEventHandlers.add(handler);
    return () => this.alarmEventHandlers.delete(handler);
  }

  onConnectionChange(handler: ConnectionHandler): () => void {
    this.connectionHandlers.add(handler);

    // 즉시 현재 상태 통지 (UI가 '연결 중...'에 멈추는 것 방지)
    try {
      handler(this.getConnectionStatus());
    } catch (error) {
      console.error('❌ 초기 연결 상태 통지 에러:', error);
    }

    return () => this.connectionHandlers.delete(handler);
  }

  onError(handler: ErrorHandler): () => void {
    this.errorHandlers.add(handler);
    return () => this.errorHandlers.delete(handler);
  }

  // ActiveAlarms.tsx 호환성 유지
  onAcknowledgment(handler: (ack: AlarmAcknowledgment) => void): () => void {
    if (this.socket) {
      this.socket.on('alarm:acknowledged', handler);
      return () => this.socket?.off('alarm:acknowledged', handler);
    }
    return () => { };
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
  // 디버깅 및 상태 조회 (정확한 정보로 수정)
  // =========================================================================
  getConnectionInfo() {
    const actualTransport = this.socket?.io.engine.transport.name;

    return {
      connected: this.isConnected(),
      socketId: this.socket?.id || null,
      tenantId: this.tenantId,
      reconnectAttempts: this.reconnectAttempts,
      maxReconnectAttempts: this.maxReconnectAttempts,
      backendUrl: 'http://localhost:3000',
      currentTransport: actualTransport || null,
      connectionMode: 'Hybrid (Polling → WebSocket)',
      socketOptions: {
        transports: ['polling', 'websocket'],
        timeout: 20000,
        upgrade: true,
        reconnection: false,
        autoConnect: true,
        forceNew: true
      },
      status: {
        isConnecting: this.isConnecting,
        hasConnectionPromise: !!this.connectionPromise
      }
    };
  }

  // 연결 상태 강제 체크
  checkConnection(): boolean {
    const connected = this.isConnected();
    const info = this.getConnectionInfo();

    console.log('🔍 WebSocket 연결 상태 체크:', {
      connected,
      socketId: info.socketId,
      transport: info.currentTransport,
      reconnectAttempts: info.reconnectAttempts,
      isConnecting: info.status.isConnecting
    });

    return connected;
  }

  // 수동 재연결
  async forceReconnect(): Promise<void> {
    console.log('🔄 WebSocket 수동 재연결 시도...');
    this.reconnectAttempts = 0; // 재연결 횟수 리셋
    return this.reconnect();
  }

  // Transport 업그레이드 강제 시도
  forceUpgrade(): void {
    if (this.socket?.io.engine) {
      console.log('🚀 WebSocket 업그레이드 강제 시도...');
      (this.socket.io.engine as any).upgrade();
    } else {
      console.warn('⚠️ Socket이 연결되지 않아 업그레이드할 수 없습니다');
    }
  }
}

// 싱글톤 인스턴스
export const alarmWebSocketService = new AlarmWebSocketService();