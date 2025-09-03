// ============================================================================
// frontend/src/components/modals/DeviceDetailModal.tsx
// 🔥 updateDeviceSettings 에러 완전 해결 - 즉시 저장 방식으로 변경
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { DeviceApiService } from '../../api/services/deviceApi';

// 분할된 컴포넌트들 import
import DeviceBasicInfoTab from './DeviceModal/DeviceBasicInfoTab';
import DeviceSettingsTab from './DeviceModal/DeviceSettingsTab';
import DeviceDataPointsTab from './DeviceModal/DeviceDataPointsTab';
import DeviceStatusTab from './DeviceModal/DeviceStatusTab';
import DeviceLogsTab from './DeviceModal/DeviceLogsTab';

// RTU 관련 컴포넌트들 import
import DeviceRtuNetworkTab from './DeviceModal/DeviceRtuNetworkTab';
import DeviceRtuMonitorTab from './DeviceModal/DeviceRtuMonitorTab';

// 타입 정의
import { Device, DeviceModalProps } from './DeviceModal/types';

// DataPoint 인터페이스
interface DataPoint {
  id: number;
  device_id: number;
  device_name?: string;
  name: string;
  description: string;
  data_type: string;
  current_value: any;
  unit?: string;
  address: string;
  is_enabled: boolean;
  created_at: string;
  updated_at: string;
}

const DeviceDetailModal: React.FC<DeviceModalProps> = ({
  device,
  isOpen,
  mode,
  onClose,
  onSave,
  onDelete
}) => {
  // 상태 관리
  const [activeTab, setActiveTab] = useState('basic');
  const [editData, setEditData] = useState<Device | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [isLoadingDataPoints, setIsLoadingDataPoints] = useState(false);
  const [dataPointsError, setDataPointsError] = useState<string | null>(null);

  // 🎨 예쁜 커스텀 모달 상태
  const [customModal, setCustomModal] = useState<{
    isOpen: boolean;
    type: 'confirm' | 'success' | 'error';
    title: string;
    message: string;
    confirmText: string;
    cancelText: string;
    onConfirm: () => void;
    onCancel: () => void;
    showCancel: boolean;
  }>({
    isOpen: false,
    type: 'confirm',
    title: '',
    message: '',
    confirmText: '확인',
    cancelText: '취소',
    onConfirm: () => {},
    onCancel: () => {},
    showCancel: true
  });

  // RTU 디바이스 판별 함수들
  const deviceHelpers = useMemo(() => ({
    isRtuDevice: (device: Device | null): boolean => {
      return device?.protocol_type === 'MODBUS_RTU';
    },
    isRtuMaster: (device: Device | null): boolean => {
      const isRtu = device?.protocol_type === 'MODBUS_RTU';
      return isRtu && 
             (device?.config?.device_role === 'master' || 
              device?.config?.is_master === true ||
              !device?.config?.slave_id);
    },
    isRtuSlave: (device: Device | null): boolean => {
      const isRtu = device?.protocol_type === 'MODBUS_RTU';
      const isMaster = isRtu && 
                       (device?.config?.device_role === 'master' || 
                        device?.config?.is_master === true ||
                        !device?.config?.slave_id);
      return isRtu && !isMaster;
    }
  }), []);

  // 새 디바이스 템플릿
  const newDeviceTemplate = useMemo(() => ({
    id: 0,
    name: '',
    description: '',
    device_type: 'PLC' as const,
    manufacturer: '',
    model: '',
    serial_number: '',
    protocol_type: 'MODBUS_TCP' as const,
    endpoint: '',
    polling_interval: 1000,
    timeout: 5000,
    retry_count: 3,
    is_enabled: true,
    settings: {}, // 🔥 settings 필드 추가
    created_at: '',
    updated_at: ''
  }), []);

  // 예쁜 모달 표시 함수들
  // DeviceDetailModal.tsx - showCustomModal 함수만 수정
// 기존 라인 148-163 부근의 showCustomModal 함수를 이렇게 교체

  const showCustomModal = (config: {
    type: 'confirm' | 'success' | 'error';
    title: string;
    message: string;
    confirmText?: string;
    cancelText?: string;
    onConfirm: () => void;
    onCancel?: () => void;
    showCancel?: boolean;
  }) => {
    console.log('📋 showCustomModal 호출:', config.type, config.title);
    
    setCustomModal({
      isOpen: true,
      type: config.type,
      title: config.title,
      message: config.message,
      confirmText: config.confirmText || '확인',
      cancelText: config.cancelText || '취소',
      onConfirm: () => {
        console.log('✅ 모달 확인 버튼 클릭됨');
        
        // 🔥 핵심 수정: 모달을 먼저 닫고, 그 다음 콜백 실행
        setCustomModal(prev => ({ ...prev, isOpen: false }));
        
        // 짧은 지연 후 콜백 실행 (모달 닫기 완료 후)
        setTimeout(() => {
          try {
            console.log('🔥 콜백 실행 시작...');
            config.onConfirm();
            console.log('✅ 콜백 실행 완료');
          } catch (error) {
            console.error('❌ 콜백 실행 오류:', error);
          }
        }, 100);
      },
      onCancel: () => {
        console.log('❌ 모달 취소 버튼 클릭됨');
        
        // 모달 먼저 닫기
        setCustomModal(prev => ({ ...prev, isOpen: false }));
        
        // 취소 콜백 실행 (있다면)
        if (config.onCancel) {
          setTimeout(() => {
            try {
              config.onCancel!();
            } catch (error) {
              console.error('❌ 취소 콜백 실행 오류:', error);
            }
          }, 100);
        }
      },
      showCancel: config.showCancel !== false
    });
  };

  // 데이터포인트 로드
  const loadDataPoints = useCallback(async (deviceId: number) => {
    if (!deviceId || deviceId <= 0) return;

    try {
      setIsLoadingDataPoints(true);
      setDataPointsError(null);
      console.log(`디바이스 ${deviceId} 데이터포인트 로드 시작...`);

      const response = await DeviceApiService.getDeviceDataPoints(deviceId, {
        page: 1,
        limit: 100,
        enabled_only: false
      });

      if (response.success && response.data) {
        const points = response.data.items || [];
        setDataPoints(points);
        console.log(`데이터포인트 ${points.length}개 로드 완료`);
      } else {
        throw new Error(response.error || '데이터포인트 조회 실패');
      }
    } catch (error) {
      console.error(`디바이스 ${deviceId} 데이터포인트 로드 실패:`, error);
      setDataPointsError(error instanceof Error ? error.message : 'Unknown error');
      setDataPoints([]);
    } finally {
      setIsLoadingDataPoints(false);
    }
  }, []);

  // 데이터포인트 관리 함수들
  const handleCreateDataPoint = useCallback((newDataPoint: DataPoint) => {
    setDataPoints(prev => [...prev, newDataPoint]);
  }, []);

  const handleUpdateDataPoint = useCallback((updatedDataPoint: DataPoint) => {
    setDataPoints(prev => 
      prev.map(dp => dp.id === updatedDataPoint.id ? updatedDataPoint : dp)
    );
  }, []);

  const handleDeleteDataPoint = useCallback((pointId: number) => {
    setDataPoints(prev => prev.filter(dp => dp.id !== pointId));
  }, []);

  const handleRefreshDataPoints = useCallback(async () => {
    const deviceId = device?.id || editData?.id;
    if (deviceId) {
      await loadDataPoints(deviceId);
    }
  }, [device?.id, editData?.id, loadDataPoints]);

  // 🎨 예쁜 저장 함수 (브라우저 기본 팝업 대신 커스텀 모달)
  const handleSave = useCallback(async () => {
    console.log('🔥 handleSave 함수 진입');
    
    if (!editData) {
      console.log('❌ editData가 없음');
      return;
    }

    const actionText = mode === 'create' ? '생성' : '수정';
    const confirmMessage = `"${editData.name}" 디바이스를 ${actionText}하시겠습니까?\n\n프로토콜: ${editData.protocol_type}\n엔드포인트: ${editData.endpoint}\n폴링 간격: ${editData.polling_interval}ms`;

    console.log('🎨 예쁜 커스텀 확인 모달 표시...');

    showCustomModal({
      type: 'confirm',
      title: `디바이스 ${actionText} 확인`,
      message: confirmMessage,
      confirmText: actionText,
      cancelText: '취소',
      onConfirm: async () => {
        console.log('✅ 사용자가 확인함 - 저장 진행');
        
        try {
          setIsLoading(true);
          let savedDevice: Device;

          if (mode === 'create') {
            console.log('🔥 디바이스 생성 시작...');
            const createData = {
              name: editData.name,
              description: editData.description,
              device_type: editData.device_type,
              manufacturer: editData.manufacturer,
              model: editData.model,
              protocol_type: editData.protocol_type,
              endpoint: editData.endpoint,
              config: editData.config,
              site_id: editData.site_id,
              device_group_id: editData.device_group_id,
              polling_interval: editData.polling_interval,
              timeout: editData.timeout,
              retry_count: editData.retry_count,
              is_enabled: editData.is_enabled
            };

            const response = await DeviceApiService.createDevice(createData);
            if (response.success && response.data) {
              savedDevice = response.data;
              console.log('🎉 디바이스 생성 성공:', savedDevice);
              
              // 🔥 핵심 수정: 즉시 성공 처리 후 모달 닫기
              showCustomModal({
                type: 'success',
                title: '디바이스 생성 완료',
                message: `"${savedDevice.name}" 디바이스가 성공적으로 생성되었습니다.\n\nID: ${savedDevice.id}\n상태: ${savedDevice.is_enabled ? '활성화' : '비활성화'}`,
                confirmText: '확인',
                showCancel: false,
                onConfirm: () => {
                  console.log('🔥 생성 성공 팝업 확인 - 콜백 실행');
                  
                  // 부모 컴포넌트에 저장된 디바이스 전달
                  if (onSave) {
                    console.log('📞 onSave 콜백 호출:', savedDevice.name);
                    onSave(savedDevice);
                  }
                  
                  // 모달 닫기
                  console.log('🚪 모달 닫기 실행');
                  onClose();
                }
              });
            } else {
              throw new Error(response.error || '생성 실패');
            }
            
          } else if (mode === 'edit') {
            console.log('🔥 디바이스 수정 시작...');
            
            // settings도 포함해서 업데이트
            const updateData = {
              name: editData.name,
              description: editData.description,
              device_type: editData.device_type,
              manufacturer: editData.manufacturer,
              model: editData.model,
              endpoint: editData.endpoint,
              config: editData.config,
              polling_interval: editData.polling_interval,
              timeout: editData.timeout,
              retry_count: editData.retry_count,
              is_enabled: editData.is_enabled,
              settings: editData.settings // 🔥 settings 포함
            };

            console.log('🚀 실제 전송할 데이터:', JSON.stringify(updateData, null, 2));
            console.log('🔍 settings 필드 확인:', updateData.settings);

            const response = await DeviceApiService.updateDevice(editData.id, updateData);
            if (response.success && response.data) {
              savedDevice = response.data;
              console.log('🎉 디바이스 수정 성공:', savedDevice);
              
              // 🔥 핵심 수정: 즉시 성공 처리 후 모달 닫기
              showCustomModal({
                type: 'success',
                title: '디바이스 수정 완료',
                message: `"${savedDevice.name}" 디바이스가 성공적으로 수정되었습니다.\n\n변경사항이 서버에 저장되었습니다.`,
                confirmText: '확인',
                showCancel: false,
                onConfirm: () => {
                  console.log('🔥 수정 성공 팝업 확인 - 콜백 실행');
                  
                  // 부모 컴포넌트에 수정된 디바이스 전달
                  if (onSave) {
                    console.log('📞 onSave 콜백 호출:', savedDevice.name);
                    onSave(savedDevice);
                  }
                  
                  // 모달 닫기
                  console.log('🚪 모달 닫기 실행');
                  onClose();
                }
              });
            } else {
              throw new Error(response.error || '수정 실패');
            }
          }
          
        } catch (error) {
          console.error('❌ 디바이스 저장 실패:', error);
          
          showCustomModal({
            type: 'error',
            title: '저장 실패',
            message: `디바이스 저장에 실패했습니다.\n\n${error instanceof Error ? error.message : 'Unknown error'}\n\n다시 시도해주세요.`,
            confirmText: '확인',
            showCancel: false,
            onConfirm: () => {
              console.log('❌ 에러 팝업 확인 - 모달은 열린 상태 유지');
              // 에러의 경우 모달은 닫지 않고 사용자가 다시 시도할 수 있도록 함
            }
          });
        } finally {
          setIsLoading(false);
        }
      }
    });
  }, [editData, mode, onSave, onClose]); // dependencies에 onSave, onClose 모두 포함

  // 🎨 예쁜 삭제 함수 (브라우저 기본 팝업 대신 커스텀 모달)
  const handleDelete = useCallback(async () => {
    console.log('🔥 handleDelete 함수 진입');
    
    if (!device) {
      console.log('❌ device가 없음');
      return;
    }

    const confirmMessage = `"${device.name}" 디바이스를 완전히 삭제하시겠습니까?\n\n⚠️ 주의사항:\n• 연결된 데이터포인트도 함께 삭제됩니다\n• 히스토리 데이터는 보존됩니다\n• 이 작업은 되돌릴 수 없습니다`;

    console.log('🎨 예쁜 커스텀 삭제 확인 모달 표시...');

    showCustomModal({
      type: 'confirm',
      title: '디바이스 삭제 확인',
      message: confirmMessage,
      confirmText: '삭제',
      cancelText: '취소',
      onConfirm: async () => {
        console.log('✅ 사용자가 삭제 확인함 - 삭제 진행');
        
        try {
          setIsLoading(true);
          console.log(`🗑️ 디바이스 삭제 시작: ${device.name} (ID: ${device.id})`);
          
          const response = await DeviceApiService.deleteDevice(device.id);
          if (response.success) {
            console.log(`✅ 디바이스 삭제 완료: ${device.name}`);
            
            showCustomModal({
              type: 'success',
              title: '디바이스 삭제 완료',
              message: `"${device.name}"이(가) 성공적으로 삭제되었습니다.\n\n디바이스 목록에서 제거됩니다.`,
              confirmText: '확인',
              showCancel: false,
              onConfirm: () => {
                onDelete?.(device.id);
                onClose();
              }
            });
          } else {
            throw new Error(response.error || '삭제 실패');
          }
        } catch (error) {
          console.error('❌ 디바이스 삭제 실패:', error);
          
          showCustomModal({
            type: 'error',
            title: '삭제 실패',
            message: `디바이스 삭제에 실패했습니다.\n\n${error instanceof Error ? error.message : 'Unknown error'}\n\n다시 시도해주세요.`,
            confirmText: '확인',
            showCancel: false,
            onConfirm: () => {}
          });
        } finally {
          setIsLoading(false);
        }
      }
    });
  }, [device, onDelete, onClose]);

  // 필드 업데이트 함수들
  const updateField = useCallback((field: string, value: any) => {
    setEditData(prev => prev ? { ...prev, [field]: value } : null);
  }, []);

  // 🔥 핵심 수정: updateSettings 함수를 간단하게 변경
  // DeviceApiService.updateDeviceSettings() 호출 제거
  const updateSettings = useCallback((field: string, value: any) => {
    console.log(`🔥 DeviceDetailModal updateSettings 호출: ${field} = ${value}`);
    
    setEditData(prev => {
      if (!prev) return null;
      
      const updatedDevice = {
        ...prev,
        settings: { 
          ...prev.settings, 
          [field]: value 
        }
      };
      
      console.log('🔄 editData 업데이트 완료:', updatedDevice.settings);
      return updatedDevice;
    });
    
    // 🔥 문제 해결: DeviceApiService.updateDeviceSettings() 호출 완전 제거
    // 대신 DeviceSettingsTab에서 변경된 값들은 전체 저장 시에 한번에 저장됨
    console.log('✅ 설정값이 로컬 상태에 저장됨 - 전체 저장 시 서버에 반영예정');
  }, []);

  useEffect(() => {
    if (process.env.NODE_ENV === 'development' && editData?.settings) {
      console.log('🔍 현재 editData.settings:', editData.settings);
    }
  }, [editData?.settings]);

  const updateRtuDevice = useCallback((updatedDevice: Device) => {
    if (mode === 'edit') {
      setEditData(updatedDevice);
    }
    onSave?.(updatedDevice);
  }, [mode, onSave]);

  const handleTabChange = useCallback((tabName: string) => {
    setActiveTab(tabName);
  }, []);

  // 라이프사이클 관리 (무한 렌더링 방지)
  useEffect(() => {
    console.log('DeviceDetailModal useEffect:', { 
      isOpen, 
      deviceId: device?.id, 
      mode 
    });

    if (!isOpen) {
      setDataPoints([]);
      setDataPointsError(null);
      setEditData(null);
      setActiveTab('basic');
      return;
    }

    if (mode === 'create') {
      console.log('새 디바이스 초기화');
      setEditData({ ...newDeviceTemplate });
      setDataPoints([]);
      setDataPointsError(null);
      setActiveTab('basic');
      return;
    }

    if (device && mode !== 'create') {
      setEditData({ ...device });
      setActiveTab('basic');
      
      if (device.id && device.id > 0) {
        loadDataPoints(device.id);
      }
    }
  }, [isOpen, device, mode, newDeviceTemplate, loadDataPoints]);

  if (!isOpen) return null;

  const displayData = device || editData;

  return (
    <>
      <div className="modal-overlay">
        <div className="modal-container">
          {/* 모달 헤더 */}
          <div className="modal-header">
            <div className="modal-title">
              <div className="title-row">
                <h2>
                  {mode === 'create' ? '새 디바이스 추가' : 
                   mode === 'edit' ? '디바이스 편집' : '디바이스 상세'}
                </h2>
                {displayData?.connection_status && (
                  <span className={`status-indicator ${displayData.connection_status}`}>
                    <i className="fas fa-circle"></i>
                    {displayData.connection_status === 'connected' ? '연결됨' :
                     displayData.connection_status === 'disconnected' ? '연결끊김' :
                     displayData.connection_status === 'connecting' ? '연결중' : '알수없음'}
                  </span>
                )}
              </div>
              {displayData && (
                <div className="device-subtitle">
                  {displayData.manufacturer} {displayData.model} • {displayData.protocol_type}
                  {deviceHelpers.isRtuDevice(displayData) && (
                    <span className="rtu-badge">
                      {deviceHelpers.isRtuMaster(displayData) ? 'RTU Master' : 'RTU Slave'}
                    </span>
                  )}
                </div>
              )}
            </div>
            <button className="close-btn" onClick={onClose}>
              <i className="fas fa-times"></i>
            </button>
          </div>

          {/* 탭 네비게이션 */}
          <div className="tab-navigation">
            <button 
              className={`tab-btn ${activeTab === 'basic' ? 'active' : ''}`}
              onClick={() => handleTabChange('basic')}
            >
              <i className="fas fa-info-circle"></i>
              기본정보
            </button>
            <button 
              className={`tab-btn ${activeTab === 'settings' ? 'active' : ''}`}
              onClick={() => handleTabChange('settings')}
            >
              <i className="fas fa-cog"></i>
              설정
            </button>
            <button 
              className={`tab-btn ${activeTab === 'datapoints' ? 'active' : ''}`}
              onClick={() => handleTabChange('datapoints')}
            >
              <i className="fas fa-list"></i>
              데이터포인트 ({dataPoints.length})
            </button>
            
            {deviceHelpers.isRtuDevice(displayData) && mode !== 'create' && (
              <>
                <button 
                  className={`tab-btn ${activeTab === 'rtu-network' ? 'active' : ''}`}
                  onClick={() => handleTabChange('rtu-network')}
                >
                  <i className="fas fa-sitemap"></i>
                  {deviceHelpers.isRtuMaster(displayData) ? 'RTU 네트워크' : 'RTU 연결'}
                </button>
                
                <button 
                  className={`tab-btn ${activeTab === 'rtu-monitor' ? 'active' : ''}`}
                  onClick={() => handleTabChange('rtu-monitor')}
                >
                  <i className="fas fa-chart-line"></i>
                  통신 모니터
                </button>
              </>
            )}

            {mode !== 'create' && (
              <button 
                className={`tab-btn ${activeTab === 'status' ? 'active' : ''}`}
                onClick={() => handleTabChange('status')}
              >
                <i className="fas fa-chart-line"></i>
                상태
              </button>
            )}
            {mode === 'view' && (
              <button 
                className={`tab-btn ${activeTab === 'logs' ? 'active' : ''}`}
                onClick={() => handleTabChange('logs')}
              >
                <i className="fas fa-file-alt"></i>
                로그
              </button>
            )}
          </div>

          {/* 탭 내용 */}
          <div className="modal-content">
            {activeTab === 'basic' && (
              <DeviceBasicInfoTab
                device={device}
                editData={editData}
                mode={mode}
                onUpdateField={updateField}
              />
            )}

            {activeTab === 'settings' && (
              <DeviceSettingsTab
                device={device}
                editData={editData}
                mode={mode}
                onUpdateField={updateField}
                onUpdateSettings={updateSettings}
              />
            )}

            {activeTab === 'datapoints' && (
              <DeviceDataPointsTab
                deviceId={device?.id || editData?.id || 0}
                dataPoints={dataPoints}
                isLoading={isLoadingDataPoints}
                error={dataPointsError}
                mode={mode}
                onRefresh={handleRefreshDataPoints}
                onCreate={handleCreateDataPoint}
                onUpdate={handleUpdateDataPoint}
                onDelete={handleDeleteDataPoint}
              />
            )}

            {activeTab === 'rtu-network' && deviceHelpers.isRtuDevice(displayData) && (
              <DeviceRtuNetworkTab 
                device={displayData}
                mode={mode}
                onUpdateDevice={updateRtuDevice}
              />
            )}

            {activeTab === 'rtu-monitor' && deviceHelpers.isRtuDevice(displayData) && (
              <DeviceRtuMonitorTab 
                device={displayData}
                mode={mode}
              />
            )}

            {activeTab === 'status' && mode !== 'create' && (
              <DeviceStatusTab device={device} />
            )}

            {activeTab === 'logs' && mode === 'view' && (
              <DeviceLogsTab deviceId={device?.id || 0} />
            )}
          </div>

          {/* 모달 푸터 */}
          <div className="modal-footer">
            <div className="footer-left">
              {mode === 'edit' && onDelete && (
                <button
                  type="button"
                  className="btn btn-error"
                  onClick={(e) => {
                    e.preventDefault();
                    e.stopPropagation();
                    console.log('🔥 삭제 버튼 클릭됨 - 커스텀 모달 표시');
                    handleDelete();
                  }}
                  disabled={isLoading}
                >
                  <i className="fas fa-trash"></i>
                  삭제
                </button>
              )}
            </div>
            <div className="footer-right">
              <button type="button" className="btn btn-secondary" onClick={onClose}>
                취소
              </button>
              {mode !== 'view' && (
                <button
                  type="button"
                  className="btn btn-primary"
                  onClick={(e) => {
                    e.preventDefault();
                    e.stopPropagation();
                    console.log('🔥 저장 버튼 클릭됨 - 커스텀 모달 표시');
                    handleSave();
                  }}
                  disabled={isLoading}
                >
                  {isLoading ? (
                    <>
                      <i className="fas fa-spinner fa-spin"></i>
                      저장 중...
                    </>
                  ) : (
                    <>
                      <i className="fas fa-save"></i>
                      {mode === 'create' ? '생성' : '저장'}
                    </>
                  )}
                </button>
              )}
            </div>
          </div>
        </div>
      </div>

      {/* 🎨 예쁜 커스텀 모달 (DeviceList 스타일과 동일) */}
      {customModal.isOpen && (
        <div className="custom-modal-overlay">
          <div className="custom-modal-content">
            <div className="custom-modal-header">
              <div className={`custom-modal-icon ${customModal.type}`}>
                <i className={`fas ${
                  customModal.type === 'success' ? 'fa-check-circle' :
                  customModal.type === 'error' ? 'fa-exclamation-triangle' :
                  'fa-info-circle'
                }`}></i>
              </div>
              <h3>{customModal.title}</h3>
            </div>
            <div className="custom-modal-body">{customModal.message}</div>
            <div className="custom-modal-footer">
              {customModal.showCancel && (
                <button 
                  onClick={customModal.onCancel} 
                  className="custom-modal-btn custom-modal-btn-cancel"
                >
                  {customModal.cancelText}
                </button>
              )}
              <button 
                onClick={customModal.onConfirm} 
                className={`custom-modal-btn custom-modal-btn-confirm custom-modal-btn-${customModal.type}`}
              >
                {customModal.confirmText}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 스타일 */}
      <style>{`
        .modal-overlay {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: rgba(0, 0, 0, 0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 1000;
        }

        .modal-container {
          background: white;
          border-radius: 0.75rem;
          width: 90vw;
          max-width: 1200px;
          height: 90vh;
          max-height: 800px;
          display: flex;
          flex-direction: column;
          box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
        }

        .modal-header {
          display: flex;
          align-items: center;
          justify-content: space-between;
          padding: 1.5rem 2rem;
          border-bottom: 1px solid #e5e7eb;
          flex-shrink: 0;
        }

        .modal-title {
          flex: 1;
        }

        .title-row {
          display: flex;
          align-items: center;
          gap: 1rem;
          margin-bottom: 0.5rem;
        }

        .title-row h2 {
          margin: 0;
          font-size: 1.5rem;
          font-weight: 600;
          color: #111827;
        }

        .device-subtitle {
          color: #6b7280;
          font-size: 0.875rem;
          display: flex;
          align-items: center;
          gap: 0.75rem;
        }

        .rtu-badge {
          display: inline-flex;
          align-items: center;
          padding: 0.125rem 0.5rem;
          background: #f0f9ff;
          color: #0369a1;
          border: 1px solid #0ea5e9;
          border-radius: 0.375rem;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .status-indicator {
          display: flex;
          align-items: center;
          gap: 0.375rem;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .status-indicator.connected {
          background: #dcfce7;
          color: #166534;
        }

        .status-indicator.disconnected {
          background: #fef2f2;
          color: #dc2626;
        }

        .status-indicator.connecting {
          background: #fef3c7;
          color: #d97706;
        }

        .close-btn {
          display: flex;
          align-items: center;
          justify-content: center;
          width: 2.5rem;
          height: 2.5rem;
          border: none;
          border-radius: 0.5rem;
          background: #f3f4f6;
          color: #6b7280;
          cursor: pointer;
          transition: all 0.2s ease;
        }

        .close-btn:hover {
          background: #e5e7eb;
          color: #374151;
        }

        .tab-navigation {
          display: flex;
          border-bottom: 1px solid #e5e7eb;
          background: #f9fafb;
          flex-shrink: 0;
          overflow-x: auto;
        }

        .tab-btn {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          padding: 1rem 1.5rem;
          border: none;
          background: none;
          color: #6b7280;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
          border-bottom: 2px solid transparent;
          white-space: nowrap;
          flex-shrink: 0;
        }

        .tab-btn:hover {
          color: #374151;
          background: #f3f4f6;
        }

        .tab-btn.active {
          color: #0ea5e9;
          border-bottom-color: #0ea5e9;
          background: white;
        }

        .modal-content {
          flex: 1;
          overflow: hidden;
          display: flex;
          flex-direction: column;
        }

        .modal-footer {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1.5rem 2rem;
          border-top: 1px solid #e5e7eb;
          background: #f9fafb;
          flex-shrink: 0;
        }

        .footer-left,
        .footer-right {
          display: flex;
          gap: 0.75rem;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 0.5rem;
          font-size: 0.875rem;
          font-weight: 500;
          text-decoration: none;
          cursor: pointer;
          transition: all 0.2s ease;
        }

        .btn-primary {
          background: #0ea5e9;
          color: white;
        }

        .btn-primary:hover:not(:disabled) {
          background: #0284c7;
        }

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-secondary:hover:not(:disabled) {
          background: #475569;
        }

        .btn-error {
          background: #dc2626;
          color: white;
        }

        .btn-error:hover:not(:disabled) {
          background: #b91c1c;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        /* 🎨 예쁜 커스텀 모달 스타일 */
        .custom-modal-overlay {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background-color: rgba(0, 0, 0, 0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 2000;
          backdrop-filter: blur(4px);
        }

        .custom-modal-content {
          background: #ffffff;
          border-radius: 12px;
          padding: 32px;
          max-width: 500px;
          width: 90%;
          box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
          border: 1px solid #e5e7eb;
          animation: modalFadeIn 0.2s ease-out;
        }

        @keyframes modalFadeIn {
          from {
            opacity: 0;
            transform: scale(0.95);
          }
          to {
            opacity: 1;
            transform: scale(1);
          }
        }

        .custom-modal-header {
          display: flex;
          align-items: center;
          gap: 16px;
          margin-bottom: 24px;
        }

        .custom-modal-icon {
          width: 48px;
          height: 48px;
          border-radius: 50%;
          display: flex;
          align-items: center;
          justify-content: center;
          font-size: 24px;
        }

        .custom-modal-icon.confirm {
          background: #eff6ff;
          color: #3b82f6;
        }

        .custom-modal-icon.success {
          background: #dcfce7;
          color: #16a34a;
        }

        .custom-modal-icon.error {
          background: #fee2e2;
          color: #dc2626;
        }

        .custom-modal-header h3 {
          font-size: 20px;
          font-weight: 700;
          color: #111827;
          margin: 0;
        }

        .custom-modal-body {
          font-size: 14px;
          color: #4b5563;
          line-height: 1.6;
          margin-bottom: 32px;
          white-space: pre-line;
        }

        .custom-modal-footer {
          display: flex;
          gap: 12px;
          justify-content: flex-end;
        }

        .custom-modal-btn {
          padding: 12px 24px;
          border-radius: 8px;
          font-size: 14px;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
          border: none;
        }

        .custom-modal-btn-cancel {
          border: 1px solid #d1d5db !important;
          background: #ffffff;
          color: #374151;
        }

        .custom-modal-btn-cancel:hover {
          background: #f9fafb;
        }

        .custom-modal-btn-confirm {
          color: white;
        }

        .custom-modal-btn-confirm {
          background: #3b82f6;
        }

        .custom-modal-btn-confirm:hover {
          background: #2563eb;
        }

        .custom-modal-btn-success {
          background: #16a34a;
        }

        .custom-modal-btn-success:hover {
          background: #15803d;
        }

        .custom-modal-btn-error {
          background: #dc2626;
        }

        .custom-modal-btn-error:hover {
          background: #b91c1c;
        }
      `}</style>
    </>
  );
};

export default DeviceDetailModal;