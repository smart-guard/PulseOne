// ============================================================================
// frontend/src/components/modals/TemplateApplyModal.tsx
// 템플릿 적용 모달 - 독립 컴포넌트 (변수 선언 순서 수정)
// ============================================================================

import React, { useState, useEffect } from 'react';
import DataPointSelectionTable from '../common/DataPointSelectionTable';
import { DataPoint } from '../../api/services/alarmTemplatesApi';

export interface AlarmTemplate {
  id: number;
  name: string;
  description: string;
  template_type: 'simple' | 'advanced' | 'script';
  condition_type: string;
  supports_script: boolean;
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

  // 호환성 체크 함수
  const checkCompatibility = (point: DataPoint): boolean => {
    if (template.template_type === 'script') return true;
    if (template.condition_type === 'pattern' && !point.supports_digital) return false;
    if (template.condition_type === 'threshold' && !point.supports_analog) return false;
    if (template.condition_type === 'range' && !point.supports_analog) return false;
    return true;
  };

  // 필터링된 데이터포인트
  const filteredDataPoints = dataPoints.filter(point => {
    const matchesSite = siteFilter === 'all' || point.site_name === siteFilter;
    const matchesDevice = deviceFilter === 'all' || point.device_name === deviceFilter;
    const matchesType = dataTypeFilter === 'all' || point.data_type === dataTypeFilter;
    return matchesSite && matchesDevice && matchesType;
  });

  // 고유 값들
  const sites = ['all', ...new Set(dataPoints.map(d => d.site_name))];
  const devices = ['all', ...new Set(dataPoints.filter(d => siteFilter === 'all' || d.site_name === siteFilter).map(d => d.device_name))];
  const dataTypes = ['all', ...new Set(dataPoints.map(d => d.data_type))];

  // 적용 처리
  const handleApply = async () => {
    if (selectedDataPoints.length === 0) {
      alert('적어도 하나의 데이터포인트를 선택해주세요.');
      return;
    }

    try {
      await onApply(selectedDataPoints);
    } catch (error) {
      console.error('템플릿 적용 실패:', error);
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
          
          {template.template_type !== 'simple' && (
            <div style={{ 
              marginTop: '16px', 
              padding: '12px', 
              background: '#fef3c7', 
              border: '1px solid #fbbf24', 
              borderRadius: '8px' 
            }}>
              <div style={{ fontSize: '14px', color: '#92400e' }}>
                ⚠️ 이 템플릿은 {template.template_type === 'advanced' ? '고급 설정' : '스크립트 기반'} 템플릿입니다. 
                적용 후 세부 설정을 확인해주세요.
              </div>
            </div>
          )}
        </div>

        {/* 모달 컨텐츠 */}
        <div className="modal-content" style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>
          {/* 디버깅 정보 */}
          <div style={{ background: '#f0f9ff', padding: '12px', margin: '12px 0', borderRadius: '6px' }}>
            디버깅: 전달받은 데이터포인트 {dataPoints.length}개, 필터링된 데이터포인트 {filteredDataPoints.length}개
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
                1️⃣ 사이트 선택
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
                2️⃣ 디바이스 선택
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
                3️⃣ 데이터 타입
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

          {/* 호환성 정보 */}
          <div style={{ 
            marginBottom: '24px', 
            padding: '16px', 
            background: '#eff6ff', 
            border: '1px solid #bfdbfe', 
            borderRadius: '12px' 
          }}>
            <div style={{ fontSize: '14px', fontWeight: '600', color: '#1d4ed8', marginBottom: '8px' }}>
              ℹ️ 템플릿 호환성 정보
            </div>
            <div style={{ fontSize: '14px', color: '#2563eb', lineHeight: 1.5 }}>
              이 템플릿은 <strong>{template.condition_type}</strong> 타입으로, 
              {template.condition_type === 'threshold' || template.condition_type === 'range' 
                ? ' 아날로그 데이터포인트' : ' 디지털 데이터포인트'}에만 적용 가능합니다.
              {template.supports_script && ' JavaScript 스크립트를 지원합니다.'}
            </div>
          </div>

          {/* 데이터포인트 선택 테이블 */}
          <DataPointSelectionTable
            dataPoints={filteredDataPoints}
            selectedIds={selectedDataPoints}
            onSelectionChange={setSelectedDataPoints}
            compatibilityCheck={checkCompatibility}
            showSelectAll={true}
            maxHeight="300px"
            compact={false}
          />
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
              disabled={selectedDataPoints.length === 0 || loading}
              style={{
                padding: '12px 24px',
                border: 'none',
                borderRadius: '8px',
                background: selectedDataPoints.length === 0 || loading ? '#d1d5db' : '#3b82f6',
                color: 'white',
                cursor: selectedDataPoints.length === 0 || loading ? 'not-allowed' : 'pointer',
                fontSize: '14px',
                fontWeight: '500',
                opacity: selectedDataPoints.length === 0 || loading ? 0.6 : 1
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