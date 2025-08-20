// ============================================================================
// frontend/src/components/common/DataPointSelectionTable.tsx
// 🔥 데이터포인트 선택 테이블 - 재사용 가능한 독립 컴포넌트
// ============================================================================

import React from 'react';

export interface DataPoint {
  id: number;
  name: string;
  device_name: string;
  site_name: string;
  data_type: string;
  unit: string;
  current_value: any;
  last_updated: string;
  supports_analog: boolean;
  supports_digital: boolean;
}

export interface DataPointSelectionTableProps {
  dataPoints: DataPoint[];
  selectedIds: number[];
  onSelectionChange: (selectedIds: number[]) => void;
  compatibilityCheck?: (point: DataPoint) => boolean;
  showSelectAll?: boolean;
  maxHeight?: string;
  compact?: boolean;
}

const DataPointSelectionTable: React.FC<DataPointSelectionTableProps> = ({
  dataPoints,
  selectedIds,
  onSelectionChange,
  compatibilityCheck,
  showSelectAll = true,
  maxHeight = '400px',
  compact = false
}) => {
  
  // 안전한 값 렌더링
  const safeRenderValue = (value: any, unit?: string): string => {
    if (value === null || value === undefined) return 'N/A';
    if (typeof value === 'object') {
      // JSON 객체인 경우 value 속성 추출
      if (value.value !== undefined) return `${value.value}${unit ? ` ${unit}` : ''}`;
      return JSON.stringify(value);
    }
    return `${value}${unit ? ` ${unit}` : ''}`;
  };

  // 포인트명 축약 표시
  const truncatePointName = (name: string, maxLength: number = 20): string => {
    if (name.length <= maxLength) return name;
    return `${name.substring(0, maxLength)}...`;
  };

  // 전체 선택/해제
  const handleSelectAll = (checked: boolean) => {
    if (checked) {
      const compatibleIds = dataPoints
        .filter(point => !compatibilityCheck || compatibilityCheck(point))
        .map(point => point.id);
      onSelectionChange(compatibleIds);
    } else {
      onSelectionChange([]);
    }
  };

  // 개별 선택
  const handleIndividualSelect = (pointId: number, checked: boolean) => {
    if (checked) {
      onSelectionChange([...selectedIds, pointId]);
    } else {
      onSelectionChange(selectedIds.filter(id => id !== pointId));
    }
  };

  const compatiblePoints = dataPoints.filter(point => !compatibilityCheck || compatibilityCheck(point));
  const allCompatibleSelected = compatiblePoints.length > 0 && 
    compatiblePoints.every(point => selectedIds.includes(point.id));

  return (
    <div className="datapoint-selection-table">
      {/* 테이블 컨테이너 */}
      <div 
        className="table-container"
        style={{
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          overflow: 'hidden',
          boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
          maxHeight,
          overflowY: 'auto'
        }}
      >
        {/* 테이블 헤더 */}
        <div 
          className="table-header"
          style={{
            display: 'grid',
            gridTemplateColumns: compact 
              ? '40px 2fr 1fr 1fr 100px' 
              : '50px 2fr 150px 150px 120px',
            background: '#f8fafc',
            borderBottom: '1px solid #e5e7eb',
            fontWeight: '600',
            fontSize: '12px',
            color: '#374151',
            position: 'sticky',
            top: 0,
            zIndex: 10
          }}
        >
          <div style={{ padding: '12px 8px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
            {showSelectAll && (
              <input
                type="checkbox"
                checked={allCompatibleSelected}
                onChange={(e) => handleSelectAll(e.target.checked)}
                style={{ width: '16px', height: '16px', accentColor: '#3b82f6' }}
              />
            )}
          </div>
          <div style={{ padding: '12px 16px', display: 'flex', alignItems: 'center' }}>
            데이터포인트
          </div>
          <div style={{ padding: '12px 16px', display: 'flex', alignItems: 'center' }}>
            디바이스
          </div>
          <div style={{ padding: '12px 16px', display: 'flex', alignItems: 'center' }}>
            사이트
          </div>
          <div style={{ padding: '12px 16px', display: 'flex', alignItems: 'center' }}>
            현재값
          </div>
        </div>

        {/* 테이블 바디 */}
        {dataPoints.map(point => {
          const isSelected = selectedIds.includes(point.id);
          const isCompatible = !compatibilityCheck || compatibilityCheck(point);
          
          return (
            <div
              key={point.id}
              className="table-row"
              style={{
                display: 'grid',
                gridTemplateColumns: compact 
                  ? '40px 2fr 1fr 1fr 100px' 
                  : '50px 2fr 150px 150px 120px',
                borderBottom: '1px solid #f1f5f9',
                transition: 'background-color 0.2s ease',
                minHeight: compact ? '50px' : '60px',
                backgroundColor: isSelected ? '#eff6ff' : (isCompatible ? 'white' : '#fafafa'),
                opacity: isCompatible ? 1 : 0.6,
                cursor: isCompatible ? 'pointer' : 'not-allowed'
              }}
              onClick={() => isCompatible && handleIndividualSelect(point.id, !isSelected)}
            >
              {/* 체크박스 */}
              <div style={{ padding: '12px 8px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                <input
                  type="checkbox"
                  checked={isSelected}
                  disabled={!isCompatible}
                  onChange={(e) => {
                    e.stopPropagation();
                    handleIndividualSelect(point.id, e.target.checked);
                  }}
                  style={{ width: '16px', height: '16px', accentColor: '#3b82f6' }}
                />
              </div>

              {/* 데이터포인트 정보 */}
              <div style={{ padding: '12px 16px', display: 'flex', flexDirection: 'column', gap: '2px', overflow: 'hidden' }}>
                <div 
                  style={{ 
                    fontWeight: '500', 
                    color: '#111827', 
                    fontSize: compact ? '13px' : '14px',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap'
                  }}
                  title={point.name} // 풀네임을 툴팁으로 표시
                >
                  {compact ? truncatePointName(point.name, 25) : point.name}
                </div>
                <div style={{ 
                  fontSize: compact ? '11px' : '12px', 
                  color: '#6b7280',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  whiteSpace: 'nowrap'
                }}>
                  {point.data_type} • {point.unit || 'N/A'}
                  {!isCompatible && (
                    <span style={{ color: '#ef4444', fontWeight: '600', marginLeft: '8px' }}>
                      (호환불가)
                    </span>
                  )}
                </div>
              </div>

              {/* 디바이스 */}
              <div style={{ 
                padding: '12px 16px', 
                display: 'flex', 
                alignItems: 'center', 
                fontSize: compact ? '12px' : '14px', 
                color: '#374151',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap'
              }}>
                {point.device_name}
              </div>

              {/* 사이트 */}
              <div style={{ 
                padding: '12px 16px', 
                display: 'flex', 
                alignItems: 'center', 
                fontSize: compact ? '12px' : '14px', 
                color: '#374151',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap'
              }}>
                {point.site_name}
              </div>

              {/* 현재값 */}
              <div style={{ 
                padding: '12px 16px', 
                display: 'flex', 
                alignItems: 'center',
                fontFamily: 'ui-monospace, "Courier New", monospace',
                fontSize: compact ? '11px' : '12px',
                color: '#111827',
                fontWeight: '500',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap'
              }}>
                {safeRenderValue(point.current_value, point.unit)}
              </div>
            </div>
          );
        })}

        {/* 빈 상태 */}
        {dataPoints.length === 0 && (
          <div style={{
            textAlign: 'center',
            padding: '40px 20px',
            color: '#6b7280',
            fontSize: '14px'
          }}>
            선택할 수 있는 데이터포인트가 없습니다.
          </div>
        )}
      </div>

      {/* 하단 정보 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        padding: '12px 0',
        fontSize: '13px',
        color: '#6b7280'
      }}>
        <span>
          총 {dataPoints.length}개 포인트 
          {compatibilityCheck && ` (호환 가능: ${compatiblePoints.length}개)`}
        </span>
        {selectedIds.length > 0 && (
          <span style={{ fontWeight: '600', color: '#1f2937' }}>
            {selectedIds.length}개 선택됨
          </span>
        )}
      </div>
    </div>
  );
};

export default DataPointSelectionTable;