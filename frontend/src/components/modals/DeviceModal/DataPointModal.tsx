// ============================================================================
// frontend/src/components/modals/DataPointModal.tsx
// 📊 데이터포인트 생성/편집 모달
// ============================================================================

import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { DataPoint, DataPointCreateData, DataPointUpdateData, DataPointApiService } from '../../../api/services/dataPointApi';

interface DataPointModalProps {
  isOpen: boolean;
  mode: 'create' | 'edit' | 'view';
  deviceId: number;
  dataPoint?: DataPoint | null;
  onClose: () => void;
  onSave: (dataPoint: DataPoint) => void;
  onDelete?: (pointId: number) => void;
}

const DataPointModal: React.FC<DataPointModalProps> = ({
  isOpen,
  mode,
  deviceId,
  dataPoint,
  onClose,
  onSave,
  onDelete
}) => {
  const [formData, setFormData] = useState<DataPointCreateData | DataPointUpdateData>({
    device_id: deviceId,
    name: '',
    description: '',
    address: '',
    address_string: '',
    data_type: 'uint16',
    access_mode: 'read',
    is_enabled: true,
    is_writable: false,
    unit: '',
    scaling_factor: 1,
    scaling_offset: 0,
    min_value: 0,
    max_value: 65535,
    is_log_enabled: true,
    log_interval_ms: 5000,
    log_deadband: 0,
    polling_interval_ms: 1000,
    group_name: '',
    tags: [],
    metadata: {},
    protocol_params: {}
  });
  const { t } = useTranslation(['devices', 'common']);

  const [isLoading, setIsLoading] = useState(false);
  const [isTestingRead, setIsTestingRead] = useState(false);
  const [isTestingWrite, setIsTestingWrite] = useState(false);
  const [testValue, setTestValue] = useState('');
  const [testResult, setTestResult] = useState<string | null>(null);
  const [errors, setErrors] = useState<Record<string, string>>({});

  // 폼 데이터 초기화
  useEffect(() => {
    if (isOpen) {
      if (mode === 'create') {
        setFormData({
          device_id: deviceId,
          name: '',
          description: '',
          address: '',
          address_string: '',
          data_type: 'uint16',
          access_mode: 'read',
          is_enabled: true,
          is_writable: false,
          unit: '',
          scaling_factor: 1,
          scaling_offset: 0,
          min_value: 0,
          max_value: 65535,
          is_log_enabled: true,
          log_interval_ms: 5000,
          log_deadband: 0,
          polling_interval_ms: 1000,
          group_name: '',
          tags: [],
          metadata: {},
          protocol_params: {}
        });
      } else if (dataPoint) {
        setFormData({
          name: dataPoint.name,
          description: dataPoint.description || '',
          address: dataPoint.address,
          address_string: dataPoint.address_string || '',
          data_type: dataPoint.data_type,
          access_mode: dataPoint.access_mode || 'read',
          is_enabled: dataPoint.is_enabled,
          is_writable: dataPoint.is_writable || false,
          unit: dataPoint.unit || '',
          scaling_factor: dataPoint.scaling_factor || 1,
          scaling_offset: dataPoint.scaling_offset || 0,
          min_value: dataPoint.min_value || 0,
          max_value: dataPoint.max_value || 65535,
          is_log_enabled: dataPoint.is_log_enabled !== false,
          log_interval_ms: dataPoint.log_interval_ms || 5000,
          log_deadband: dataPoint.log_deadband || 0,
          polling_interval_ms: dataPoint.polling_interval_ms || 1000,
          group_name: dataPoint.group_name || '',
          tags: dataPoint.tags || [],
          metadata: dataPoint.metadata || {},
          protocol_params: dataPoint.protocol_params || {}
        });
      }
      setErrors({});
      setTestResult(null);
    }
  }, [isOpen, mode, dataPoint, deviceId]);

  // 폼 필드 업데이트
  const updateField = (field: string, value: any) => {
    setFormData(prev => ({ ...prev, [field]: value }));
    if (errors[field]) {
      setErrors(prev => ({ ...prev, [field]: '' }));
    }
  };

  // 유효성 검사
  const validateForm = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!formData.name?.trim()) {
      newErrors.name = t('dpModal2.validationName', { ns: 'devices' });
    }

    if (!formData.address?.trim()) {
      newErrors.address = t('dpModal2.validationAddress', { ns: 'devices' });
    }

    if (!formData.data_type) {
      newErrors.data_type = t('dpModal2.validationDataType', { ns: 'devices' });
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  // 저장 처리
  const handleSave = async () => {
    if (!validateForm()) return;

    try {
      setIsLoading(true);
      let savedDataPoint: DataPoint;

      if (mode === 'create') {
        const response = await DataPointApiService.createDataPoint(deviceId, formData as DataPointCreateData);
        if (response.success && response.data) {
          savedDataPoint = response.data;
        } else {
          throw new Error(response.error || 'Create failed');
        }
      } else if (mode === 'edit' && dataPoint) {
        const response = await DataPointApiService.updateDataPoint(deviceId, dataPoint.id, formData as DataPointUpdateData);
        if (response.success && response.data) {
          savedDataPoint = response.data;
        } else {
          throw new Error(response.error || 'Update failed');
        }
      } else {
        return;
      }

      onSave(savedDataPoint);
      onClose();
    } catch (error) {
      console.error('데이터포인트 저장 실패:', error);
      alert(t('dpModal2.saveFailed', { ns: 'devices', error: error.message }));
    } finally {
      setIsLoading(false);
    }
  };

  // 삭제 처리
  const handleDelete = async () => {
    if (!dataPoint || !onDelete) return;

    if (confirm(t('dpModal2.deleteConfirmMsg', { ns: 'devices', name: dataPoint.name }))) {
      try {
        setIsLoading(true);
        const response = await DataPointApiService.deleteDataPoint(deviceId, dataPoint.id);
        if (response.success) {
          onDelete(dataPoint.id);
          onClose();
        } else {
          throw new Error(response.error || 'Delete failed');
        }
      } catch (error) {
        console.error('데이터포인트 삭제 실패:', error);
        alert(t('dpModal2.deleteFailed', { ns: 'devices', error: error.message }));
      } finally {
        setIsLoading(false);
      }
    }
  };

  // 읽기 테스트
  const handleTestRead = async () => {
    if (!dataPoint) return;

    try {
      setIsTestingRead(true);
      setTestResult(null);

      const response = await DataPointApiService.testDataPointRead(dataPoint.id);

      if (response.success && response.data) {
        const result = response.data;
        if (result.test_successful) {
          setTestResult(`✅ Read success: ${result.current_value} (response: ${result.response_time_ms}ms)`);
        } else {
          setTestResult(`❌ Read failed: ${result.error_message}`);
        }
      } else {
        setTestResult(`❌ Test failed: ${response.error}`);
      }
    } catch (error) {
      setTestResult(`❌ Test error: ${error.message}`);
    } finally {
      setIsTestingRead(false);
    }
  };

  // 쓰기 테스트
  const handleTestWrite = async () => {
    if (!dataPoint || !testValue) return;

    try {
      setIsTestingWrite(true);
      setTestResult(null);

      const value = parseFloat(testValue) || testValue;
      const response = await DataPointApiService.testDataPointWrite(dataPoint.id, value);

      if (response.success && response.data) {
        const result = response.data;
        if (result.test_successful) {
          setTestResult(`✅ Write success: ${result.written_value} (response: ${result.response_time_ms}ms)`);
        } else {
          setTestResult(`❌ Write failed: ${result.error_message}`);
        }
      } else {
        setTestResult(`❌ Test failed: ${response.error}`);
      }
    } catch (error) {
      setTestResult(`❌ Test error: ${error.message}`);
    } finally {
      setIsTestingWrite(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-container datapoint-modal">
        {/* 헤더 */}
        <div className="modal-header">
          <h2>
            {mode === 'create' ? t('dpModal2.addDataPoint', { ns: 'devices' }) :
              mode === 'edit' ? t('dpModal2.editDataPoint', { ns: 'devices' }) : t('dpModal2.dataPointDetail', { ns: 'devices' })}
          </h2>
          <button className="close-btn" onClick={onClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 내용 */}
        <div className="modal-content">
          <div className="form-grid">
            {/* 기본 정보 */}
            <div className="form-section">
              <h3>{t('dpModal2.basicInfo', { ns: 'devices' })}</h3>

              <div className="form-group">
                <label>{t('dpModal2.pointName', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.name}</div>
                ) : (
                  <>
                    <input
                      type="text"
                      value={formData.name || ''}
                      onChange={(e) => updateField('name', e.target.value)}
                      placeholder="Data point name"
                      className={errors.name ? 'error' : ''}
                    />
                    {errors.name && <span className="error-text">{errors.name}</span>}
                  </>
                )}
              </div>

              <div className="form-group">
                <label>{t('dpTab.description', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.description || 'N/A'}</div>
                ) : (
                  <textarea
                    value={formData.description || ''}
                    onChange={(e) => updateField('description', e.target.value)}
                    placeholder={t('dpModal2.descriptionPlaceholder', { ns: 'devices' })}
                    rows={2}
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('dpModal2.address', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value address">{dataPoint?.address}</div>
                ) : (
                  <>
                    <input
                      type="text"
                      value={formData.address || ''}
                      onChange={(e) => updateField('address', e.target.value)}
                      placeholder="e.g. 40001, 1001, 0x1000"
                      className={errors.address ? 'error' : ''}
                    />
                    {errors.address && <span className="error-text">{errors.address}</span>}
                  </>
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.addressString', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.address_string || 'N/A'}</div>
                ) : (
                  <input
                    type="text"
                    value={formData.address_string || ''}
                    onChange={(e) => updateField('address_string', e.target.value)}
                    placeholder="e.g. 4:40001, MB:1001"
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('dpModal2.dataType', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.data_type}</div>
                ) : (
                  <select
                    value={formData.data_type || 'uint16'}
                    onChange={(e) => updateField('data_type', e.target.value)}
                    className={errors.data_type ? 'error' : ''}
                  >
                    <option value="bool">{t('labels.boolean', { ns: 'devices' })}</option>
                    <option value="uint8">UInt8</option>
                    <option value="int8">Int8</option>
                    <option value="uint16">UInt16</option>
                    <option value="int16">Int16</option>
                    <option value="uint32">UInt32</option>
                    <option value="int32">Int32</option>
                    <option value="uint64">UInt64</option>
                    <option value="int64">Int64</option>
                    <option value="float32">Float32</option>
                    <option value="float64">Float64</option>
                    <option value="string">{t('labels.string', { ns: 'devices' })}</option>
                    <option value="bytes">{t('labels.bytes', { ns: 'devices' })}</option>
                  </select>
                )}
              </div>

              <div className="form-group">
                <label>{t('dpModal2.bitIndex', { ns: 'devices' })} <span style={{ color: '#94a3b8', fontSize: '11px' }}>단일 비트 (0~15)</span></label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.protocol_params?.bit_index ?? 'N/A'}</div>
                ) : (
                  <input
                    type="number"
                    min="0"
                    max="15"
                    value={formData.protocol_params?.bit_index || ''}
                    disabled={!!(formData.protocol_params?.bit_start !== undefined || formData.protocol_params?.bit_end !== undefined)}
                    onChange={(e) => {
                      const value = e.target.value;
                      const newParams = { ...formData.protocol_params };
                      if (value === '') {
                        delete newParams.bit_index;
                      } else {
                        delete newParams.bit_start;
                        delete newParams.bit_end;
                        newParams.bit_index = value;
                      }
                      updateField('protocol_params', newParams);
                    }}
                    placeholder={t('dpModal2.bitIndexPlaceholder', { ns: 'devices' })}
                  />
                )}
              </div>

              <div className="form-group">
                <label>비트 묶음 (bit_start ~ bit_end) <span style={{ color: '#94a3b8', fontSize: '11px' }}>범위를 하나의 정수값으로</span></label>
                {mode === 'view' ? (
                  <div className="form-value">
                    {dataPoint?.protocol_params?.bit_start !== undefined
                      ? `bit${dataPoint.protocol_params.bit_start} ~ bit${dataPoint.protocol_params.bit_end} (0~${(1 << (Number(dataPoint.protocol_params.bit_end) - Number(dataPoint.protocol_params.bit_start) + 1)) - 1})`
                      : 'N/A'}
                  </div>
                ) : (
                  <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                    <input
                      type="number" min="0" max="15"
                      placeholder="시작 (0~15)"
                      disabled={!!(formData.protocol_params?.bit_index)}
                      value={formData.protocol_params?.bit_start ?? ''}
                      onChange={(e) => {
                        const v = e.target.value;
                        const newParams = { ...formData.protocol_params };
                        if (v === '') { delete newParams.bit_start; }
                        else { delete newParams.bit_index; newParams.bit_start = v; }
                        updateField('protocol_params', newParams);
                      }}
                      style={{ width: '50%' }}
                    />
                    <span>~</span>
                    <input
                      type="number" min="0" max="15"
                      placeholder="끝 (0~15)"
                      disabled={!!(formData.protocol_params?.bit_index)}
                      value={formData.protocol_params?.bit_end ?? ''}
                      onChange={(e) => {
                        const v = e.target.value;
                        const newParams = { ...formData.protocol_params };
                        if (v === '') { delete newParams.bit_end; }
                        else { delete newParams.bit_index; newParams.bit_end = v; }
                        updateField('protocol_params', newParams);
                      }}
                      style={{ width: '50%' }}
                    />
                    {formData.protocol_params?.bit_start !== undefined && formData.protocol_params?.bit_end !== undefined && (
                      <span style={{ fontSize: 10, color: '#7c3aed', whiteSpace: 'nowrap' }}>
                        0~{(1 << (Number(formData.protocol_params.bit_end) - Number(formData.protocol_params.bit_start) + 1)) - 1}
                      </span>
                    )}
                  </div>
                )}
              </div>


              <div className="form-group">
                <label>{t('labels.accessMode', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.access_mode || 'read'}</div>
                ) : (
                  <select
                    value={formData.access_mode || 'read'}
                    onChange={(e) => updateField('access_mode', e.target.value)}
                  >
                    <option value="read">{t('labels.readOnly', { ns: 'devices' })}</option>
                    <option value="write">{t('labels.writeOnly', { ns: 'devices' })}</option>
                    <option value="read_write">{t('labels.readwrite', { ns: 'devices' })}</option>
                  </select>
                )}
              </div>

              <div className="form-group">
                <label>{t('dpModal.unit', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.unit || 'N/A'}</div>
                ) : (
                  <input
                    type="text"
                    value={formData.unit || ''}
                    onChange={(e) => updateField('unit', e.target.value)}
                    placeholder="e.g. °C, kWh, m/s"
                  />
                )}
              </div>
            </div>

            {/* 스케일링 및 범위 */}
            <div className="form-section">
              <h3>{t('dpModal2.scalingRange', { ns: 'devices' })}</h3>

              <div className="form-group">
                <label>{t('dataPoint.scalingFactor', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.scaling_factor || 1}</div>
                ) : (
                  <input
                    type="number"
                    step="0.001"
                    value={formData.scaling_factor || 1}
                    onChange={(e) => updateField('scaling_factor', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.scalingOffset', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.scaling_offset || 0}</div>
                ) : (
                  <input
                    type="number"
                    step="0.001"
                    value={formData.scaling_offset || 0}
                    onChange={(e) => updateField('scaling_offset', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.minValue', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.min_value || 0}</div>
                ) : (
                  <input
                    type="number"
                    value={formData.min_value || 0}
                    onChange={(e) => updateField('min_value', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.maxValue', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.max_value || 65535}</div>
                ) : (
                  <input
                    type="number"
                    value={formData.max_value || 65535}
                    onChange={(e) => updateField('max_value', parseFloat(e.target.value))}
                  />
                )}
              </div>
            </div>

            {/* 로깅 및 폴링 설정 */}
            <div className="form-section">
              <h3>{t('dpModal2.loggingPolling', { ns: 'devices' })}</h3>

              <div className="form-group">
                <label>{t('labels.pollingIntervalMs', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.polling_interval_ms || 1000}</div>
                ) : (
                  <input
                    type="number"
                    min="100"
                    max="3600000"
                    value={formData.polling_interval_ms || 1000}
                    onChange={(e) => updateField('polling_interval_ms', parseInt(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.logIntervalMs', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.log_interval_ms || 5000}</div>
                ) : (
                  <input
                    type="number"
                    min="1000"
                    max="3600000"
                    value={formData.log_interval_ms || 5000}
                    onChange={(e) => updateField('log_interval_ms', parseInt(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.deadband', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.log_deadband || 0}</div>
                ) : (
                  <input
                    type="number"
                    step="0.001"
                    min="0"
                    value={formData.log_deadband || 0}
                    onChange={(e) => updateField('log_deadband', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.groupName', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.group_name || 'N/A'}</div>
                ) : (
                  <input
                    type="text"
                    value={formData.group_name || ''}
                    onChange={(e) => updateField('group_name', e.target.value)}
                    placeholder={t('dpModal2.groupPlaceholder', { ns: 'devices' })}
                  />
                )}
              </div>
            </div>

            {/* 상태 및 옵션 */}
            <div className="form-section">
              <h3>{t('dpModal2.statusOptions', { ns: 'devices' })}</h3>

              <div className="form-group">
                <label>{t('status.enabled', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">
                    <span className={`status-badge ${dataPoint?.is_enabled ? 'enabled' : 'disabled'}`}>
                      {dataPoint?.is_enabled ? t('dpModal2.enabled', { ns: 'devices' }) : t('dpModal2.disabled', { ns: 'devices' })}
                    </span>
                  </div>
                ) : (
                  <label className="switch">
                    <input
                      type="checkbox"
                      checked={formData.is_enabled || false}
                      onChange={(e) => updateField('is_enabled', e.target.checked)}
                    />
                    <span className="slider"></span>
                  </label>
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.writable', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">
                    <span className={`status-badge ${dataPoint?.is_writable ? 'enabled' : 'disabled'}`}>
                      {dataPoint?.is_writable ? t('dpModal2.yes', { ns: 'devices' }) : t('dpModal2.no', { ns: 'devices' })}
                    </span>
                  </div>
                ) : (
                  <label className="switch">
                    <input
                      type="checkbox"
                      checked={formData.is_writable || false}
                      onChange={(e) => updateField('is_writable', e.target.checked)}
                    />
                    <span className="slider"></span>
                  </label>
                )}
              </div>

              <div className="form-group">
                <label>{t('labels.loggingEnabled', { ns: 'devices' })}</label>
                {mode === 'view' ? (
                  <div className="form-value">
                    <span className={`status-badge ${dataPoint?.is_log_enabled ? 'enabled' : 'disabled'}`}>
                      {dataPoint?.is_log_enabled ? t('dpModal2.enabled', { ns: 'devices' }) : t('dpModal2.disabled', { ns: 'devices' })}
                    </span>
                  </div>
                ) : (
                  <label className="switch">
                    <input
                      type="checkbox"
                      checked={formData.is_log_enabled !== false}
                      onChange={(e) => updateField('is_log_enabled', e.target.checked)}
                    />
                    <span className="slider"></span>
                  </label>
                )}
              </div>
            </div>

            {/* 현재값 및 테스트 (view/edit 모드에서만) */}
            {mode !== 'create' && dataPoint && (
              <div className="form-section">
                <h3>{t('dpModal2.currentValueTest', { ns: 'devices' })}</h3>

                <div className="form-group">
                  <label>{t('labels.currentValue', { ns: 'devices' })}</label>
                  <div className="form-value current-value">
                    {dataPoint.current_value !== null && dataPoint.current_value !== undefined
                      ? (typeof dataPoint.current_value === 'object' ? dataPoint.current_value.value : dataPoint.current_value)
                      : 'N/A'}
                  </div>
                </div>

                <div className="form-group">
                  <label>{t('labels.quality', { ns: 'devices' })}</label>
                  <div className="form-value">
                    <span className={`quality-badge quality-${dataPoint.quality?.toLowerCase() || 'unknown'}`}>
                      {dataPoint.quality || 'Unknown'}
                    </span>
                  </div>
                </div>

                <div className="form-group">
                  <label>{t('labels.lastRead', { ns: 'devices' })}</label>
                  <div className="form-value">
                    {dataPoint.last_update ? new Date(dataPoint.last_update).toLocaleString() : 'N/A'}
                  </div>
                </div>

                {/* 테스트 버튼들 */}
                <div className="test-actions">
                  <button
                    type="button"
                    className="btn btn-info btn-sm"
                    onClick={handleTestRead}
                    disabled={isTestingRead}
                  >
                    {isTestingRead ? (
                      <>
                        <i className="fas fa-spinner fa-spin"></i>
                        {t('dpModal2.readTestInProgress', { ns: 'devices' })}
                      </>
                    ) : (
                      <>
                        <i className="fas fa-download"></i>
                        {t('dpModal2.readTest', { ns: 'devices' })}
                      </>
                    )}
                  </button>

                  {(formData.is_writable || dataPoint.is_writable) && (
                    <div className="write-test">
                      <div className="input-group">
                        <input
                          type="text"
                          value={testValue}
                          onChange={(e) => setTestValue(e.target.value)}
                          placeholder={t('dpModal2.writeTestPlaceholder', { ns: 'devices' })}
                          className="test-input"
                        />
                        <button
                          type="button"
                          className="btn btn-warning btn-sm"
                          onClick={handleTestWrite}
                          disabled={isTestingWrite || !testValue}
                        >
                          {isTestingWrite ? (
                            <>
                              <i className="fas fa-spinner fa-spin"></i>
                              {t('dpModal2.writeTestInProgress', { ns: 'devices' })}
                            </>
                          ) : (
                            <>
                              <i className="fas fa-upload"></i>
                              {t('dpModal2.writeTest', { ns: 'devices' })}
                            </>
                          )}
                        </button>
                      </div>
                    </div>
                  )}
                </div>

                {/* 테스트 결과 */}
                {testResult && (
                  <div className="test-result">
                    {testResult}
                  </div>
                )}
              </div>
            )}
          </div>
        </div>

        {/* 푸터 */}
        <div className="modal-footer">
          <div className="footer-left">
            {mode === 'edit' && onDelete && (
              <button
                type="button"
                className="btn btn-error"
                onClick={handleDelete}
                disabled={isLoading}
              >
                <i className="fas fa-trash"></i>
                {t('modal.delete', { ns: 'devices' })}
              </button>
            )}
          </div>
          <div className="footer-right">
            <button type="button" className="btn btn-secondary" onClick={onClose}>
              {t('modal.cancel', { ns: 'devices' })}
            </button>
            {mode !== 'view' && (
              <button
                type="button"
                className="btn btn-primary"
                onClick={handleSave}
                disabled={isLoading}
              >
                {isLoading ? (
                  <>
                    <i className="fas fa-spinner fa-spin"></i>
                    {t('dpModal2.saving', { ns: 'devices' })}
                  </>
                ) : (
                  <>
                    <i className="fas fa-save"></i>
                    {mode === 'create' ? t('dpModal2.create', { ns: 'devices' }) : t('dpModal2.save', { ns: 'devices' })}
                  </>
                )}
              </button>
            )}
          </div>
        </div>

        <style>{`
          .datapoint-modal {
            max-width: 800px;
            width: 90vw;
            max-height: 90vh;
          }

          .form-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 2rem;
            padding: 1rem;
          }

          .form-section {
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 1.5rem;
          }

          .form-section h3 {
            margin: 0 0 1rem 0;
            font-size: 1rem;
            font-weight: 600;
            color: #1f2937;
          }

          .form-group {
            margin-bottom: 1rem;
          }

          .form-group label {
            display: block;
            font-size: 0.875rem;
            font-weight: 500;
            color: #374151;
            margin-bottom: 0.5rem;
          }

          .form-group input,
          .form-group select,
          .form-group textarea {
            width: 100%;
            padding: 0.5rem;
            border: 1px solid #d1d5db;
            border-radius: 4px;
            font-size: 0.875rem;
          }

          .form-group input.error,
          .form-group select.error {
            border-color: #dc2626;
          }

          .form-value {
            padding: 0.5rem;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 4px;
            font-size: 0.875rem;
            color: #374151;
          }

          .form-value.address {
            font-family: 'Courier New', monospace;
            background: #f0f9ff;
            border-color: #0ea5e9;
            color: #0c4a6e;
          }

          .form-value.current-value {
            font-weight: 600;
            color: #059669;
          }

          .error-text {
            color: #dc2626;
            font-size: 0.75rem;
            margin-top: 0.25rem;
            display: block;
          }

          .status-badge {
            display: inline-flex;
            align-items: center;
            gap: 0.25rem;
            padding: 0.25rem 0.75rem;
            border-radius: 9999px;
            font-size: 0.75rem;
            font-weight: 500;
          }

          .status-badge.enabled {
            background: #dcfce7;
            color: #166534;
          }

          .status-badge.disabled {
            background: #fee2e2;
            color: #991b1b;
          }

          .quality-badge {
            padding: 0.25rem 0.5rem;
            border-radius: 4px;
            font-size: 0.75rem;
            font-weight: 500;
          }

          .quality-good {
            background: #dcfce7;
            color: #166534;
          }

          .quality-bad {
            background: #fee2e2;
            color: #991b1b;
          }

          .quality-uncertain {
            background: #fef3c7;
            color: #92400e;
          }

          .quality-unknown {
            background: #f3f4f6;
            color: #6b7280;
          }

          .switch {
            position: relative;
            display: inline-block;
            width: 3rem;
            height: 1.5rem;
          }

          .switch input {
            opacity: 0;
            width: 0;
            height: 0;
          }

          .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: #cbd5e1;
            transition: 0.2s;
            border-radius: 1.5rem;
          }

          .slider:before {
            position: absolute;
            content: "";
            height: 1.125rem;
            width: 1.125rem;
            left: 0.1875rem;
            bottom: 0.1875rem;
            background: white;
            transition: 0.2s;
            border-radius: 50%;
          }

          input:checked + .slider {
            background: #0ea5e9;
          }

          input:checked + .slider:before {
            transform: translateX(1.5rem);
          }

          .test-actions {
            margin-top: 1rem;
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
          }

          .write-test {
            margin-top: 0.5rem;
          }

          .input-group {
            display: flex;
            gap: 0.5rem;
          }

          .test-input {
            flex: 1;
          }

          .test-result {
            margin-top: 0.5rem;
            padding: 0.5rem;
            border-radius: 4px;
            font-size: 0.875rem;
            font-family: 'Courier New', monospace;
            background: #f0f9ff;
            border: 1px solid #0ea5e9;
            color: #0c4a6e;
          }

          .btn {
            display: inline-flex;
            align-items: center;
            gap: 0.5rem;
            padding: 0.5rem 1rem;
            border: none;
            border-radius: 4px;
            font-size: 0.875rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s ease;
          }

          .btn-sm {
            padding: 0.375rem 0.75rem;
            font-size: 0.75rem;
          }

          .btn-primary {
            background: #0ea5e9;
            color: white;
          }

          .btn-secondary {
            background: #64748b;
            color: white;
          }

          .btn-info {
            background: #0891b2;
            color: white;
          }

          .btn-warning {
            background: #f59e0b;
            color: white;
          }

          .btn-error {
            background: #dc2626;
            color: white;
          }

          .btn:hover:not(:disabled) {
            opacity: 0.9;
          }

          .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
          }
        `}</style>
      </div>
    </div>
  );
};

export default DataPointModal;