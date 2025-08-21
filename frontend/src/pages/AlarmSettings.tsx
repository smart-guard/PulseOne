import React, { useState, useEffect } from 'react';
import { AlarmApiService, AlarmRule, AlarmRuleCreateData } from '../api/services/alarmApi';
import AlarmSettingsModal from './modals/AlarmSettingsModal';
import '../styles/alarm-settings.css';

interface AlarmSettingsProps {}

// 데이터포인트 목록을 위한 타입
interface DataPoint {
  id: number;
  name: string;
  device_id: number;
  device_name: string;
  data_type: string;
  unit?: string;
  address?: string;
}

// 디바이스 목록을 위한 타입
interface Device {
  id: number;
  name: string;
  device_type: string;
  site_name?: string;
}

const AlarmSettings: React.FC<AlarmSettingsProps> = () => {
  // 상태 관리
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 데이터포인트 및 디바이스 목록
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [devices, setDevices] = useState<Device[]>([]);
  const [loadingTargetData, setLoadingTargetData] = useState(false);
  
  // 모달 상태
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  
  // 뷰 타입 상태
  const [viewType, setViewType] = useState<'card' | 'table'>('card');
  
  // 필터 상태
  const [filters, setFilters] = useState({
    search: '',
    severity: 'all',
    status: 'all',
    category: 'all',
    tag: ''
  });

  // 폼 상태 
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    target_type: 'data_point' as const,
    target_id: '',
    selected_device_id: '', // 디바이스 선택을 위한 추가 필드
    target_group: '',
    alarm_type: 'analog' as const,
    
    // 임계값들
    high_high_limit: '',
    high_limit: '',
    low_limit: '',
    low_low_limit: '',
    deadband: '2.0',
    rate_of_change: '',
    
    // 기타 필드들
    trigger_condition: '',
    condition_script: '',
    message_script: '',
    message_config: {},
    message_template: '',
    severity: 'medium' as const,
    priority: 100,
    auto_acknowledge: false,
    acknowledge_timeout_min: 0,
    auto_clear: true,
    suppression_rules: {},
    notification_enabled: true,
    notification_delay_sec: 0,
    notification_repeat_interval_min: 0,
    notification_channels: {},
    notification_recipients: {},
    is_enabled: true,
    is_latched: false,
    template_id: '',
    rule_group: '',
    created_by_template: false,
    last_template_update: '',
    escalation_enabled: false,
    escalation_max_level: 3,
    escalation_rules: {},
    category: '',
    tags: [] as string[]
  });

  const getDeviceOptions = () => {
    if (!Array.isArray(devices)) {
      return [];
    }
    
    return devices
      .sort((a, b) => a.id - b.id) // ID 순으로 정렬
      .map(device => ({
        value: device.id.toString(),
        label: `[${device.id}] ${device.name} (${device.device_type || 'Unknown'})`,
        extra: device.site_name ? ` - ${device.site_name}` : ''
      }));
  };

  // 선택된 디바이스에 속한 데이터포인트만 필터링 및 정렬
  const getFilteredDataPoints = () => {
    if (!formData.selected_device_id || !Array.isArray(dataPoints)) {
      return [];
    }
    
    const deviceId = parseInt(formData.selected_device_id);
    return dataPoints
      .filter(dp => dp.device_id === deviceId)
      .sort((a, b) => a.id - b.id); // ID 순으로 정렬
  };

  // 선택된 디바이스의 데이터포인트 옵션 생성
  const getDataPointOptions = () => {
    const filteredPoints = getFilteredDataPoints();
    
    return filteredPoints.map(dp => ({
      value: dp.id.toString(),
      label: `[${dp.id}] ${dp.name}`,
      extra: dp.unit ? ` (${dp.unit})` : (dp.data_type ? ` (${dp.data_type})` : ''),
      address: dp.address ? ` - ${dp.address}` : ''
    }));
  };

  // 디바이스 선택 변경 시 데이터포인트 초기화
  const handleDeviceChange = (deviceId: string) => {
    setFormData(prev => ({
      ...prev,
      selected_device_id: deviceId,
      target_id: '' // 디바이스 변경 시 선택된 데이터포인트 초기화
    }));
  };

  // 타겟 타입 변경 시 선택 초기화
  const handleTargetTypeChange = (targetType: string) => {
    setFormData(prev => ({
      ...prev,
      target_type: targetType as any,
      selected_device_id: '',
      target_id: ''
    }));
  };

  // 선택된 디바이스 이름 가져오기
  const getSelectedDeviceName = () => {
    if (!formData.selected_device_id) return '';
    const device = devices.find(d => d.id === parseInt(formData.selected_device_id));
    return device ? device.name : '';
  };


  const TargetSelectionUI = () => {
    return (
      <div className="form-section">
        <div className="section-title">타겟 설정</div>
        
        <div className="form-row">
          <div className="form-group">
            <label className="form-label">타겟 타입 *</label>
            <select
              className="form-select"
              value={formData.target_type}
              onChange={(e) => handleTargetTypeChange(e.target.value)}
            >
              <option value="data_point">데이터포인트</option>
              <option value="device">디바이스</option>
              <option value="virtual_point">가상포인트</option>
            </select>
          </div>
        </div>

        {/* 데이터포인트 선택 시 계층적 선택 */}
        {formData.target_type === 'data_point' && (
          <>
            <div className="form-row">
              <div className="form-group">
                <label className="form-label">디바이스 선택 *</label>
                {loadingTargetData ? (
                  <div className="form-input loading-input">
                    <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                  </div>
                ) : (
                  <select
                    className="form-select"
                    value={formData.selected_device_id}
                    onChange={(e) => handleDeviceChange(e.target.value)}
                  >
                    <option value="">디바이스를 선택하세요</option>
                    {getDeviceOptions().map(option => (
                      <option key={option.value} value={option.value}>
                        {option.label}{option.extra}
                      </option>
                    ))}
                  </select>
                )}
                <small className="form-help">
                  [ID] 디바이스명 (타입) 형식으로 표시됩니다
                </small>
              </div>
            </div>

            {formData.selected_device_id && (
              <div className="form-row">
                <div className="form-group">
                  <label className="form-label">데이터포인트 선택 *</label>
                  <select
                    className="form-select"
                    value={formData.target_id}
                    onChange={(e) => setFormData(prev => ({ ...prev, target_id: e.target.value }))}
                  >
                    <option value="">데이터포인트를 선택하세요</option>
                    {getDataPointOptions().map(option => (
                      <option key={option.value} value={option.value}>
                        {option.label}{option.extra}{option.address}
                      </option>
                    ))}
                  </select>
                  <small className="form-help">
                    {getSelectedDeviceName()}의 데이터포인트들 (ID 순 정렬)
                  </small>
                </div>
              </div>
            )}
          </>
        )}

        {/* 디바이스 직접 선택 시 */}
        {formData.target_type === 'device' && (
          <div className="form-row">
            <div className="form-group">
              <label className="form-label">디바이스 선택 *</label>
              {loadingTargetData ? (
                <div className="form-input loading-input">
                  <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                </div>
              ) : (
                <select
                  className="form-select"
                  value={formData.target_id}
                  onChange={(e) => setFormData(prev => ({ ...prev, target_id: e.target.value }))}
                >
                  <option value="">디바이스를 선택하세요</option>
                  {getDeviceOptions().map(option => (
                    <option key={option.value} value={option.value}>
                      {option.label}{option.extra}
                    </option>
                  ))}
                </select>
              )}
              <small className="form-help">
                [ID] 디바이스명 (타입) 형식으로 표시됩니다
              </small>
            </div>
          </div>
        )}

        {/* 가상포인트 선택 시 */}
        {formData.target_type === 'virtual_point' && (
          <div className="form-row">
            <div className="form-group">
              <label className="form-label">가상포인트 선택 *</label>
              <div className="form-input disabled-input">
                가상포인트 API 구현 필요
              </div>
              <small className="form-help">
                가상포인트 관리 기능은 추후 구현 예정입니다
              </small>
            </div>
          </div>
        )}

        <div className="form-row">
          <div className="form-group">
            <label className="form-label">타겟 그룹</label>
            <input
              type="text"
              className="form-input"
              value={formData.target_group}
              onChange={(e) => setFormData(prev => ({ ...prev, target_group: e.target.value }))}
              placeholder="타겟 그룹 (선택사항)"
            />
            <small className="form-help">
              관련 타겟들을 그룹으로 묶어 관리할 수 있습니다
            </small>
          </div>
        </div>
      </div>
    );
  };
  // 초기 데이터 로딩
  useEffect(() => {
    loadAlarmRules();
    loadTargetData();
  }, []);

  // 타겟 타입 변경 시 타겟 ID 초기화
  useEffect(() => {
    setFormData(prev => ({ ...prev, target_id: '' }));
  }, [formData.target_type]);



  // 디버깅을 위한 상태 로깅 개선
  useEffect(() => {
    console.log('Current dataPoints:', dataPoints, 'Length:', dataPoints.length);
    console.log('Current devices:', devices, 'Length:', devices.length);
    console.log('Loading target data:', loadingTargetData);
    console.log('Target options for', formData.target_type, ':', getTargetOptions());
  }, [dataPoints, devices, loadingTargetData, formData.target_type]);


  const loadAlarmRules = async () => {
    try {
      setLoading(true);
      setError(null);
      
      const response = await AlarmApiService.getAlarmRules({
        page: 1,
        limit: 100,
        ...filters
      });
      
      if (response.success && response.data) {
        const rules = response.data.items || [];
        setAlarmRules(rules);
        console.log('로드된 알람 규칙들:', rules);
      } else {
        setError(response.message || '알람 규칙을 불러오는데 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 로딩 실패:', error);
      setError(error instanceof Error ? error.message : '데이터 로딩에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 타겟 데이터 로딩 (데이터포인트, 디바이스 목록)
  // API 응답 구조에 맞는 데이터 처리 수정

  const loadTargetData = async () => {
    try {
      setLoadingTargetData(true);
      
      const [dataPointsResponse, devicesResponse] = await Promise.all([
        fetch('/api/data/points'),
        fetch('/api/devices')
      ]);
      
      if (dataPointsResponse.ok) {
        const dataPointsData = await dataPointsResponse.json();
        console.log('로드된 데이터포인트:', dataPointsData);
        
        // API 응답 구조에 맞게 items 배열 추출
        if (dataPointsData.success && dataPointsData.data && Array.isArray(dataPointsData.data.items)) {
          setDataPoints(dataPointsData.data.items);
        } else if (dataPointsData.items && Array.isArray(dataPointsData.items)) {
          setDataPoints(dataPointsData.items);
        } else {
          console.warn('예상치 못한 데이터포인트 응답 구조:', dataPointsData);
          setDataPoints([]);
        }
      } else {
        console.error('데이터포인트 로딩 실패:', dataPointsResponse.status);
        setDataPoints([]);
      }
      
      if (devicesResponse.ok) {
        const devicesData = await devicesResponse.json();
        console.log('로드된 디바이스:', devicesData);
        
        // API 응답 구조에 맞게 items 배열 추출
        if (devicesData.success && devicesData.data && Array.isArray(devicesData.data.items)) {
          setDevices(devicesData.data.items);
        } else if (devicesData.items && Array.isArray(devicesData.items)) {
          setDevices(devicesData.items);
        } else {
          console.warn('예상치 못한 디바이스 응답 구조:', devicesData);
          setDevices([]);
        }
      } else {
        console.error('디바이스 로딩 실패:', devicesResponse.status);
        setDevices([]);
      }
    } catch (error) {
      console.error('타겟 데이터 로딩 실패:', error);
      setDataPoints([]);
      setDevices([]);
    } finally {
      setLoadingTargetData(false);
    }
  };

  // 디바이스 이름을 매핑하는 함수 추가
  const getDeviceName = (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    return device ? device.name : `Device #${deviceId}`;
  };

  // 새 알람 규칙 생성
  const handleCreateRule = async () => {
    try {
      setLoading(true);
      
      const createData: AlarmRuleCreateData = {
        name: formData.name,
        description: formData.description,
        target_type: formData.target_type,
        target_id: formData.target_id ? parseInt(formData.target_id) : undefined,
        target_group: formData.target_group || undefined,
        alarm_type: formData.alarm_type,
        
        // 개별 임계값들
        high_high_limit: formData.high_high_limit ? parseFloat(formData.high_high_limit) : undefined,
        high_limit: formData.high_limit ? parseFloat(formData.high_limit) : undefined,
        low_limit: formData.low_limit ? parseFloat(formData.low_limit) : undefined,
        low_low_limit: formData.low_low_limit ? parseFloat(formData.low_low_limit) : undefined,
        deadband: formData.deadband ? parseFloat(formData.deadband) : undefined,
        rate_of_change: formData.rate_of_change ? parseFloat(formData.rate_of_change) : undefined,
        
        // 디지털/스크립트 조건
        trigger_condition: formData.trigger_condition || undefined,
        condition_script: formData.condition_script || undefined,
        message_script: formData.message_script || undefined,
        
        // 메시지 관련
        message_config: formData.message_config,
        message_template: formData.message_template || `${formData.name} 알람이 발생했습니다`,
        
        severity: formData.severity,
        priority: formData.priority,
        
        // 동작 설정
        auto_acknowledge: formData.auto_acknowledge,
        acknowledge_timeout_min: formData.acknowledge_timeout_min,
        auto_clear: formData.auto_clear,
        suppression_rules: formData.suppression_rules,
        
        // 알림 설정
        notification_enabled: formData.notification_enabled,
        notification_delay_sec: formData.notification_delay_sec,
        notification_repeat_interval_min: formData.notification_repeat_interval_min,
        notification_channels: formData.notification_channels,
        notification_recipients: formData.notification_recipients,
        
        // 상태
        is_enabled: formData.is_enabled,
        is_latched: formData.is_latched,
        
        // 템플릿 관련
        template_id: formData.template_id ? parseInt(formData.template_id) : undefined,
        rule_group: formData.rule_group || undefined,
        created_by_template: formData.created_by_template,
        last_template_update: formData.last_template_update || undefined,
        
        // 에스컬레이션 관련
        escalation_enabled: formData.escalation_enabled,
        escalation_max_level: formData.escalation_max_level,
        escalation_rules: formData.escalation_rules,
        
        // 카테고리 및 태그
        category: formData.category || undefined,
        tags: formData.tags.length > 0 ? formData.tags : undefined
      };
      
      console.log('생성할 알람 규칙 데이터:', createData);
      
      const response = await AlarmApiService.createAlarmRule(createData);
      
      if (response.success) {
        await loadAlarmRules();
        setShowCreateModal(false);
        resetForm();
        alert('알람 규칙이 성공적으로 생성되었습니다.');
      } else {
        throw new Error(response.message || '알람 규칙 생성에 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 생성 실패:', error);
      alert(error instanceof Error ? error.message : '알람 규칙 생성에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 알람 규칙 삭제
  const handleDeleteRule = async (ruleId: number) => {
    if (!confirm('정말로 이 알람 규칙을 삭제하시겠습니까?')) {
      return;
    }

    try {
      setLoading(true);
      
      const response = await AlarmApiService.deleteAlarmRule(ruleId);
      
      if (response.success) {
        await loadAlarmRules();
        alert('알람 규칙이 삭제되었습니다.');
      } else {
        throw new Error(response.message || '알람 규칙 삭제에 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 삭제 실패:', error);
      alert(error instanceof Error ? error.message : '알람 규칙 삭제에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 알람 규칙 활성화/비활성화 토글 (DB 업데이트 포함)
  const handleToggleRule = async (ruleId: number, currentStatus: boolean) => {
    try {
      setLoading(true);
      
      const response = await AlarmApiService.updateAlarmRule(ruleId, {
        is_enabled: !currentStatus
      });
      
      if (response.success) {
        await loadAlarmRules();
      } else {
        throw new Error(response.message || '상태 변경에 실패했습니다.');
      }
    } catch (error) {
      console.error('상태 변경 실패:', error);
      alert(error instanceof Error ? error.message : '상태 변경에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 폼 초기화
  const resetForm = () => {
    setFormData({
      name: '',
      description: '',
      target_type: 'data_point',
      target_id: '',
      target_group: '',
      alarm_type: 'analog',
      
      // 임계값들
      high_high_limit: '',
      high_limit: '',
      low_limit: '',
      low_low_limit: '',
      deadband: '2.0',
      rate_of_change: '',
      
      // 디지털/스크립트 조건
      trigger_condition: '',
      condition_script: '',
      message_script: '',
      
      // 메시지 관련
      message_config: {},
      message_template: '',
      
      severity: 'medium',
      priority: 100,
      
      // 동작 설정
      auto_acknowledge: false,
      acknowledge_timeout_min: 0,
      auto_clear: true,
      suppression_rules: {},
      
      // 알림 설정
      notification_enabled: true,
      notification_delay_sec: 0,
      notification_repeat_interval_min: 0,
      notification_channels: {},
      notification_recipients: {},
      
      // 상태
      is_enabled: true,
      is_latched: false,
      
      // 템플릿 관련
      template_id: '',
      rule_group: '',
      created_by_template: false,
      last_template_update: '',
      
      // 에스컬레이션 관련
      escalation_enabled: false,
      escalation_max_level: 3,
      escalation_rules: {},
      
      // 카테고리 및 태그
      category: '',
      tags: []
    });
  };

  // 알람 규칙 편집을 위한 폼 데이터 설정
  const handleEditRule = (rule: AlarmRule) => {
    setFormData({
      name: rule.name,
      description: rule.description || '',
      target_type: rule.target_type,
      target_id: rule.target_id?.toString() || '',
      target_group: rule.target_group || '',
      alarm_type: rule.alarm_type,
      
      // 임계값들 (개별 필드에서 가져오기)
      high_high_limit: rule.high_high_limit?.toString() || '',
      high_limit: rule.high_limit?.toString() || '',
      low_limit: rule.low_limit?.toString() || '',
      low_low_limit: rule.low_low_limit?.toString() || '',
      deadband: rule.deadband?.toString() || '2.0',
      rate_of_change: rule.rate_of_change?.toString() || '',
      
      // 디지털/스크립트 조건
      trigger_condition: rule.trigger_condition || '',
      condition_script: rule.condition_script || '',
      message_script: rule.message_script || '',
      
      // 메시지 관련
      message_config: rule.message_config || {},
      message_template: rule.message_template || '',
      
      severity: rule.severity as any,
      priority: rule.priority || 100,
      
      // 동작 설정
      auto_acknowledge: rule.auto_acknowledge || false,
      acknowledge_timeout_min: rule.acknowledge_timeout_min || 0,
      auto_clear: rule.auto_clear || false,
      suppression_rules: rule.suppression_rules || {},
      
      // 알림 설정
      notification_enabled: rule.notification_enabled || false,
      notification_delay_sec: rule.notification_delay_sec || 0,
      notification_repeat_interval_min: rule.notification_repeat_interval_min || 0,
      notification_channels: rule.notification_channels || {},
      notification_recipients: rule.notification_recipients || {},
      
      // 상태
      is_enabled: rule.is_enabled,
      is_latched: rule.is_latched || false,
      
      // 템플릿 관련
      template_id: rule.template_id?.toString() || '',
      rule_group: rule.rule_group || '',
      created_by_template: rule.created_by_template || false,
      last_template_update: rule.last_template_update || '',
      
      // 에스컬레이션 관련
      escalation_enabled: rule.escalation_enabled || false,
      escalation_max_level: rule.escalation_max_level || 3,
      escalation_rules: rule.escalation_rules || {},
      
      // 카테고리 및 태그
      category: rule.category || '',
      tags: AlarmApiService.formatTags(rule.tags || [])
    });
    setSelectedRule(rule);
    setShowEditModal(true);
  };

  // 알람 규칙 수정
  const handleUpdateRule = async () => {
    if (!selectedRule) return;

    try {
      setLoading(true);
      
      const updateData = {
        name: formData.name,
        description: formData.description,
        target_type: formData.target_type,
        target_id: formData.target_id ? parseInt(formData.target_id) : undefined,
        target_group: formData.target_group || undefined,
        alarm_type: formData.alarm_type,
        
        // 개별 임계값들
        high_high_limit: formData.high_high_limit ? parseFloat(formData.high_high_limit) : undefined,
        high_limit: formData.high_limit ? parseFloat(formData.high_limit) : undefined,
        low_limit: formData.low_limit ? parseFloat(formData.low_limit) : undefined,
        low_low_limit: formData.low_low_limit ? parseFloat(formData.low_low_limit) : undefined,
        deadband: formData.deadband ? parseFloat(formData.deadband) : undefined,
        rate_of_change: formData.rate_of_change ? parseFloat(formData.rate_of_change) : undefined,
        
        // 메시지 관련
        message_template: formData.message_template,
        
        severity: formData.severity,
        priority: formData.priority,
        
        // 동작 설정
        auto_acknowledge: formData.auto_acknowledge,
        auto_clear: formData.auto_clear,
        
        // 알림 설정
        notification_enabled: formData.notification_enabled,
        
        // 상태
        is_enabled: formData.is_enabled,
        
        // 카테고리 및 태그
        category: formData.category || undefined,
        tags: formData.tags.length > 0 ? formData.tags : undefined
      };
      
      const response = await AlarmApiService.updateAlarmRule(selectedRule.id, updateData);
      
      if (response.success) {
        await loadAlarmRules();
        setShowEditModal(false);
        setSelectedRule(null);
        resetForm();
        alert('알람 규칙이 성공적으로 수정되었습니다.');
      } else {
        throw new Error(response.message || '알람 규칙 수정에 실패했습니다.');
      }
    } catch (error) {
      console.error('알람 규칙 수정 실패:', error);
      alert(error instanceof Error ? error.message : '알람 규칙 수정에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  // 필터링된 알람 규칙
  const filteredRules = alarmRules.filter(rule => {
    if (filters.search && !rule.name.toLowerCase().includes(filters.search.toLowerCase())) {
      return false;
    }
    if (filters.severity !== 'all' && rule.severity !== filters.severity) {
      return false;
    }
    if (filters.status === 'enabled' && !rule.is_enabled) {
      return false;
    }
    if (filters.status === 'disabled' && rule.is_enabled) {
      return false;
    }
    if (filters.category !== 'all' && rule.category !== filters.category) {
      return false;
    }
    if (filters.tag && (!rule.tags || !rule.tags.some(tag => 
      String(tag).toLowerCase().includes(filters.tag.toLowerCase())
    ))) {
      return false;
    }
    return true;
  });

  // 타겟 표시 함수 (개선됨)
  const getTargetDisplay = (rule: AlarmRule) => {
    if (rule.target_display) {
      return rule.target_display;
    }
    
    switch (rule.target_type) {
      case 'device':
        return rule.device_name || `디바이스 #${rule.target_id}`;
      case 'data_point':
        return rule.data_point_name || `데이터포인트 #${rule.target_id}`;
      case 'virtual_point':
        return rule.virtual_point_name || `가상포인트 #${rule.target_id}`;
      default:
        return rule.target_id ? `${rule.target_type} #${rule.target_id}` : 'N/A';
    }
  };

  // 타겟 ID 표시를 위한 함수 (개선됨)
  const getTargetIdDisplay = (targetType: string, targetId: string) => {
    if (!targetId) return '';
    
    const id = parseInt(targetId);
    
    switch (targetType) {
      case 'data_point':
        const dataPoint = dataPoints.find(dp => dp.id === id);
        if (dataPoint) {
          const device = devices.find(d => d.id === dataPoint.device_id);
          const deviceName = device ? device.name : `Device #${dataPoint.device_id}`;
          return `[${id}] ${dataPoint.name} (${deviceName})`;
        }
        return `[${id}] 데이터포인트`;
      
      case 'device':
        const device = devices.find(dev => dev.id === id);
        if (device) {
          return `[${id}] ${device.name} (${device.device_type || 'Unknown'})`;
        }
        return `[${id}] 디바이스`;
      
      case 'virtual_point':
        return `[${id}] 가상포인트`;
      
      default:
        return targetId;
    }
  };


  // 조건 표시 함수 (기존 유지)
  const getConditionDisplay = (rule: AlarmRule) => {
    if (rule.condition_display) {
      return rule.condition_display;
    }
    
    const parts = [];
    
    if (rule.high_high_limit !== null && rule.high_high_limit !== undefined) {
      parts.push(`HH: ${rule.high_high_limit}`);
    }
    if (rule.high_limit !== null && rule.high_limit !== undefined) {
      parts.push(`H: ${rule.high_limit}`);
    }
    if (rule.low_limit !== null && rule.low_limit !== undefined) {
      parts.push(`L: ${rule.low_limit}`);
    }
    if (rule.low_low_limit !== null && rule.low_low_limit !== undefined) {
      parts.push(`LL: ${rule.low_low_limit}`);
    }
    
    return parts.length > 0 ? parts.join(' | ') : rule.alarm_type;
  };

  // 단위 추론 함수
  const getUnit = (rule: AlarmRule) => {
    if (rule.unit) return rule.unit;
    
    if (rule.name.toLowerCase().includes('temp') || rule.name.toLowerCase().includes('온도')) {
      return '°C';
    }
    if (rule.name.toLowerCase().includes('current') || rule.name.toLowerCase().includes('전류')) {
      return 'A';
    }
    if (rule.name.toLowerCase().includes('speed') || rule.name.toLowerCase().includes('속도')) {
      return ' m/min';
    }
    if (rule.name.toLowerCase().includes('count') || rule.name.toLowerCase().includes('개수')) {
      return ' pcs';
    }
    return '';
  };

  // 카테고리 표시명
  const getCategoryDisplayName = (category?: string) => {
    if (!category) return '';
    return AlarmApiService.getCategoryDisplayName(category);
  };

  // 태그 추가/제거 함수
  const addTag = (tag: string) => {
    if (tag.trim() && !formData.tags.includes(tag.trim())) {
      setFormData(prev => ({
        ...prev,
        tags: [...prev.tags, tag.trim()]
      }));
    }
  };

  const removeTag = (tagToRemove: string) => {
    setFormData(prev => ({
      ...prev,
      tags: prev.tags.filter(tag => tag !== tagToRemove)
    }));
  };

  // 타겟 옵션 생성
  const getTargetOptions = () => {
    switch (formData.target_type) {
      case 'data_point':
        if (!Array.isArray(dataPoints)) {
          console.warn('dataPoints is not an array:', dataPoints);
          return [];
        }
        return dataPoints.map(dp => ({
          value: dp.id.toString(),
          label: `${dp.id}: ${dp.name} (${getDeviceName(dp.device_id)})`,
          extra: dp.unit ? ` - ${dp.unit}` : (dp.data_type ? ` - ${dp.data_type}` : '')
        }));
      
      case 'device':
        if (!Array.isArray(devices)) {
          console.warn('devices is not an array:', devices);
          return [];
        }
        return devices.map(dev => ({
          value: dev.id.toString(),
          label: `${dev.id}: ${dev.name} (${dev.device_type || 'Unknown Type'})`,
          extra: dev.site_name ? ` - ${dev.site_name}` : ''
        }));
      
      case 'virtual_point':
        return [{
          value: '',
          label: '가상포인트 API 구현 필요',
          extra: ''
        }];
      
      default:
        return [];
    }
  };

  return (
    <div className="alarm-settings-container">
      {/* 헤더 */}
      <div className="header">
        <h1>
          <i className="fas fa-bell"></i>
          알람 설정 관리
        </h1>
        <button 
          className="btn btn-primary"
          onClick={() => setShowCreateModal(true)}
        >
          <i className="fas fa-plus"></i>
          새 알람 규칙 추가
        </button>
      </div>

      {/* 필터 */}
      <div className="filters">
        <div className="filter-group flex-1">
          <label className="filter-label">검색</label>
          <input
            type="text"
            className="input"
            placeholder="알람 규칙 이름 검색..."
            value={filters.search}
            onChange={(e) => setFilters(prev => ({ ...prev, search: e.target.value }))}
          />
        </div>
        
        <div className="filter-group">
          <label className="filter-label">심각도</label>
          <select
            className="select"
            value={filters.severity}
            onChange={(e) => setFilters(prev => ({ ...prev, severity: e.target.value }))}
          >
            <option value="all">전체</option>
            <option value="critical">Critical</option>
            <option value="high">High</option>
            <option value="medium">Medium</option>
            <option value="low">Low</option>
            <option value="info">Info</option>
          </select>
        </div>
        
        <div className="filter-group">
          <label className="filter-label">상태</label>
          <select
            className="select"
            value={filters.status}
            onChange={(e) => setFilters(prev => ({ ...prev, status: e.target.value }))}
          >
            <option value="all">전체</option>
            <option value="enabled">활성</option>
            <option value="disabled">비활성</option>
          </select>
        </div>

        <div className="filter-group">
          <label className="filter-label">카테고리</label>
          <select
            className="select"
            value={filters.category}
            onChange={(e) => setFilters(prev => ({ ...prev, category: e.target.value }))}
          >
            <option value="all">전체</option>
            <option value="temperature">온도</option>
            <option value="pressure">압력</option>
            <option value="flow">유량</option>
            <option value="level">레벨</option>
            <option value="vibration">진동</option>
            <option value="electrical">전기</option>
            <option value="safety">안전</option>
            <option value="general">일반</option>
          </select>
        </div>

        <div className="filter-group">
          <label className="filter-label">보기 방식</label>
          <div className="view-toggle">
            <button
              className={`view-toggle-btn ${viewType === 'card' ? 'active' : ''}`}
              onClick={() => setViewType('card')}
            >
              <i className="fas fa-th-large"></i>
              카드
            </button>
            <button
              className={`view-toggle-btn ${viewType === 'table' ? 'active' : ''}`}
              onClick={() => setViewType('table')}
            >
              <i className="fas fa-list"></i>
              테이블
            </button>
          </div>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="error">
          <i className="fas fa-exclamation-triangle"></i>
          {error}
        </div>
      )}

      {/* 로딩 상태 */}
      {loading && (
        <div className="loading">
          <i className="fas fa-spinner fa-spin"></i>
          데이터를 불러오는 중...
        </div>
      )}

      {/* 알람 규칙 목록 */}
      {!loading && (
        <div className={`alarm-list ${viewType}-view`}>
          {filteredRules.length === 0 ? (
            <div className={`empty-state ${viewType}-view`}>
              <div className="empty-state-icon">🔔</div>
              <p>설정된 알람 규칙이 없습니다.</p>
              <button 
                className="btn btn-primary"
                onClick={() => setShowCreateModal(true)}
              >
                첫 번째 알람 규칙 만들기
              </button>
            </div>
          ) : viewType === 'table' ? (
            // 테이블 뷰
            <table className="alarm-table">
              <thead>
                <tr>
                  <th>알람 규칙</th>
                  <th>타겟</th>
                  <th>조건</th>
                  <th>심각도</th>
                  <th>카테고리</th>
                  <th>상태</th>
                  <th>작업</th>
                </tr>
              </thead>
              <tbody>
                {filteredRules.map(rule => (
                  <tr key={rule.id}>
                    <td>
                      <div className="table-rule-name">{rule.name}</div>
                      {rule.description && (
                        <div className="table-rule-description">{rule.description}</div>
                      )}
                      {rule.tags && rule.tags.length > 0 && (
                        <div className="table-tags">
                          {AlarmApiService.formatTags(rule.tags).map(tag => (
                            <span key={tag} className="tag-badge">{tag}</span>
                          ))}
                        </div>
                      )}
                    </td>
                    <td>
                      <div className="table-target-info">
                        {getTargetDisplay(rule)}
                      </div>
                      <div className="table-target-type">{rule.target_type}</div>
                    </td>
                    <td>
                      <div className="table-condition">
                        <div className="table-condition-type">{rule.alarm_type}</div>
                        <div className="table-condition-values">
                          {getConditionDisplay(rule)}
                        </div>
                      </div>
                    </td>
                    <td>
                      <span className={`severity-badge severity-${rule.severity}`}>
                        {rule.severity}
                      </span>
                    </td>
                    <td>
                      {rule.category && (
                        <span className="category-badge">
                          {getCategoryDisplayName(rule.category)}
                        </span>
                      )}
                    </td>
                    <td>
                      <span className={`status-badge ${rule.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                        {rule.is_enabled ? '활성' : '비활성'}
                      </span>
                    </td>
                    <td>
                      <div className="table-actions">
                        <button
                          className="btn btn-secondary"
                          onClick={() => handleToggleRule(rule.id, rule.is_enabled)}
                          title={rule.is_enabled ? '비활성화' : '활성화'}
                          disabled={loading}
                        >
                          <i className={`fas ${rule.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>
                        </button>
                        <button
                          className="btn btn-secondary"
                          onClick={() => handleEditRule(rule)}
                          title="수정"
                        >
                          <i className="fas fa-edit"></i>
                        </button>
                        <button
                          className="btn btn-danger"
                          onClick={() => handleDeleteRule(rule.id)}
                          title="삭제"
                        >
                          <i className="fas fa-trash"></i>
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            // 카드 뷰
            filteredRules.map(rule => (
              <div key={rule.id} className="alarm-item card-item">
                {/* 카드 헤더 */}
                <div className="card-header-top">
                  <div className="card-title">{rule.name}</div>
                  <div className="card-badges">
                    <span className={`severity-badge severity-${rule.severity}`}>
                      {rule.severity.toUpperCase()}
                    </span>
                    <span className={`status-badge ${rule.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                      {rule.is_enabled ? '활성' : '비활성'}
                    </span>
                    {rule.category && (
                      <span className="category-badge">
                        {getCategoryDisplayName(rule.category)}
                      </span>
                    )}
                  </div>
                </div>
                
                {/* 조건 정보 블록 */}
                <div className="card-condition-block">
                  <div className="condition-row">
                    <span className="condition-type-badge">
                      {rule.alarm_type}
                    </span>
                    <div className="condition-values">
                      {rule.high_high_limit && (
                        <span className="condition-item">
                          <span className="condition-label">HH</span>
                          <span className="condition-value">{rule.high_high_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.high_limit && (
                        <span className="condition-item">
                          <span className="condition-label">H</span>
                          <span className="condition-value">{rule.high_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.low_limit && (
                        <span className="condition-item">
                          <span className="condition-label">L</span>
                          <span className="condition-value">{rule.low_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.low_low_limit && (
                        <span className="condition-item">
                          <span className="condition-label">LL</span>
                          <span className="condition-value">{rule.low_low_limit}{getUnit(rule)}</span>
                        </span>
                      )}
                      {rule.deadband !== undefined && rule.deadband !== null && (
                        <span className="condition-item">
                          <span className="condition-label">DB</span>
                          <span className="condition-value">{rule.deadband}{getUnit(rule)}</span>
                        </span>
                      )}
                    </div>
                  </div>
                </div>
                
                {/* 태그 표시 */}
                {rule.tags && rule.tags.length > 0 && (
                  <div className="card-tags-block">
                    {AlarmApiService.formatTags(rule.tags).map(tag => (
                      <span key={tag} className="tag-badge" style={{backgroundColor: AlarmApiService.getTagColor(tag)}}>
                        {tag}
                      </span>
                    ))}
                  </div>
                )}
                
                {/* 설명 */}
                {rule.description && (
                  <div className="card-description-block">
                    {rule.description}
                  </div>
                )}

                {/* 하단 정보 및 액션 */}
                <div className="card-footer">
                  <div className="card-target-compact">
                    <span className="target-label">타겟</span>
                    <span className="target-value">{getTargetDisplay(rule)}</span>
                    <span className="target-type">({rule.target_type})</span>
                  </div>
                  
                  <div className="card-actions">
                    <button
                      className="btn btn-secondary btn-sm"
                      onClick={() => handleToggleRule(rule.id, rule.is_enabled)}
                      title={rule.is_enabled ? '비활성화' : '활성화'}
                      disabled={loading}
                    >
                      <i className={`fas ${rule.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>
                    </button>
                    <button
                      className="btn btn-secondary btn-sm"
                      onClick={() => handleEditRule(rule)}
                      title="수정"
                    >
                      <i className="fas fa-edit"></i>
                    </button>
                    <button
                      className="btn btn-danger btn-sm"
                      onClick={() => handleDeleteRule(rule.id)}
                      title="삭제"
                    >
                      <i className="fas fa-trash"></i>
                    </button>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* 새 알람 규칙 생성 모달 - 개선된 타겟 선택 */}
      {showCreateModal && (
        <div className="modal-overlay">
          <div className="modal">
            <div className="modal-header">
              <h2 className="modal-title">새 알람 규칙 추가</h2>
              <button
                className="close-button"
                onClick={() => {
                  setShowCreateModal(false);
                  resetForm();
                }}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="form-section">
                <div className="section-title">기본 정보</div>
                
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">규칙 이름 *</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.name}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                      placeholder="알람 규칙 이름을 입력하세요"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">설명</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.description}
                      onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                      placeholder="알람 규칙에 대한 설명을 입력하세요"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">타겟 타입 *</label>
                    <select
                      className="form-select"
                      value={formData.target_type}
                      onChange={(e) => setFormData(prev => ({ ...prev, target_type: e.target.value as any }))}
                    >
                      <option value="device">디바이스</option>
                      <option value="data_point">데이터포인트</option>
                      <option value="virtual_point">가상포인트</option>
                    </select>
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">타겟 선택 *</label>
                    {loadingTargetData ? (
                      <div className="form-input loading-input">
                        <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                      </div>
                    ) : (
                      <select
                        className="form-select"
                        value={formData.target_id}
                        onChange={(e) => setFormData(prev => ({ ...prev, target_id: e.target.value }))}
                      >
                        <option value="">타겟을 선택하세요</option>
                        {getTargetOptions().map(option => (
                          <option key={option.value} value={option.value}>
                            {option.label}{option.extra}
                          </option>
                        ))}
                      </select>
                    )}
                    <small className="form-help">
                      {formData.target_type === 'data_point' && 'ID:데이터포인트명 (디바이스명) 형식으로 표시됩니다'}
                      {formData.target_type === 'device' && 'ID:디바이스명 (타입) 형식으로 표시됩니다'}
                      {formData.target_type === 'virtual_point' && '가상포인트를 선택하세요'}
                    </small>
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">카테고리</label>
                    <select
                      className="form-select"
                      value={formData.category}
                      onChange={(e) => setFormData(prev => ({ ...prev, category: e.target.value }))}
                    >
                      <option value="">선택하세요</option>
                      <option value="temperature">온도</option>
                      <option value="pressure">압력</option>
                      <option value="flow">유량</option>
                      <option value="level">레벨</option>
                      <option value="vibration">진동</option>
                      <option value="electrical">전기</option>
                      <option value="safety">안전</option>
                      <option value="general">일반</option>
                    </select>
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">태그</label>
                    <div className="tags-input">
                      <div className="tags-list">
                        {formData.tags.map(tag => (
                          <span key={tag} className="tag-item">
                            {tag}
                            <button type="button" onClick={() => removeTag(tag)}>×</button>
                          </span>
                        ))}
                      </div>
                      <input
                        type="text"
                        className="form-input"
                        placeholder="태그를 입력하고 Enter를 누르세요"
                        onKeyPress={(e) => {
                          if (e.key === 'Enter') {
                            e.preventDefault();
                            addTag(e.currentTarget.value);
                            e.currentTarget.value = '';
                          }
                        }}
                      />
                    </div>
                  </div>
                </div>
              </div>

              <div className="form-section">
                <div className="section-title">조건 설정</div>
                
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">알람 타입 *</label>
                    <select
                      className="form-select"
                      value={formData.alarm_type}
                      onChange={(e) => setFormData(prev => ({ ...prev, alarm_type: e.target.value as any }))}
                    >
                      <option value="analog">아날로그</option>
                      <option value="digital">디지털</option>
                      <option value="script">스크립트</option>
                    </select>
                  </div>
                  
                  <div className="form-group">
                    <label className="form-label">심각도 *</label>
                    <select
                      className="form-select"
                      value={formData.severity}
                      onChange={(e) => setFormData(prev => ({ ...prev, severity: e.target.value as any }))}
                    >
                      <option value="critical">Critical</option>
                      <option value="high">High</option>
                      <option value="medium">Medium</option>
                      <option value="low">Low</option>
                      <option value="info">Info</option>
                    </select>
                  </div>
                </div>

                {formData.alarm_type === 'analog' && (
                  <div className="form-subsection">
                    <div className="subsection-title">아날로그 임계값</div>
                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">HH (High High)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.high_high_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, high_high_limit: e.target.value }))}
                          placeholder="최고 상한값"
                        />
                      </div>
                      
                      <div className="form-group">
                        <label className="form-label">H (High)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.high_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, high_limit: e.target.value }))}
                          placeholder="상한값"
                        />
                      </div>
                      
                      <div className="form-group">
                        <label className="form-label">L (Low)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.low_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, low_limit: e.target.value }))}
                          placeholder="하한값"
                        />
                      </div>
                      
                      <div className="form-group">
                        <label className="form-label">LL (Low Low)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.low_low_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, low_low_limit: e.target.value }))}
                          placeholder="최저 하한값"
                        />
                      </div>
                    </div>

                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">데드밴드</label>
                        <input
                          type="number"
                          step="0.1"
                          className="form-input"
                          value={formData.deadband}
                          onChange={(e) => setFormData(prev => ({ ...prev, deadband: e.target.value }))}
                          placeholder="데드밴드 값 (0은 비활성화)"
                          min="0"
                        />
                        <small className="form-help">알람 chattering 방지용 데드밴드 (0 = 비활성화)</small>
                      </div>
                      
                      <div className="form-group">
                        <label className="form-label">변화율</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.rate_of_change}
                          onChange={(e) => setFormData(prev => ({ ...prev, rate_of_change: e.target.value }))}
                          placeholder="변화율 임계값"
                        />
                      </div>
                    </div>
                  </div>
                )}

                {formData.alarm_type === 'digital' && (
                  <div className="form-subsection">
                    <div className="subsection-title">디지털 조건</div>
                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">트리거 조건</label>
                        <select
                          className="form-select"
                          value={formData.trigger_condition}
                          onChange={(e) => setFormData(prev => ({ ...prev, trigger_condition: e.target.value }))}
                        >
                          <option value="">선택하세요</option>
                          <option value="on_true">True가 될 때</option>
                          <option value="on_false">False가 될 때</option>
                          <option value="on_change">값이 변경될 때</option>
                        </select>
                      </div>
                    </div>
                  </div>
                )}

                {formData.alarm_type === 'script' && (
                  <div className="form-subsection">
                    <div className="subsection-title">스크립트 조건</div>
                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">조건 스크립트</label>
                        <textarea
                          className="form-textarea"
                          value={formData.condition_script}
                          onChange={(e) => setFormData(prev => ({ ...prev, condition_script: e.target.value }))}
                          placeholder="JavaScript 조건식을 입력하세요"
                          rows={4}
                        />
                      </div>
                    </div>
                  </div>
                )}

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">메시지 템플릿</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.message_template}
                      onChange={(e) => setFormData(prev => ({ ...prev, message_template: e.target.value }))}
                      placeholder="알람 메시지 템플릿"
                    />
                  </div>
                </div>
              </div>

              <div className="form-section">
                <div className="section-title">동작 설정</div>
                
                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.is_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, is_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알람 규칙 활성화</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.auto_clear}
                    onChange={(e) => setFormData(prev => ({ ...prev, auto_clear: e.target.checked }))}
                  />
                  <label className="checkbox-label">자동 해제</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.auto_acknowledge}
                    onChange={(e) => setFormData(prev => ({ ...prev, auto_acknowledge: e.target.checked }))}
                  />
                  <label className="checkbox-label">자동 확인</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.notification_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, notification_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알림 활성화</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.escalation_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, escalation_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">에스컬레이션 활성화</label>
                </div>

                {formData.escalation_enabled && (
                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">최대 에스컬레이션 레벨</label>
                      <input
                        type="number"
                        className="form-input"
                        value={formData.escalation_max_level}
                        onChange={(e) => setFormData(prev => ({ ...prev, escalation_max_level: parseInt(e.target.value) || 3 }))}
                        min="1"
                        max="10"
                      />
                    </div>
                  </div>
                )}
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-outline"
                onClick={() => {
                  setShowCreateModal(false);
                  resetForm();
                }}
              >
                취소
              </button>
              <button
                className="btn btn-primary"
                onClick={handleCreateRule}
                disabled={!formData.name || !formData.target_id || loading}
              >
                {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-plus"></i>}
                생성
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 알람 규칙 수정 모달 - 개선된 타겟 선택 */}
      {showEditModal && selectedRule && (
        <div className="modal-overlay">
          <div className="modal">
            <div className="modal-header">
              <h2 className="modal-title">알람 규칙 수정: {selectedRule.name}</h2>
              <button
                className="close-button"
                onClick={() => {
                  setShowEditModal(false);
                  setSelectedRule(null);
                  resetForm();
                }}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="form-section readonly-section">
                <div className="section-title">기본 정보</div>
                
                <div className="readonly-grid">
                  <div className="readonly-item">
                    <label>규칙 ID</label>
                    <span>#{selectedRule.id}</span>
                  </div>
                  <div className="readonly-item">
                    <label>현재 타겟</label>
                    <span>{getTargetIdDisplay(selectedRule.target_type, selectedRule.target_id?.toString() || '')}</span>
                  </div>
                  <div className="readonly-item">
                    <label>알람 타입</label>
                    <span>{selectedRule.alarm_type}</span>
                  </div>
                  <div className="readonly-item">
                    <label>생성일</label>
                    <span>{new Date(selectedRule.created_at).toLocaleString()}</span>
                  </div>
                  <div className="readonly-item">
                    <label>수정일</label>
                    <span>{new Date(selectedRule.updated_at).toLocaleString()}</span>
                  </div>
                </div>
              </div>

              {/* 수정 가능한 필드들 */}
              <div className="form-section">
                <div className="section-title">수정 가능한 설정</div>
                
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">규칙 이름 *</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.name}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">설명</label>
                    <input
                      type="text"
                      className="form-input"
                      value={formData.description}
                      onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                    />
                  </div>
                </div>

                {/* 타겟 수정 추가 */}
                <div className="form-subsection">
                  <div className="subsection-title">타겟 설정 수정</div>
                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">타겟 타입</label>
                      <select
                        className="form-select"
                        value={formData.target_type}
                        onChange={(e) => setFormData(prev => ({ ...prev, target_type: e.target.value as any }))}
                      >
                        <option value="device">디바이스</option>
                        <option value="data_point">데이터포인트</option>
                        <option value="virtual_point">가상포인트</option>
                      </select>
                    </div>
                    
                    <div className="form-group">
                      <label className="form-label">타겟 선택</label>
                      {loadingTargetData ? (
                        <div className="form-input loading-input">
                          <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                        </div>
                      ) : (
                        <select
                          className="form-select"
                          value={formData.target_id}
                          onChange={(e) => setFormData(prev => ({ ...prev, target_id: e.target.value }))}
                        >
                          <option value="">타겟을 선택하세요</option>
                          {getTargetOptions().map(option => (
                            <option key={option.value} value={option.value}>
                              {option.label}{option.extra}
                            </option>
                          ))}
                        </select>
                      )}
                    </div>
                  </div>

                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">타겟 그룹</label>
                      <input
                        type="text"
                        className="form-input"
                        value={formData.target_group}
                        onChange={(e) => setFormData(prev => ({ ...prev, target_group: e.target.value }))}
                        placeholder="타겟 그룹 (선택사항)"
                      />
                    </div>
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">카테고리</label>
                    <select
                      className="form-select"
                      value={formData.category}
                      onChange={(e) => setFormData(prev => ({ ...prev, category: e.target.value }))}
                    >
                      <option value="">선택하세요</option>
                      <option value="temperature">온도</option>
                      <option value="pressure">압력</option>
                      <option value="flow">유량</option>
                      <option value="level">레벨</option>
                      <option value="vibration">진동</option>
                      <option value="electrical">전기</option>
                      <option value="safety">안전</option>
                      <option value="general">일반</option>
                    </select>
                  </div>
                </div>

                {/* 임계값들 수정 */}
                {selectedRule.alarm_type === 'analog' && (
                  <div className="form-subsection">
                    <div className="subsection-title">임계값 수정</div>
                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">HH (High High)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.high_high_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, high_high_limit: e.target.value }))}
                        />
                      </div>
                      
                      <div className="form-group">
                        <label className="form-label">H (High)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.high_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, high_limit: e.target.value }))}
                        />
                      </div>
                    </div>

                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">L (Low)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.low_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, low_limit: e.target.value }))}
                        />
                      </div>
                      
                      <div className="form-group">
                        <label className="form-label">LL (Low Low)</label>
                        <input
                          type="number"
                          step="0.01"
                          className="form-input"
                          value={formData.low_low_limit}
                          onChange={(e) => setFormData(prev => ({ ...prev, low_low_limit: e.target.value }))}
                        />
                      </div>
                    </div>

                    <div className="form-row">
                      <div className="form-group">
                        <label className="form-label">데드밴드</label>
                        <input
                          type="number"
                          step="0.1"
                          className="form-input"
                          value={formData.deadband}
                          onChange={(e) => setFormData(prev => ({ ...prev, deadband: e.target.value }))}
                          min="0"
                        />
                        <small className="form-help">0 = 데드밴드 비활성화</small>
                      </div>
                    </div>
                  </div>
                )}

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">태그</label>
                    <div className="tags-input">
                      <div className="tags-list">
                        {formData.tags.map(tag => (
                          <span key={tag} className="tag-item">
                            {tag}
                            <button type="button" onClick={() => removeTag(tag)}>×</button>
                          </span>
                        ))}
                      </div>
                      <input
                        type="text"
                        className="form-input"
                        placeholder="태그를 입력하고 Enter를 누르세요"
                        onKeyPress={(e) => {
                          if (e.key === 'Enter') {
                            e.preventDefault();
                            addTag(e.currentTarget.value);
                            e.currentTarget.value = '';
                          }
                        }}
                      />
                    </div>
                  </div>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.is_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, is_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알람 규칙 활성화</label>
                </div>

                <div className="checkbox-group">
                  <input
                    type="checkbox"
                    className="checkbox"
                    checked={formData.notification_enabled}
                    onChange={(e) => setFormData(prev => ({ ...prev, notification_enabled: e.target.checked }))}
                  />
                  <label className="checkbox-label">알림 활성화</label>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-outline"
                onClick={() => {
                  setShowEditModal(false);
                  setSelectedRule(null);
                  resetForm();
                }}
              >
                취소
              </button>
              <button
                className="btn btn-primary"
                onClick={handleUpdateRule}
                disabled={loading}
              >
                {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-save"></i>}
                수정
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AlarmSettings;