// ============================================================================
// frontend/src/services/AlarmWebSocketService.ts
// 즉시 연결 해결 버전 - Transport 우선순위 동기화 + 연결 로직 단순화
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

type AlarmEventHandler = (event: WebSocketAlarmEvent) => void;
type ConnectionHandler = (status: ConnectionStatus) => void;
type ErrorHandler = (error: string) => void;

export class AlarmWebSocketService {
  private socket: Socket | null = null;
  private isConnecting = false;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 3;

  private alarmEventHandlers: Set<AlarmEventHandler> = new Set();
  private connectionHandlers: Set<ConnectionHandler> = new Set();
  private errorHandlers: Set<ErrorHandler> = new Set();

  constructor(private tenantId: number = 1) {}

  // =========================================================================
  // Backend와 동일한 설정으로 연결
  // =========================================================================
  connect(): Promise<void> {
    if (this.socket && this.socket.connected) {
      console.log('이미 연결됨');
      return Promise.resolve();
    }

    if (this.isConnecting) {
      console.log('연결 진행 중');
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      this.isConnecting = true;
      
      console.log('🚀 WebSocket 연결 시작 (Backend 동기화 버전)...');
      
      // Backend와 정확히 동일한 URL 사용
      const backendUrl = 'http://localhost:3000';

      // 🔥 Backend와 정확히 동일한 설정
      const socketOptions = {
        // 🎯 Backend와 동일한 transport 우선순위
        transports: ['polling', 'websocket'], // Backend와 동일하게 polling 우선
        
        // 🎯 Backend와 동일한 타임아웃 설정
        timeout: 90000,           // Backend connectTimeout과 동일
        autoConnect: true,
        forceNew: true,
        
        // 🎯 Backend와 동일한 설정들
        upgrade: true,
        rememberUpgrade: false,
        
        // 쿼리 파라미터
        query: {
          tenant_id: this.tenantId
        },
        
        // CORS 설정
        withCredentials: false,
        
        // 재연결 설정 (단순화)
        reconnection: false // 수동으로 관리
      };

      console.log('📋 Socket.IO 연결 정보 (Backend 동기화):');
      console.log('   URL:', backendUrl);
      console.log('   Transports:', socketOptions.transports);
      console.log('   Timeout:', socketOptions.timeout + 'ms');
      console.log('   Tenant ID:', this.tenantId);

      try {
        // Socket 생성
        this.socket = io(backendUrl, socketOptions);
        
        console.log('🔧 Socket 생성 완료, 이벤트 핸들러 설정 중...');

        // 🎯 연결 성공 핸들러 (더 빠른 응답)
        this.socket.on('connect', () => {
          console.log('✅ WebSocket 연결 성공!');
          console.log('   Socket ID:', this.socket?.id);
          console.log('   Transport:', this.socket?.io.engine.transport.name);
          console.log('   연결 시간:', new Date().toISOString());
          
          this.isConnecting = false;
          this.reconnectAttempts = 0;
          
          // 즉시 테넌트 룸 조인
          this.socket?.emit('join_tenant', this.tenantId);
          
          this.notifyConnectionChange({
            status: 'connected',
            tenant_id: this.tenantId,
            socket_id: this.socket?.id,
            timestamp: new Date().toISOString()
          });

          // 연결 테스트 메시지
          this.socket?.emit('test-message', {
            message: 'Frontend 연결 테스트',
            timestamp: new Date().toISOString(),
            tenant_id: this.tenantId
          });
          
          resolve();
        });

        // 🎯 연결 실패 핸들러 (즉시 피드백)
        this.socket.on('connect_error', (error) => {
          console.error('❌ WebSocket 연결 실패:');
          console.error('   에러:', error.message);
          console.error('   URL:', backendUrl);
          console.error('   설정:', socketOptions);
          
          this.isConnecting = false;
          this.reconnectAttempts++;
          
          this.notifyConnectionChange({
            status: 'error',
            timestamp: new Date().toISOString(),
            error: error.message
          });

          // 재연결 시도 (제한적)
          if (this.reconnectAttempts < this.maxReconnectAttempts) {
            console.log(`🔄 ${this.reconnectAttempts}/${this.maxReconnectAttempts} 재연결 시도 중...`);
            setTimeout(() => {
              this.reconnect();
            }, 2000);
          } else {
            console.error('❌ 최대 재연결 시도 초과');
            reject(error);
          }
        });

        // 연결 해제
        this.socket.on('disconnect', (reason) => {
          console.warn('⚠️ WebSocket 연결 해제:', reason);
          this.notifyConnectionChange({
            status: 'disconnected',
            timestamp: new Date().toISOString()
          });
        });

        // 🎯 알람 이벤트 핸들러들
        this.socket.on('alarm:new', (data: WebSocketAlarmEvent) => {
          console.log('🚨 새 알람:', data);
          this.notifyAlarmEvent(data);
        });

        this.socket.on('alarm:critical', (data: WebSocketAlarmEvent) => {
          console.log('🚨🚨 긴급 알람:', data);
          this.notifyAlarmEvent(data);
        });

        // 서버 응답 핸들러들
        this.socket.on('test-response', (data) => {
          console.log('✅ 서버 테스트 응답:', data);
        });

        this.socket.on('room_joined', (data) => {
          console.log('👥 룸 조인 완료:', data);
        });

        this.socket.on('connection_status', (data) => {
          console.log('📡 서버 연결 상태:', data);
        });

        // 에러 핸들러
        this.socket.on('error', (error) => {
          console.error('❌ Socket 에러:', error);
          this.notifyError(error.message || '알 수 없는 에러');
        });

        // 🎯 타임아웃 처리 (Backend 설정과 동일)
        const timeoutId = setTimeout(() => {
          if (this.isConnecting) {
            console.error('❌ 연결 타임아웃 (90초)');
            this.cleanup();
            reject(new Error('연결 타임아웃'));
          }
        }, 90000); // Backend와 동일한 90초

        // 연결 성공 시 타임아웃 해제
        this.socket.once('connect', () => {
          clearTimeout(timeoutId);
        });

      } catch (error) {
        console.error('❌ Socket 초기화 에러:', error);
        this.cleanup();
        reject(error);
      }
    });
  }

  // =========================================================================
  // 유틸리티 메서드들
  // =========================================================================
  disconnect(): void {
    if (this.socket) {
      console.log('🔌 WebSocket 연결 해제');
      this.socket.disconnect();
      this.socket = null;
    }
    
    this.notifyConnectionChange({
      status: 'disconnected',
      timestamp: new Date().toISOString()
    });
  }

  private cleanup(): void {
    this.isConnecting = false;
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

  // 알람 확인
  acknowledgeAlarm(occurrenceId: number, userId: number, comment: string = ''): void {
    if (!this.socket || !this.socket.connected) {
      console.error('❌ WebSocket 연결 없음');
      return;
    }

    this.socket.emit('acknowledge_alarm', {
      occurrence_id: occurrenceId,
      user_id: userId,
      comment: comment,
      timestamp: new Date().toISOString()
    });
  }

  // 🎯 테스트 알람 전송
  async sendTestAlarm(): Promise<void> {
    try {
      const response = await fetch('http://localhost:3000/api/test/alarm', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
      });
      
      if (!response.ok) {
        throw new Error(`테스트 알람 전송 실패: ${response.status}`);
      }
      
      const result = await response.json();
      console.log('📧 테스트 알람 전송됨:', result);
    } catch (error) {
      console.error('❌ 테스트 알람 전송 실패:', error);
      this.notifyError(`테스트 알람 전송 실패: ${error}`);
    }
  }

  // 이벤트 핸들러 등록
  onAlarmEvent(handler: AlarmEventHandler): () => void {
    this.alarmEventHandlers.add(handler);
    return () => this.alarmEventHandlers.delete(handler);
  }

  onAcknowledgment(handler: (ack: any) => void): () => void {
    // ActiveAlarms.tsx 호환성 유지
    return () => {};
  }

  onConnectionChange(handler: ConnectionHandler): () => void {
    this.connectionHandlers.add(handler);
    return () => this.connectionHandlers.delete(handler);
  }

  onError(handler: ErrorHandler): () => void {
    this.errorHandlers.add(handler);
    return () => this.errorHandlers.delete(handler);
  }

  // 알림 메서드들
  private notifyAlarmEvent(event: WebSocketAlarmEvent): void {
    this.alarmEventHandlers.forEach(handler => {
      try {
        handler(event);
      } catch (error) {
        console.error('❌ 알람 핸들러 에러:', error);
      }
    });
  }

  private notifyConnectionChange(status: ConnectionStatus): void {
    this.connectionHandlers.forEach(handler => {
      try {
        handler(status);
      } catch (error) {
        console.error('❌ 연결 핸들러 에러:', error);
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

  // 연결 정보
  getConnectionInfo() {
    return {
      connected: this.isConnected(),
      socketId: this.socket?.id || null,
      tenantId: this.tenantId,
      reconnectAttempts: this.reconnectAttempts,
      backendUrl: 'http://localhost:3000',
      transport: this.socket?.io.engine.transport.name || null,
      options: {
        transports: ['polling', 'websocket'],
        timeout: 90000
      }
    };
  }
}

// 싱글톤
export const alarmWebSocketService = new AlarmWebSocketService();