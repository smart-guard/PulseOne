// ============================================================================
// frontend/src/contexts/AlarmContext.tsx
// 전역 알람 상태 관리
// ============================================================================

import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import { AlarmApiService } from '../api/services/alarmApi';
import { alarmWebSocketService, WebSocketAlarmEvent } from '../services/AlarmWebSocketService';

interface AlarmContextType {
  activeAlarmCount: number;
  criticalAlarmCount: number;
  updateAlarmCount: (count: number) => void;
  incrementAlarmCount: () => void;
  decrementAlarmCount: () => void;
}

const AlarmContext = createContext<AlarmContextType | undefined>(undefined);

export const useAlarmContext = () => {
  const context = useContext(AlarmContext);
  if (!context) {
    throw new Error('useAlarmContext must be used within an AlarmProvider');
  }
  return context;
};

interface AlarmProviderProps {
  children: ReactNode;
}

export const AlarmProvider: React.FC<AlarmProviderProps> = ({ children }) => {
  const [activeAlarmCount, setActiveAlarmCount] = useState<number>(0);
  const [criticalAlarmCount, setCriticalAlarmCount] = useState<number>(0);

  // 알람 개수 로드 (백엔드 통계 API 사용)
  const loadAlarmCounts = async () => {
    try {
      const response = await AlarmApiService.getAlarmStatistics();

      if (response.success && response.data) {
        const stats = response.data;
        // active_alarms는 Active + Acknowledged를 포함함
        setActiveAlarmCount(stats.occurrences.active_alarms || 0);
        // dashboard_summary나 다른 경로를 통해 critical 개수를 가져올 수 있음
        // 만약 통계에 critical 전용 필드가 없다면 전체 활성에서 필터링하거나 
        // 백엔드 통계를 확장해야 함. 여기서는 기존 로직 유지를 위해 getActiveAlarms를 백그라운드에서 활용할 수도 있음.

        // 일단 unacknowledged_alarms 정보를 활용하여 bell에 표시할 개수 결정 가능
        // 여기서는 user_information 등에 따라 active_alarms(전체 활성)를 사용함
      }
    } catch (error) {
      console.error('알람 개수 로드 실패:', error);
    }
  };

  // WebSocket을 통한 실시간 알람 개수 업데이트
  useEffect(() => {
    loadAlarmCounts();

    // 알람 이벤트 통합 핸들러
    const unsubscribeAlarm = alarmWebSocketService.onAlarmEvent((event: WebSocketAlarmEvent) => {
      console.log('Context received alarm event:', event.type, event.data);

      switch (event.type) {
        case 'onCreate':
          setActiveAlarmCount(prev => prev + 1);
          if (event.data.severity === 'critical' || event.data.severity === 'high') {
            setCriticalAlarmCount(prev => prev + 1);
          }
          break;

        case 'onAcknowledge':
          // '확인' 시에는 '전체 활성(Active+Ack)' 개수는 변하지 않음
          // 미확인 개수를 따로 관리한다면 여기서 감소시켜야 함
          if (event.data.severity === 'critical' || event.data.severity === 'high') {
            // 필요 시 critical 미확인 개수 관리 로직 추가
          }
          break;

        case 'onClear':
          // '해제' 시에는 전체 활성 개수 감소
          setActiveAlarmCount(prev => Math.max(0, prev - 1));
          if (event.data.severity === 'critical' || event.data.severity === 'high') {
            setCriticalAlarmCount(prev => Math.max(0, prev - 1));
          }
          break;

        default:
          // 기타 이벤트 시 필요하면 loadAlarmCounts() 호출하여 동기화
          loadAlarmCounts();
          break;
      }
    });

    return () => {
      unsubscribeAlarm();
    };
  }, []);

  const updateAlarmCount = (count: number) => {
    setActiveAlarmCount(count);
  };

  const incrementAlarmCount = () => {
    setActiveAlarmCount(prev => prev + 1);
  };

  const decrementAlarmCount = () => {
    setActiveAlarmCount(prev => Math.max(0, prev - 1));
  };

  return (
    <AlarmContext.Provider value={{
      activeAlarmCount,
      criticalAlarmCount,
      updateAlarmCount,
      incrementAlarmCount,
      decrementAlarmCount
    }}>
      {children}
    </AlarmContext.Provider>
  );
};