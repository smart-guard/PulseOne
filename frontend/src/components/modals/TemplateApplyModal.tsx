// ============================================================================
// frontend/src/components/modals/TemplateApplyModal.tsx
// 단순화된 템플릿 적용 모달 - 직접 API 호출
// ============================================================================

import React, { useState, useEffect } from 'react';
import DataPointSelectionTable from '../common/DataPointSelectionTable';
import alarmTemplatesApi, { DataPoint } from '../../api/services/alarmTemplatesApi';
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
  onSuccess?: () => void; // 성공 시 호출될 콜백 (데이터 새로고침 등)
}

const TemplateApplyModal: React.FC<TemplateApplyModalProps> = ({
  isOpen,
  template,
  dataPoints,
  onClose,
  onSuccess
}) => {
  const { showConfirm } = useConfirmContext();
  
  const [selectedDataPoints, setSelectedDataPoints] = useState<number[]>([]);
  const [siteFilter, setSiteFilter] = useState('all');
  const [deviceFilter, setDeviceFilter] = useState('all');
  const [dataTypeFilter, setDataTypeFilter] = useState('all');
  const [loading, setLoading] = useState(false);
  const [showConfirmDialog, setShowConfirmDialog] = useState(false);

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
    console.log(`호환성 체크: ${point.name}`, {
      point: {
        name: point.name,
        data_type: point.data_type,
        unit: point.unit,
        device_name: point.device_name
      },
      template: {
        condition_type: template.condition_type,
        category: template.category
      }
    });

    // 1. 기본 데이터 타입 체크
    if (template.condition_type === 'range' || template.condition_type === 'threshold') {
      const isNumeric = ['float', 'double', 'number', 'int', 'real', 'uint32', 'int32'].includes(point.data_type.toLowerCase());
      console.log(`  숫자형 체크: ${isNumeric} (data_type: ${point.data_type})`);
      if (!isNumeric) {
        console.log(`  ❌ 비호환: 숫자형이 아님`);
        return false;
      }
    }
    
    // 2. 단위 기반 필터링 (템플릿 카테고리별)
    if (template.category === 'pressure') {
      const pressureUnits = ['bar', 'psi', 'pa', 'mpa', 'kpa'];
      const isCompatible = pressureUnits.some(unit => point.unit.toLowerCase().includes(unit));
      console.log(`  압력 단위 체크: ${isCompatible} (unit: ${point.unit})`);
      return isCompatible;
    }
    
    if (template.category === 'temperature') {
      const tempUnits = ['°c', '°f', 'k', 'celsius', 'fahrenheit', 'temp'];
      const isCompatible = tempUnits.some(unit => point.unit.toLowerCase().includes(unit));
      console.log(`  온도 단위 체크: ${isCompatible} (unit: ${point.unit})`);
      return isCompatible;
    }
    
    console.log(`  ✅ 호환: 기본 조건 통과`);
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

  // 템플릿 적용 처리
  const handleApply = async () => {
    console.log('적용하기 버튼 클릭됨', {
      selectedDataPoints: selectedDataPoints,
      selectedCount: selectedDataPoints.length,
      loading: loading,
      filteredDataPointsLength: filteredDataPoints.length
    });

    // 선택된 데이터포인트가 없을 때 처리
    if (selectedDataPoints.length === 0) {
      console.log('선택된 데이터포인트가 없음');
      alert('아무것도 선택하지 않았습니다.\n적어도 하나의 데이터포인트를 선택해주세요.');
      return;
    }

    // 내장 확인 대화상자 표시
    console.log('내장 확인 대화상자 표시');
    setShowConfirmDialog(true);
  };

  // 확인 후 실제 실행
  const handleConfirmApply = async () => {
    setShowConfirmDialog(false);
    console.log('사용자가 확인함, 템플릿 적용 시작');
    await executeTemplateApplication();
  };

  // 취소 처리
  const handleCancelApply = () => {
    setShowConfirmDialog(false);
    console.log('사용자가 취소함');
  };

  // 실제 템플릿 적용 실행
  const executeTemplateApplication = async () => {
    setLoading(true);
    console.log('로딩 상태 시작');
    
    try {
      console.log('템플릿 적용 시작:', {
        templateId: template.id,
        templateName: template.name,
        dataPointIds: selectedDataPoints
      });

      // API 요청
      const request = {
        target_ids: selectedDataPoints,
        target_type: 'data_point',
        custom_configs: {},
        rule_group_name: `${template.name}_${new Date().toISOString().split('T')[0]}`
      };

      console.log('API 요청 보내는 중...', request);

      const result = await alarmTemplatesApi.applyTemplate(template.id, request);

      console.log('API 응답 받음:', result);

      if (result && result.success) {
        const rulesCreated = result.data?.rules_created || 0;
        const ruleGroupId = result.data?.rule_group_id || 'Unknown';
        
        console.log('성공! 규칙 생성됨:', rulesCreated);
        
        // 성공 메시지 - ConfirmProvider 사용 또는 fallback
        if (typeof showConfirm === 'function') {
          showConfirm({
            title: '템플릿 적용 성공',
            message: `성공!\n\n${rulesCreated}개의 알람 규칙이 생성되었습니다.\n규칙 그룹: ${ruleGroupId}\n\n"생성된 규칙" 탭에서 확인할 수 있습니다.`,
            type: 'success',
            onConfirm: () => {
              // 성공 콜백 호출 (부모에서 데이터 새로고침 등)
              if (onSuccess) {
                console.log('성공 콜백 호출 중...');
                onSuccess();
              }
              
              // 모달 닫기
              console.log('모달 닫는 중...');
              onClose();
            }
          });
        } else {
          // fallback: 기본 alert 사용
          alert(`성공!\n\n${rulesCreated}개의 알람 규칙이 생성되었습니다.\n규칙 그룹: ${ruleGroupId}\n\n"생성된 규칙" 탭에서 확인할 수 있습니다.`);
          
          // 성공 콜백 호출 (부모에서 데이터 새로고침 등)
          if (onSuccess) {
            console.log('성공 콜백 호출 중...');
            onSuccess();
          }
          
          // 모달 닫기
          console.log('모달 닫는 중...');
          onClose();
        }
        
      } else {
        const errorMessage = result?.message || result?.error || '알 수 없는 오류가 발생했습니다.';
        console.log('API 응답에서 실패 감지:', errorMessage);
        throw new Error(errorMessage);
      }

    } catch (error) {
      console.error('템플릿 적용 실패:', error);
      
      let errorMessage = '템플릿 적용에 실패했습니다.';
      
      if (error instanceof Error) {
        errorMessage = error.message;
      }
      
      // 사용자 친화적 에러 메시지
      if (errorMessage.includes('404')) {
        errorMessage = '템플릿을 찾을 수 없습니다.\n페이지를 새로고침하고 다시 시도해주세요.';
      } else if (errorMessage.includes('500')) {
        errorMessage = '서버 오류가 발생했습니다.\n잠시 후 다시 시도해주세요.';
      } else if (errorMessage.includes('Network') || errorMessage.includes('fetch')) {
        errorMessage = '네트워크 연결 문제가 발생했습니다.\n인터넷 연결을 확인하고 다시 시도해주세요.';
      }
      
      // 실패 메시지 - ConfirmProvider 사용 또는 fallback
      if (typeof showConfirm === 'function') {
        showConfirm({
          title: '템플릿 적용 실패',
          message: errorMessage,
          type: 'danger',
          onConfirm: () => {}
        });
      } else {
        alert(`템플릿 적용 실패\n\n${errorMessage}`);
      }
      
    } finally {
      console.log('로딩 상태 종료');
      setLoading(false);
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

        {/* 내장 확인 대화상자 */}
        {showConfirmDialog && (
          <div style={{
            position: 'absolute',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: 'rgba(0, 0, 0, 0.8)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 10000
          }}>
            <div style={{
              backgroundColor: 'white',
              borderRadius: '12px',
              padding: '32px',
              maxWidth: '500px',
              boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.25)'
            }}>
              <h3 style={{
                fontSize: '20px',
                fontWeight: '600',
                margin: '0 0 16px 0',
                color: '#111827'
              }}>
                템플릿 적용 확인
              </h3>
              <p style={{
                fontSize: '16px',
                color: '#374151',
                lineHeight: '1.6',
                margin: '0 0 24px 0'
              }}>
                "{template.name}" 템플릿을<br/>
                {selectedDataPoints.length}개의 데이터포인트에 적용하시겠습니까?<br/><br/>
                이 작업으로 {selectedDataPoints.length}개의 새로운 알람 규칙이 생성됩니다.
              </p>
              <div style={{
                display: 'flex',
                gap: '12px',
                justifyContent: 'flex-end'
              }}>
                <button
                  onClick={handleCancelApply}
                  style={{
                    padding: '12px 24px',
                    border: '1px solid #d1d5db',
                    borderRadius: '8px',
                    background: '#ffffff',
                    color: '#374151',
                    cursor: 'pointer',
                    fontSize: '14px',
                    fontWeight: '500'
                  }}
                >
                  취소
                </button>
                <button
                  onClick={handleConfirmApply}
                  style={{
                    padding: '12px 24px',
                    border: 'none',
                    borderRadius: '8px',
                    background: '#3b82f6',
                    color: 'white',
                    cursor: 'pointer',
                    fontSize: '14px',
                    fontWeight: '500'
                  }}
                >
                  확인
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default TemplateApplyModal;