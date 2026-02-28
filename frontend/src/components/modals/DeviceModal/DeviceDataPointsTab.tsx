// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx
// 📊 데이터포인트 탭 - 고급 필드 (스케일링, 로깅 등) 포함 확장 버전
// ============================================================================

import React, { useState, useEffect } from 'react';
import { useSearchParams } from 'react-router-dom';
import { DataApiService, DataPoint } from '../../../api/services/dataApi';
import { DeviceDataPointsTabProps } from './types';
import DeviceDataPointsBulkModal from './DeviceDataPointsBulkModal';

const DeviceDataPointsTab: React.FC<DeviceDataPointsTabProps> = ({
  deviceId,
  protocolType, // 🔥 NEW
  dataPoints,
  isLoading,
  error,
  mode,
  onRefresh,
  onCreate,
  onUpdate,
  onDelete,
  showModal
}) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [selectedDataPoints, setSelectedDataPoints] = useState<number[]>([]);
  const [searchTerm, setSearchTerm] = useState('');
  const [filterEnabled, setFilterEnabled] = useState<string>('all');
  const [filterDataType, setFilterDataType] = useState<string>('all');
  const [isProcessing, setIsProcessing] = useState(false);
  const [showCreateForm, setShowCreateForm] = useState(false);
  const [showEditForm, setShowEditForm] = useState(false);
  const [editingPoint, setEditingPoint] = useState<DataPoint | null>(null);
  const [showBulkModal, setShowBulkModal] = useState(false);

  // URL Params 관리
  const [searchParams, setSearchParams] = useSearchParams();

  // URL 파라미터로 Bulk Modal 상태 복원
  useEffect(() => {
    const bulkParam = searchParams.get('bulk');
    if (bulkParam === 'true') {
      setShowBulkModal(true);
    } else {
      setShowBulkModal(false);
    }
  }, [searchParams]);

  // Bulk Modal 상태 변경 시 URL 업데이트 핸들러
  const handleBulkModalChange = (isOpen: boolean) => {
    setShowBulkModal(isOpen);
    setSearchParams(prev => {
      const newParams = new URLSearchParams(prev);
      if (isOpen) {
        newParams.set('bulk', 'true');
      } else {
        newParams.delete('bulk');
      }
      return newParams;
    }, { replace: true });
  };

  // 폼 탭 관리
  const [activeFormTab, setActiveFormTab] = useState<'basic' | 'engineering' | 'logging' | 'alarm'>('basic');

  // 편집용 초기 데이터
  const initialPointData: Partial<DataPoint> = {
    name: '',
    description: '',
    address: '',
    data_type: 'number',
    unit: '',
    is_enabled: true,
    access_mode: 'read',
    // Engineering
    scaling_factor: 1,
    scaling_offset: 0,
    min_value: undefined,
    max_value: undefined,
    // Logging
    is_log_enabled: true,
    log_interval_ms: 0,
    log_deadband: 0,
    // Alarm
    is_alarm_enabled: false,
    alarm_priority: 'medium',
    high_alarm_limit: undefined,
    low_alarm_limit: undefined,
    alarm_deadband: 0,
    // Metadata & Group
    group_name: '',
    tags: [],
    metadata: {},
    protocol_params: {},
    address_string: '',
    mapping_key: ''
  };

  const [formData, setFormData] = useState<Partial<DataPoint>>(initialPointData);

  const canEdit = mode === 'edit' || mode === 'create';

  // ========================================================================
  // 필터링
  // ========================================================================
  const filteredDataPoints = React.useMemo(() => {
    return (dataPoints || []).filter(dp => {
      const matchesSearch = !searchTerm ||
        dp.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        (dp.description && dp.description.toLowerCase().includes(searchTerm.toLowerCase())) ||
        dp.address.toLowerCase().includes(searchTerm.toLowerCase());

      const matchesEnabled = filterEnabled === 'all' ||
        (filterEnabled === 'enabled' && dp.is_enabled) ||
        (filterEnabled === 'disabled' && !dp.is_enabled);

      const matchesDataType = filterDataType === 'all' || dp.data_type === filterDataType;

      return matchesSearch && matchesEnabled && matchesDataType;
    });
  }, [dataPoints, searchTerm, filterEnabled, filterDataType]);

  // ========================================================================
  // 핸들러
  // ========================================================================
  const handleOpenCreateCtx = () => {
    setFormData(initialPointData);
    setActiveFormTab('basic');
    setShowCreateForm(true);
  };

  const handleOpenEditCtx = (dp: DataPoint) => {
    setEditingPoint(dp);
    setFormData({ ...dp }); // 복사
    setActiveFormTab('basic');
    setShowEditForm(true);
  };

  const handleSave = async (isCreate: boolean) => {
    const nameStr = (formData.name || '').toString().trim();
    const addrStr = protocolType === 'MQTT'
      ? (formData.address_string || '').toString().trim()
      : (formData.address || '').toString().trim();

    if (!nameStr || !addrStr) {
      if (showModal) {
        showModal({
          type: 'error',
          title: '입력 오류',
          message: '필수 입력값(이름, 주소)을 확인해주세요.',
          onConfirm: () => { }
        });
      } else {
        alert('필수 입력값(이름, 주소)을 확인해주세요.');
      }
      return;
    }

    // 변경 사항 확인 (편집 시에만)
    if (!isCreate && editingPoint) {
      const changedKeys = Object.keys(formData).filter(key => {
        if (['updated_at', 'created_at', 'current_value', 'value', 'id', 'device_id'].includes(key)) return false;
        const oldVal = (editingPoint as any)[key];
        const newVal = (formData as any)[key];
        return String(oldVal ?? '') !== String(newVal ?? '');
      });

      if (changedKeys.length === 0) {
        if (showModal) {
          showModal({
            type: 'success',
            title: '변경 내용 없음',
            message: '수정된 내용이 없습니다.',
            onConfirm: () => {
              setShowEditForm(false);
              setEditingPoint(null);
            }
          });
        }
        return;
      }
    }

    // [변경 요청] 저장 클릭 시 "확인" 과정 없이 바로 실행 (어차피 메인에서 최종 저장 필요)
    try {
      setIsProcessing(true);
      const payload = {
        ...formData,
        id: isCreate ? Date.now() : editingPoint!.id,
        device_id: deviceId,
        // MQTT일 경우 address가 필수이므로 더미 값 할당 (중복 방지를 위해 Date.now 사용)
        address: protocolType === 'MQTT' ? (formData.address || Math.floor(Date.now() / 1000) % 1000000) : formData.address,
        updated_at: new Date().toISOString(),
        created_at: isCreate ? new Date().toISOString() : formData.created_at,
        // 🔥 Ensure engineering fields are preserved
        scaling_factor: formData.scaling_factor ?? 1,
        scaling_offset: formData.scaling_offset ?? 0,
        min_value: formData.min_value,
        max_value: formData.max_value,
        unit: formData.unit
      } as DataPoint;

      if (isCreate) {
        onCreate(payload);
      } else {
        onUpdate(payload);
      }

      if (showModal) {
        showModal({
          type: 'success',
          title: '저장 완료',
          message: '데이터포인트 정보가 임시 반영되었습니다.\n장치 모달 우측 하단의 [저장] 버튼을 클릭해야 서버에 최종 저장됩니다.',
          onConfirm: () => {
            setShowCreateForm(false);
            setShowEditForm(false);
            setEditingPoint(null);
          }
        });
      } else {
        alert('저장되었습니다.');
        setShowCreateForm(false);
        setShowEditForm(false);
        setEditingPoint(null);
      }
    } catch (error) {
      console.error('DataPoint save error:', error);
      if (showModal) {
        showModal({
          type: 'error',
          title: '저장 실패',
          message: '저장 중 오류가 발생했습니다.',
          onConfirm: () => { }
        });
      } else {
        alert('저장 중 오류가 발생했습니다.');
      }
    } finally {
      setIsProcessing(false);
    }
  };

  const handleTestRead = async (dp: DataPoint) => {
    try {
      setIsProcessing(true);
      const res = await DataApiService.getCurrentValues({ point_ids: [dp.id], include_metadata: true });
      if (res.success && res.data?.current_values) {
        const val = res.data.current_values.find(v => v.point_id === dp.id);
        const msg = val ? `값: ${val.value} (${val.quality})\n시간: ${val.timestamp}` : '값을 읽지 못했습니다.';
        if (showModal) {
          showModal({
            type: 'success',
            title: '읽기 테스트 결과',
            message: msg,
            onConfirm: () => { }
          });
        } else {
          alert(msg);
        }
      } else {
        if (showModal) {
          showModal({ type: 'error', title: '읽기 실패', message: '데이터를 읽어오지 못했습니다.', onConfirm: () => { } });
        } else {
          alert('읽기 실패');
        }
      }
    } catch (e) {
      if (showModal) {
        showModal({ type: 'error', title: '통신 오류', message: '백엔드 서버와 통신할 수 없습니다.', onConfirm: () => { } });
      } else {
        alert('통신 오류');
      }
    } finally {
      setIsProcessing(false);
    }
  };

  const handleBulkCreate = async (points: Partial<DataPoint>[]) => {
    try {
      setIsProcessing(true);
      // TODO: 백엔드에 Bulk Insert API가 있다면 그것을 사용 (현재는 개별 생성 반복)
      // 병렬 처리 시 부하 고려하여 순차 처리 또는 Promise.all 사용
      // 여기서는 UI 반응성을 위해 순차 처리 + 로딩 표시 (최대 20개씩 병렬)

      const BATCH_SIZE = 20;
      let successCount = 0;
      let failCount = 0;

      for (let i = 0; i < points.length; i += BATCH_SIZE) {
        const batch = points.slice(i, i + BATCH_SIZE);
        await Promise.all(batch.map(async (point) => {
          try {
            const payload = {
              ...point,
              id: Date.now() + Math.random(), // 임시 ID
              device_id: deviceId,
              updated_at: new Date().toISOString(),
              created_at: new Date().toISOString()
            } as DataPoint;

            await onCreate(payload);
            successCount++;
          } catch (e) {
            console.error('Failed to create point', point, e);
            failCount++;
          }
        }));
      }

      const msg = `완료: 성공 ${successCount}건${failCount > 0 ? `, 실패 ${failCount}건` : ''}\n(디바이스 전체 저장 시 서버에 최종 반영됩니다)`;
      if (showModal) {
        showModal({
          type: 'success',
          title: '일괄 등록 완료',
          message: msg,
          onConfirm: () => handleBulkModalChange(false)
        });
      } else {
        alert(msg);
        handleBulkModalChange(false);
      }
    } catch (e) {
      const errMsg = `일괄 저장 중 오류가 발생했습니다: ${e instanceof Error ? e.message : 'Unknown'}`;
      if (showModal) {
        showModal({ type: 'error', title: '일괄 등록 실패', message: errMsg, onConfirm: () => { } });
      } else {
        alert(errMsg);
      }
    } finally {
      setIsProcessing(false);
    }
  };

  // ========================================================================
  // 렌더링 헬퍼
  // ========================================================================
  const renderFormContent = () => {
    return (
      <div className="form-content-scroll">
        {activeFormTab === 'basic' && (
          <div className="form-grid-2">
            <div className="form-field">
              <label>이름 *</label>
              <input
                type="text"
                value={formData.name || ''}
                onChange={e => setFormData({ ...formData, name: e.target.value })}
                placeholder="Ex: Temp_Sensor_1"
              />
            </div>
            <div className="form-field">
              <label>{protocolType === 'MQTT' ? 'Sub-Topic' : '주소 (Address)'} *</label>
              <input
                type="text"
                value={protocolType === 'MQTT' ? (formData.address_string || '') : (formData.address || '')}
                onChange={e => {
                  if (protocolType === 'MQTT') {
                    setFormData({ ...formData, address_string: e.target.value });
                  } else {
                    setFormData({ ...formData, address: e.target.value });
                  }
                }}
                placeholder={protocolType === 'MQTT' ? "Ex: /sensor/temp" : "Ex: 40001"}
              />
            </div>

            {protocolType === 'MQTT' && (
              <div className="form-field">
                <label>JSON Key (Mapping)</label>
                <input
                  type="text"
                  value={formData.mapping_key || ''}
                  onChange={e => setFormData({
                    ...formData,
                    mapping_key: e.target.value
                  })}
                  placeholder="Ex: temperature"
                />
              </div>
            )}

            {/* Modbus 전용: Function Code 선택 */}
            {(protocolType === 'MODBUS_TCP' || protocolType === 'MODBUS_RTU' || protocolType === 'MODBUS') && (
              <div className="form-field">
                <label>Function Code (FC) *</label>
                <select
                  value={
                    typeof formData.protocol_params === 'object'
                      ? (formData.protocol_params as any)?.function_code || '3'
                      : '3'
                  }
                  onChange={e => {
                    const params = typeof formData.protocol_params === 'object'
                      ? { ...(formData.protocol_params as any) }
                      : {};
                    params.function_code = e.target.value;
                    setFormData({ ...formData, protocol_params: params });
                  }}
                >
                  <option value="1">FC01 — Read Coils (CO:xxx, BOOL)</option>
                  <option value="2">FC02 — Read Discrete Inputs (DI:xxx)</option>
                  <option value="3">FC03 — Read Holding Registers (HR:xxx)</option>
                  <option value="4">FC04 — Read Input Registers (IR:xxx)</option>
                </select>
                <span className="hint">FC03이 일반적인 레지스터. BOOL 포인트는 FC01</span>
              </div>
            )}

            {/* Modbus 전용: Bit Index (비트맵 레지스터) */}
            {(protocolType === 'MODBUS_TCP' || protocolType === 'MODBUS_RTU' || protocolType === 'MODBUS') && (
              <div className="form-field">
                <label>Bit Index (0-15, 선택)</label>
                <input
                  type="number"
                  min="0"
                  max="15"
                  value={
                    typeof formData.protocol_params === 'object'
                      ? (formData.protocol_params as any)?.bit_index ?? ''
                      : ''
                  }
                  onChange={e => {
                    const params = typeof formData.protocol_params === 'object'
                      ? { ...(formData.protocol_params as any) }
                      : {};
                    if (e.target.value === '') {
                      delete params.bit_index;
                    } else {
                      params.bit_index = e.target.value;
                    }
                    setFormData({ ...formData, protocol_params: params });
                  }}
                  placeholder="비워두면 레지스터 전체 값 사용"
                />
                <span className="hint">한 레지스터를 비트 단위로 쪼갤 때 사용 (0=LSB)</span>
              </div>
            )}

            <div className="form-field">
              <label>데이터 타입</label>
              <select
                value={formData.data_type || 'FLOAT32'}
                onChange={e => setFormData({ ...formData, data_type: e.target.value as any })}
              >
                <optgroup label="Common">
                  <option value="INT16">INT16 (2 bytes)</option>
                  <option value="UINT16">UINT16 (2 bytes)</option>
                  <option value="FLOAT32">FLOAT32 (4 bytes)</option>
                  <option value="BOOL">Boolean (1 bit/byte)</option>
                  <option value="STRING">String</option>
                </optgroup>
                <optgroup label="32-bit / 64-bit High Precision">
                  <option value="INT32">INT32 (4 bytes)</option>
                  <option value="UINT32">UINT32 (4 bytes)</option>
                  <option value="FLOAT64">FLOAT64 (8 bytes)</option>
                  <option value="INT64">INT64 (8 bytes)</option>
                  <option value="UINT64">UINT64 (8 bytes)</option>
                </optgroup>
                <optgroup label="8-bit / Others">
                  <option value="INT8">INT8 (1 byte)</option>
                  <option value="UINT8">UINT8 (1 byte)</option>
                  <option value="DATETIME">DATETIME (Time)</option>
                  <option value="UNKNOWN">Unknown / Other</option>
                </optgroup>
              </select>
            </div>
            <div className="form-field">
              <label>접근 모드</label>
              <select
                value={formData.access_mode || 'read'}
                onChange={e => setFormData({ ...formData, access_mode: e.target.value as any })}
              >
                <option value="read">Read Only</option>
                <option value="write">Write Only</option>
                <option value="read_write">Read / Write</option>
              </select>
            </div>

            <div className="form-field full">
              <label>설명</label>
              <input
                type="text"
                value={formData.description || ''}
                onChange={e => setFormData({ ...formData, description: e.target.value })}
              />
            </div>

            <div className="form-field">
              <label>그룹명 (Group)</label>
              <input
                type="text"
                value={formData.group_name || ''}
                onChange={e => setFormData({ ...formData, group_name: e.target.value })}
                placeholder="Ex: MainEngine"
              />
            </div>
            <div className="form-field">
              <label>태그 (Tags, 콤마 구분)</label>
              <input
                type="text"
                value={Array.isArray(formData.tags) ? formData.tags.join(', ') : ''}
                onChange={e => setFormData({ ...formData, tags: e.target.value.split(',').map(s => s.trim()).filter(s => s) })}
                placeholder="Ex: critical, temp"
              />
            </div>

            <div className="form-field">
              <label className="checkbox-wrap">
                <input
                  type="checkbox"
                  checked={formData.is_enabled !== false}
                  onChange={e => setFormData({ ...formData, is_enabled: e.target.checked })}
                />
                포인트 활성화
              </label>
            </div>

            <div className="form-field full">
              <label>메타데이터 (JSON)</label>
              <textarea
                style={{ height: '60px', fontSize: '12px', padding: '8px', border: '1px solid #cbd5e1', borderRadius: '4px' }}
                value={typeof formData.metadata === 'object' ? JSON.stringify(formData.metadata) : formData.metadata || '{}'}
                onChange={e => {
                  try {
                    const parsed = JSON.parse(e.target.value);
                    setFormData({ ...formData, metadata: parsed });
                  } catch (err) {
                    setFormData({ ...formData, metadata: e.target.value });
                  }
                }}
              />
            </div>
          </div>
        )}

        {activeFormTab === 'engineering' && (
          <div className="form-grid-2">
            <div className="form-field">
              <label>단위 (Unit)</label>
              <input
                type="text"
                value={formData.unit || ''}
                onChange={e => setFormData({ ...formData, unit: e.target.value })}
                placeholder="Ex: °C, kg, m/s"
              />
            </div>
            <div className="form-field">
              {/* Empty spacer */}
            </div>

            <div className="form-field">
              <label>스케일링 팩터 (Scale)</label>
              <input
                type="number"
                value={formData.scaling_factor ?? 1}
                onChange={e => setFormData({ ...formData, scaling_factor: parseFloat(e.target.value) })}
              />
              <span className="hint">Raw Value * Factor</span>
            </div>
            <div className="form-field">
              <label>오프셋 (Offset)</label>
              <input
                type="number"
                value={formData.scaling_offset ?? 0}
                onChange={e => setFormData({ ...formData, scaling_offset: parseFloat(e.target.value) })}
              />
              <span className="hint">+ Offset</span>
            </div>

            <div className="form-field">
              <label>최소값 (Min)</label>
              <input
                type="number"
                value={formData.min_value ?? ''}
                onChange={e => setFormData({ ...formData, min_value: parseFloat(e.target.value) })}
                placeholder="No Limit"
              />
            </div>
            <div className="form-field">
              <label>최대값 (Max)</label>
              <input
                type="number"
                value={formData.max_value ?? ''}
                onChange={e => setFormData({ ...formData, max_value: parseFloat(e.target.value) })}
                placeholder="No Limit"
              />
            </div>
          </div>
        )}

        {activeFormTab === 'logging' && (
          <div className="form-stack">
            <div className="form-field">
              <label className="checkbox-wrap">
                <input
                  type="checkbox"
                  checked={formData.is_log_enabled !== false}
                  onChange={e => setFormData({ ...formData, is_log_enabled: e.target.checked })}
                />
                데이터 로깅 활성화
              </label>
            </div>

            {formData.is_log_enabled && (
              <div className="form-grid-2">
                <div className="form-field">
                  <label>로깅 간격 (ms)</label>
                  <input
                    type="number"
                    value={formData.log_interval_ms ?? 0}
                    onChange={e => setFormData({ ...formData, log_interval_ms: parseInt(e.target.value) })}
                    min="0"
                  />
                  <span className="hint">0 = 변경 시마다 로깅 (COV)</span>
                </div>
                <div className="form-field">
                  <label>로깅 데드밴드 (Deadband)</label>
                  <input
                    type="number"
                    step="0.01"
                    value={formData.log_deadband ?? 0}
                    onChange={e => setFormData({ ...formData, log_deadband: parseFloat(e.target.value) })}
                  />
                  <span className="hint">값 변화량이 이보다 클 때만 저장</span>
                </div>
              </div>
            )}
          </div>
        )}

        {activeFormTab === 'alarm' && (
          <div className="form-stack">
            <div className="form-field">
              <label className="checkbox-wrap">
                <input
                  type="checkbox"
                  checked={formData.is_alarm_enabled === true}
                  onChange={e => setFormData({ ...formData, is_alarm_enabled: e.target.checked })}
                />
                알람 사용
              </label>
            </div>

            {formData.is_alarm_enabled && (
              <div className="form-grid-2">
                <div className="form-field">
                  <label>알람 우선순위</label>
                  <select
                    value={formData.alarm_priority || 'medium'}
                    onChange={e => setFormData({ ...formData, alarm_priority: e.target.value as any })}
                  >
                    <option value="low">Low</option>
                    <option value="medium">Medium</option>
                    <option value="high">High</option>
                    <option value="critical">Critical</option>
                  </select>
                </div>
                <div className="form-field">
                  <label>알람 데드밴드</label>
                  <input
                    type="number"
                    step="0.01"
                    value={formData.alarm_deadband ?? 0}
                    onChange={e => setFormData({ ...formData, alarm_deadband: parseFloat(e.target.value) })}
                  />
                </div>
                <div className="form-field">
                  <label>상한 임계치 (High Limit)</label>
                  <input
                    type="number"
                    step="0.01"
                    value={formData.high_alarm_limit ?? ''}
                    onChange={e => setFormData({ ...formData, high_alarm_limit: parseFloat(e.target.value) })}
                    placeholder="No Limit"
                  />
                </div>
                <div className="form-field">
                  <label>하한 임계치 (Low Limit)</label>
                  <input
                    type="number"
                    step="0.01"
                    value={formData.low_alarm_limit ?? ''}
                    onChange={e => setFormData({ ...formData, low_alarm_limit: parseFloat(e.target.value) })}
                    placeholder="No Limit"
                  />
                </div>
              </div>
            )}
          </div>
        )}
      </div>
    );
  };

  // ========================================================================
  // 메인 렌더링
  // ========================================================================
  return (
    <div className="dp-container">
      {/* 헤더 */}
      <div className="dp-header">
        <div className="left">
          <h3>데이터포인트 <span className="count">({filteredDataPoints.length})</span></h3>
        </div>
        <div className="right">
          {canEdit && (
            <button className="btn-primary-sm" onClick={handleOpenCreateCtx}>
              <i className="fas fa-plus"></i> 추가
            </button>
          )}
          {canEdit && (
            <button className="btn-secondary-sm" onClick={() => handleBulkModalChange(true)}>
              <i className="fas fa-paste"></i> 대량 등록
            </button>
          )}
          <button className="btn-icon" onClick={onRefresh} disabled={isLoading} title="새로고침">
            <i className={`fas ${isLoading ? 'fa-spinner fa-spin' : 'fa-sync-alt'}`}></i>
          </button>
        </div>
      </div>

      {/* 필터 바 */}
      <div className="dp-toolbar">
        <div className="search-box">
          <i className="fas fa-search"></i>
          <input
            type="text"
            placeholder="검색..."
            value={searchTerm}
            onChange={e => setSearchTerm(e.target.value)}
          />
        </div>
        <select value={filterDataType} onChange={e => setFilterDataType(e.target.value)}>
          <option value="all">모든 타입</option>
          <optgroup label="Categories">
            <option value="number">Number (Any)</option>
            <option value="boolean">Boolean</option>
            <option value="string">String</option>
          </optgroup>
          <optgroup label="Specific Types">
            <option value="INT16">INT16</option>
            <option value="UINT16">UINT16</option>
            <option value="INT32">INT32</option>
            <option value="UINT32">UINT32</option>
            <option value="FLOAT32">FLOAT32</option>
            <option value="FLOAT64">FLOAT64</option>
          </optgroup>
        </select>
        <select value={filterEnabled} onChange={e => setFilterEnabled(e.target.value)}>
          <option value="all">모든 상태</option>
          <option value="enabled">활성화됨</option>
          <option value="disabled">비활성화됨</option>
        </select>
      </div>

      {/* 테이블 */}
      <div className="dp-table-wrap">
        <div className="dp-table-head">
          <div className="th col-id">ID</div>
          <div className="th col-name">이름 / 설명</div>
          <div className="th col-addr">주소</div>
          <div className="th col-type">타입 / 단위</div>
          <div className="th col-access">권한</div>
          <div className="th col-scale">스케일</div>
          <div className="th col-range">범위 (Min~Max)</div>
          <div className="th col-val">현재값</div>
          <div className="th col-action"></div>
        </div>
        <div className="dp-table-body">
          {filteredDataPoints.map(dp => (
            <div key={dp.id} className="tr">
              <div className="td col-id">
                <span className="id-tag">{dp.id}</span>
              </div>
              <div className="td col-name">
                <div className="name-row">
                  <span className="name">{dp.name}</span>
                  {!dp.is_enabled && <span className="badge disabled">OFF</span>}
                </div>
                {dp.description && <div className="desc">{dp.description}</div>}
              </div>
              <div className="td col-addr">
                <span className="addr-tag">{dp.address}</span>
              </div>
              <div className="td col-type">
                <div className="type-tag">{dp.data_type}</div>
                {dp.unit && <div className="unit">{dp.unit}</div>}
              </div>
              <div className="td col-access">
                <span className={`access-tag ${dp.access_mode || 'read'}`}>
                  {dp.access_mode === 'read_write' ? 'R/W' : (dp.access_mode === 'write' ? 'W' : 'R')}
                </span>
              </div>
              <div className="td col-scale">
                <div className="scale-row">x{dp.scaling_factor ?? 1}</div>
                <div className="scale-row offset">{(dp.scaling_offset ?? 0) >= 0 ? '+' : ''}{dp.scaling_offset ?? 0}</div>
              </div>
              <div className="td col-range">
                {(dp.min_value !== undefined || dp.max_value !== undefined) ? (
                  <span className="range-tag">
                    {dp.min_value ?? '-∞'} ~ {dp.max_value ?? '+∞'}
                  </span>
                ) : (
                  <span className="range-none">-</span>
                )}
              </div>
              <div className="td col-val">
                {dp.current_value && dp.current_value.value !== undefined ? (
                  <span className="val">
                    {((dp.data_type as string) === 'DATETIME' || (dp.data_type as string) === 'datetime' || dp.name.toLowerCase().includes('timestamp')) ? (
                      (() => {
                        const val = Number(dp.current_value.value);
                        if (isNaN(val) || val <= 0) return String(dp.current_value.value);
                        // ms 단위인지 s 단위인지 체크 (10^12 이상이면 ms로 간주)
                        const d = new Date(val > 1000000000000 ? val : val * 1000);
                        return d.toLocaleString('ko-KR', {
                          year: 'numeric', month: '2-digit', day: '2-digit',
                          hour: '2-digit', minute: '2-digit', second: '2-digit',
                          hour12: false
                        }).replace(/\. /g, '-').replace(/\./g, '');
                      })()
                    ) : (
                      String(dp.current_value.value)
                    )}
                  </span>
                ) : (
                  <span className="no-val">-</span>
                )}
              </div>
              <div className="td col-action">
                <div className="btn-group">
                  <button onClick={() => handleTestRead(dp)} title="읽기 테스트"><i className="fas fa-play"></i></button>
                  {canEdit && (
                    <>
                      <button onClick={() => handleOpenEditCtx(dp)} title="편집"><i className="fas fa-pencil-alt"></i></button>
                      <button onClick={() => {
                        if (showModal) {
                          showModal({
                            type: 'confirm',
                            title: '삭제 확인',
                            message: `[${dp.name}] 데이터포인트를 삭제하시겠습니까?`,
                            confirmText: '삭제',
                            onConfirm: () => onDelete(dp.id)
                          });
                        } else if (confirm('삭제하시겠습니까?')) {
                          onDelete(dp.id);
                        }
                      }} title="삭제" className="danger"><i className="fas fa-trash"></i></button>
                    </>
                  )}
                </div>
              </div>
            </div>
          ))}
          {filteredDataPoints.length === 0 && (
            <div className="empty-msg">데이터가 없습니다.</div>
          )}
        </div>
      </div>

      {/* 모달 (생성/수정) */}
      {(showCreateForm || showEditForm) && (
        <div className="modal-overlay">
          <div className="modal-dialog">
            <div className="modal-hdr">
              <h3>{showCreateForm ? '새 데이터포인트' : '데이터포인트 편집'}</h3>
              <button onClick={() => { setShowCreateForm(false); setShowEditForm(false); }}>
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-tabs">
              <button className={activeFormTab === 'basic' ? 'active' : ''} onClick={() => setActiveFormTab('basic')}>기본 정보</button>
              <button className={activeFormTab === 'engineering' ? 'active' : ''} onClick={() => setActiveFormTab('engineering')}>엔지니어링</button>
              <button className={activeFormTab === 'logging' ? 'active' : ''} onClick={() => setActiveFormTab('logging')}>로깅</button>
              <button className={activeFormTab === 'alarm' ? 'active' : ''} onClick={() => setActiveFormTab('alarm')}>알람</button>
            </div>

            <div className="modal-body icon-inputs">
              {renderFormContent()}
            </div>

            <div className="modal-ftr">
              <button className="btn-sec" onClick={() => { setShowCreateForm(false); setShowEditForm(false); }}>취소</button>
              <button className="btn-pri" onClick={() => handleSave(showCreateForm)}>저장</button>
            </div>
          </div>
        </div>
      )}

      {/* 대량 등록 모달 */}
      <DeviceDataPointsBulkModal
        deviceId={deviceId}
        isOpen={showBulkModal}
        onClose={() => handleBulkModalChange(false)}
        onSave={handleBulkCreate}
        existingAddresses={dataPoints?.map(dp => dp.address) || []}
        protocolType={protocolType}
      />

      <style>{`
         .dp-container { display: flex; flex-direction: column; height: 100%; background: #f8fafc; font-family: 'Inter', sans-serif; gap: 8px; }
         .dp-header { display: flex; justify-content: space-between; align-items: center; padding: 12px 16px; background: white; border-bottom: 1px solid #e2e8f0; }
         .dp-header h3 { margin: 0; font-size: 15px; font-weight: 600; color: #1e293b; }
         .dp-header .count { color: #64748b; font-size: 13px; font-weight: 500; }
         .dp-header .right { display: flex; gap: 8px; align-items: center; }

         .dp-toolbar { display: flex; padding: 8px 16px; gap: 8px; background: white; border-bottom: 1px solid #e2e8f0; }
         .search-box { display: flex; align-items: center; background: #f1f5f9; border-radius: 4px; padding: 0 8px; flex: 1; border: 1px solid transparent; }
         .search-box:focus-within { border-color: #3b82f6; background: white; }
         .search-box i { color: #94a3b8; font-size: 12px; margin-right: 6px; }
         .search-box input { border: none; background: transparent; height: 32px; font-size: 13px; width: 100%; outline: none; }
         
         .dp-toolbar select { padding: 0 12px; height: 34px; border: 1px solid #cbd5e1; border-radius: 4px; font-size: 13px; color: #475569; outline: none; background: white; }

         .btn-primary-sm { background: #3b82f6; color: white; border: none; padding: 6px 12px; border-radius: 4px; font-size: 12px; font-weight: 500; cursor: pointer; display: inline-flex; gap: 6px; align-items: center; }
         .btn-primary-sm:hover { background: #2563eb; }
         .btn-secondary-sm { background: white; color: #475569; border: 1px solid #cbd5e1; padding: 6px 12px; border-radius: 4px; font-size: 12px; font-weight: 500; cursor: pointer; display: inline-flex; gap: 6px; align-items: center; }
         .btn-secondary-sm:hover { background: #f8fafc; border-color: #94a3b8; }
         .btn-icon { background: white; border: 1px solid #cbd5e1; width: 32px; height: 32px; border-radius: 4px; color: #64748b; cursor: pointer; display: inline-flex; justify-content: center; align-items: center; }
         .btn-icon:hover { background: #f1f5f9; color: #3b82f6; }

          /* Table */
          .dp-table-wrap { flex: 1; display: flex; flex-direction: column; overflow: hidden; background: white; }
          .dp-table-head { display: grid; grid-template-columns: 60px minmax(200px, 2fr) 90px 100px 60px 70px 120px minmax(80px, 0.8fr) 110px; background: #f8fafc; border-bottom: 1px solid #e2e8f0; font-size: 11px; font-weight: 600; color: #64748b; text-transform: uppercase; }
          .th { padding: 10px 12px; display: flex; align-items: center; justify-content: center; }
          .th.col-id { border-right: 1px solid transparent; }
          .th.col-name { justify-content: flex-start; }
          
          .dp-table-body { flex: 1; overflow-y: auto; }
          .tr { display: grid; grid-template-columns: 60px minmax(200px, 2fr) 90px 100px 60px 70px 120px minmax(80px, 0.8fr) 110px; border-bottom: 1px solid #f1f5f9; font-size: 13px; color: #334155; }
          .tr:hover { background: #f8fafc; }
          .td { padding: 8px 12px; display: flex; flex-direction: column; justify-content: center; align-items: center; overflow: hidden; text-align: center; }
          .td.col-id { border-right: 1px solid transparent; color: #94a3b8; font-family: monospace; font-size: 11px; }
          .td.col-name { align-items: flex-start; text-align: left; }
         
         .name-row { display: flex; align-items: center; gap: 6px; overflow: hidden; }
         .name { font-weight: 500; color: #1e293b; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; display: block; }
         .desc { font-size: 11px; color: #94a3b8; margin-top: 2px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
         .badge.disabled { background: #f1f5f9; color: #94a3b8; font-size: 10px; padding: 1px 4px; border-radius: 3px; border: 1px solid #e2e8f0; }
         
         .addr-tag { font-family: monospace; background: #eff6ff; color: #1d4ed8; padding: 2px 6px; border-radius: 4px; font-size: 12px; }
         .type-tag { font-size: 12px; font-weight: 500; }
         .unit { font-size: 11px; color: #94a3b8; }
         
         .access-tag { font-size: 10px; font-weight: 700; padding: 2px 6px; border-radius: 3px; text-transform: uppercase; }
         .access-tag.read { background: #f0fdf4; color: #166534; border: 1px solid #bbf7d0; }
         .access-tag.write { background: #fff1f2; color: #be123c; border: 1px solid #fecdd3; }
         .access-tag.read_write { background: #fff7ed; color: #c2410c; border: 1px solid #fed7aa; }

         .scale-row { font-size: 12px; color: #475569; width: 100%; text-align: center; }
         .scale-row.offset { font-size: 11px; color: #94a3b8; }
         
         .range-tag { font-size: 11px; color: #475569; background: #f1f5f9; padding: 2px 6px; border-radius: 4px; white-space: nowrap; }
         .range-none { color: #cbd5e1; }
         
         .td.col-action { overflow: visible; align-items: center; }
         .val { font-weight: 600; color: #0f172a; }
         .no-val { color: #cbd5e1; }

         .btn-group { display: flex; gap: 4px; }
         .btn-group button { width: 26px; height: 26px; border: 1px solid #e2e8f0; background: white; border-radius: 4px; color: #64748b; cursor: pointer; font-size: 11px; display: flex; justify-content: center; align-items: center; box-sizing: border-box; }
         .btn-group button:hover { border-color: #3b82f6; color: #3b82f6; }
         .btn-group button.danger:hover { border-color: #ef4444; color: #ef4444; }

         .empty-msg { padding: 32px; text-align: center; color: #94a3b8; font-size: 13px; }

         /* Modal */
         .modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.4); display: flex; justify-content: center; align-items: center; z-index: 50; }
         .modal-dialog { background: white; width: 500px; border-radius: 8px; box-shadow: 0 10px 25px rgba(0,0,0,0.1); display: flex; flex-direction: column; overflow: hidden; max-height: 90vh; }
         .modal-hdr { display: flex; justify-content: space-between; padding: 16px; border-bottom: 1px solid #e2e8f0; align-items: center; }
         .modal-hdr h3 { margin: 0; font-size: 16px; }
         .modal-hdr button { background: none; border: none; cursor: pointer; color: #94a3b8; font-size: 16px; }

         .modal-tabs { display: flex; padding: 0 16px; border-bottom: 1px solid #e2e8f0; background: #f8fafc; gap: 16px; }
         .modal-tabs button { background: none; border: none; padding: 12px 0; font-size: 13px; color: #64748b; cursor: pointer; position: relative; font-weight: 500; }
         .modal-tabs button.active { color: #3b82f6; }
         .modal-tabs button.active::after { content: ''; position: absolute; bottom: -1px; left: 0; right: 0; height: 2px; background: #3b82f6; }

         .modal-body { padding: 20px; overflow-y: auto; }
         .modal-ftr { padding: 16px; border-top: 1px solid #e2e8f0; display: flex; justify-content: flex-end; gap: 8px; background: #f8fafc; }
         
         .form-grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
         .form-stack { display: flex; flex-direction: column; gap: 16px; }
         .form-field { display: flex; flex-direction: column; gap: 6px; }
         .form-field.full { grid-column: span 2; }
         .form-field label { font-size: 12px; font-weight: 500; color: #475569; }
         .form-field input, .form-field select { height: 34px; padding: 0 10px; border: 1px solid #cbd5e1; border-radius: 4px; font-size: 13px; outline: none; }
         .form-field input:focus, .form-field select:focus { border-color: #3b82f6; box-shadow: 0 0 0 1px rgba(59,130,246,0.1); }
         .hint { font-size: 10px; color: #94a3b8; }
         .checkbox-wrap { display: flex; flex-direction: row; align-items: center; gap: 8px; font-weight: normal; cursor: pointer; }
         
         .btn-pri { background: #3b82f6; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-size: 13px; font-weight: 500; cursor: pointer; }
         .btn-sec { background: white; border: 1px solid #cbd5e1; color: #475569; padding: 8px 16px; border-radius: 4px; font-size: 13px; font-weight: 500; cursor: pointer; }

          @media (max-width: 1200px) {
             .dp-table-head, .tr { grid-template-columns: 50px minmax(150px, 1.5fr) 80px 90px 50px 60px 100px minmax(70px, 0.8fr) 100px; }
          }
      `}</style>
    </div>
  );
};

export default DeviceDataPointsTab;