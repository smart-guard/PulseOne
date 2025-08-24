import React, { useState } from 'react';
// virtualPointsApi import 추가
import { virtualPointsApi } from '../../api/services/virtualPointsApi';

export interface VirtualPointCardProps {
  virtualPoint: any;
  onEdit: (point: any) => void;
  onDelete?: (point: any) => void;  // optional로 변경
  onTest: (point: any) => void;
  onExecute: (pointId: number) => void;
  onToggleEnabled: (pointId: number) => void;
  isExecuting?: boolean;
  onRefresh?: () => void; // 데이터 새로고침용
}

export const VirtualPointCard: React.FC<VirtualPointCardProps> = ({
  virtualPoint,
  onEdit,
  onDelete,
  onTest,
  onExecute,
  onToggleEnabled,
  isExecuting = false,
  onRefresh
}) => {
  const [isProcessing, setIsProcessing] = useState(false);
  const [isToggling, setIsToggling] = useState(false);

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'active': 
      case 'success': return '#10b981';
      case 'error': return '#ef4444';
      case 'calculating': return '#f59e0b';
      case 'disabled': return '#6b7280';
      default: return '#6b7280';
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'active':
      case 'success': return 'fas fa-check-circle';
      case 'error': return 'fas fa-times-circle';
      case 'calculating': return 'fas fa-spinner fa-spin';
      case 'disabled': return 'fas fa-pause-circle';
      default: return 'fas fa-question-circle';
    }
  };

  const getCategoryColor = (category: string) => {
    const colors: Record<string, string> = {
      '온도': '#ef4444',
      '압력': '#3b82f6',
      '유량': '#10b981',
      '전력': '#f59e0b',
      '생산': '#8b5cf6',
      '품질': '#ec4899',
      '안전': '#f97316',
      '유지보수': '#6b7280',
      '에너지': '#84cc16',
      'Custom': '#14b8a6',
      'calculation': '#8b5cf6'
    };
    return colors[category] || '#6b7280';
  };

  const formatValue = (value: any, unit?: string) => {
    if (value === null || value === undefined) return 'N/A';
    
    let formattedValue = value;
    if (typeof value === 'number') {
      formattedValue = Number.isInteger(value) ? value.toString() : value.toFixed(2);
    }
    
    return unit ? `${formattedValue} ${unit}` : formattedValue.toString();
  };

  const getLastCalculatedText = (timestamp?: string) => {
    if (!timestamp) return 'N/A';
    
    try {
      const calculated = new Date(timestamp);
      return calculated.toLocaleDateString('ko-KR');
    } catch {
      return 'N/A';
    }
  };

  // 실제 버튼 핸들러들 - 기존 API 사용
  const handleExecute = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (!virtualPoint.is_enabled || isExecuting) return;
    
    try {
      await onExecute(virtualPoint.id);
    } catch (error) {
      console.error('가상포인트 실행 실패:', error);
    }
  };

  const handleTest = async (e: React.MouseEvent) => {
    e.stopPropagation();
    try {
      await onTest(virtualPoint);
    } catch (error) {
      console.error('가상포인트 테스트 실패:', error);
    }
  };

  const handleEdit = (e: React.MouseEvent) => {
    e.stopPropagation();
    onEdit(virtualPoint);
  };

  const handleDeleteClick = async (e: React.MouseEvent) => {
    e.stopPropagation();
    
    // 브라우저 기본 confirm 사용
    const confirmed = confirm(`"${virtualPoint.name}" 가상포인트를 삭제하시겠습니까?\n\n이 작업은 되돌릴 수 없습니다.`);
    
    if (confirmed) {
      setIsProcessing(true);
      try {
        // 직접 API 호출 - onDelete 호출하지 않음
        await virtualPointsApi.deleteVirtualPoint(virtualPoint.id);
        console.log('가상포인트 삭제 완료:', virtualPoint.id);
        
        // 상위 컴포넌트의 데이터 새로고침만 호출
        if (onRefresh) {
          onRefresh();
        }
      } catch (error) {
        console.error('가상포인트 삭제 실패:', error);
        alert('삭제 실패: ' + (error instanceof Error ? error.message : '알 수 없는 오류'));
      } finally {
        setIsProcessing(false);
      }
    }
  };

  const handleToggleEnabled = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (isToggling) return;
    
    setIsToggling(true);
    try {
      await onToggleEnabled(virtualPoint.id);
    } catch (error) {
      console.error('가상포인트 활성화 토글 실패:', error);
    } finally {
      setIsToggling(false);
    }
  };

  return (
    <div style={{
      background: 'white',
      borderRadius: '12px',
      border: '1px solid #e5e7eb',
      padding: '20px',
      transition: 'all 0.2s',
      cursor: 'pointer',
      height: 'fit-content'
    }}
    onMouseEnter={(e) => {
      e.currentTarget.style.boxShadow = '0 4px 12px rgba(0, 0, 0, 0.15)';
      e.currentTarget.style.transform = 'translateY(-2px)';
    }}
    onMouseLeave={(e) => {
      e.currentTarget.style.boxShadow = '0 1px 3px rgba(0, 0, 0, 0.1)';
      e.currentTarget.style.transform = 'translateY(0)';
    }}
    >
        {/* 카드 헤더 */}
        <div style={{ 
          display: 'flex', 
          justifyContent: 'space-between', 
          alignItems: 'flex-start', 
          marginBottom: '16px' 
        }}>
          <div style={{ flex: 1 }}>
            <h3 style={{ 
              fontSize: '16px', 
              fontWeight: 600, 
              color: '#111827', 
              margin: '0 0 8px 0',
              lineHeight: '1.4'
            }}>
              {virtualPoint.name}
            </h3>
            
            {virtualPoint.description && (
              <p style={{ 
                fontSize: '13px', 
                color: '#6b7280', 
                margin: '0 0 12px 0',
                lineHeight: '1.4'
              }}>
                {virtualPoint.description}
              </p>
            )}
            
            {/* 뱃지들 */}
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap', marginBottom: '12px' }}>
              <span style={{
                display: 'inline-flex',
                alignItems: 'center',
                gap: '4px',
                padding: '4px 8px',
                background: getStatusColor(virtualPoint.calculation_status || 'disabled'),
                color: 'white',
                borderRadius: '12px',
                fontSize: '10px',
                fontWeight: 500,
                height: '20px',
                minWidth: 'fit-content'
              }}>
                <i className={getStatusIcon(virtualPoint.calculation_status || 'disabled').replace('fa-spin', '')} 
                   style={{ fontSize: '9px' }}></i>
                {virtualPoint.calculation_status === 'success' || virtualPoint.calculation_status === 'active' ? '활성' : 
                 virtualPoint.calculation_status === 'error' ? '오류' :
                 virtualPoint.calculation_status === 'calculating' ? '계산중' : '비활성'}
              </span>
              
              <span style={{
                display: 'inline-flex',
                alignItems: 'center',
                padding: '4px 8px',
                background: getCategoryColor(virtualPoint.category || 'Custom'),
                color: 'white',
                borderRadius: '12px',
                fontSize: '10px',
                fontWeight: 500,
                height: '20px',
                minWidth: 'fit-content'
              }}>
                {virtualPoint.category || 'Custom'}
              </span>
            </div>
          </div>
          
          {/* 액션 버튼들 */}
          <div style={{ display: 'flex', gap: '4px', marginLeft: '12px' }}>
            {/* 실행 버튼 */}
            <button
              onClick={handleExecute}
              disabled={!virtualPoint.is_enabled || isExecuting}
              style={{
                padding: '6px',
                background: 'none',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                color: virtualPoint.is_enabled ? '#374151' : '#9ca3af',
                cursor: virtualPoint.is_enabled ? 'pointer' : 'not-allowed',
                opacity: virtualPoint.is_enabled ? 1 : 0.5
              }}
              title="실행"
            >
              <i className="fas fa-play" style={{ fontSize: '12px' }}></i>
            </button>
            
            {/* 테스트 버튼 */}
            <button
              onClick={handleTest}
              style={{
                padding: '6px',
                background: 'none',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                color: '#374151',
                cursor: 'pointer'
              }}
              title="테스트"
            >
              <i className="fas fa-vial" style={{ fontSize: '12px' }}></i>
            </button>
            
            {/* 편집 버튼 */}
            <button
              onClick={handleEdit}
              style={{
                padding: '6px',
                background: 'none',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                color: '#374151',
                cursor: 'pointer'
              }}
              title="편집"
            >
              <i className="fas fa-edit" style={{ fontSize: '12px' }}></i>
            </button>
            
            {/* 삭제 버튼 */}
            <button
              onClick={handleDeleteClick}
              style={{
                padding: '6px',
                background: 'none',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                color: '#dc2626',
                cursor: 'pointer'
              }}
              title="삭제"
            >
              <i className="fas fa-trash" style={{ fontSize: '12px' }}></i>
            </button>
          </div>
        </div>

        {/* 수식 */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ fontSize: '12px', color: '#6b7280', marginBottom: '4px', fontWeight: 500 }}>
            수식:
          </div>
          <code style={{
            display: 'block',
            padding: '8px',
            background: '#f3f4f6',
            borderRadius: '6px',
            fontSize: '12px',
            color: '#374151',
            border: '1px solid #e5e7eb',
            wordBreak: 'break-all',
            fontFamily: 'JetBrains Mono, Consolas, monospace',
            lineHeight: '1.4'
          }}>
            {virtualPoint.formula || virtualPoint.expression || 'N/A'}
          </code>
        </div>

        {/* 현재값 */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ fontSize: '12px', color: '#6b7280', marginBottom: '4px', fontWeight: 500 }}>
            현재값:
          </div>
          <div style={{
            padding: '8px 12px',
            background: virtualPoint.calculation_status === 'success' || virtualPoint.calculation_status === 'active' ? '#f0fdf4' : '#f9fafb',
            borderRadius: '6px',
            fontSize: '14px',
            fontWeight: 600,
            color: virtualPoint.calculation_status === 'success' || virtualPoint.calculation_status === 'active' ? '#166534' : '#6b7280',
            fontFamily: 'JetBrains Mono, Consolas, monospace',
            border: '1px solid #e5e7eb'
          }}>
            {formatValue(virtualPoint.current_value, virtualPoint.unit)}
          </div>
        </div>

        {/* 메타데이터 그리드 */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr',
          gap: '8px 16px',
          fontSize: '12px',
          padding: '12px',
          background: '#f9fafb',
          borderRadius: '6px',
          border: '1px solid #e5e7eb',
          marginBottom: '12px'
        }}>
          {[
            { 
              label: '범위', 
              value: virtualPoint.scope_type === 'global' || virtualPoint.scope_type === 'tenant' ? '전역' : 
                     virtualPoint.scope_type === 'site' ? '사이트' : '디바이스' 
            },
            { 
              label: '실행', 
              value: virtualPoint.calculation_trigger === 'time_based' || virtualPoint.calculation_trigger === 'timer' ? '시간기반' : 
                     virtualPoint.calculation_trigger === 'event_driven' || virtualPoint.calculation_trigger === 'onchange' ? '이벤트' : '수동' 
            },
            { 
              label: '주기', 
              value: virtualPoint.calculation_interval ? `${virtualPoint.calculation_interval}ms` : 'N/A' 
            },
            { 
              label: '입력', 
              value: `${virtualPoint.inputs?.length || 0}개` 
            },
            { 
              label: '우선순위', 
              value: virtualPoint.priority || 'N/A' 
            },
            { 
              label: '마지막 계산', 
              value: getLastCalculatedText(virtualPoint.last_calculated) 
            }
          ].map((item, index) => (
            <div key={index} style={{ 
              display: 'flex', 
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <span style={{ color: '#6b7280', fontWeight: 500 }}>{item.label}:</span>
              <span style={{ color: '#374151' }}>{item.value}</span>
            </div>
          ))}
        </div>

        {/* 카드 푸터 */}
        <div style={{ 
          display: 'flex', 
          justifyContent: 'space-between', 
          alignItems: 'center',
          paddingTop: '12px',
          borderTop: '1px solid #f3f4f6'
        }}>
          <span style={{ fontSize: '12px', color: '#6b7280' }}>
            생성: {new Date(virtualPoint.created_at).toLocaleDateString('ko-KR')}
          </span>
          
          {/* 활성화/비활성화 토글 - Play 버튼과 다른 기능 */}
          <button
            onClick={handleToggleEnabled}
            disabled={isToggling}
            style={{
              padding: '4px 8px',
              background: 'none',
              border: 'none',
              color: virtualPoint.is_enabled ? '#10b981' : '#6b7280',
              cursor: isToggling ? 'not-allowed' : 'pointer',
              fontSize: '12px',
              fontWeight: 500,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              borderRadius: '4px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (!isToggling) {
                e.currentTarget.style.background = virtualPoint.is_enabled ? '#f0fdf4' : '#f3f4f6';
              }
            }}
            onMouseLeave={(e) => {
              if (!isToggling) {
                e.currentTarget.style.background = 'none';
              }
            }}
          >
            {isToggling ? (
              <i className="fas fa-spinner fa-spin"></i>
            ) : (
              <i className={`fas ${virtualPoint.is_enabled ? 'fa-toggle-on' : 'fa-toggle-off'}`}></i>
            )}
            {isToggling ? '변경 중...' : virtualPoint.is_enabled ? '활성화' : '비활성화'}
          </button>
      </div>
    </div>
  );
};