// ============================================================================
// frontend/src/pages/ActiveAlarms.tsx
// 커스텀 알람 팝업 시스템 적용 - 최종 완성 버전
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { AlarmApiService } from '../api/services/alarmApi';
import { 
  alarmWebSocketService, 
  AlarmEvent, 
  WebSocketAlarmEvent, 
  AlarmAcknowledgment,
  ConnectionStatus 
} from '../services/AlarmWebSocketService';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { AlarmNotificationManager, showAlarmNotification } from '../components/AlarmPopup/AlarmNotificationManager';
import { useAlarmContext } from '../contexts/AlarmContext';

// CSS 파일들 import
import '../styles/base.css';
import '../styles/active-alarms.css';
import '../styles/pagination.css';

// 알람 인터페이스
interface ActiveAlarm {
  id: number;
  rule_id: number;
  rule_name: string;
  device_id?: number;
  device_name?: string;
  data_point_id?: number;
  data_point_name?: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  priority: number;
  message: string;
  description?: string;
  triggered_value?: any;
  threshold_value?: any;
  condition_type?: string;
  triggered_at: string;
  acknowledged_at?: string;
  acknowledged_by?: string;
  acknowledgment_comment?: string;
  state: 'active' | 'acknowledged' | 'cleared';
  quality: string;
  tags?: string[];
  metadata?: any;
  is_new?: boolean;
  ws_updated_at?: string;
}

interface AlarmStats {
  total_active: number;
  critical_count: number;
  high_count: number;
  medium_count: number;
  low_count: number;
  unacknowledged_count: number;
  acknowledged_count: number;
  by_device: Array<{
    device_id: number;
    device_name: string;
    alarm_count: number;
  }>;
  by_severity: Array<{
    severity: string;
    count: number;
  }>;
}

const ActiveAlarms: React.FC = () => {
  // Context에서 알람 개수 관리 함수 가져오기
  const { updateAlarmCount, decrementAlarmCount } = useAlarmContext();
  
  // 기존 pagination, state 관리는 동일
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 25,
    totalCount: 0
  });

  // 상태 관리
  const [alarms, setAlarms] = useState<ActiveAlarm[]>([]);
  const [alarmStats, setAlarmStats] = useState<AlarmStats>({
    total_active: 0,
    critical_count: 0,
    high_count: 0,
    medium_count: 0,
    low_count: 0,
    unacknowledged_count: 0,
    acknowledged_count: 0,
    by_device: [],
    by_severity: []
  });

  const [isLoading, setIsLoading] = useState(true);
  const [isProcessing, setIsProcessing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // 필터링 상태
  const [severityFilter, setSeverityFilter] = useState<string>('all');
  const [stateFilter, setStateFilter] = useState<string>('active');
  const [searchTerm, setSearchTerm] = useState('');

  // WebSocket 및 알림 상태
  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>({
    status: 'connecting',
    timestamp: new Date().toISOString()
  });
  const [soundEnabled, setSoundEnabled] = useState(true);
  const [realTimeEnabled, setRealTimeEnabled] = useState(true);
  const [popupNotificationsEnabled, setPopupNotificationsEnabled] = useState(true);
  
  const [newAlarmsCount, setNewAlarmsCount] = useState(0);
  const [pendingNewAlarms, setPendingNewAlarms] = useState<ActiveAlarm[]>([]);
  
  // 모달 상태
  const [showAckModal, setShowAckModal] = useState(false);
  const [ackComment, setAckComment] = useState('');
  const [selectedAlarmForAck, setSelectedAlarmForAck] = useState<ActiveAlarm | null>(null);

  // Refs
  const isInitializedRef = useRef(false);
  const isWebSocketConnectedRef = useRef(false);

  // =============================================================================
  // 유틸리티 함수들
  // =============================================================================

  const mapSeverity = (severity: string): 'low' | 'medium' | 'high' | 'critical' => {
    const severityMap: { [key: string]: 'low' | 'medium' | 'high' | 'critical' } = {
      'info': 'low',
      'low': 'low',
      'medium': 'medium',
      'high': 'high',
      'critical': 'critical'
    };
    return severityMap[severity?.toLowerCase()] || 'low';
  };

  const getSeverityLevel = (severity: string): number => {
    const levelMap: { [key: string]: number } = {
      'low': 1,
      'medium': 2,
      'high': 3,
      'critical': 4
    };
    return levelMap[severity?.toLowerCase()] || 1;
  };

  const getSeverityIcon = (severity: string) => {
    switch (severity.toLowerCase()) {
      case 'critical': return 'fas fa-exclamation-triangle';
      case 'high': return 'fas fa-exclamation-circle';
      case 'medium': return 'fas fa-exclamation';
      case 'low': return 'fas fa-info-circle';
      default: return 'fas fa-question-circle';
    }
  };

  const formatTimestamp = (timestamp: string) => {
    return new Date(timestamp).toLocaleString('ko-KR');
  };

  const formatDuration = (triggeredAt: string) => {
    const now = new Date();
    const triggered = new Date(triggeredAt);
    const diffMs = now.getTime() - triggered.getTime();
    
    const hours = Math.floor(diffMs / (1000 * 60 * 60));
    const minutes = Math.floor((diffMs % (1000 * 60 * 60)) / (1000 * 60));
    
    return hours > 0 ? `${hours}시간 ${minutes}분` : `${minutes}분`;
  };

  const playAlarmSound = (severityLevel: number) => {
    if (!soundEnabled) return;
    
    try {
      const audioContext = new (window.AudioContext || (window as any).webkitAudioContext)();
      const oscillator = audioContext.createOscillator();
      const gainNode = audioContext.createGain();
      
      oscillator.connect(gainNode);
      gainNode.connect(audioContext.destination);
      
      const frequency = severityLevel >= 3 ? 800 : 400;
      oscillator.frequency.value = frequency;
      oscillator.type = 'sine';
      
      gainNode.gain.setValueAtTime(0.1, audioContext.currentTime);
      gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + 0.5);
      
      oscillator.start(audioContext.currentTime);
      oscillator.stop(audioContext.currentTime + 0.5);
    } catch (error) {
      console.warn('사운드 재생 실패:', error);
    }
  };

  // 커스텀 팝업 알림 표시 함수
  const showCustomNotification = useCallback((alarmData: ActiveAlarm) => {
    if (!popupNotificationsEnabled) {
      console.log('팝업 알림이 비활성화됨');
      return;
    }

    console.log('커스텀 팝업 알림 표시:', alarmData);
    showAlarmNotification({
      severity: alarmData.severity,
      message: alarmData.message,
      device_name: alarmData.device_name,
      triggered_value: alarmData.triggered_value,
      rule_name: alarmData.rule_name
    });
  }, [popupNotificationsEnabled]);

  // =============================================================================
  // 필터링 및 유틸리티 콜백들
  // =============================================================================

  const checkAlarmMatchesFilters = useCallback((alarm: ActiveAlarm): boolean => {
    if (severityFilter !== 'all' && alarm.severity !== severityFilter) {
      return false;
    }
    
    if (stateFilter !== 'all' && alarm.state !== stateFilter) {
      return false;
    }
    
    if (searchTerm && !alarm.message.toLowerCase().includes(searchTerm.toLowerCase())) {
      return false;
    }
    
    return true;
  }, [severityFilter, stateFilter, searchTerm]);

  const updateAlarmStats = useCallback((alarmList?: ActiveAlarm[]) => {
    const currentAlarms = alarmList || alarms;
    const activeAlarms = currentAlarms.filter(a => a.state === 'active');
    
    const stats: AlarmStats = {
      total_active: activeAlarms.length,
      critical_count: activeAlarms.filter(a => a.severity === 'critical').length,
      high_count: activeAlarms.filter(a => a.severity === 'high').length,
      medium_count: activeAlarms.filter(a => a.severity === 'medium').length,
      low_count: activeAlarms.filter(a => a.severity === 'low').length,
      unacknowledged_count: activeAlarms.filter(a => !a.acknowledged_at).length,
      acknowledged_count: currentAlarms.filter(a => a.state === 'acknowledged').length,
      by_device: [],
      by_severity: []
    };
    
    setAlarmStats(stats);
    
    // Context의 알람 개수도 업데이트
    updateAlarmCount(stats.total_active);
  }, [alarms, updateAlarmCount]);

  const goToFirstPageWithNewAlarms = useCallback(() => {
    if (pagination.currentPage !== 1) {
      pagination.goToPage(1);
    }
    
    setPendingNewAlarms([]);
    setNewAlarmsCount(0);
  }, [pagination]);

  // =============================================================================
  // API 호출 함수들
  // =============================================================================

  const fetchActiveAlarms = useCallback(async (): Promise<void> => {
    try {
      setError(null);
      
      const response = await AlarmApiService.getActiveAlarms({
        page: pagination.currentPage,
        limit: pagination.pageSize,
        severity: severityFilter !== 'all' ? severityFilter : undefined,
        state: stateFilter !== 'all' ? stateFilter : undefined,
        search: searchTerm || undefined
      });

      if (response.success && response.data?.items) {
        const formattedAlarms = response.data.items.map((alarm: any) => ({
          id: alarm.id,
          rule_id: alarm.rule_id,
          rule_name: alarm.rule_name,
          device_id: alarm.device_id,
          device_name: alarm.device_name,
          data_point_id: alarm.data_point_id,
          data_point_name: alarm.data_point_name,
          severity: mapSeverity(alarm.severity),
          priority: getSeverityLevel(alarm.severity),
          message: alarm.alarm_message || alarm.message,
          description: alarm.description,
          triggered_value: alarm.trigger_value || alarm.triggered_value,
          threshold_value: alarm.threshold_value,
          condition_type: alarm.condition_type,
          triggered_at: alarm.occurrence_time || alarm.triggered_at,
          acknowledged_at: alarm.acknowledged_at,
          acknowledged_by: alarm.acknowledged_by,
          acknowledgment_comment: alarm.acknowledgment_comment,
          state: alarm.state,
          quality: alarm.quality || 'good',
          tags: alarm.tags,
          metadata: alarm.metadata,
          is_new: false
        }));

        setAlarms(formattedAlarms);
        
        if (response.data.pagination) {
          pagination.updateTotalCount(response.data.pagination.total);
        }
        
        updateAlarmStats(formattedAlarms);
      } else {
        setError(response.message || '알람 데이터 조회에 실패했습니다');
      }
    } catch (err) {
      console.error('활성 알람 조회 실패:', err);
      setError(err instanceof Error ? err.message : '알람 데이터 조회에 실패했습니다');
    } finally {
      setIsLoading(false);
    }
  }, [pagination.currentPage, pagination.pageSize, severityFilter, stateFilter, searchTerm, pagination, updateAlarmStats]);

  const handleAcknowledgeAlarm = useCallback(async (alarmId: number, comment: string = ''): Promise<void> => {
    try {
      setIsProcessing(true);
      
      const response = await AlarmApiService.acknowledgeAlarm(alarmId, {
        comment: comment
      });

      if (!response.success) {
        throw new Error(response.message || '확인 처리 실패');
      }

      setAlarms(prev => prev.map(alarm => 
        alarm.id === alarmId 
          ? { 
              ...alarm, 
              state: 'acknowledged' as const,
              acknowledged_at: new Date().toISOString(),
              acknowledged_by: '관리자',
              acknowledgment_comment: comment,
              is_new: false
            }
          : alarm
      ));

      updateAlarmStats();
      setShowAckModal(false);
      setAckComment('');
      setSelectedAlarmForAck(null);

    } catch (err) {
      setError(err instanceof Error ? err.message : '알람 확인 처리 실패');
    } finally {
      setIsProcessing(false);
    }
  }, [updateAlarmStats]);

  const handleClearAlarm = useCallback(async (alarmId: number): Promise<void> => {
    try {
      setIsProcessing(true);
      
      const response = await AlarmApiService.clearAlarm(alarmId, {
        comment: '사용자에 의해 해제됨'
      });

      if (!response.success) {
        throw new Error(response.message || '해제 처리 실패');
      }

      setAlarms(prev => prev.filter(alarm => alarm.id !== alarmId));
      
      // Context의 알람 개수 감소
      decrementAlarmCount();
      
      updateAlarmStats();

    } catch (err) {
      setError(err instanceof Error ? err.message : '알람 해제 처리 실패');
    } finally {
      setIsProcessing(false);
    }
  }, [updateAlarmStats, decrementAlarmCount]);

  // =============================================================================
  // WebSocket 관련 함수들
  // =============================================================================

  const initializeWebSocket = useCallback(async () => {
    if (isWebSocketConnectedRef.current || !realTimeEnabled) {
      console.log('WebSocket 이미 연결되어 있거나 비활성화됨');
      return;
    }

    try {
      console.log('WebSocket 초기화 시작...');
      
      setConnectionStatus({
        status: 'connecting',
        timestamp: new Date().toISOString()
      });

      await alarmWebSocketService.connect();
      isWebSocketConnectedRef.current = true;
      
      console.log('WebSocket 연결 성공');
      
    } catch (err) {
      console.error('WebSocket 연결 실패:', err);
      isWebSocketConnectedRef.current = false;
      setError(`실시간 연결에 실패했습니다: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
      setConnectionStatus({
        status: 'error',
        timestamp: new Date().toISOString(),
        error: err instanceof Error ? err.message : '알 수 없는 오류'
      });
    }
  }, [realTimeEnabled]);

  const cleanupWebSocket = useCallback(() => {
    if (isWebSocketConnectedRef.current) {
      console.log('WebSocket 정리 중...');
      alarmWebSocketService.disconnect();
      isWebSocketConnectedRef.current = false;
    }
  }, []);

  const toggleRealTime = useCallback(() => {
    setRealTimeEnabled(prev => {
      const newValue = !prev;
      console.log('실시간 모드 토글:', newValue);
      
      if (newValue && !isWebSocketConnectedRef.current) {
        initializeWebSocket();
      } else if (!newValue && isWebSocketConnectedRef.current) {
        cleanupWebSocket();
      }
      
      return newValue;
    });
  }, [initializeWebSocket, cleanupWebSocket]);

  // =============================================================================
  // WebSocket 이벤트 처리
  // =============================================================================

  const handleAlarmEvent = useCallback((event: WebSocketAlarmEvent) => {
    console.log('새 알람 이벤트 수신:', event);
    
    const newAlarm: ActiveAlarm = {
      id: event.data.occurrence_id,
      rule_id: event.data.rule_id,
      rule_name: event.data.source_name || `규칙 ${event.data.rule_id}`,
      device_id: parseInt(event.data.device_id || '0'),
      device_name: event.data.location || `Device ${event.data.device_id}`,
      data_point_id: event.data.point_id,
      data_point_name: `Point ${event.data.point_id}`,
      severity: mapSeverity(event.data.severity),
      priority: event.data.severity_level,
      message: event.data.message,
      description: event.data.message,
      triggered_value: event.data.trigger_value,
      threshold_value: undefined,
      triggered_at: new Date(event.data.timestamp).toISOString(),
      state: event.data.state === 1 ? 'active' : 'acknowledged',
      quality: 'good',
      is_new: true
    };

    // 알람 목록 업데이트
    if (pagination.currentPage !== 1) {
      setPendingNewAlarms(prev => [newAlarm, ...prev.slice(0, 9)]);
      setNewAlarmsCount(prev => prev + 1);
    } else {
      const matchesFilters = checkAlarmMatchesFilters(newAlarm);
      if (matchesFilters) {
        setAlarms(prev => {
          const exists = prev.find(alarm => alarm.id === newAlarm.id);
          if (exists) {
            return prev.map(alarm => 
              alarm.id === newAlarm.id ? { ...alarm, ...newAlarm } : alarm
            );
          } else {
            const newList = [newAlarm, ...prev];
            return newList.slice(0, pagination.pageSize);
          }
        });
        
        pagination.updateTotalCount(pagination.totalCount + 1);
      }
      
      setNewAlarmsCount(prev => prev + 1);
    }

    setLastUpdate(new Date());

    // 사운드 재생
    if (soundEnabled && event.data.severity_level >= 2) {
      playAlarmSound(event.data.severity_level);
    }

    // 커스텀 팝업 알림 표시
    showCustomNotification(newAlarm);

    // NEW 태그 자동 제거
    setTimeout(() => {
      setAlarms(prev => prev.map(alarm => 
        alarm.id === newAlarm.id ? { ...alarm, is_new: false } : alarm
      ));
    }, 5000);

  }, [pagination.currentPage, pagination.pageSize, pagination.totalCount, pagination, soundEnabled, checkAlarmMatchesFilters, showCustomNotification]);

  const handleAcknowledgment = useCallback((ack: AlarmAcknowledgment) => {
    console.log('알람 확인 알림 수신:', ack);
    
    setAlarms(prev => prev.map(alarm => 
      alarm.id === ack.occurrence_id 
        ? { 
            ...alarm, 
            state: 'acknowledged',
            acknowledged_at: ack.timestamp,
            acknowledgment_comment: ack.comment
          }
        : alarm
    ));
  }, []);

  const handleConnectionChange = useCallback((status: ConnectionStatus) => {
    console.log('연결 상태 변경:', status);
    setConnectionStatus(status);
  }, []);

  const handleWebSocketError = useCallback((error: string) => {
    console.error('WebSocket 에러:', error);
    setError(`실시간 연결 오류: ${error}`);
  }, []);

  // =============================================================================
  // useEffect들
  // =============================================================================

  useEffect(() => {
    if (isInitializedRef.current) {
      return;
    }
    
    console.log('ActiveAlarms 컴포넌트 초기화 시작...');
    
    const unsubscribeAlarm = alarmWebSocketService.onAlarmEvent(handleAlarmEvent);
    const unsubscribeAck = alarmWebSocketService.onAcknowledgment(handleAcknowledgment);
    const unsubscribeConnection = alarmWebSocketService.onConnectionChange(handleConnectionChange);
    const unsubscribeError = alarmWebSocketService.onError(handleWebSocketError);

    if (realTimeEnabled) {
      initializeWebSocket();
    }

    fetchActiveAlarms();
    
    isInitializedRef.current = true;
    console.log('ActiveAlarms 컴포넌트 초기화 완료');

    return () => {
      console.log('ActiveAlarms 컴포넌트 언마운트');
      unsubscribeAlarm();
      unsubscribeAck();
      unsubscribeConnection();
      unsubscribeError();
      cleanupWebSocket();
      isInitializedRef.current = false;
      isWebSocketConnectedRef.current = false;
    };
  }, []);

  useEffect(() => {
    if (!isInitializedRef.current) {
      return;
    }
    
    console.log('필터/페이징 변경으로 인한 데이터 재로드');
    fetchActiveAlarms();
  }, [
    pagination.currentPage, 
    pagination.pageSize, 
    severityFilter, 
    stateFilter, 
    searchTerm
  ]);

  useEffect(() => {
    updateAlarmStats();
  }, [alarms, updateAlarmStats]);

  useEffect(() => {
    if (newAlarmsCount > 0) {
      const timer = setTimeout(() => {
        setNewAlarmsCount(0);
      }, 5000);
      return () => clearTimeout(timer);
    }
  }, [newAlarmsCount]);

  // =============================================================================
  // 렌더링
  // =============================================================================

  if (isLoading) {
    return (
      <div style={{
        width: '100%',
        padding: '24px',
        background: '#f8fafc',
        minHeight: '100vh',
        fontFamily: '"Noto Sans KR", "Malgun Gothic", "Apple SD Gothic Neo", sans-serif',
        display: 'flex',
        justifyContent: 'center',
        alignItems: 'center'
      }}>
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: '16px',
          fontSize: '1.2rem',
          color: '#6b7280'
        }}>
          <i className="fas fa-spinner fa-spin" style={{ fontSize: '2rem', color: '#3b82f6' }}></i>
          활성 알람 로드 중...
        </div>
      </div>
    );
  }

  return (
    <div style={{
      width: '100%',
      padding: '24px',
      background: '#f8fafc',
      minHeight: '100vh',
      fontFamily: '"Noto Sans KR", "Malgun Gothic", "Apple SD Gothic Neo", sans-serif'
    }}>
      {/* 알람 팝업 관리자 컴포넌트 */}
      <AlarmNotificationManager />

      {/* 페이지 헤더 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'flex-start',
        marginBottom: '24px',
        background: 'white',
        padding: '24px',
        borderRadius: '12px',
        boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
        border: '1px solid #e5e7eb'
      }}>
        <div>
          <h1 style={{ 
            fontSize: '2rem', 
            fontWeight: 700, 
            color: '#111827', 
            margin: '0 0 8px 0' 
          }}>
            활성 알람
          </h1>
          <div style={{ 
            fontSize: '1rem', 
            color: '#6b7280', 
            margin: 0,
            display: 'flex',
            alignItems: 'center',
            gap: '12px'
          }}>
            현재 발생 중인 알람을 실시간으로 모니터링하고 관리합니다
            <span style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '8px',
              padding: '4px 8px',
              borderRadius: '12px',
              fontSize: '0.75rem',
              fontWeight: 500,
              background: connectionStatus.status === 'connected' ? '#f0fdf4' : '#fef2f2',
              color: connectionStatus.status === 'connected' ? '#16a34a' : '#dc2626'
            }}>
              {connectionStatus.status === 'connected' ? '🟢 실시간 연결됨' : '🔴 연결 끊김'}
              {newAlarmsCount > 0 && (
                <span style={{
                  marginLeft: '8px',
                  background: '#f59e0b',
                  color: 'white',
                  padding: '2px 6px',
                  borderRadius: '8px',
                  fontSize: '0.625rem',
                  fontWeight: 600
                }}>
                  새 알람 {newAlarmsCount}개
                </span>
              )}
            </span>
          </div>
        </div>
        
        {/* 컨트롤 버튼들 */}
        <div style={{
          display: 'flex',
          gap: '8px',
          alignItems: 'center'
        }}>
          <button 
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              padding: '8px 12px',
              borderRadius: '8px',
              fontSize: '0.75rem',
              fontWeight: 500,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              border: 'none',
              background: realTimeEnabled ? '#10b981' : '#f3f4f6',
              color: realTimeEnabled ? 'white' : '#374151',
              whiteSpace: 'nowrap'
            }}
            onClick={toggleRealTime}
          >
            <i className={`fas fa-${realTimeEnabled ? 'wifi' : 'wifi-slash'}`}></i>
            <span style={{ whiteSpace: 'nowrap' }}>실시간 {realTimeEnabled ? '켜짐' : '꺼짐'}</span>
          </button>

          <button 
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              padding: '8px 12px',
              borderRadius: '8px',
              fontSize: '0.75rem',
              fontWeight: 500,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              border: 'none',
              background: popupNotificationsEnabled ? '#8b5cf6' : '#f3f4f6',
              color: popupNotificationsEnabled ? 'white' : '#374151',
              whiteSpace: 'nowrap'
            }}
            onClick={() => setPopupNotificationsEnabled(!popupNotificationsEnabled)}
          >
            <i className={`fas fa-${popupNotificationsEnabled ? 'bell' : 'bell-slash'}`}></i>
            <span style={{ whiteSpace: 'nowrap' }}>팝업 알림</span>
          </button>
          
          <button 
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              padding: '8px 12px',
              borderRadius: '8px',
              fontSize: '0.75rem',
              fontWeight: 500,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              border: 'none',
              background: soundEnabled ? '#3b82f6' : '#f3f4f6',
              color: soundEnabled ? 'white' : '#374151',
              whiteSpace: 'nowrap'
            }}
            onClick={() => setSoundEnabled(!soundEnabled)}
          >
            <i className={`fas fa-volume-${soundEnabled ? 'up' : 'mute'}`}></i>
            <span style={{ whiteSpace: 'nowrap' }}>사운드</span>
          </button>
          
          <button 
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              padding: '8px 12px',
              borderRadius: '8px',
              fontSize: '0.75rem',
              fontWeight: 500,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              border: 'none',
              background: '#f59e0b',
              color: 'white',
              whiteSpace: 'nowrap'
            }}
            onClick={() => {
              console.log('테스트 알람 버튼 클릭됨');
              
              // 강제로 새로운 알람 생성 및 표시
              const testAlarm: ActiveAlarm = {
                id: Date.now(), // 고유 ID 생성
                rule_id: 999,
                rule_name: '테스트 압력 센서 알람',
                device_id: 1,
                device_name: 'RTU Device 001',
                data_point_id: 101,
                data_point_name: 'Pressure Point 1',
                severity: 'critical',
                priority: 4,
                message: 'RTU pressure sensor critical low: 75.2bar below low limit 80.0bar',
                description: '압력 센서 임계값 초과로 인한 긴급 알람',
                triggered_value: '75.2 bar',
                threshold_value: '80.0 bar',
                condition_type: 'low_limit',
                triggered_at: new Date().toISOString(),
                state: 'active',
                quality: 'good',
                is_new: true
              };

              // 1. 팝업 알림 표시
              if (popupNotificationsEnabled) {
                showAlarmNotification({
                  severity: testAlarm.severity,
                  message: testAlarm.message,
                  device_name: testAlarm.device_name,
                  triggered_value: testAlarm.triggered_value,
                  rule_name: testAlarm.rule_name
                });
              }

              // 2. 사운드 재생
              if (soundEnabled) {
                playAlarmSound(testAlarm.priority);
              }

              // 3. 알람 리스트에 추가 (첫 페이지에서만)
              if (pagination.currentPage === 1) {
                setAlarms(prev => {
                  const newList = [testAlarm, ...prev];
                  return newList.slice(0, pagination.pageSize);
                });
                pagination.updateTotalCount(pagination.totalCount + 1);
              } else {
                setNewAlarmsCount(prev => prev + 1);
              }

              // 4. 통계 업데이트
              updateAlarmStats();
              setLastUpdate(new Date());

              // 5. NEW 태그 자동 제거
              setTimeout(() => {
                setAlarms(prev => prev.map(alarm => 
                  alarm.id === testAlarm.id ? { ...alarm, is_new: false } : alarm
                ));
              }, 5000);

              // 6. WebSocket을 통한 테스트 알람도 시도 (백그라운드에서)
              try {
                alarmWebSocketService.sendTestAlarm();
              } catch (error) {
                console.warn('WebSocket 테스트 알람 전송 실패:', error);
              }
            }}
          >
            <i className="fas fa-flask"></i>
            <span style={{ whiteSpace: 'nowrap' }}>테스트 알람</span>
          </button>

          <button 
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '6px',
              padding: '8px 12px',
              borderRadius: '8px',
              fontSize: '0.75rem',
              fontWeight: 500,
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              border: 'none',
              background: '#ef4444',
              color: 'white',
              whiteSpace: 'nowrap'
            }}
            onClick={() => {
              if (window.alarmNotificationManager) {
                window.alarmNotificationManager.clearAllNotifications();
              }
            }}
          >
            <i className="fas fa-times-circle"></i>
            <span style={{ whiteSpace: 'nowrap' }}>팝업 모두 닫기</span>
          </button>
        </div>
      </div>

      {/* 새 알람 알림 배너 */}
      {pagination.currentPage !== 1 && newAlarmsCount > 0 && (
        <div style={{
          background: 'linear-gradient(135deg, #fbbf24, #f59e0b)',
          color: 'white',
          padding: '12px 24px',
          borderRadius: '8px',
          marginBottom: '24px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          boxShadow: '0 4px 12px rgba(245, 158, 11, 0.3)'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <i className="fas fa-bell"></i>
            <span>새로운 알람 {newAlarmsCount}개가 있습니다.</span>
          </div>
          <button 
            onClick={goToFirstPageWithNewAlarms}
            style={{
              background: 'rgba(255, 255, 255, 0.2)',
              color: 'white',
              border: '1px solid rgba(255, 255, 255, 0.3)',
              padding: '6px 12px',
              borderRadius: '6px',
              cursor: 'pointer',
              transition: 'all 0.2s ease'
            }}
          >
            첫 페이지로 보기
          </button>
        </div>
      )}

      {/* 알람 통계 패널 */}
      <div style={{
        background: 'white',
        borderRadius: '12px',
        boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
        border: '1px solid #e5e7eb',
        padding: '16px',
        marginBottom: '24px',
        display: 'grid',
        gridTemplateColumns: 'repeat(5, 1fr)',
        gap: '16px'
      }}>
        {[
          { type: 'critical', label: '심각', count: alarmStats.critical_count, color: '#dc2626', bg: 'linear-gradient(135deg, #fef2f2, white)' },
          { type: 'high', label: '높음', count: alarmStats.high_count, color: '#ea580c', bg: 'linear-gradient(135deg, #fff7ed, white)' },
          { type: 'medium', label: '보통', count: alarmStats.medium_count, color: '#2563eb', bg: 'linear-gradient(135deg, #eff6ff, white)' },
          { type: 'low', label: '낮음', count: alarmStats.low_count, color: '#059669', bg: 'linear-gradient(135deg, #ecfdf5, white)' },
          { type: 'unack', label: '미확인', count: alarmStats.unacknowledged_count, color: '#8b5cf6', bg: 'linear-gradient(135deg, #f3f4f6, white)' }
        ].map(stat => (
          <div key={stat.type} style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '16px',
            borderRadius: '12px',
            border: `1px solid ${stat.color}`,
            background: stat.bg,
            minHeight: '80px'
          }}>
            <div>
              <div style={{ fontSize: '0.875rem', color: '#6b7280', fontWeight: 500, marginBottom: '4px' }}>
                {stat.label}
              </div>
              <div style={{ fontSize: '2rem', fontWeight: 700, color: '#111827' }}>
                {stat.count}
              </div>
            </div>
            <div style={{
              width: '48px',
              height: '48px',
              borderRadius: '50%',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '1.25rem',
              color: 'white',
              background: stat.color
            }}>
              <i className={
                stat.type === 'critical' ? 'fas fa-exclamation-triangle' :
                stat.type === 'high' ? 'fas fa-exclamation-circle' :
                stat.type === 'medium' ? 'fas fa-exclamation' :
                stat.type === 'low' ? 'fas fa-info-circle' : 'fas fa-bell'
              }></i>
            </div>
          </div>
        ))}
      </div>

      {/* 필터 컨트롤 */}
      <div style={{
        background: 'white',
        borderRadius: '12px',
        boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
        border: '1px solid #e5e7eb',
        padding: '20px',
        marginBottom: '24px',
        display: 'flex',
        gap: '16px',
        alignItems: 'center',
        flexWrap: 'wrap'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <label style={{ fontSize: '0.875rem', fontWeight: 500, color: '#374151' }}>
            심각도:
          </label>
          <select
            value={severityFilter}
            onChange={(e) => setSeverityFilter(e.target.value)}
            style={{
              padding: '6px 12px',
              borderRadius: '6px',
              border: '1px solid #d1d5db',
              fontSize: '0.875rem'
            }}
          >
            <option value="all">전체</option>
            <option value="critical">심각</option>
            <option value="high">높음</option>
            <option value="medium">보통</option>
            <option value="low">낮음</option>
          </select>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <label style={{ fontSize: '0.875rem', fontWeight: 500, color: '#374151' }}>
            상태:
          </label>
          <select
            value={stateFilter}
            onChange={(e) => setStateFilter(e.target.value)}
            style={{
              padding: '6px 12px',
              borderRadius: '6px',
              border: '1px solid #d1d5db',
              fontSize: '0.875rem'
            }}
          >
            <option value="all">전체</option>
            <option value="active">활성</option>
            <option value="acknowledged">확인됨</option>
          </select>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1, maxWidth: '300px' }}>
          <label style={{ fontSize: '0.875rem', fontWeight: 500, color: '#374151' }}>
            검색:
          </label>
          <input
            type="text"
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            placeholder="알람 메시지 검색..."
            style={{
              flex: 1,
              padding: '6px 12px',
              borderRadius: '6px',
              border: '1px solid #d1d5db',
              fontSize: '0.875rem'
            }}
          />
        </div>
      </div>

      {/* 에러 표시 */}
      {error && (
        <div style={{
          background: '#fef2f2',
          border: '1px solid #fecaca',
          color: '#991b1b',
          padding: '16px',
          borderRadius: '8px',
          marginBottom: '16px',
          display: 'flex',
          alignItems: 'center',
          gap: '12px'
        }}>
          <i className="fas fa-exclamation-circle"></i>
          {error}
          <button 
            onClick={() => setError(null)}
            style={{
              background: 'none',
              border: 'none',
              color: '#991b1b',
              cursor: 'pointer',
              marginLeft: 'auto'
            }}
          >
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 알람 목록 테이블 */}
      <div style={{
        background: 'white',
        borderRadius: '12px',
        boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
        border: '1px solid #e5e7eb',
        overflow: 'hidden',
        marginBottom: '24px'
      }}>
        {alarms.length === 0 ? (
          <div style={{
            padding: '60px 20px',
            textAlign: 'center',
            color: '#6b7280',
            fontSize: '1.1rem'
          }}>
            <i className="fas fa-check-circle" style={{ fontSize: '3rem', marginBottom: '16px', color: '#10b981' }}></i>
            <div>현재 활성 알람이 없습니다.</div>
            <div style={{ fontSize: '0.9rem', marginTop: '8px' }}>
              마지막 업데이트: {lastUpdate.toLocaleString()}
            </div>
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse' }}>
            <thead style={{ background: '#f9fafb', borderBottom: '2px solid #e5e7eb' }}>
              <tr>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>심각도</th>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>메시지</th>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>디바이스</th>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>현재값/임계값</th>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>발생시간</th>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>지속시간</th>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>상태</th>
                <th style={{ 
                  padding: '16px 24px', 
                  textAlign: 'left', 
                  fontSize: '0.75rem', 
                  fontWeight: 600, 
                  color: '#374151', 
                  textTransform: 'uppercase', 
                  letterSpacing: '0.05em' 
                }}>작업</th>
              </tr>
            </thead>
            <tbody>
              {alarms.map((alarm) => {
                const severityColors = {
                  critical: { color: '#dc2626', bg: '#fef2f2', border: '#fecaca' },
                  high: { color: '#ea580c', bg: '#fff7ed', border: '#fed7aa' },
                  medium: { color: '#2563eb', bg: '#eff6ff', border: '#bfdbfe' },
                  low: { color: '#059669', bg: '#ecfdf5', border: '#a7f3d0' }
                };
                
                const severityStyle = severityColors[alarm.severity];
                
                return (
                  <tr 
                    key={alarm.id}
                    style={{
                      background: alarm.is_new ? 'linear-gradient(90deg, #fefce8, transparent)' : 'white',
                      borderLeft: `4px solid ${severityStyle.color}`,
                      transition: 'all 0.2s ease'
                    }}
                  >
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      <span style={{
                        display: 'inline-flex',
                        alignItems: 'center',
                        gap: '8px',
                        padding: '8px 12px',
                        borderRadius: '16px',
                        fontSize: '0.75rem',
                        fontWeight: 600,
                        minWidth: '90px',
                        justifyContent: 'center',
                        background: severityStyle.bg,
                        color: severityStyle.color,
                        border: `1px solid ${severityStyle.border}`
                      }}>
                        <i className={getSeverityIcon(alarm.severity)}></i>
                        {alarm.severity.toUpperCase()}
                        {alarm.is_new && (
                          <span style={{
                            background: '#f59e0b',
                            color: 'white',
                            padding: '2px 6px',
                            borderRadius: '10px',
                            fontSize: '0.625rem',
                            fontWeight: 700,
                            marginLeft: '4px'
                          }}>NEW</span>
                        )}
                      </span>
                    </td>
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      <div>
                        <div style={{ fontSize: '0.875rem', fontWeight: 500, color: '#111827', marginBottom: '4px' }}>
                          {alarm.message}
                        </div>
                        {alarm.description && (
                          <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                            {alarm.description}
                          </div>
                        )}
                      </div>
                    </td>
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      <div>
                        <div style={{ fontSize: '0.875rem', fontWeight: 500, color: '#111827', marginBottom: '4px' }}>
                          {alarm.device_name || `Device ${alarm.device_id}`}
                        </div>
                        {alarm.data_point_name && (
                          <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                            {alarm.data_point_name}
                          </div>
                        )}
                      </div>
                    </td>
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      {alarm.triggered_value && (
                        <div style={{ fontSize: '0.875rem', fontFamily: 'Courier New, monospace' }}>
                          <span style={{ color: '#ef4444', fontWeight: 600 }}>{alarm.triggered_value}</span>
                          {alarm.threshold_value && (
                            <>
                              <span style={{ color: '#6b7280', margin: '0 4px' }}>/</span>
                              <span style={{ color: '#6b7280' }}>{alarm.threshold_value}</span>
                            </>
                          )}
                        </div>
                      )}
                    </td>
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      <span style={{ fontSize: '0.875rem', color: '#374151', fontFamily: 'Courier New, monospace' }}>
                        {formatTimestamp(alarm.triggered_at)}
                      </span>
                    </td>
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      <span style={{ fontSize: '0.75rem', color: '#6b7280', fontWeight: 500 }}>
                        {formatDuration(alarm.triggered_at)}
                      </span>
                    </td>
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      <span style={{
                        display: 'inline-flex',
                        alignItems: 'center',
                        padding: '4px 12px',
                        borderRadius: '12px',
                        fontSize: '0.75rem',
                        fontWeight: 500,
                        background: alarm.state === 'active' ? '#fef2f2' : alarm.state === 'acknowledged' ? '#f9fafb' : '#f3f4f6',
                        color: alarm.state === 'active' ? '#dc2626' : alarm.state === 'acknowledged' ? '#6b7280' : '#6b7280'
                      }}>
                        {alarm.state === 'active' ? '활성' : 
                         alarm.state === 'acknowledged' ? '확인됨' : 
                         alarm.state === 'cleared' ? '해제됨' : alarm.state}
                      </span>
                      {alarm.acknowledged_at && (
                        <div style={{ marginTop: '4px', fontSize: '0.75rem', color: '#6b7280' }}>
                          <small>확인: {alarm.acknowledged_by}</small>
                        </div>
                      )}
                    </td>
                    <td style={{ padding: '16px 24px', borderBottom: '1px solid #f1f5f9', verticalAlign: 'middle' }}>
                      <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
                        {alarm.state === 'active' && (
                          <button 
                            onClick={() => {
                              setSelectedAlarmForAck(alarm);
                              setShowAckModal(true);
                            }}
                            disabled={isProcessing}
                            style={{
                              display: 'inline-flex',
                              alignItems: 'center',
                              padding: '4px 8px',
                              fontSize: '0.75rem',
                              borderRadius: '6px',
                              border: 'none',
                              background: '#f59e0b',
                              color: 'white',
                              cursor: 'pointer',
                              transition: 'all 0.2s ease'
                            }}
                            title="확인"
                          >
                            <i className="fas fa-check"></i>
                          </button>
                        )}
                        <button 
                          onClick={() => handleClearAlarm(alarm.id)}
                          disabled={isProcessing}
                          style={{
                            display: 'inline-flex',
                            alignItems: 'center',
                            padding: '4px 8px',
                            fontSize: '0.75rem',
                            borderRadius: '6px',
                            border: 'none',
                            background: '#10b981',
                            color: 'white',
                            cursor: 'pointer',
                            transition: 'all 0.2s ease'
                          }}
                          title="해제"
                        >
                          <i className="fas fa-times"></i>
                        </button>
                      </div>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        )}
      </div>

      {/* 페이징 */}
      {alarms.length > 0 && (
        <div style={{
          display: 'flex',
          justifyContent: 'center',
          marginBottom: '24px'
        }}>
          <Pagination
            current={pagination.currentPage}
            total={pagination.totalCount}
            pageSize={pagination.pageSize}
            pageSizeOptions={[10, 25, 50, 100]}
            showSizeChanger={true}
            showTotal={true}
            onChange={(page, pageSize) => {
              if (pageSize !== pagination.pageSize) {
                pagination.changePageSize(pageSize);
                pagination.goToPage(1);
              } else {
                pagination.goToPage(page);
              }
            }}
            onShowSizeChange={(current, size) => {
              pagination.changePageSize(size);
              pagination.goToPage(1);
            }}
          />
        </div>
      )}

      {/* 상태 정보 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginTop: '24px',
        padding: '16px',
        background: 'white',
        borderRadius: '8px',
        border: '1px solid #e5e7eb',
        fontSize: '0.875rem',
        color: '#6b7280'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          {pagination.totalCount > 0 && (
            <span>
              {((pagination.currentPage - 1) * pagination.pageSize) + 1}-
              {Math.min(pagination.currentPage * pagination.pageSize, pagination.totalCount)} / 
              총 {pagination.totalCount}개 알람
            </span>
          )}
          {realTimeEnabled && (
            <span style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '8px',
              padding: '4px 8px',
              borderRadius: '12px',
              fontSize: '0.75rem',
              fontWeight: 500,
              background: '#f0fdf4',
              color: '#16a34a'
            }}>
              실시간: 연결됨
            </span>
          )}
          {isProcessing && (
            <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: '#3b82f6' }}>
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
        </div>
      </div>

      {/* 알람 확인 모달 */}
      {showAckModal && selectedAlarmForAck && (
        <div 
          style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            background: 'rgba(0, 0, 0, 0.5)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 1000,
            backdropFilter: 'blur(4px)'
          }}
          onClick={() => setShowAckModal(false)}
        >
          <div 
            onClick={(e) => e.stopPropagation()}
            style={{
              background: 'white',
              borderRadius: '12px',
              boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.25)',
              width: '100%',
              maxWidth: '500px',
              margin: '16px'
            }}
          >
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              padding: '24px',
              borderBottom: '1px solid #e5e7eb'
            }}>
              <h3 style={{ margin: 0, fontSize: '1.25rem', fontWeight: 600, color: '#111827' }}>
                알람 확인
              </h3>
              <button 
                onClick={() => setShowAckModal(false)}
                style={{
                  background: 'none',
                  border: 'none',
                  color: '#6b7280',
                  cursor: 'pointer',
                  padding: '4px',
                  borderRadius: '4px',
                  transition: 'all 0.2s ease'
                }}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div style={{ padding: '24px' }}>
              <div style={{
                marginBottom: '16px',
                padding: '16px',
                background: '#f8fafc',
                borderRadius: '8px'
              }}>
                <div>
                  <span style={{
                    display: 'inline-flex',
                    alignItems: 'center',
                    gap: '8px',
                    padding: '8px 12px',
                    borderRadius: '16px',
                    fontSize: '0.75rem',
                    fontWeight: 600,
                    background: selectedAlarmForAck.severity === 'critical' ? '#fef2f2' : 
                               selectedAlarmForAck.severity === 'high' ? '#fff7ed' :
                               selectedAlarmForAck.severity === 'medium' ? '#eff6ff' : '#ecfdf5',
                    color: selectedAlarmForAck.severity === 'critical' ? '#dc2626' : 
                           selectedAlarmForAck.severity === 'high' ? '#ea580c' :
                           selectedAlarmForAck.severity === 'medium' ? '#2563eb' : '#059669'
                  }}>
                    <i className={getSeverityIcon(selectedAlarmForAck.severity)}></i>
                    {selectedAlarmForAck.severity.toUpperCase()}
                  </span>
                  <div style={{ fontSize: '0.875rem', fontWeight: 500, margin: '8px 0' }}>
                    {selectedAlarmForAck.message}
                  </div>
                  <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                    {selectedAlarmForAck.device_name || `Device ${selectedAlarmForAck.device_id}`}
                  </div>
                </div>
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                <label style={{ fontSize: '0.875rem', fontWeight: 500, color: '#374151' }}>
                  확인 코멘트:
                </label>
                <textarea
                  value={ackComment}
                  onChange={(e) => setAckComment(e.target.value)}
                  placeholder="알람 확인에 대한 설명을 입력하세요 (선택사항)"
                  rows={3}
                  style={{
                    padding: '12px',
                    border: '1px solid #d1d5db',
                    borderRadius: '8px',
                    fontSize: '0.875rem',
                    resize: 'vertical',
                    minHeight: '80px',
                    transition: 'border-color 0.2s ease'
                  }}
                />
              </div>
            </div>
            <div style={{
              display: 'flex',
              justifyContent: 'flex-end',
              gap: '12px',
              padding: '24px',
              borderTop: '1px solid #e5e7eb'
            }}>
              <button 
                onClick={() => setShowAckModal(false)}
                style={{
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '8px',
                  padding: '8px 16px',
                  borderRadius: '8px',
                  fontSize: '0.875rem',
                  fontWeight: 500,
                  cursor: 'pointer',
                  border: '1px solid #d1d5db',
                  background: '#f3f4f6',
                  color: '#374151',
                  transition: 'all 0.2s ease'
                }}
              >
                취소
              </button>
              <button 
                onClick={() => selectedAlarmForAck && handleAcknowledgeAlarm(selectedAlarmForAck.id, ackComment)}
                disabled={isProcessing}
                style={{
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '8px',
                  padding: '8px 16px',
                  borderRadius: '8px',
                  fontSize: '0.875rem',
                  fontWeight: 500,
                  cursor: isProcessing ? 'not-allowed' : 'pointer',
                  border: 'none',
                  background: isProcessing ? '#d97706' : '#f59e0b',
                  color: 'white',
                  transition: 'all 0.2s ease',
                  opacity: isProcessing ? 0.7 : 1
                }}
              >
                {isProcessing ? (
                  <>
                    <i className="fas fa-spinner fa-spin"></i> 처리 중...
                  </>
                ) : (
                  <>
                    <i className="fas fa-check"></i> 확인
                  </>
                )}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default ActiveAlarms;