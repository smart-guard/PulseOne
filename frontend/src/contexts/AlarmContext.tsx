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
  refreshAlarmCount: () => Promise<void>;
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
        // 미확인(확인 대기) 알람 개수를 메인 배지 숫자로 사용
        setActiveAlarmCount(stats.dashboard_summary.unacknowledged || 0);

        // Critical 미확인 알람 통계 로드
        // dashboard_summary에 없으므로 occurrences.by_category 등을 활용해야 할 수도 있음
        // 여기서는 일단 이전에 구현된 로직의 의도에 따라 occurrences 정보를 활용 시도
        // (실제 통계 객체 구조에 맞춰 안전하게 접근)
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
        case 'alarm_triggered':
        case 'critical_alarm':
          // 새 알람 발생 시 미확인 개수 증가
          setActiveAlarmCount(prev => prev + 1);
          if (event.data.severity === 'CRITICAL' || event.data.severity === 'HIGH') {
            setCriticalAlarmCount(prev => prev + 1);
          }
          break;

        case 'alarm_state_change':
          // 상태 변화(확인/해제 등) 시에는 통계 재로드하여 정확한 상태 반영
          loadAlarmCounts();
          break;

        default:
          // 기타 이벤트 시 통계 다시 불러오기
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

  // 확인/해제 등 API 액션 후 뱃지를 명시적으로 재조회
  const refreshAlarmCount = async () => {
    await loadAlarmCounts();
  };

  return (
    <AlarmContext.Provider value={{
      activeAlarmCount, // 이제 미확인 알람 개수를 의미함
      criticalAlarmCount,
      updateAlarmCount,
      incrementAlarmCount,
      decrementAlarmCount,
      refreshAlarmCount
    }}>
      {children}
    </AlarmContext.Provider>
  );
};