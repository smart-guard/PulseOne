// ============================================================================
// frontend/src/components/AlarmPopup/AlarmNotificationManager.tsx
// 커스텀 알람 팝업 시스템 (수정 버전)
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { createPortal } from 'react-dom';

interface AlarmNotification {
  id: string;
  title: string;
  message: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  timestamp: string;
  deviceName?: string;
  value?: string;
  autoClose?: boolean;
  autoCloseDelay?: number;
}

interface AlarmPopupProps {
  notification: AlarmNotification;
  position: { top: number; left: number };
  onClose: (id: string) => void;
  zIndex: number;
  index: number;
}

// 개별 팝업 컴포넌트
const AlarmPopup: React.FC<AlarmPopupProps> = ({ 
  notification, 
  position, 
  onClose, 
  zIndex, 
  index 
}) => {
  const [isVisible, setIsVisible] = useState(false);
  const [isClosing, setIsClosing] = useState(false);

  // 등장 애니메이션
  useEffect(() => {
    const timer = setTimeout(() => setIsVisible(true), 100 + (index * 200));
    return () => clearTimeout(timer);
  }, [index]);

  // 자동 닫기
  useEffect(() => {
    if (notification.autoClose && notification.autoCloseDelay) {
      const timer = setTimeout(() => {
        handleClose();
      }, notification.autoCloseDelay);
      return () => clearTimeout(timer);
    }
  }, [notification.autoClose, notification.autoCloseDelay]);

  const handleClose = useCallback(() => {
    setIsClosing(true);
    setTimeout(() => {
      onClose(notification.id);
    }, 300);
  }, [notification.id, onClose]);

  const getSeverityConfig = (severity: string) => {
    const configs = {
      critical: {
        bgGradient: 'linear-gradient(135deg, #dc2626, #991b1b)',
        borderColor: '#dc2626',
        icon: '🚨',
        iconBg: '#dc2626',
        textColor: '#ffffff',
        shadowColor: 'rgba(220, 38, 38, 0.4)',
        pulse: true
      },
      high: {
        bgGradient: 'linear-gradient(135deg, #ea580c, #c2410c)',
        borderColor: '#ea580c',
        icon: '⚠️',
        iconBg: '#ea580c',
        textColor: '#ffffff',
        shadowColor: 'rgba(234, 88, 12, 0.4)',
        pulse: false
      },
      medium: {
        bgGradient: 'linear-gradient(135deg, #2563eb, #1d4ed8)',
        borderColor: '#2563eb',
        icon: '🔔',
        iconBg: '#2563eb',
        textColor: '#ffffff',
        shadowColor: 'rgba(37, 99, 235, 0.4)',
        pulse: false
      },
      low: {
        bgGradient: 'linear-gradient(135deg, #059669, #047857)',
        borderColor: '#059669',
        icon: 'ℹ️',
        iconBg: '#059669',
        textColor: '#ffffff',
        shadowColor: 'rgba(5, 150, 105, 0.4)',
        pulse: false
      }
    };
    return configs[severity] || configs.low;
  };

  const config = getSeverityConfig(notification.severity);

  const animationStyle = isVisible 
    ? (isClosing ? 'translateY(-20px) scale(0.9)' : 'translateY(0) scale(1)')
    : 'translateY(-100px) scale(0.8)';

  return createPortal(
    <>
      {/* 커스텀 키프레임 애니메이션 스타일 */}
      <style>
        {`
          @keyframes alarmPulse {
            0%, 100% { 
              box-shadow: 0 0 0 0 ${config.shadowColor};
              transform: scale(1);
            }
            50% { 
              box-shadow: 0 0 0 10px transparent;
              transform: scale(1.02);
            }
          }
          
          @keyframes slideInDown {
            0% { 
              transform: translateY(-100px) scale(0.8);
              opacity: 0;
            }
            100% { 
              transform: translateY(0) scale(1);
              opacity: 1;
            }
          }
          
          @keyframes slideOutUp {
            0% { 
              transform: translateY(0) scale(1);
              opacity: 1;
            }
            100% { 
              transform: translateY(-100px) scale(0.8);
              opacity: 0;
            }
          }
        `}
      </style>

      <div
        style={{
          position: 'fixed',
          top: position.top,
          left: position.left,
          zIndex: zIndex,
          transform: animationStyle,
          opacity: isVisible && !isClosing ? 1 : 0,
          transition: 'all 0.4s cubic-bezier(0.34, 1.56, 0.64, 1)',
          fontFamily: '"Noto Sans KR", "Malgun Gothic", "Apple SD Gothic Neo", sans-serif',
          animation: isVisible 
            ? (isClosing ? 'slideOutUp 0.3s ease-in' : 'slideInDown 0.4s ease-out')
            : 'none',
          cursor: 'pointer'
        }}
        onClick={handleClose}
      >
        <div
          style={{
            width: '400px',
            minHeight: '120px',
            background: config.bgGradient,
            borderRadius: '16px',
            boxShadow: `0 8px 32px ${config.shadowColor}, 0 4px 16px rgba(0, 0, 0, 0.12)`,
            border: `2px solid ${config.borderColor}`,
            overflow: 'hidden',
            position: 'relative',
            animation: config.pulse ? 'alarmPulse 2s infinite' : 'none'
          }}
        >
          {/* 상단 인디케이터 */}
          <div
            style={{
              position: 'absolute',
              top: 0,
              left: 0,
              right: 0,
              height: '4px',
              background: 'rgba(255, 255, 255, 0.9)',
              animation: notification.severity === 'critical' ? 'pulse 1s infinite' : 'none'
            }}
          />

          {/* 메인 콘텐츠 */}
          <div style={{ 
            padding: '20px',
            display: 'flex',
            alignItems: 'flex-start',
            gap: '16px'
          }}>
            {/* 아이콘 영역 */}
            <div style={{
              width: '48px',
              height: '48px',
              borderRadius: '50%',
              background: 'rgba(255, 255, 255, 0.2)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '24px',
              flexShrink: 0,
              border: '2px solid rgba(255, 255, 255, 0.3)'
            }}>
              {config.icon}
            </div>

            {/* 텍스트 콘텐츠 */}
            <div style={{ flex: 1, minWidth: 0 }}>
              {/* 헤더 */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: '8px'
              }}>
                <div style={{
                  fontSize: '12px',
                  fontWeight: 700,
                  color: 'rgba(255, 255, 255, 0.9)',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px'
                }}>
                  {notification.severity} 알람
                </div>
                <div style={{
                  fontSize: '11px',
                  color: 'rgba(255, 255, 255, 0.7)',
                  fontFamily: 'monospace'
                }}>
                  {new Date(notification.timestamp).toLocaleTimeString('ko-KR')}
                </div>
              </div>

              {/* 제목 */}
              <div style={{
                fontSize: '16px',
                fontWeight: 600,
                color: config.textColor,
                marginBottom: '6px',
                lineHeight: '1.3',
                wordBreak: 'break-word'
              }}>
                {notification.title}
              </div>

              {/* 메시지 */}
              <div style={{
                fontSize: '14px',
                color: 'rgba(255, 255, 255, 0.9)',
                lineHeight: '1.4',
                marginBottom: '12px',
                wordBreak: 'break-word'
              }}>
                {notification.message}
              </div>

              {/* 추가 정보 */}
              {(notification.deviceName || notification.value) && (
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  padding: '8px 12px',
                  background: 'rgba(255, 255, 255, 0.1)',
                  borderRadius: '8px',
                  fontSize: '12px',
                  color: 'rgba(255, 255, 255, 0.8)',
                  marginBottom: '8px'
                }}>
                  {notification.deviceName && (
                    <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      📍 {notification.deviceName}
                    </span>
                  )}
                  {notification.value && (
                    <span style={{ 
                      fontFamily: 'monospace',
                      background: 'rgba(255, 255, 255, 0.1)',
                      padding: '2px 6px',
                      borderRadius: '4px'
                    }}>
                      {notification.value}
                    </span>
                  )}
                </div>
              )}

              {/* 닫기 힌트 */}
              <div style={{
                fontSize: '11px',
                color: 'rgba(255, 255, 255, 0.6)',
                textAlign: 'center',
                fontStyle: 'italic'
              }}>
                클릭하면 닫힙니다
              </div>
            </div>
          </div>

          {/* 닫기 버튼 */}
          <button
            onClick={(e) => {
              e.stopPropagation();
              handleClose();
            }}
            style={{
              position: 'absolute',
              top: '12px',
              right: '12px',
              background: 'rgba(255, 255, 255, 0.2)',
              border: 'none',
              borderRadius: '50%',
              width: '28px',
              height: '28px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              cursor: 'pointer',
              color: config.textColor,
              fontSize: '14px',
              transition: 'all 0.2s ease',
              backdropFilter: 'blur(4px)'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.3)';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.2)';
            }}
          >
            ✕
          </button>
        </div>
      </div>
    </>,
    document.body
  );
};

// 메인 알람 관리자 컴포넌트
export const AlarmNotificationManager: React.FC = () => {
  const [notifications, setNotifications] = useState<AlarmNotification[]>([]);

  const addNotification = useCallback((notification: Omit<AlarmNotification, 'id'>) => {
    const id = `alarm-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const newNotification: AlarmNotification = {
      ...notification,
      id,
      autoClose: notification.severity === 'low' || notification.severity === 'medium',
      autoCloseDelay: notification.severity === 'low' ? 8000 : 
                     notification.severity === 'medium' ? 10000 : 0
    };

    setNotifications(prev => {
      // 최대 5개까지만 표시
      const updatedNotifications = [newNotification, ...prev].slice(0, 5);
      return updatedNotifications;
    });
  }, []);

  const removeNotification = useCallback((id: string) => {
    setNotifications(prev => prev.filter(n => n.id !== id));
  }, []);

  const clearAllNotifications = useCallback(() => {
    setNotifications([]);
  }, []);

  // 글로벌 메서드 등록
  useEffect(() => {
    // @ts-ignore
    window.alarmNotificationManager = {
      addNotification,
      removeNotification,
      clearAllNotifications
    };

    // 디버그용
    console.log('AlarmNotificationManager 등록됨');

    return () => {
      // @ts-ignore
      delete window.alarmNotificationManager;
    };
  }, [addNotification, removeNotification, clearAllNotifications]);

  // 팝업 위치 계산 (중앙 상단)
  const getPopupPosition = (index: number) => {
    const screenWidth = window.innerWidth;
    const popupWidth = 400;
    const topOffset = 60 + (index * 140); // 상단에서 60px 시작, 각 팝업은 140px 간격
    const leftOffset = (screenWidth - popupWidth) / 2; // 화면 중앙

    return {
      top: topOffset,
      left: leftOffset
    };
  };

  return (
    <>
      {notifications.map((notification, index) => (
        <AlarmPopup
          key={notification.id}
          notification={notification}
          position={getPopupPosition(index)}
          onClose={removeNotification}
          zIndex={10000 + index}
          index={index}
        />
      ))}
    </>
  );
};

// 알람 표시 헬퍼 함수
export const showAlarmNotification = (alarm: {
  severity: string;
  message: string;
  device_name?: string;
  triggered_value?: any;
  rule_name?: string;
}) => {
  console.log('showAlarmNotification 호출됨:', alarm);
  
  // @ts-ignore
  if (window.alarmNotificationManager) {
    console.log('AlarmNotificationManager 존재, 팝업 생성');
    // @ts-ignore
    window.alarmNotificationManager.addNotification({
      title: alarm.rule_name || '시스템 알람',
      message: alarm.message,
      severity: alarm.severity.toLowerCase() as 'low' | 'medium' | 'high' | 'critical',
      timestamp: new Date().toISOString(),
      deviceName: alarm.device_name,
      value: alarm.triggered_value?.toString()
    });
  } else {
    console.warn('AlarmNotificationManager가 존재하지 않음');
  }
};

// TypeScript 타입 선언
declare global {
  interface Window {
    alarmNotificationManager?: {
      addNotification: (notification: Omit<AlarmNotification, 'id'>) => void;
      removeNotification: (id: string) => void;
      clearAllNotifications: () => void;
    };
  }
}