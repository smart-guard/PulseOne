// ============================================================================
// frontend/src/components/modals/DeviceDetailModal.tsx
// 🔥 updateDeviceSettings 에러 완전 해결 - 즉시 저장 방식으로 변경
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { DeviceApiService } from '../../api/services/deviceApi';
import { DataPoint } from '../../api/services/dataApi';

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


const DeviceDetailModal: React.FC<DeviceModalProps> = ({
  device,
  isOpen,
  mode,
  onClose,
  onSave,
  onDeviceCreated,
  onDelete,
  onEdit,
  initialTab,
  onTabChange
}) => {
  const { t } = useTranslation(['devices', 'common']);
  // 상태 관리
  const [activeTab, setActiveTab] = useState(initialTab || 'basic');
  const [wizardStep, setWizardStep] = useState(1);

  // URL 변경 등으로 initialTab이 바뀌면 탭도 변경 (단, 이미 열려있는 상태에서 탭만 바뀌는 경우)
  useEffect(() => {
    if (initialTab && isOpen) {
      setActiveTab(initialTab);
    }
  }, [initialTab, isOpen]);

  // 🔥 수정 모드 진입 시 로그 탭 등 유효하지 않은 탭 처리
  useEffect(() => {
    if (mode === 'edit' && activeTab === 'logs') {
      console.log('🔄 로그 탭에서 수정 모드로 전환됨 - 기본정보 탭으로 강제 이동');
      setActiveTab('basic');
    }
  }, [mode, activeTab]);
  const [editData, setEditData] = useState<Device | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [originalDataPoints, setOriginalDataPoints] = useState<DataPoint[]>([]);
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
    confirmText: 'OK',
    cancelText: 'Cancel',
    onConfirm: () => { },
    onCancel: () => { },
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
    site_id: 1, // Default Site ID
    protocol_id: 1, // Default to 1 (Modbus TCP usually)
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
      confirmText: config.confirmText || 'OK',
      cancelText: config.cancelText || 'Cancel',
      onConfirm: () => {
        console.log('✅ 모달 확인 버튼 클릭됨');

        // 🔥 핵심 수정: 모달을 먼저 닫고, 그 다음 콜백 실행
        setCustomModal(prev => ({ ...prev, isOpen: false }));

        // 짧은 지연 후 콜백 실행 (모달 닫기 완료 후)
        setTimeout(async () => {
          try {
            console.log('🔥 콜백 실행 시작...');
            const result = config.onConfirm() as any;
            // Promise 인지 확인하고 대기
            if (result && typeof result.then === 'function') {
              await result;
            }
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
      showCancel: config.showCancel !== undefined ? config.showCancel : (config.type === 'confirm')
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
        setOriginalDataPoints(JSON.parse(JSON.stringify(points))); // Deep copy
        console.log(`데이터포인트 ${points.length}개 로드 완료 (원본 백업 완료)`);
      } else {
        throw new Error(response.error || 'Failed to load data points');
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

    // ========================================================================
    // 🔍 스마트 변경 감지 (Diffing)
    // ========================================================================
    const changes: string[] = [];
    const isEdit = mode === 'edit';

    if (isEdit && device) {
      // 1. 기본 정보 및 통신 설정 비교
      const checkFields = [
        { key: 'name', label: t('devices:modal.diffName') },
        { key: 'tenant_id', label: t('devices:modal.diffTenant') },
        { key: 'site_id', label: t('devices:modal.diffSite') },
        { key: 'device_group_id', label: t('devices:modal.diffGroup') },
        { key: 'edge_server_id', label: t('devices:modal.diffCollector') },
        { key: 'description', label: t('devices:modal.diffDescription') },
        { key: 'manufacturer', label: t('devices:modal.diffManufacturer') },
        { key: 'model', label: t('devices:modal.diffModel') },
        { key: 'serial_number', label: t('devices:modal.diffSerial') },
        { key: 'device_type', label: t('devices:modal.diffDeviceType') },
        { key: 'protocol_type', label: t('devices:modal.diffProtocolType') },
        { key: 'protocol_id', label: t('devices:modal.diffProtocolId') },
        { key: 'protocol_instance_id', label: t('devices:modal.diffProtocolInstance') },
        { key: 'endpoint', label: t('devices:modal.diffEndpoint') },
        { key: 'polling_interval', label: t('devices:modal.diffPolling'), unit: 'ms' },
        { key: 'timeout', label: t('devices:modal.diffTimeout'), unit: 'ms' },
        { key: 'retry_count', label: t('devices:modal.diffRetry'), unit: '' },
        { key: 'is_enabled', label: t('devices:modal.diffEnabled'), isBool: true }
      ];

      checkFields.forEach(f => {
        const oldVal = (device as any)[f.key];
        const newVal = (editData as any)[f.key];

        if (String(oldVal ?? '') !== String(newVal ?? '')) {
          let displayOld = oldVal ?? '-';
          let displayNew = newVal ?? '-';

          if (f.isBool) {
            displayOld = oldVal ? t('devices:modal.diffActive') : t('devices:modal.diffInactive');
            displayNew = newVal ? t('devices:modal.diffActive') : t('devices:modal.diffInactive');
          } else if (f.unit) {
            displayOld = `${displayOld}${f.unit}`;
            displayNew = `${displayNew}${f.unit}`;
          }

          changes.push(`- ${f.label}: ${displayOld} → ${displayNew}`);
        }
      });

      // 1.5. 상세 설정(Settings) 비교
      if (editData.settings) {
        const oldSettings = device.settings || {};
        const newSettings = editData.settings as any;

        const settingsFields = [
          { key: 'is_auto_registration_enabled', label: t('devices:modal.diffAutoReg'), isBool: true },
          { key: 'polling_interval_ms', label: t('devices:modal.diffPollingMs'), unit: 'ms' },
          { key: 'max_retry_count', label: t('devices:modal.diffMaxRetry'), unit: '' }
        ];

        settingsFields.forEach(f => {
          const oldV = oldSettings[f.key];
          const newV = newSettings[f.key];

          if (String(oldV ?? '') !== String(newV ?? '')) {
            let oStr = oldV ?? '-';
            let nStr = newV ?? '-';

            if (f.isBool) {
              oStr = oldV ? 'ON' : 'OFF';
              nStr = newV ? 'ON' : 'OFF';
            } else if (f.unit) {
              oStr = `${oStr}${f.unit}`;
              nStr = `${nStr}${f.unit}`;
            }

            changes.push(`- ${t('devices:modal.diffSettingsPrefix', { label: f.label })}: ${oStr} → ${nStr}`);
          }
        });
      }

      // 2. 데이터포인트 변경 감지
      const added = dataPoints.filter(dp => !originalDataPoints.some(o => o.id === dp.id));
      const removed = originalDataPoints.filter(o => !dataPoints.some(dp => dp.id === o.id));
      const modified = dataPoints.filter(dp => {
        const original = originalDataPoints.find(o => o.id === dp.id);
        if (!original) return false;
        return dp.name !== original.name ||
          dp.address !== original.address ||
          dp.data_type !== original.data_type ||
          dp.unit !== original.unit ||
          dp.access_mode !== original.access_mode ||
          dp.is_enabled !== original.is_enabled ||
          dp.scaling_factor !== original.scaling_factor ||
          dp.scaling_offset !== original.scaling_offset;
      });

      if (added.length > 0) changes.push(t('devices:modal.diffPointAdded', { count: added.length, names: added.map(a => a.name).join(', ') }));
      if (removed.length > 0) changes.push(t('devices:modal.diffPointRemoved', { count: removed.length, names: removed.map(r => r.name).join(', ') }));

      modified.forEach(dp => {
        const original = originalDataPoints.find(o => o.id === dp.id)!;
        const dpChanges: string[] = [];

        const dpFields = [
          { key: 'name', label: t('devices:modal.dpDiffName') },
          { key: 'address', label: t('devices:modal.dpDiffAddress') },
          { key: 'data_type', label: t('devices:modal.dpDiffType') },
          { key: 'unit', label: t('devices:modal.dpDiffUnit') },
          { key: 'access_mode', label: t('devices:modal.dpDiffAccess') },
          { key: 'is_enabled', label: t('devices:modal.dpDiffStatus'), isBool: true },
          { key: 'scaling_factor', label: t('devices:modal.dpDiffScale') },
          { key: 'scaling_offset', label: t('devices:modal.dpDiffOffset') }
        ];

        dpFields.forEach(f => {
          const oldV = (original as any)[f.key];
          const newV = (dp as any)[f.key];
          if (String(oldV ?? '') !== String(newV ?? '')) {
            let oStr = oldV ?? '-';
            let nStr = newV ?? '-';
            if (f.isBool) {
              oStr = oldV ? 'ON' : 'OFF';
              nStr = newV ? 'ON' : 'OFF';
            }
            dpChanges.push(`${f.label}: ${oStr}→${nStr}`);
          }
        });

        if (dpChanges.length > 0) {
          changes.push(t('devices:modal.diffPointModified', { name: dp.name, changes: dpChanges.join(', ') }));
        }
      });

      // 변경사항이 전혀 없는 경우
      if (changes.length === 0) {
        showCustomModal({
          type: 'success',
          title: t('devices:modal.noChanges'),
          message: t('devices:modal.noChangesMsg'),
          confirmText: t('devices:modal.ok'),
          showCancel: false,
          onConfirm: () => {
            onClose();
          }
        });
        return;
      }
    }

    const actionText = mode === 'create' ? t('devices:modal.create') : t('devices:modal.edit');
    const confirmMessage = isEdit
      ? t('devices:modal.confirmEditMsg', { changes: changes.join('\n') })
      : t('devices:modal.confirmCreateMsg', { name: editData.name, protocol: editData.protocol_type, endpoint: editData.endpoint, count: dataPoints.length });

    console.log('🎨 예쁜 커스텀 확인 모달 표시...');

    showCustomModal({
      type: 'confirm',
      title: mode === 'create' ? t('devices:modal.confirmCreate') : t('devices:modal.confirmEdit'),
      message: confirmMessage,
      confirmText: actionText,
      cancelText: t('devices:modal.cancel'),
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
              protocol_id: editData.protocol_id,
              protocol_type: editData.protocol_type,
              endpoint: editData.endpoint,
              config: editData.config,
              site_id: editData.site_id,
              retry_count: editData.retry_count,
              is_enabled: editData.is_enabled,
              settings: editData.settings || {},
              group_ids: editData.group_ids || (editData.device_group_id ? [editData.device_group_id] : []),
              data_points: dataPoints // 🔥 NEW: 일괄 생성용 데이터포인트
            };

            const response = await DeviceApiService.createDevice(createData);
            if (response.success && response.data) {
              savedDevice = response.data;
              console.log('🎉 디바이스 생성 성공:', savedDevice);

              showCustomModal({
                type: 'success',
                title: t('devices:modal.createSuccess'),
                message: t('devices:modal.createSuccessMsg', { name: savedDevice.name, id: savedDevice.id }),
                confirmText: t('devices:tabs.dataPoints'),
                cancelText: t('devices:modal.close'),
                showCancel: true,
                onConfirm: () => {
                  console.log('구 생성 성공 - 수정 모드로 전환 후 데이터포인트 탭 오픈');
                  if (onSave) onSave(savedDevice);
                  // onDeviceCreated가 있으면 부모가 edit 모드+datapoints 탭으로 이동
                  if (onDeviceCreated) {
                    onDeviceCreated(savedDevice);
                  } else {
                    onClose();
                  }
                },
                onCancel: () => {
                  if (onSave) onSave(savedDevice);
                  onClose();
                }
              });
            } else {
              throw new Error(response.error || 'Create failed');
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
              serial_number: editData.serial_number, // 🔥 추가
              endpoint: editData.endpoint,
              config: editData.config,
              site_id: editData.site_id,
              tenant_id: editData.tenant_id,
              protocol_id: editData.protocol_id,      // 🔥 추가
              protocol_type: editData.protocol_type,  // 🔥 추가
              protocol_instance_id: editData.protocol_instance_id, // 🔥 추가
              instance_name: editData.instance_name,  // 🔥 추가
              device_group_id: editData.device_group_id,
              edge_server_id: editData.edge_server_id,
              polling_interval: editData.polling_interval,
              timeout: editData.timeout,
              retry_count: editData.retry_count,
              is_enabled: editData.is_enabled,
              settings: editData.settings || {},
              tags: editData.tags,
              metadata: editData.metadata,
              custom_fields: editData.custom_fields,
              group_ids: editData.group_ids,
              data_points: dataPoints
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
                title: savedDevice.sync_warning ? t('devices:msg.saveCompleteWithWarning', { defaultValue: '저장 완료 (동기화 경고)' }) : t('devices:msg.saveSuccess', 'Edit Complete'),
                message: savedDevice.sync_warning
                  ? `✅ ${t('devices:msg.syncWarningMsg', { defaultValue: 'DB에 저장되었습니다.' })}\n\n⚠️ ${t('devices:msg.syncFailed', { defaultValue: '콜렉터 동기화 실패.' })}\n(${savedDevice.sync_warning})\n\n${t('devices:msg.checkCollector', { defaultValue: '콜렉터가 실행 중인지 확인하세요.' })}`
                  : t('devices:msg.saveSuccessDetail', { name: savedDevice.name, defaultValue: `"${savedDevice.name}" 디바이스가 성공적하로 수정되었습니다.` }),
                confirmText: t('devices:modal.ok'),
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
              throw new Error(response.error || 'Update failed');
            }
          }

        } catch (error) {
          console.error('❌ 디바이스 저장 실패:', error);

          showCustomModal({
            type: 'error',
            title: t('devices:msg.saveFailed'),
            message: `${t('devices:msg.saveFailedMsg', { defaultValue: '디바이스 저장에 실패했습니다.' })}\n\n${error instanceof Error ? error.message : t('devices:msg.unknown', { defaultValue: '알 수 없는 오류' })}\n\n${t('devices:msg.tryAgain', { defaultValue: '다시 시도해주세요.' })}`,
            confirmText: t('devices:modal.ok'),
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
  }, [editData, mode, onSave, onClose, dataPoints]); // dependencies에 dataPoints 추가

  // 🎨 예쁜 삭제 함수 (브라우저 기본 팝업 대신 커스텀 모달)
  const handleDelete = useCallback(async () => {
    console.log('🔥 handleDelete 함수 진입');

    if (!device) {
      console.log('❌ device가 없음');
      return;
    }

    const confirmMessage = t('devices:msg.deleteConfirmMsg', {
      name: device.name,
      defaultValue: `"${device.name}" 디바이스를 삭제하시걌습니까?\n\n⚠️ 주의 사항:\n• 데이터 포인트도 함께 삭제됩니다\n• 이력 데이터는 보존됩니다\n• 이 작업은 취소할 수 없습니다`
    });

    showCustomModal({
      type: 'confirm',
      title: t('devices:msg.deleteConfirm'),
      message: confirmMessage,
      confirmText: t('devices:modal.delete'),
      cancelText: t('devices:modal.cancel'),
      onConfirm: async () => {
        console.log('✅ 사용자가 삭제 확인함 - 삭제 진행');

        try {
          setIsLoading(true);
          console.log(`🗑️ 디바이스 삭제 시작: ${device.name} (ID: ${device.id})`);

          const response = await DeviceApiService.deleteDevice(device.id);
          if (response.success) {
            console.log(`✅ 디바이스 삭제 완료: ${device.name}`);
            const syncWarning = response.data?.sync_warning;

            showCustomModal({
              type: 'success',
              title: t('devices:modal.deleteSuccess'),
              message: t('devices:modal.deleteSuccessMsg', { name: device.name }) + (syncWarning ? `\n\n⚠️ Collector sync failed: ${syncWarning}` : ''),
              confirmText: t('devices:modal.ok'),
              showCancel: false,
              onConfirm: () => {
                onDelete?.(device.id);
                onClose();
              }
            });
          } else {
            throw new Error(response.error || 'Delete failed');
          }
        } catch (error) {
          console.error('❌ 디바이스 삭제 실패:', error);

          showCustomModal({
            type: 'error',
            title: t('devices:msg.deleteFailed'),
            message: t('devices:modal.deleteFailedMsg', { error: error instanceof Error ? error.message : 'Unknown error' }),
            confirmText: t('devices:modal.ok'),
            showCancel: false,
            onConfirm: () => { }
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
    onTabChange?.(tabName);
  }, [onTabChange]);

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
      setActiveTab(initialTab || 'basic');
      setWizardStep(1); // 위저드 단계 초기화
      return;
    }

    if (device) {
      // 🔥 핵심 수정: 이미 편집 중인 동일한 디바이스라면 덮어쓰지 않음
      if (editData && editData.id === device.id && mode === 'edit') {
        return;
      }

      const initialGroupIds = device.groups ? device.groups.map(g => g.id) :
        (device.device_group_id ? [device.device_group_id] : []);

      // JSON 필드 파싱 (문자열로 온 경우)
      const parseJson = (val: any) => {
        if (typeof val === 'string') {
          try {
            return JSON.parse(val);
          } catch (e) {
            return val;
          }
        }
        return val;
      };

      setEditData({
        ...device,
        group_ids: initialGroupIds,
        settings: device.settings || {}, // 🔥 초기값 보장
        tags: parseJson(device.tags) || [],
        metadata: parseJson(device.metadata) || {},
        custom_fields: parseJson(device.custom_fields) || {}
      });
      // 초기 탭 설정 (URL 파라미터 우선)
      setActiveTab(initialTab || 'basic');

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
                  {mode === 'create' ? t('devices:modal.addNew') :
                    mode === 'edit' ? t('devices:modal.editTitle', { id: displayData?.id }) : t('devices:modal.detailTitle', { id: displayData?.id })}
                </h2>
                {displayData?.connection_status && (
                  <span className={`status-indicator ${displayData.connection_status}`}>
                    <i className="fas fa-circle"></i>
                    {displayData.connection_status === 'connected' ? t('devices:modal.connectionStatus.connected') :
                      displayData.connection_status === 'disconnected' ? t('devices:modal.connectionStatus.disconnected') :
                        displayData.connection_status === 'connecting' ? t('devices:modal.connectionStatus.connecting') : t('devices:modal.connectionStatus.unknown')}
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

          {/* 위저드 진행 표시기 (생성 모드일 때만) */}
          {mode === 'create' && (
            <div className="wizard-steps-header">
              <div className={`wizard-step-item ${wizardStep >= 1 ? 'active' : ''} ${wizardStep > 1 ? 'completed' : ''}`}>
                <div className="step-number">{wizardStep > 1 ? <i className="fas fa-check"></i> : '1'}</div>
                <div className="step-text">{t('devices:modal.wizardStep1')}</div>
              </div>
              <div className={`wizard-step-line ${wizardStep > 1 ? 'completed' : ''}`}></div>
              <div className={`wizard-step-item ${wizardStep >= 2 ? 'active' : ''} ${wizardStep > 2 ? 'completed' : ''}`}>
                <div className="step-number">{wizardStep > 2 ? <i className="fas fa-check"></i> : '2'}</div>
                <div className="step-text">{t('devices:modal.wizardStep2')}</div>
              </div>
              <div className={`wizard-step-line ${wizardStep > 2 ? 'completed' : ''}`}></div>
              <div className={`wizard-step-item ${wizardStep >= 3 ? 'active' : ''}`}>
                <div className="step-number">3</div>
                <div className="step-text">{t('devices:modal.wizardStep3')}</div>
              </div>
            </div>
          )}

          {/* 탭 네비게이션 (상세/수정 모드일 때만) */}
          {mode !== 'create' && (
            <div className="tab-navigation">
              <button
                className={`tab-btn ${activeTab === 'basic' ? 'active' : ''}`}
                onClick={() => handleTabChange('basic')}
              >
                <i className="fas fa-info-circle"></i>
                {t('devices:modal.tabBasic')}
              </button>
              <button
                className={`tab-btn ${activeTab === 'settings' ? 'active' : ''}`}
                onClick={() => handleTabChange('settings')}
              >
                <i className="fas fa-cog"></i>
                {t('devices:modal.tabSettings')}
              </button>
              <button
                className={`tab-btn ${activeTab === 'datapoints' ? 'active' : ''}`}
                onClick={() => handleTabChange('datapoints')}
              >
                <i className="fas fa-list"></i>
                {t('devices:modal.tabDataPoints', { count: dataPoints.length })}
              </button>

              {deviceHelpers.isRtuDevice(displayData) && (
                <>
                  <button
                    className={`tab-btn ${activeTab === 'rtu-network' ? 'active' : ''}`}
                    onClick={() => handleTabChange('rtu-network')}
                  >
                    <i className="fas fa-sitemap"></i>
                    {deviceHelpers.isRtuMaster(displayData) ? t('devices:modal.tabRtuNetwork') : t('devices:modal.tabRtuConnection')}
                  </button>

                  <button
                    className={`tab-btn ${activeTab === 'rtu-monitor' ? 'active' : ''}`}
                    onClick={() => handleTabChange('rtu-monitor')}
                  >
                    <i className="fas fa-chart-line"></i>
                    {t('devices:modal.tabRtuMonitor')}
                  </button>
                </>
              )}

              <button
                className={`tab-btn ${activeTab === 'status' ? 'active' : ''}`}
                onClick={() => handleTabChange('status')}
              >
                <i className="fas fa-chart-line"></i>
                {t('devices:modal.tabStatus')}
              </button>

              {mode === 'view' && (
                <button
                  className={`tab-btn ${activeTab === 'logs' ? 'active' : ''}`}
                  onClick={() => handleTabChange('logs')}
                >
                  <i className="fas fa-file-alt"></i>
                  {t('devices:modal.tabLogs')}
                </button>
              )}
            </div>
          )}

          {/* 탭 내용 */}
          <div className={`modal-content ${mode === 'create' ? 'wizard-mode' : ''}`}>
            {mode === 'create' ? (
              <div className="wizard-registration-layout">
                <div className="wizard-form-area">
                  {wizardStep === 1 && (
                    <div className="wizard-section">
                      <h3 className="wizard-section-title"><i className="fas fa-info-circle"></i> {t('devices:modal.wizardSection1')}</h3>
                      <DeviceBasicInfoTab
                        device={device}
                        editData={editData}
                        mode={mode}
                        onUpdateField={updateField}
                        onUpdateSettings={updateSettings}
                        showModal={showCustomModal}
                      />
                    </div>
                  )}

                  {wizardStep === 2 && (
                    <div className="wizard-section">
                      <h3 className="wizard-section-title"><i className="fas fa-cog"></i> {t('devices:modal.wizardSection2')}</h3>
                      <DeviceSettingsTab
                        device={device}
                        editData={editData}
                        mode={mode}
                        onUpdateField={updateField}
                        onUpdateSettings={updateSettings}
                      />
                    </div>
                  )}

                  {wizardStep === 3 && (
                    <div className="wizard-section">
                      <h3 className="wizard-section-title"><i className="fas fa-list"></i> {t('devices:modal.wizardSection3')}</h3>
                      <div className="wizard-step3-grid">
                        <div className="wizard-points-mini-tab">
                          <DeviceDataPointsTab
                            deviceId={0}
                            protocolType={editData?.protocol_type}
                            dataPoints={dataPoints as any}
                            isLoading={isLoadingDataPoints}
                            error={dataPointsError}
                            mode={mode}
                            onRefresh={handleRefreshDataPoints}
                            onCreate={handleCreateDataPoint}
                            onUpdate={handleUpdateDataPoint}
                            onDelete={handleDeleteDataPoint}
                            showModal={showCustomModal}
                          />
                        </div>
                      </div>
                    </div>
                  )}
                </div>

                <div className="wizard-summary-area">
                  <div className="wizard-summary-box">
                    <h4 className="summary-title"><i className="fas fa-clipboard-list"></i> {t('devices:modal.summaryTitle')}</h4>
                    <div className="summary-content">
                      <div className="summary-item">
                        <label>{t('devices:modal.summaryName')}</label>
                        <span>{editData?.name || t('devices:modal.summaryEmpty')}</span>
                      </div>
                      <div className="summary-item">
                        <label>{t('devices:modal.summaryManufacturer')}</label>
                        <span>{editData?.manufacturer || '-'} / {editData?.model || '-'}</span>
                      </div>
                      <div className="summary-item">
                        <label>{t('devices:modal.summarySiteId')}</label>
                        <span>{editData?.site_id || '1'} {t('devices:modal.summaryDefaultSite')}</span>
                      </div>
                      <div className="summary-item">
                        <label>{t('devices:modal.summaryProtocol')}</label>
                        <span className="protocol-badge">{editData?.protocol_type || '-'}</span>
                      </div>
                      <div className="summary-item">
                        <label>{t('devices:modal.summaryEndpoint')}</label>
                        <span className="endpoint-text">{editData?.endpoint || t('devices:modal.summaryEmpty')}</span>
                      </div>
                      {editData?.protocol_type === 'MQTT' && (
                        <div className="summary-item">
                          <label>Base Topic</label>
                          <span style={{ color: 'var(--primary-600)' }}>
                            {(editData?.config as any)?.topic || t('devices:modal.summaryEmpty')}
                          </span>
                        </div>
                      )}
                      <div className="summary-item">
                        <label>{t('devices:modal.summaryPoints')}</label>
                        <span>{dataPoints.length}</span>
                      </div>
                    </div>

                    <div className="summary-footer">
                      {wizardStep < 3 ? (
                        <p className="summary-hint default">
                          <i className="fas fa-info-circle"></i>
                          {wizardStep === 1 ? t('devices:modal.wizardHint1') : t('devices:modal.wizardHint2')}
                        </p>
                      ) : (
                        <p className="summary-hint success">
                          <i className="fas fa-check-circle"></i>
                          {t('devices:modal.wizardReady')}
                        </p>
                      )}
                    </div>
                  </div>
                </div>
              </div>
            ) : (
              <>
                {activeTab === 'basic' && (
                  <DeviceBasicInfoTab
                    device={device}
                    editData={editData}
                    mode={mode}
                    onUpdateField={updateField}
                    onUpdateSettings={updateSettings}
                    showModal={showCustomModal}
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
                    protocolType={displayData?.protocol_type}
                    dataPoints={dataPoints as any}
                    isLoading={isLoadingDataPoints}
                    error={dataPointsError}
                    mode={mode}
                    onRefresh={handleRefreshDataPoints}
                    onCreate={handleCreateDataPoint}
                    onUpdate={handleUpdateDataPoint}
                    onDelete={handleDeleteDataPoint}
                    showModal={showCustomModal}
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

                {activeTab === 'status' && (
                  <DeviceStatusTab device={device} dataPoints={dataPoints as any} />
                )}

                {activeTab === 'logs' && mode === 'view' && (
                  <DeviceLogsTab deviceId={device?.id || 0} />
                )}
              </>
            )}
          </div>

          {/* 모달 푸터 */}
          <div className="modal-footer">
            <div className="footer-left">
              {mode === 'create' ? (
                <button type="button" className="btn btn-secondary" onClick={onClose}>
                  {t('devices:modal.cancel')}
                </button>
              ) : (
                mode === 'edit' && onDelete && (
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
                    {t('devices:modal.delete')}
                  </button>
                )
              )}
            </div>
            <div className="footer-right">
              {mode !== 'create' && (
                <button type="button" className="btn btn-secondary" onClick={onClose}>
                  {t('devices:modal.close')}
                </button>
              )}

              {mode === 'create' && (
                <>
                  {wizardStep > 1 && (
                    <button type="button" className="btn btn-outline" onClick={() => setWizardStep(prev => prev - 1)}>
                      <i className="fas fa-arrow-left"></i>
                      {t('devices:modal.prev')}
                    </button>
                  )}
                  {wizardStep < 3 ? (
                    <button type="button" className="btn btn-primary" onClick={() => setWizardStep(prev => prev + 1)}>
                      {t('devices:modal.next')}
                      <i className="fas fa-arrow-right"></i>
                    </button>
                  ) : (
                    <button
                      type="button"
                      className="btn btn-primary"
                      onClick={(e) => {
                        e.preventDefault();
                        e.stopPropagation();
                        console.log('🔥 생성 버튼 클릭됨 - 커스텀 모달 표시');
                        handleSave();
                      }}
                      disabled={isLoading || !editData?.name}
                    >
                      {isLoading ? (
                        <>
                          <i className="fas fa-spinner fa-spin"></i>
                          {t('devices:modal.creating')}
                        </>
                      ) : (
                        <>
                          <i className="fas fa-plus"></i>
                          {t('devices:modal.create')}
                        </>
                      )}
                    </button>
                  )}
                </>
              )}

              {mode === 'view' && (
                <button
                  type="button"
                  className="btn btn-primary"
                  onClick={() => {
                    if (onEdit) {
                      onEdit();
                    }
                  }}
                >
                  <i className="fas fa-edit"></i>
                  {t('devices:modal.edit')}
                </button>
              )}
              {(mode === 'edit') && (
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
                      {t('devices:modal.saving')}
                    </>
                  ) : (
                    <>
                      <i className="fas fa-save"></i>
                      {t('devices:modal.save')}
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
                <i className={`fas ${customModal.type === 'success' ? 'fa-check-circle' :
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
          width: 95vw;
          max-width: 1400px; /* 위저드를 위해 조금 더 넓게 설정 */
          max-height: 90vh;
          display: flex;
          flex-direction: column;
          box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
        }

        /* 위저드 단계 진행 표시기 스타일 */
        .wizard-steps-header {
          display: flex;
          align-items: center;
          justify-content: center;
          padding: 1.5rem 2rem;
          background: #f8fafc;
          border-bottom: 1px solid #e2e8f0;
          gap: 1rem;
          flex-shrink: 0;
        }

        .wizard-step-item {
          display: flex;
          align-items: center;
          gap: 0.75rem;
          color: #94a3b8;
          font-weight: 500;
        }

        .wizard-step-item.active {
          color: #2563eb;
        }

        .wizard-step-item.completed {
          color: #10b981;
        }

        .step-number {
          width: 2rem;
          height: 2rem;
          border-radius: 50%;
          background: #f1f5f9;
          display: flex;
          align-items: center;
          justify-content: center;
          font-size: 0.875rem;
          border: 1px solid #e2e8f0;
        }

        .wizard-step-item.active .step-number {
          background: #2563eb;
          color: white;
          border-color: #2563eb;
        }

        .wizard-step-item.completed .step-number {
          background: #d1fae5;
          color: #10b981;
          border-color: #10b981;
        }

        .wizard-step-line {
          flex: 0.1;
          height: 1px;
          background: #e2e8f0;
          min-width: 20px;
        }

        .wizard-step-line.completed {
          background: #10b981;
        }

        .modal-header {
          display: flex;
          align-items: center;
          justify-content: space-between;
          padding: 1.25rem 1.5rem;
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
          margin: 1.25rem 1.5rem 0 1.5rem; /* Floating Tab Bar */
          padding: 0 0.5rem;
          background: #f8fafc;
          border: 1px solid #e2e8f0;
          border-radius: 0.5rem;
          flex-shrink: 0;
          overflow-x: auto;
        }

        .tab-btn {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.75rem 1rem;
          border: none;
          background: none;
          color: #64748b;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
          position: relative;
          white-space: nowrap;
          flex-shrink: 0;
        }

        .tab-btn:hover {
          color: #374151;
          background: #f3f4f6;
        }

        .tab-btn.active {
          color: #0284c7;
          background: #e0f2fe;
          font-weight: 600;
          border-radius: 0.375rem;
          margin: 0.25rem 0;
        }

        .modal-content {
          flex: 1;
          overflow: hidden;
          display: flex;
          flex-direction: column;
          padding: 1.5rem;
        }

        .modal-content.wizard-mode {
          padding: 0;
          background: #f8fafc;
        }

        .wizard-registration-layout {
          display: flex;
          height: 100%;
          overflow: hidden;
        }

        .wizard-form-area {
          flex: 1;
          overflow-y: auto;
          padding: 2rem;
          display: flex;
          flex-direction: column;
          gap: 2rem;
        }

        .wizard-section {
          background: white;
          border-radius: 12px;
          border: 1px solid #e2e8f0;
          padding: 1.5rem;
          box-shadow: 0 1px 3px rgba(0,0,0,0.05);
        }

        .wizard-section-title {
          font-size: 1rem;
          font-weight: 700;
          color: #1e293b;
          margin: 0 0 1.5rem 0;
          display: flex;
          align-items: center;
          gap: 0.75rem;
          padding-bottom: 1rem;
          border-bottom: 1px solid #f1f5f9;
        }

        .wizard-section-title i {
          color: #22c55e;
        }

        .wizard-summary-area {
          width: 320px;
          background: white;
          border-left: 1px solid #e2e8f0;
          padding: 1.5rem;
          flex-shrink: 0;
        }

        .wizard-summary-box {
          position: sticky;
          top: 0;
          display: flex;
          flex-direction: column;
          gap: 1.5rem;
        }

        .summary-title {
          font-size: 0.875rem;
          font-weight: 700;
          color: #64748b;
          margin: 0;
          display: flex;
          align-items: center;
          gap: 0.5rem;
          text-transform: uppercase;
          letter-spacing: 0.025em;
        }

        .summary-content {
          display: flex;
          flex-direction: column;
          gap: 1.25rem;
          padding: 1.25rem;
          background: #f8fafc;
          border-radius: 12px;
          border: 1px solid #e2e8f0;
        }

        .summary-item {
          display: flex;
          flex-direction: column;
          gap: 0.375rem;
        }

        .summary-item label {
          font-size: 0.75rem;
          font-weight: 600;
          color: #94a3b8;
        }

        .summary-item span {
          font-size: 0.875rem;
          font-weight: 700;
          color: #1e293b;
          word-break: break-all;
        }

        .protocol-badge {
          display: inline-block;
          padding: 0.25rem 0.5rem;
          background: #dcfce7;
          color: #166534;
          border-radius: 6px;
          font-size: 0.75rem !important;
        }

        .endpoint-text {
          font-family: 'JetBrains Mono', monospace;
          color: #0284c7 !important;
        }

        .summary-footer {
          padding-top: 1rem;
          border-top: 1px solid #f1f5f9;
        }

        .summary-hint {
          font-size: 0.75rem;
          color: #64748b;
          line-height: 1.5;
          margin: 0;
          display: flex;
          gap: 0.5rem;
        }

        .summary-hint i {
          color: #f59e0b;
          margin-top: 2px;
        }

        .modal-footer {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1rem 1.5rem;
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
          z-index: 9000 !important;
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