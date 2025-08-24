// VirtualPointCard.tsx 수정 - 테스트 기능을 프론트엔드에서 처리

// 기존 imports...
import React, { useState } from 'react';
import { virtualPointsApi } from '../../api/services/virtualPointsApi';

// 프론트엔드 테스트 함수 추가
const evaluateExpression = (expression: string, testInputs: Record<string, any> = {}): any => {
  try {
    // 빈 수식 체크
    if (!expression || expression.trim() === '') {
      throw new Error('수식이 비어있습니다');
    }

    // 변수 치환
    let code = expression;
    Object.entries(testInputs).forEach(([name, value]) => {
      const regex = new RegExp(`\\b${name}\\b`, 'g');
      code = code.replace(regex, String(value));
    });

    // 수학 함수 지원
    code = code.replace(/\bmax\(/g, 'Math.max(');
    code = code.replace(/\bmin\(/g, 'Math.min(');
    code = code.replace(/\babs\(/g, 'Math.abs(');
    code = code.replace(/\bsqrt\(/g, 'Math.sqrt(');
    code = code.replace(/\bpow\(/g, 'Math.pow(');
    code = code.replace(/\bround\(/g, 'Math.round(');
    code = code.replace(/\bfloor\(/g, 'Math.floor(');
    code = code.replace(/\bceil\(/g, 'Math.ceil(');

    // 안전한 평가
    const func = new Function('Math', `return ${code}`);
    const result = func(Math);
    
    // 결과 검증
    if (typeof result === 'number' && !isFinite(result)) {
      throw new Error('계산 결과가 유효하지 않습니다 (무한대 또는 NaN)');
    }
    
    return result;
  } catch (error) {
    throw new Error('수식 계산 실패: ' + (error as Error).message);
  }
};

// 테스트 입력값 생성 함수
const generateTestInputs = (virtualPoint: any): Record<string, any> => {
  const testInputs: Record<string, any> = {};
  
  // inputs 배열에서 변수 추출
  if (virtualPoint.inputs && Array.isArray(virtualPoint.inputs)) {
    virtualPoint.inputs.forEach((input: any) => {
      if (input.variable_name) {
        // constant_value가 있으면 사용, 없으면 기본값
        testInputs[input.variable_name] = input.constant_value || 10;
      }
    });
  }
  
  // 수식에서 변수 자동 추출 (백업)
  const formula = virtualPoint.formula || virtualPoint.expression || '';
  const variableMatches = formula.match(/\b[a-zA-Z_][a-zA-Z0-9_]*\b/g);
  
  if (variableMatches) {
    variableMatches.forEach(variable => {
      // Math 함수들은 제외
      if (!variable.startsWith('Math') && 
          !['max', 'min', 'abs', 'sqrt', 'pow', 'round', 'floor', 'ceil'].includes(variable) &&
          !testInputs[variable]) {
        testInputs[variable] = 10; // 기본 테스트 값
      }
    });
  }
  
  return testInputs;
};

export const VirtualPointCard: React.FC<VirtualPointCardProps> = ({
  // 기존 props...
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
  const [isTesting, setIsTesting] = useState(false);

  // 기존 함수들... (getStatusColor, formatValue 등)
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

  const ensureRefresh = async () => {
    if (onRefresh) {
      await onRefresh();
    } else {
      window.location.reload();
    }
  };

  // 수정된 테스트 함수 - 프론트엔드에서 처리
  const handleTest = async (e: React.MouseEvent) => {
    e.stopPropagation();
    
    if (isTesting || isProcessing) return;
    
    setIsTesting(true);
    
    try {
      // 테스트 입력값 생성
      const testInputs = generateTestInputs(virtualPoint);
      const formula = virtualPoint.formula || virtualPoint.expression || '';
      
      console.log('테스트 시작:', { 
        formula, 
        testInputs, 
        virtualPointName: virtualPoint.name 
      });
      
      // 프론트엔드에서 계산
      const startTime = performance.now();
      const result = evaluateExpression(formula, testInputs);
      const endTime = performance.now();
      const executionTime = Math.round((endTime - startTime) * 100) / 100;
      
      // 성공 알림
      const inputsText = Object.entries(testInputs)
        .map(([key, value]) => `${key}=${value}`)
        .join(', ');
      
      alert(
        `테스트 성공!\n\n` +
        `수식: ${formula}\n` +
        `입력: ${inputsText}\n` +
        `결과: ${formatValue(result, virtualPoint.unit)}\n` +
        `실행시간: ${executionTime}ms`
      );
      
      console.log('테스트 성공:', { 
        result, 
        executionTime, 
        testInputs 
      });
      
    } catch (error) {
      console.error('테스트 실패:', error);
      alert(`테스트 실패:\n\n${error instanceof Error ? error.message : '알 수 없는 오류'}`);
    } finally {
      setIsTesting(false);
    }
  };

  // 기존 다른 함수들... (handleExecute, handleEdit, handleDeleteClick, handleToggleEnabled)
  const handleExecute = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (!virtualPoint.is_enabled || isExecuting) return;
    
    try {
      await onExecute(virtualPoint.id);
      await ensureRefresh();
    } catch (error) {
      console.error('가상포인트 실행 실패:', error);
      alert('실행 실패: ' + (error instanceof Error ? error.message : '알 수 없는 오류'));
    }
  };

  const handleEdit = (e: React.MouseEvent) => {
    e.stopPropagation();
    onEdit(virtualPoint);
  };

  const handleDeleteClick = async (e: React.MouseEvent) => {
    e.stopPropagation();
    
    const confirmed = confirm(`"${virtualPoint.name}" 가상포인트를 삭제하시겠습니까?\n\n이 작업은 되돌릴 수 없습니다.`);
    
    if (confirmed) {
      setIsProcessing(true);
      try {
        await virtualPointsApi.deleteVirtualPoint(virtualPoint.id);
        console.log('가상포인트 삭제 완료:', virtualPoint.id);
        await ensureRefresh();
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
    if (isToggling || isProcessing) return;
    
    const newStatus = !virtualPoint.is_enabled;
    const actionText = newStatus ? '활성화' : '비활성화';
    
    const confirmed = confirm(
      `"${virtualPoint.name}" 가상포인트를 ${actionText}하시겠습니까?\n\n` +
      `${newStatus ? 
        '활성화하면 설정된 조건에 따라 자동으로 계산이 수행됩니다.' : 
        '비활성화하면 더 이상 계산이 수행되지 않습니다.'
      }`
    );
    
    if (!confirmed) return;
    
    setIsToggling(true);
    try {
      await virtualPointsApi.toggleVirtualPoint(virtualPoint.id, newStatus);
      console.log(`가상포인트 ${virtualPoint.id} ${actionText} 완료`);
      await ensureRefresh();
    } catch (error) {
      console.error('가상포인트 활성화 토글 실패:', error);
      alert(`${actionText} 실패: ` + (error instanceof Error ? error.message : '알 수 없는 오류'));
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
      height: 'fit-content',
      position: 'relative',
      opacity: isProcessing ? 0.7 : 1,
      pointerEvents: isProcessing ? 'none' : 'auto'
    }}
    onMouseEnter={(e) => {
      if (!isProcessing) {
        e.currentTarget.style.boxShadow = '0 4px 12px rgba(0, 0, 0, 0.15)';
        e.currentTarget.style.transform = 'translateY(-2px)';
      }
    }}
    onMouseLeave={(e) => {
      if (!isProcessing) {
        e.currentTarget.style.boxShadow = '0 1px 3px rgba(0, 0, 0, 0.1)';
        e.currentTarget.style.transform = 'translateY(0)';
      }
    }}
    >
        {/* 처리중 오버레이 */}
        {isProcessing && (
          <div style={{
            position: 'absolute',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            background: 'rgba(255, 255, 255, 0.9)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            borderRadius: '12px',
            zIndex: 10
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <i className="fas fa-spinner fa-spin" style={{ color: '#3b82f6' }}></i>
              <span style={{ color: '#3b82f6', fontWeight: 500 }}>처리 중...</span>
            </div>
          </div>
        )}

        {/* 나머지 JSX는 기존과 동일하되, 테스트 버튼만 수정 */}
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
          
          {/* 액션 버튼들 - 테스트 버튼 수정 */}
          <div style={{ display: 'flex', gap: '4px', marginLeft: '12px' }}>
            {/* 실행 버튼 */}
            <button
              onClick={handleExecute}
              disabled={!virtualPoint.is_enabled || isExecuting || isProcessing}
              style={{
                padding: '6px',
                background: 'none',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                color: (virtualPoint.is_enabled && !isProcessing) ? '#374151' : '#9ca3af',
                cursor: (virtualPoint.is_enabled && !isProcessing) ? 'pointer' : 'not-allowed',
                opacity: (virtualPoint.is_enabled && !isProcessing) ? 1 : 0.5
              }}
              title="실행"
            >
              {isExecuting ? 
                <i className="fas fa-spinner fa-spin" style={{ fontSize: '12px' }}></i> :
                <i className="fas fa-play" style={{ fontSize: '12px' }}></i>
              }
            </button>
            
            {/* 수정된 테스트 버튼 */}
            <button
              onClick={handleTest}
              disabled={isProcessing || isTesting}
              style={{
                padding: '6px',
                background: 'none',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                color: (isProcessing || isTesting) ? '#9ca3af' : '#374151',
                cursor: (isProcessing || isTesting) ? 'not-allowed' : 'pointer',
                opacity: (isProcessing || isTesting) ? 0.5 : 1
              }}
              title="테스트 (프론트엔드 처리)"
            >
              {isTesting ? 
                <i className="fas fa-spinner fa-spin" style={{ fontSize: '12px' }}></i> :
                <i className="fas fa-vial" style={{ fontSize: '12px' }}></i>
              }
            </button>
            
            {/* 기존 편집, 삭제 버튼들... */}
            <button onClick={handleEdit} disabled={isProcessing} style={{ padding: '6px', background: 'none', border: '1px solid #d1d5db', borderRadius: '6px', color: isProcessing ? '#9ca3af' : '#374151', cursor: isProcessing ? 'not-allowed' : 'pointer', opacity: isProcessing ? 0.5 : 1 }} title="편집">
              <i className="fas fa-edit" style={{ fontSize: '12px' }}></i>
            </button>
            
            <button onClick={handleDeleteClick} disabled={isProcessing} style={{ padding: '6px', background: 'none', border: '1px solid #d1d5db', borderRadius: '6px', color: isProcessing ? '#9ca3af' : '#dc2626', cursor: isProcessing ? 'not-allowed' : 'pointer', opacity: isProcessing ? 0.5 : 1 }} title="삭제">
              {isProcessing ? <i className="fas fa-spinner fa-spin" style={{ fontSize: '12px' }}></i> : <i className="fas fa-trash" style={{ fontSize: '12px' }}></i>}
            </button>
          </div>
        </div>

        {/* 수식 표시 */}
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

        {/* 나머지 기존 JSX... */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ fontSize: '12px', color: '#6b7280', marginBottom: '4px', fontWeight: 500 }}>현재값:</div>
          <div style={{ padding: '8px 12px', background: virtualPoint.calculation_status === 'success' || virtualPoint.calculation_status === 'active' ? '#f0fdf4' : '#f9fafb', borderRadius: '6px', fontSize: '14px', fontWeight: 600, color: virtualPoint.calculation_status === 'success' || virtualPoint.calculation_status === 'active' ? '#166534' : '#6b7280', fontFamily: 'JetBrains Mono, Consolas, monospace', border: '1px solid #e5e7eb' }}>
            {formatValue(virtualPoint.current_value, virtualPoint.unit)}
          </div>
        </div>

        {/* 메타데이터 그리드와 푸터는 기존과 동일 */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px 16px', fontSize: '12px', padding: '12px', background: '#f9fafb', borderRadius: '6px', border: '1px solid #e5e7eb', marginBottom: '12px' }}>
          {[
            { label: '범위', value: virtualPoint.scope_type === 'global' || virtualPoint.scope_type === 'tenant' ? '전역' : virtualPoint.scope_type === 'site' ? '사이트' : '디바이스' },
            { label: '실행', value: virtualPoint.calculation_trigger === 'time_based' || virtualPoint.calculation_trigger === 'timer' ? '시간기반' : virtualPoint.calculation_trigger === 'event_driven' || virtualPoint.calculation_trigger === 'onchange' ? '이벤트' : '수동' },
            { label: '주기', value: virtualPoint.calculation_interval ? `${virtualPoint.calculation_interval}ms` : 'N/A' },
            { label: '입력', value: `${virtualPoint.inputs?.length || 0}개` },
            { label: '우선순위', value: virtualPoint.priority || 'N/A' },
            { label: '마지막 계산', value: getLastCalculatedText(virtualPoint.last_calculated) }
          ].map((item, index) => (
            <div key={index} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ color: '#6b7280', fontWeight: 500 }}>{item.label}:</span>
              <span style={{ color: '#374151' }}>{item.value}</span>
            </div>
          ))}
        </div>

        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', paddingTop: '12px', borderTop: '1px solid #f3f4f6' }}>
          <span style={{ fontSize: '12px', color: '#6b7280' }}>
            생성: {new Date(virtualPoint.created_at).toLocaleDateString('ko-KR')}
          </span>
          
          <button onClick={handleToggleEnabled} disabled={isToggling || isProcessing} style={{ padding: '4px 8px', background: 'none', border: 'none', color: (isToggling || isProcessing) ? '#9ca3af' : virtualPoint.is_enabled ? '#10b981' : '#6b7280', cursor: (isToggling || isProcessing) ? 'not-allowed' : 'pointer', fontSize: '12px', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '4px', borderRadius: '4px', transition: 'all 0.2s', opacity: (isToggling || isProcessing) ? 0.5 : 1 }}>
            {isToggling ? <i className="fas fa-spinner fa-spin"></i> : <i className={`fas ${virtualPoint.is_enabled ? 'fa-toggle-on' : 'fa-toggle-off'}`}></i>}
            {isToggling ? '변경 중...' : virtualPoint.is_enabled ? '활성화' : '비활성화'}
          </button>
        </div>
    </div>
  );
};