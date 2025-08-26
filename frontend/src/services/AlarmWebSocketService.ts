// ============================================================================
// frontend/src/services/AlarmWebSocketService.ts
// Docker 환경변수 완전 지원 버전
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
  state: number; // 0: INACTIVE, 1: ACTIVE, 2: ACKNOWLEDGED, 3: CLEARED
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

  // 이벤트 핸들러들
  private alarmEventHandlers: Set<AlarmEventHandler> = new Set();
  private acknowledgmentHandlers: Set<AcknowledgmentHandler> = new Set();
  private connectionHandlers: Set<ConnectionHandler> = new Set();
  private errorHandlers: Set<ErrorHandler> = new Set();

  constructor(private tenantId: number = 1) {}

  // =========================================================================
  // 백엔드 URL 결정 로직 (Docker 환경변수 완전 지원)
  // =========================================================================
  private getBackendUrl(): string {
    // 1순위: Docker 환경변수에서 WebSocket URL 직접 사용
    if (typeof import.meta !== 'undefined' && import.meta.env) {
      // WebSocket URL이 설정되어 있으면 HTTP로 변환해서 사용
      const wsUrl = import.meta.env.VITE_WEBSOCKET_URL;
      if (wsUrl) {
        return wsUrl.replace('ws://', 'http://').replace('wss://', 'https://');
      }
      
      // API URL이 설정되어 있으면 사용
      const apiUrl = import.meta.env.VITE_API_URL;
      if (apiUrl) {
        return apiUrl;
      }
      
      // 백엔드 URL이 설정되어 있으면 사용
      const backendUrl = import.meta.env.VITE_BACKEND_URL;
      if (backendUrl) {
        return backendUrl;
      }
    }
    
    // 2순위: 브라우저 현재 위치 기반으로 추론
    const protocol = window.location.protocol;
    const hostname = window.location.hostname;
    const currentPort = window.location.port;
    
    // 개발 환경 감지
    if (hostname === 'localhost' || hostname === '127.0.0.1') {
      // Vite dev server (5173) → Backend (3000)
      if (currentPort === '5173') {
        return 'http://localhost:3000';
      }
      // Frontend가 3000에서 실행 중이면 Backend도 3000
      if (currentPort === '3000') {
        return 'http://localhost:3000';
      }
    }
    
    // 3순위: 프로덕션 환경 - 현재 도메인의 3000 포트 사용
    if (hostname !== 'localhost' && hostname !== '127.0.0.1') {
      return `${protocol}//${hostname}:3000`;
    }
    
    // 4순위: 기본값
    return 'http://localhost:3000';
  }

  // =========================================================================
  // WebSocket URL 결정 (HTTP URL에서 변환)
  // =========================================================================
  private getWebSocketUrl(): string {
    // 1순위: 직접 WebSocket URL 환경변수 사용
    if (typeof import.meta !== 'undefined' && import.meta.env) {
      const wsUrl = import.meta.env.VITE_WEBSOCKET_URL;
      if (wsUrl) {
        return wsUrl;
      }
    }
    
    // 2순위: HTTP URL을 WebSocket URL로 변환
    const httpUrl = this.getBackendUrl();
    return httpUrl.replace('http://', 'ws://').replace('https://', 'wss://');
  }

  // =========================================================================
  // 연결 관리
  // =========================================================================

  connect(): Promise<void> {
    if (this.socket && this.socket.connected) {
      console.log('Already connected to WebSocket');
      return Promise.resolve();
    }

    if (this.isConnecting) {
      console.log('Connection already in progress');
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      try {
        this.isConnecting = true;
        
        console.log('🔌 WebSocket 연결 시작...');
        
        // 연결 URL 결정 및 로깅
        const backendUrl = this.getBackendUrl();
        const wsUrl = this.getWebSocketUrl();
        
        console.log('🌐 HTTP Backend URL:', backendUrl);
        console.log('🌐 WebSocket URL:', wsUrl);
        console.log('🔧 환경변수 정보:', {
          VITE_API_URL: import.meta.env?.VITE_API_URL,
          VITE_WEBSOCKET_URL: import.meta.env?.VITE_WEBSOCKET_URL,
          VITE_BACKEND_URL: import.meta.env?.VITE_BACKEND_URL
        });
        
        // Socket.IO는 HTTP URL을 받아서 자동으로 WebSocket으로 업그레이드
        this.socket = io(backendUrl, {
          transports: ['websocket', 'polling'], // polling을 fallback으로 추가
          autoConnect: true,
          reconnection: true,
          reconnectionDelay: this.reconnectDelay,
          reconnectionAttempts: this.maxReconnectAttempts,
          timeout: 10000,
          forceNew: true
        });

        this.setupEventHandlers();

        // 연결 성공 시
        this.socket.once('connect', () => {
          console.log('✅ WebSocket 연결 성공:', this.socket?.id);
          console.log('🔗 실제 사용된 transport:', this.socket?.io.engine.transport.name);
          
          this.isConnecting = false;
          this.reconnectAttempts = 0;
          
          // 테넌트 룸 조인
          this.joinTenantRoom();
          this.joinAdminRoom();
          
          this.notifyConnectionChange({
            status: 'connected',
            tenant_id: this.tenantId,
            socket_id: this.socket?.id,
            timestamp: new Date().toISOString()
          });
          
          resolve();
        });

        // 연결 실패 시
        this.socket.once('connect_error', (error) => {
          console.error('❌ WebSocket 연결 실패:', error);
          console.error('❌ 시도한 URL:', backendUrl);
          
          this.isConnecting = false;
          this.reconnectAttempts++;
          
          this.notifyConnectionChange({
            status: 'error',
            timestamp: new Date().toISOString(),
            error: error.message
          });
          
          this.notifyError(`연결 실패: ${error.message}`);
          reject(error);
        });

      } catch (error) {
        this.isConnecting = false;
        console.error('❌ WebSocket 초기화 실패:', error);
        reject(error);
      }
    });
  }

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

  isConnected(): boolean {
    return this.socket?.connected ?? false;
  }

  // =========================================================================
  // 이벤트 핸들러 설정
  // =========================================================================

  private setupEventHandlers(): void {
    if (!this.socket) return;

    // 연결 상태 이벤트
    this.socket.on('connection_status', (data) => {
      console.log('📡 연결 상태 업데이트:', data);
    });

    this.socket.on('disconnect', (reason) => {
      console.log('🔌 WebSocket 연결 해제:', reason);
      this.notifyConnectionChange({
        status: 'disconnected',
        timestamp: new Date().toISOString()
      });
    });

    this.socket.on('reconnect', (attemptNumber) => {
      console.log('🔄 WebSocket 재연결 성공:', attemptNumber);
      this.joinTenantRoom();
      this.joinAdminRoom();
      
      this.notifyConnectionChange({
        status: 'connected',
        socket_id: this.socket?.id,
        timestamp: new Date().toISOString()
      });
    });

    this.socket.on('reconnect_failed', () => {
      console.error('❌ WebSocket 재연결 실패');
      this.notifyConnectionChange({
        status: 'error',
        timestamp: new Date().toISOString(),
        error: '재연결 실패'
      });
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

    // 테스트 메시지 수신
    this.socket.on('test_message', (data) => {
      console.log('🧪 테스트 메시지 수신:', data);
    });

    // 에러 처리
    this.socket.on('error', (error) => {
      console.error('❌ Socket 에러:', error);
      this.notifyError(`Socket 에러: ${error.message || error}`);
    });
  }

  // =========================================================================
  // 룸 관리
  // =========================================================================

  private joinTenantRoom(): void {
    if (this.socket) {
      this.socket.emit('join_tenant', this.tenantId);
      console.log(`📡 테넌트 룸 조인: ${this.tenantId}`);
    }
  }

  private joinAdminRoom(): void {
    if (this.socket) {
      this.socket.emit('join_admin');
      console.log('👤 관리자 룸 조인');
    }
  }

  // =========================================================================
  // 알람 액션
  // =========================================================================

  acknowledgeAlarm(occurrenceId: number, userId: number, comment: string = ''): void {
    if (!this.socket || !this.socket.connected) {
      this.notifyError('WebSocket이 연결되지 않음');
      return;
    }

    this.socket.emit('acknowledge_alarm', {
      occurrence_id: occurrenceId,
      user_id: userId,
      comment: comment
    });

    console.log(`📝 알람 확인 요청 전송: ${occurrenceId}`);
  }

  // =========================================================================
  // 이벤트 리스너 관리
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
        console.error('알람 이벤트 핸들러 에러:', error);
      }
    });
  }

  private notifyAcknowledgment(ack: AlarmAcknowledgment): void {
    this.acknowledgmentHandlers.forEach(handler => {
      try {
        handler(ack);
      } catch (error) {
        console.error('확인 이벤트 핸들러 에러:', error);
      }
    });
  }

  private notifyConnectionChange(status: ConnectionStatus): void {
    this.connectionHandlers.forEach(handler => {
      try {
        handler(status);
      } catch (error) {
        console.error('연결 상태 핸들러 에러:', error);
      }
    });
  }

  private notifyError(error: string): void {
    this.errorHandlers.forEach(handler => {
      try {
        handler(error);
      } catch (error) {
        console.error('에러 핸들러 에러:', error);
      }
    });
  }

  // =========================================================================
  // 유틸리티 메서드들
  // =========================================================================

  getConnectionInfo() {
    return {
      connected: this.isConnected(),
      socketId: this.socket?.id,
      tenantId: this.tenantId,
      reconnectAttempts: this.reconnectAttempts,
      backendUrl: this.getBackendUrl(),
      webSocketUrl: this.getWebSocketUrl(),
      environment: {
        VITE_API_URL: import.meta.env?.VITE_API_URL,
        VITE_WEBSOCKET_URL: import.meta.env?.VITE_WEBSOCKET_URL,
        VITE_BACKEND_URL: import.meta.env?.VITE_BACKEND_URL,
        location: {
          hostname: window.location.hostname,
          port: window.location.port,
          protocol: window.location.protocol
        }
      }
    };
  }

  // 테스트용 알람 전송
  async sendTestAlarm(): Promise<void> {
    try {
      const backendUrl = this.getBackendUrl();
      const response = await fetch(`${backendUrl}/api/test/alarm`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
      });
      
      if (!response.ok) {
        throw new Error(`테스트 알람 전송 실패: ${response.status} ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log('📧 테스트 알람 전송됨:', result);
    } catch (error) {
      console.error('❌ 테스트 알람 전송 실패:', error);
      this.notifyError(`테스트 알람 전송 실패: ${error}`);
    }
  }
}

// 싱글톤 인스턴스
export const alarmWebSocketService = new AlarmWebSocketService();