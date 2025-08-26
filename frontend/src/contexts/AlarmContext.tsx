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

  // 초기 알람 개수 로드
  const loadAlarmCounts = async () => {
    try {
      const response = await AlarmApiService.getActiveAlarms({ 
        page: 1, 
        limit: 1000 // 전체 개수를 가져오기 위해 큰 값 사용
      });
      
      if (response.success && response.data?.items) {
        const activeAlarms = response.data.items.filter((alarm: any) => alarm.state === 'active');
        const criticalAlarms = activeAlarms.filter((alarm: any) => 
          alarm.severity === 'critical' || alarm.severity === 'high'
        );
        
        setActiveAlarmCount(activeAlarms.length);
        setCriticalAlarmCount(criticalAlarms.length);
      }
    } catch (error) {
      console.error('알람 개수 로드 실패:', error);
    }
  };

  // WebSocket을 통한 실시간 알람 개수 업데이트
  useEffect(() => {
    loadAlarmCounts();

    // 새로운 알람 발생 시 개수 증가
    const unsubscribeAlarm = alarmWebSocketService.onAlarmEvent((event: WebSocketAlarmEvent) => {
      setActiveAlarmCount(prev => prev + 1);
      
      // critical 또는 high 심각도인 경우 critical 개수도 증가
      if (event.data.severity === 'critical' || event.data.severity === 'high') {
        setCriticalAlarmCount(prev => prev + 1);
      }
    });

    // 알람 확인 시 개수 감소는 ActiveAlarms 컴포넌트에서 직접 호출
    
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