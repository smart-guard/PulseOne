// ============================================================================
// frontend/src/components/modals/TemplateApplyModal.tsx
// 정리된 템플릿 적용 모달 - 단위 기반 필터링 적용
// ============================================================================

import React, { useState, useEffect } from 'react';
import DataPointSelectionTable from '../common/DataPointSelectionTable';
import { DataPoint } from '../../api/services/alarmTemplatesApi';
import { useConfirmContext } from '../common/ConfirmProvider';

export interface AlarmTemplate {
  id: number;
  name: string;
  description: string;
  template_type: 'simple' | 'advanced' | 'script';
  condition_type: string;
  supports_script: boolean;
  applicable_data_types?: string[];
  default_config?: any;
  category?: string;
}

export interface TemplateApplyModalProps {
  isOpen: boolean;
  template: AlarmTemplate | null;
  dataPoints: DataPoint[];
  onClose: () => void;
  onApply: (dataPointIds: number[]) => Promise<void>;
  loading?: boolean;
}

const TemplateApplyModal: React.FC<TemplateApplyModalProps> = ({
  isOpen,
  template,
  dataPoints,
  onClose,
  onApply,
  loading = false
}) => {
  const { showConfirm } = useConfirmContext();
  
  const [selectedDataPoints, setSelectedDataPoints] = useState<number[]>([]);
  const [siteFilter, setSiteFilter] = useState('all');
  const [deviceFilter, setDeviceFilter] = useState('all');
  const [dataTypeFilter, setDataTypeFilter] = useState('all');

  // 모달이 열릴 때마다 초기화
  useEffect(() => {
    if (isOpen) {
      setSelectedDataPoints([]);
      setSiteFilter('all');
      setDeviceFilter('all');
      setDataTypeFilter('all');
    }
  }, [isOpen]);

  // 모달이 닫혀있으면 렌더링하지 않음
  if (!isOpen || !template) return null;

  // 호환성 체크 함수 - 단순하고 효과적인 버전
  const checkCompatibility = (point: DataPoint): boolean => {
    // 1. 기본 데이터 타입 체크
    if (template.condition_type === 'range' || template.condition_type === 'threshold') {
      const isNumeric = ['float', 'double', 'number', 'int', 'real'].includes(point.data_type.toLowerCase());
      if (!isNumeric) return false;
    }
    
    // 2. 단위 기반 필터링 (템플릿 카테고리별)
    if (template.category === 'pressure') {
      const pressureUnits = ['bar', 'psi', 'pa', 'mpa', 'kpa'];
      return pressureUnits.some(unit => point.unit.toLowerCase().includes(unit));
    }
    
    if (template.category === 'temperature') {
      const tempUnits = ['°c', '°f', 'k', 'celsius', 'fahrenheit'];
      return tempUnits.some(unit => point.unit.toLowerCase().includes(unit));
    }
    
    return true; // 기타 템플릿은 숫자형이면 허용
  };

  // 필터링된 데이터포인트
  const compatiblePoints = dataPoints.filter(checkCompatibility);
  const filteredDataPoints = compatiblePoints.filter(point => {
    const matchesSite = siteFilter === 'all' || point.site_name === siteFilter;
    const matchesDevice = deviceFilter === 'all' || point.device_name === deviceFilter;
    const matchesType = dataTypeFilter === 'all' || point.data_type === dataTypeFilter;
    return matchesSite && matchesDevice && matchesType;
  });

  // 고유 값들
  const sites = ['all', ...new Set(compatiblePoints.map(d => d.site_name))];
  const devices = siteFilter === 'all' 
    ? ['all', ...new Set(compatiblePoints.map(d => d.device_name))]
    : ['all', ...new Set(compatiblePoints.filter(d => d.site_name === siteFilter).map(d => d.device_name))];
  const dataTypes = ['all', ...new Set(compatiblePoints.map(d => d.data_type))];

  // 호환성 정보 메시지
  const getCompatibilityMessage = (): string => {
    const conditionType = template.condition_type;
    const templateType = template.template_type;

    if (templateType === 'script' || conditionType === 'script') {
      return '이 템플릿은 스크립트 기반으로, 모든 타입의 데이터포인트에 적용 가능합니다.';
    }

    switch (conditionType) {
      case 'threshold':
        return '이 템플릿은 임계값 타입으로, 숫자형 데이터포인트에만 적용 가능합니다.';
      case 'range':
        return '이 템플릿은 범위 타입으로, 숫자형 데이터포인트에만 적용 가능합니다.';
      case 'digital':
      case 'pattern':
        return '이 템플릿은 디지털/패턴 타입으로, 불린형 데이터포인트에만 적용 가능합니다.';
      default:
        return `이 템플릿은 ${conditionType} 타입입니다.`;
    }
  };

  // 적용 처리
  const handleApply = async () => {
    console.log('적용 버튼 클릭됨', {
      selectedCount: selectedDataPoints.length,
      templateId: template.id
    });

    if (selectedDataPoints.length === 0) {
      showConfirm({
        title: '데이터포인트 선택 필요',
        message: '적어도 하나의 데이터포인트를 선택해주세요.',
        type: 'warning',
        onConfirm: () => {}
      });
      return;
    }

    console.log('onApply 호출 시작');
    try {
      await onApply(selectedDataPoints);
      console.log('onApply 완료');
    } catch (error) {
      console.error('onApply 에러:', error);
      showConfirm({
        title: '적용 실패',
        message: error instanceof Error ? error.message : '템플릿 적용 중 오류가 발생했습니다.',
        type: 'danger',
        onConfirm: () => {}
      });
    }
  };

  return (
    <div 
      className="modal-overlay"
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 9999,
        padding: '20px'
      }}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div 
        className="modal-container"
        style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.1)',
          width: '100%',
          maxWidth: '1200px',
          maxHeight: '90vh',
          display: 'flex',
          flexDirection: 'column'
        }}
      >
        {/* 모달 헤더 */}
        <div className="modal-header" style={{ padding: '24px', borderBottom: '1px solid #e5e7eb' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
            <div style={{ flex: 1 }}>
              <h2 style={{ fontSize: '24px', fontWeight: '700', margin: '0 0 8px 0', color: '#111827' }}>
                템플릿 적용: {template.name}
              </h2>
              <p style={{ fontSize: '14px', color: '#6b7280', margin: 0 }}>
                이 템플릿을 적용할 데이터포인트를 선택하세요
              </p>
            </div>
            <button
              onClick={onClose}
              style={{
                background: 'none',
                border: 'none',
                fontSize: '24px',
                cursor: 'pointer',
                color: '#6b7280',
                padding: '4px'
              }}
            >
              ×
            </button>
          </div>
          
          {/* 호환성 알림 */}
          <div style={{ 
            marginTop: '16px', 
            padding: '12px', 
            background: '#eff6ff', 
            border: '1px solid #bfdbfe', 
            borderRadius: '8px' 
          }}>
            <div style={{ fontSize: '14px', color: '#1e40af', lineHeight: '1.5' }}>
              {getCompatibilityMessage()}
            </div>
          </div>
        </div>

        {/* 모달 컨텐츠 */}
        <div className="modal-content" style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>
          {/* 디버깅 정보 */}
          <div style={{ background: '#f0f9ff', padding: '12px', margin: '12px 0', borderRadius: '6px', fontSize: '12px' }}>
            <strong>디버깅 정보:</strong><br />
            전체 데이터포인트: {dataPoints.length}개<br />
            호환 가능한 포인트: {compatiblePoints.length}개<br />
            필터링된 포인트: {filteredDataPoints.length}개<br />
            선택된 포인트: {selectedDataPoints.length}개<br />
            템플릿 타입: {template.condition_type}<br />
            카테고리: {template.category || 'general'}
          </div>

          {/* 필터 섹션 */}
          <div style={{ 
            display: 'grid', 
            gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', 
            gap: '16px', 
            marginBottom: '24px', 
            padding: '20px', 
            background: '#f8fafc', 
            borderRadius: '12px', 
            border: '1px solid #f1f5f9' 
          }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <label style={{ fontSize: '14px', fontWeight: '600', color: '#374151' }}>
                사이트 선택 ({sites.length - 1}개)
              </label>
              <select 
                value={siteFilter} 
                onChange={(e) => {
                  setSiteFilter(e.target.value);
                  setDeviceFilter('all');
                }}
                style={{ 
                  padding: '10px 12px', 
                  border: '1px solid #e5e7eb', 
                  borderRadius: '8px', 
                  fontSize: '14px', 
                  background: 'white', 
                  cursor: 'pointer' 
                }}
              >
                {sites.map(site => (
                  <option key={site} value={site}>
                    {site === 'all' ? '모든 사이트' : site}
                  </option>
                ))}
              </select>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <label style={{ fontSize: '14px', fontWeight: '600', color: '#374151' }}>
                디바이스 선택 ({devices.length - 1}개)
              </label>
              <select 
                value={deviceFilter} 
                onChange={(e) => setDeviceFilter(e.target.value)}
                style={{ 
                  padding: '10px 12px', 
                  border: '1px solid #e5e7eb', 
                  borderRadius: '8px', 
                  fontSize: '14px', 
                  background: 'white', 
                  cursor: 'pointer' 
                }}
              >
                {devices.map(device => (
                  <option key={device} value={device}>
                    {device === 'all' ? '모든 디바이스' : device}
                  </option>
                ))}
              </select>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <label style={{ fontSize: '14px', fontWeight: '600', color: '#374151' }}>
                데이터 타입 ({dataTypes.length - 1}개)
              </label>
              <select 
                value={dataTypeFilter} 
                onChange={(e) => setDataTypeFilter(e.target.value)}
                style={{ 
                  padding: '10px 12px', 
                  border: '1px solid #e5e7eb', 
                  borderRadius: '8px', 
                  fontSize: '14px', 
                  background: 'white', 
                  cursor: 'pointer' 
                }}
              >
                {dataTypes.map(type => (
                  <option key={type} value={type}>
                    {type === 'all' ? '모든 타입' : type}
                  </option>
                ))}
              </select>
            </div>
          </div>

          {/* 호환성 경고 */}
          {filteredDataPoints.length === 0 && (
            <div style={{ 
              marginBottom: '24px', 
              padding: '16px', 
              background: '#fef2f2', 
              border: '1px solid #fecaca', 
              borderRadius: '12px' 
            }}>
              <div style={{ fontSize: '14px', fontWeight: '600', color: '#dc2626', marginBottom: '8px' }}>
                호환 가능한 데이터포인트가 없습니다
              </div>
              <div style={{ fontSize: '14px', color: '#b91c1c', lineHeight: 1.5 }}>
                선택한 필터 조건에서 이 템플릿과 호환되는 데이터포인트를 찾을 수 없습니다.<br />
                필터 조건을 변경하거나 다른 템플릿을 선택해주세요.
              </div>
            </div>
          )}

          {/* 데이터포인트 선택 테이블 */}
          {filteredDataPoints.length > 0 && (
            <DataPointSelectionTable
              dataPoints={filteredDataPoints}
              selectedIds={selectedDataPoints}
              onSelectionChange={setSelectedDataPoints}
              compatibilityCheck={checkCompatibility}
              showSelectAll={true}
              maxHeight="auto"
              compact={false}
            />
          )}
        </div>

        {/* 모달 푸터 */}
        <div style={{ 
          display: 'flex', 
          justifyContent: 'space-between', 
          alignItems: 'center', 
          padding: '24px', 
          borderTop: '1px solid #e5e7eb', 
          background: '#f8fafc' 
        }}>
          <div>
            {selectedDataPoints.length > 0 && (
              <span style={{ fontWeight: '600', color: '#1f2937', fontSize: '14px' }}>
                {selectedDataPoints.length}개 포인트 선택됨
              </span>
            )}
            {filteredDataPoints.length === 0 && (
              <span style={{ fontSize: '14px', color: '#dc2626' }}>
                호환 가능한 데이터포인트가 없습니다
              </span>
            )}
          </div>
          <div style={{ display: 'flex', gap: '12px' }}>
            <button
              onClick={onClose}
              disabled={loading}
              style={{
                padding: '12px 24px',
                border: '1px solid #d1d5db',
                borderRadius: '8px',
                background: '#ffffff',
                color: '#374151',
                cursor: loading ? 'not-allowed' : 'pointer',
                fontSize: '14px',
                fontWeight: '500',
                opacity: loading ? 0.6 : 1
              }}
            >
              취소
            </button>
            <button
              onClick={handleApply}
              disabled={selectedDataPoints.length === 0 || loading || filteredDataPoints.length === 0}
              style={{
                padding: '12px 24px',
                border: 'none',
                borderRadius: '8px',
                background: selectedDataPoints.length === 0 || loading || filteredDataPoints.length === 0 ? '#d1d5db' : '#3b82f6',
                color: 'white',
                cursor: selectedDataPoints.length === 0 || loading || filteredDataPoints.length === 0 ? 'not-allowed' : 'pointer',
                fontSize: '14px',
                fontWeight: '500',
                opacity: selectedDataPoints.length === 0 || loading || filteredDataPoints.length === 0 ? 0.6 : 1
              }}
            >
              {loading ? '적용 중...' : `적용하기 (${selectedDataPoints.length}개)`}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default TemplateApplyModal;