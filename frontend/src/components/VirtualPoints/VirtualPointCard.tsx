// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointCard.tsx
// 가상포인트 카드 컴포넌트
// ============================================================================

import React from 'react';
import { VirtualPoint } from '../../types/virtualPoints';
import { virtualPointsApi } from '../../api/services/virtualPointsApi';

export interface VirtualPointCardProps {
  virtualPoint: VirtualPoint;
  onEdit: (point: VirtualPoint) => void;
  onDelete: (point: VirtualPoint) => void;
  onTest: (point: VirtualPoint) => void;
  onExecute: (pointId: number) => void;
  isExecuting?: boolean;
}

export const VirtualPointCard: React.FC<VirtualPointCardProps> = ({
  virtualPoint,
  onEdit,
  onDelete,
  onTest,
  onExecute,
  isExecuting = false
}) => {
  // ========================================================================
  // 유틸리티 함수들
  // ========================================================================
  
  const getStatusColor = (status: string) => {
    return virtualPointsApi.getStatusColor(status);
  };

  const getStatusIcon = (status: string) => {
    return virtualPointsApi.getStatusIcon(status);
  };

  const getCategoryColor = (category: string) => {
    return virtualPointsApi.getCategoryColor(category);
  };

  const getDataTypeIcon = (dataType: string) => {
    return virtualPointsApi.getDataTypeIcon(dataType);
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
    
    const now = new Date();
    const calculated = new Date(timestamp);
    const diffMs = now.getTime() - calculated.getTime();
    const diffMinutes = Math.floor(diffMs / (1000 * 60));
    
    if (diffMinutes < 1) return '방금 전';
    if (diffMinutes < 60) return `${diffMinutes}분 전`;
    if (diffMinutes < 1440) return `${Math.floor(diffMinutes / 60)}시간 전`;
    return calculated.toLocaleDateString('ko-KR');
  };

  const truncateFormula = (formula: string, maxLength: number = 100) => {
    if (formula.length <= maxLength) return formula;
    return formula.substring(0, maxLength) + '...';
  };

  // ========================================================================
  // 렌더링
  // ========================================================================
  
  return (
    <div className={`virtual-point-card ${virtualPoint.calculation_status}`}>
      {/* 카드 헤더 */}
      <div className="card-header">
        <div className="card-title-section">
          <div className="card-title-row">
            <h3 className="card-title">{virtualPoint.name}</h3>
            <div className="card-badges">
              <span 
                className="status-badge"
                style={{ 
                  backgroundColor: getStatusColor(virtualPoint.calculation_status),
                  color: 'white'
                }}
              >
                <i className={getStatusIcon(virtualPoint.calculation_status)}></i>
                {virtualPoint.calculation_status === 'active' ? '활성' : 
                 virtualPoint.calculation_status === 'error' ? '오류' :
                 virtualPoint.calculation_status === 'disabled' ? '비활성' : '계산중'}
              </span>
              <span 
                className="category-badge"
                style={{ 
                  backgroundColor: getCategoryColor(virtualPoint.category),
                  color: 'white'
                }}
              >
                {virtualPoint.category}
              </span>
            </div>
          </div>
          
          {virtualPoint.description && (
            <p className="card-description">{virtualPoint.description}</p>
          )}
        </div>
        
        <div className="card-actions">
          <button
            className="action-btn toggle-btn"
            onClick={() => onExecute(virtualPoint.id)}
            disabled={isExecuting || !virtualPoint.is_enabled}
            title="실행"
          >
            <i className={`fas ${isExecuting ? 'fa-spinner fa-spin' : 'fa-play'}`}></i>
          </button>
          
          <button
            className="action-btn test-btn"
            onClick={() => onTest(virtualPoint)}
            title="테스트"
          >
            <i className="fas fa-vial"></i>
          </button>
          
          <button
            className="action-btn edit-btn"
            onClick={() => onEdit(virtualPoint)}
            title="편집"
          >
            <i className="fas fa-edit"></i>
          </button>
          
          <button
            className="action-btn delete-btn"
            onClick={() => onDelete(virtualPoint)}
            title="삭제"
          >
            <i className="fas fa-trash"></i>
          </button>
        </div>
      </div>

      {/* 수식 표시 */}
      <div className="card-formula">
        <div className="formula-label">
          <i className="fas fa-code"></i>
          수식:
        </div>
        <div className="formula-content">
          <code>{truncateFormula(virtualPoint.expression)}</code>
          {virtualPoint.expression.length > 100 && (
            <button className="formula-expand" title="전체 수식 보기">
              <i className="fas fa-expand-alt"></i>
            </button>
          )}
        </div>
      </div>

      {/* 현재값 표시 */}
      <div className="card-current-value">
        <div className="current-value-header">
          <span className="current-value-label">
            <i className={getDataTypeIcon(virtualPoint.data_type)}></i>
            현재값:
          </span>
          <span className="data-type-badge">{virtualPoint.data_type}</span>
        </div>
        <div className={`current-value-display ${virtualPoint.calculation_status}`}>
          {formatValue(virtualPoint.current_value, virtualPoint.unit)}
        </div>
      </div>

      {/* 에러 메시지 */}
      {virtualPoint.error_message && (
        <div className="card-error">
          <i className="fas fa-exclamation-triangle"></i>
          <span>{virtualPoint.error_message}</span>
        </div>
      )}

      {/* 메타데이터 */}
      <div className="card-metadata">
        <div className="metadata-grid">
          <div className="metadata-item">
            <span className="metadata-label">범위:</span>
            <span className="metadata-value">
              {virtualPoint.scope_type === 'global' ? '전역' :
               virtualPoint.scope_type === 'site' ? '사이트' : '디바이스'}
            </span>
          </div>
          
          <div className="metadata-item">
            <span className="metadata-label">실행:</span>
            <span className="metadata-value">
              {virtualPoint.calculation_trigger === 'time_based' ? '시간기반' :
               virtualPoint.calculation_trigger === 'event_driven' ? '이벤트' : '수동'}
            </span>
          </div>
          
          <div className="metadata-item">
            <span className="metadata-label">주기:</span>
            <span className="metadata-value">
              {virtualPoint.calculation_interval ? `${virtualPoint.calculation_interval}ms` : 'N/A'}
            </span>
          </div>
          
          <div className="metadata-item">
            <span className="metadata-label">입력:</span>
            <span className="metadata-value">
              {virtualPoint.inputs?.length || 0}개
            </span>
          </div>
          
          <div className="metadata-item">
            <span className="metadata-label">우선순위:</span>
            <span className="metadata-value">
              {virtualPoint.priority || 'N/A'}
            </span>
          </div>
          
          <div className="metadata-item">
            <span className="metadata-label">마지막 계산:</span>
            <span className="metadata-value">
              {getLastCalculatedText(virtualPoint.last_calculated)}
            </span>
          </div>
        </div>
      </div>

      {/* 태그들 */}
      {virtualPoint.tags && virtualPoint.tags.length > 0 && (
        <div className="card-tags">
          {virtualPoint.tags.map(tag => (
            <span key={tag} className="tag">
              #{tag}
            </span>
          ))}
        </div>
      )}

      {/* 카드 푸터 */}
      <div className="card-footer">
        <div className="footer-info">
          <span className="created-info">
            생성: {new Date(virtualPoint.created_at).toLocaleDateString('ko-KR')}
          </span>
          {virtualPoint.created_by && (
            <span className="created-by">
              by {virtualPoint.created_by}
            </span>
          )}
        </div>
        
        <div className="footer-status">
          <span className={`enabled-indicator ${virtualPoint.is_enabled ? 'enabled' : 'disabled'}`}>
            <i className={`fas ${virtualPoint.is_enabled ? 'fa-toggle-on' : 'fa-toggle-off'}`}></i>
            {virtualPoint.is_enabled ? '활성화' : '비활성화'}
          </span>
        </div>
      </div>
    </div>
  );
};