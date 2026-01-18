import React, { useState } from 'react';
import { virtualPointsApi } from '../../api/services/virtualPointsApi';
import { useConfirmContext } from '../common/ConfirmProvider';


interface VirtualPointCardProps {
  virtualPoint: any;
  onEdit: (point: any) => void;
  onDelete?: (id: number) => void;
  onRestore?: (id: number) => Promise<void>;
  onTest?: (point: any) => void;
  onExecute: (id: number) => Promise<void>;
  onToggleEnabled: (id: number) => Promise<void>;
  onRowClick?: (point: any) => void;
  isExecuting?: boolean;
  onRefresh?: () => Promise<void>;
}

// 프론트엔드 테스트 함수 추가
const evaluateExpression = (expression: string, testInputs: Record<string, any> = {}): any => {
  try {
    if (!expression || expression.trim() === '') {
      throw new Error('수식이 비어있습니다');
    }

    let code = expression;
    Object.entries(testInputs).forEach(([name, value]) => {
      const regex = new RegExp(`\\b${name}\\b`, 'g');
      code = code.replace(regex, String(value));
    });

    code = code.replace(/\bmax\(/g, 'Math.max(');
    code = code.replace(/\bmin\(/g, 'Math.min(');
    code = code.replace(/\babs\(/g, 'Math.abs(');
    code = code.replace(/\bsqrt\(/g, 'Math.sqrt(');
    code = code.replace(/\bpow\(/g, 'Math.pow(');
    code = code.replace(/\bround\(/g, 'Math.round(');
    code = code.replace(/\bfloor\(/g, 'Math.floor(');
    code = code.replace(/\bceil\(/g, 'Math.ceil(');

    const func = new Function('Math', `return ${code}`);
    const result = func(Math);

    if (typeof result === 'number' && !isFinite(result)) {
      throw new Error('계산 결과가 유효하지 않습니다 (무한대 또는 NaN)');
    }

    return result;
  } catch (error) {
    throw new Error('수식 계산 실패: ' + (error as Error).message);
  }
};

const generateTestInputs = (virtualPoint: any): Record<string, any> => {
  const testInputs: Record<string, any> = {};
  if (virtualPoint.inputs && Array.isArray(virtualPoint.inputs)) {
    virtualPoint.inputs.forEach((input: any) => {
      if (input.variable_name) {
        testInputs[input.variable_name] = input.constant_value || 10;
      }
    });
  }
  const formula = virtualPoint.formula || virtualPoint.expression || '';
  const variableMatches = formula.match(/\b[a-zA-Z_][a-zA-Z0-9_]*\b/g);
  if (variableMatches) {
    variableMatches.forEach(variable => {
      if (!variable.startsWith('Math') &&
        !['max', 'min', 'abs', 'sqrt', 'pow', 'round', 'floor', 'ceil'].includes(variable) &&
        !testInputs[variable]) {
        testInputs[variable] = 10;
      }
    });
  }
  return testInputs;
};

export const VirtualPointCard: React.FC<VirtualPointCardProps> = ({
  virtualPoint,
  onEdit,
  onDelete,
  onTest,
  onExecute,
  onToggleEnabled,
  onRestore,
  onRowClick,
  isExecuting = false,
  onRefresh
}) => {
  const { confirm } = useConfirmContext();
  const [isProcessing, setIsProcessing] = useState(false);
  const [isToggling, setIsToggling] = useState(false);
  const [isTesting, setIsTesting] = useState(false);

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
      '온도': '#ef4444', '압력': '#3b82f6', '유량': '#10b981', '전력': '#f59e0b',
      'Custom': '#14b8a6', 'calculation': '#8b5cf6'
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
      return new Date(timestamp).toLocaleDateString('ko-KR');
    } catch { return 'N/A'; }
  };

  const handleTest = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (isTesting || isProcessing) return;
    setIsTesting(true);
    try {
      const testInputs = generateTestInputs(virtualPoint);
      const formula = virtualPoint.formula || virtualPoint.expression || '';
      const startTime = performance.now();
      const result = evaluateExpression(formula, testInputs);
      const endTime = performance.now();
      const executionTime = Math.round((endTime - startTime) * 100) / 100;
      const inputsText = Object.entries(testInputs).map(([k, v]) => `${k}=${v}`).join(', ');
      alert(`테스트 성공!\n결과: ${formatValue(result, virtualPoint.unit)}\n입력: ${inputsText}\n실행시간: ${executionTime}ms`);
    } catch (error) {
      alert(`테스트 실패:\n${error instanceof Error ? error.message : '알 수 없는 오류'}`);
    } finally { setIsTesting(false); }
  };

  const handleClick = () => {
    if (onRowClick) onRowClick(virtualPoint);
    else onEdit(virtualPoint);
  };

  const handleExecute = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (!virtualPoint.is_enabled || isExecuting) return;
    try { await onExecute(virtualPoint.id); if (onRefresh) await onRefresh(); }
    catch (error) { alert('실행 실패: ' + (error instanceof Error ? error.message : '오류')); }
  };

  const handleEdit = (e: React.MouseEvent) => { e.stopPropagation(); onEdit(virtualPoint); };

  const handleDeleteClick = async (e: React.MouseEvent) => {
    e.stopPropagation();

    const isConfirmed = await confirm({
      title: '가상 포인트 삭제',
      message: `"${virtualPoint.name}" 가상 포인트를 삭제하시겠습니까?\n삭제된 항목은 '삭제된 항목 보기' 필터를 통해 복원할 수 있습니다.`,
      confirmText: '삭제하기',
      cancelText: '취소',
      confirmButtonType: 'danger'
    });

    if (isConfirmed) {
      setIsProcessing(true);
      try {
        await virtualPointsApi.deleteVirtualPoint(virtualPoint.id);
        if (onRefresh) await onRefresh();

        await confirm({
          title: '삭제 완료',
          message: '가상포인트가 성공적으로 삭제되었습니다.',
          confirmText: '확인',
          showCancelButton: false
        });
      } catch (error) {
        await confirm({
          title: '삭제 실패',
          message: '가상 포인트를 삭제하는 중 오류가 발생했습니다.',
          confirmText: '확인',
          showCancelButton: false
        });
      } finally {
        setIsProcessing(false);
      }
    }
  };

  const handleRestoreClick = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (!onRestore) return;

    const isConfirmed = await confirm({
      title: '가상 포인트 복원',
      message: `"${virtualPoint.name}" 가상 포인트를 복원하시겠습니까?`,
      confirmText: '복원하기',
      cancelText: '취소'
    });

    if (isConfirmed) {
      setIsProcessing(true);
      try {
        await onRestore(virtualPoint.id);

        await confirm({
          title: '복원 완료',
          message: '가상포인트가 성공적으로 복원되었습니다.',
          confirmText: '확인',
          showCancelButton: false
        });
      } catch (error) {
        alert('복원 실패: ' + (error instanceof Error ? error.message : '오류'));
      } finally {
        setIsProcessing(false);
      }
    }
  };


  const handleToggleEnabled = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (isToggling || isProcessing) return;
    setIsToggling(true);
    try {
      await virtualPointsApi.toggleVirtualPoint(virtualPoint.id, !virtualPoint.is_enabled);
      if (onRefresh) await onRefresh();

      await confirm({
        title: '상태 변경 완료',
        message: `가상포인트가 ${!virtualPoint.is_enabled ? '활성화' : '비활성화'} 되었습니다.`,
        confirmText: '확인',
        showCancelButton: false
      });
    }
    catch (error) { alert('상태 변경 실패'); }
    finally { setIsToggling(false); }
  };

  return (
    <div className={`premium-point-card status-${virtualPoint.calculation_status || 'disabled'} ${virtualPoint.is_deleted ? 'is-deleted' : ''}`}
      style={virtualPoint.is_deleted ? { opacity: 0.7, filter: 'grayscale(0.5)' } : {}}
      onClick={handleClick}>
      <div className="card-body">
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
          <div style={{ flex: 1 }}>
            <h3 className="card-title" style={{ color: '#0f172a', margin: 0 }}>{virtualPoint.name}</h3>
            {virtualPoint.description && <p className="card-desc">{virtualPoint.description}</p>}
          </div>
          <div style={{ display: 'flex', gap: '6px' }}>
            {!!virtualPoint.is_deleted && (
              <span className="status-pill disabled" style={{ fontSize: '10px', background: '#f1f5f9', color: '#64748b' }}>
                <i className="fas fa-trash-alt"></i> 삭제됨
              </span>
            )}
            <span className={`status-pill ${virtualPoint.calculation_status || 'disabled'}`} style={{ fontSize: '10px' }}>
              <i className={getStatusIcon(virtualPoint.calculation_status || 'disabled').replace('fa-spin', '')}></i>
              {virtualPoint.calculation_status === 'success' || virtualPoint.calculation_status === 'active' ? '정상' : '오류'}
            </span>
          </div>
        </div>

        <div className="formula-preview">
          <code>{virtualPoint.formula || virtualPoint.expression || 'N/A'}</code>
        </div>

        <div className="current-value-box">
          <div className="value-label">현재 값</div>
          <div className="value-display">{formatValue(virtualPoint.current_value, virtualPoint.unit)}</div>
        </div>

        <div className="card-meta">
          <div className="meta-item"><span className="label">주기</span><span className="value">{virtualPoint.calculation_interval}ms</span></div>
          <div className="meta-item"><span className="label">트리거</span><span className="value">{virtualPoint.calculation_trigger}</span></div>
          <div className="meta-item"><span className="label">변수</span><span className="value">{virtualPoint.inputs?.length || 0}개</span></div>
          <div className="meta-item"><span className="label">계산</span><span className="value">{getLastCalculatedText(virtualPoint.last_calculated)}</span></div>
        </div>
      </div>

      {isProcessing && <div className="card-overlay"><i className="fas fa-spinner fa-spin"></i></div>}
    </div>
  );
};
