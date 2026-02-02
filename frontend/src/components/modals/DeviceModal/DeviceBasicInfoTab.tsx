// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx
// 📋 디바이스 기본정보 탭 컴포넌트 - 4 Column Horizontal Layout
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService, ProtocolInfo } from '../../../api/services/deviceApi';
import { ProtocolApiService, ProtocolInstance } from '../../../api/services/protocolApi';
import { GroupApiService, DeviceGroup } from '../../../api/services/groupApi';
import { CollectorApiService, EdgeServer } from '../../../api/services/collectorApi';
import { ManufactureApiService } from '../../../api/services/manufactureApi';
import { SiteApiService } from '../../../api/services/siteApi';
import { Manufacturer, DeviceModel } from '../../../types/manufacturing';
import { Site } from '../../../types/common';
import { DeviceBasicInfoTabProps } from './types';
import '../../../styles/management.css';

const DeviceBasicInfoTab: React.FC<DeviceBasicInfoTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField,
  onUpdateSettings,
  showModal
}) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [availableProtocols, setAvailableProtocols] = useState<ProtocolInfo[]>([]);
  const [availableGroups, setAvailableGroups] = useState<DeviceGroup[]>([]);
  const [availableCollectors, setAvailableCollectors] = useState<EdgeServer[]>([]);
  const [availableManufacturers, setAvailableManufacturers] = useState<Manufacturer[]>([]);
  const [availableModels, setAvailableModels] = useState<DeviceModel[]>([]);
  const [availableSites, setAvailableSites] = useState<Site[]>([]);
  const [isLoadingProtocols, setIsLoadingProtocols] = useState(false);
  const [isLoadingGroups, setIsLoadingGroups] = useState(false);
  const [isLoadingCollectors, setIsLoadingCollectors] = useState(false);
  const [isLoadingManufacturers, setIsLoadingManufacturers] = useState(false);
  const [isLoadingModels, setIsLoadingModels] = useState(false);
  const [isLoadingSites, setIsLoadingSites] = useState(false);
  const [isTestingConnection, setIsTestingConnection] = useState(false);
  const [isSlaveIdDuplicate, setIsSlaveIdDuplicate] = useState(false);
  const [checkingSlaveId, setCheckingSlaveId] = useState(false);
  const [isAutoMatched, setIsAutoMatched] = useState(false);

  // 🔥 NEW: 프로토콜 인스턴스 상태
  const [availableInstances, setAvailableInstances] = useState<ProtocolInstance[]>([]);
  const [isLoadingInstances, setIsLoadingInstances] = useState(false);

  // JSON 문자열 상태 (원활한 편집을 위해)
  const [metadataStr, setMetadataStr] = useState('');
  const [customFieldsStr, setCustomFieldsStr] = useState('');

  const displayData = device || editData;

  // RTU 설정 파싱
  const getRtuConfig = () => {
    try {
      const config = typeof editData?.config === 'string'
        ? JSON.parse(editData.config)
        : editData?.config || {};
      return config;
    } catch {
      return {};
    }
  };

  const rtuConfig = getRtuConfig();
  const isRtuDevice = editData?.protocol_type === 'MODBUS_RTU';

  // ========================================================================
  // API 호출 함수들
  // ========================================================================

  /**
   * 지원 프로토콜 목록 로드
   */
  const loadAvailableProtocols = async () => {
    try {
      setIsLoadingProtocols(true);
      // DeviceApiService의 ProtocolManager 사용
      await DeviceApiService.initialize();
      const protocols = DeviceApiService.getProtocolManager().getAllProtocols();
      setAvailableProtocols(protocols);
    } catch (error) {
      console.error('❌ 프로토콜 목록 로드 실패:', error);
      setAvailableProtocols([]); // API 실패 시 빈 배열 (사용자 요청: 서버 데이터만 표시)
    } finally {
      setIsLoadingProtocols(false);
    }
  };

  /**
   * 그룹 목록 로드
   */
  const loadGroups = async () => {
    try {
      setIsLoadingGroups(true);
      const response = await GroupApiService.getGroupTree();
      if (response.success && response.data) {
        // 평탄화 작업 (단순 Select용)
        const flattened: DeviceGroup[] = [];
        const flatten = (items: DeviceGroup[]) => {
          items.forEach(item => {
            flattened.push(item);
            if (item.children) flatten(item.children);
          });
        };
        flatten(response.data);
        setAvailableGroups(flattened);
      }
    } catch (error) {
      console.error('❌ 그룹 목록 로드 실패:', error);
    } finally {
      setIsLoadingGroups(false);
    }
  };

  /**
   * 콜렉터 목록 로드
   */
  const loadCollectors = async () => {
    try {
      setIsLoadingCollectors(true);
      const response = await CollectorApiService.getAllCollectors();
      if (response.success && response.data) {
        setAvailableCollectors(response.data);
      }
    } catch (error) {
      console.error('❌ 콜렉터 목록 로드 실패:', error);
    } finally {
      setIsLoadingCollectors(false);
    }
  };

  /**
   * 제조사 목록 로드
   */
  const loadManufacturers = async () => {
    try {
      setIsLoadingManufacturers(true);
      const response = await ManufactureApiService.getManufacturers({ limit: 100 });
      if (response.success && response.data) {
        setAvailableManufacturers(response.data.items || []);
      }
    } catch (error) {
      console.error('❌ 제조사 목록 로드 실패:', error);
    } finally {
      setIsLoadingManufacturers(false);
    }
  };

  /**
   * 🔥 NEW: 프로토콜 인스턴스 목록 로드
   */
  const loadProtocolInstances = async (protocolId: number) => {
    if (!protocolId) {
      setAvailableInstances([]);
      return;
    }

    try {
      setIsLoadingInstances(true);
      const response = await ProtocolApiService.getProtocolInstances(protocolId);
      if (response.success && response.data) {
        // PaginatedResponse에서 items 추출 또는 배열인 경우 그대로 사용
        const list = (response.data as any).items || (Array.isArray(response.data) ? response.data : []);
        setAvailableInstances(list);
      } else {
        setAvailableInstances([]);
      }
    } catch (error) {
      console.error('❌ 프로토콜 인스턴스 로드 실패:', error);
      setAvailableInstances([]);
    } finally {
      setIsLoadingInstances(false);
    }
  };

  /**
   * 특정 제조사의 모델 목록 로드
   */
  const loadModels = async (manufacturerName: string) => {
    if (!manufacturerName) {
      setAvailableModels([]);
      return;
    }

    try {
      setIsLoadingModels(true);
      // 이름으로 제조사 ID 찾기
      const manufacturer = availableManufacturers.find(m => m.name === manufacturerName);
      if (manufacturer) {
        const response = await ManufactureApiService.getModelsByManufacturer(manufacturer.id, { limit: 100 });
        if (response.success && response.data) {
          setAvailableModels(response.data.items || []);
        }
      } else {
        // ID를 못 찾으면 전체 모델 중 필터링 시도 or 검색 API
        const response = await ManufactureApiService.getModels({ limit: 100 });
        if (response.success && response.data) {
          const filtered = response.data.items?.filter(m => m.manufacturer_name === manufacturerName);
          setAvailableModels(filtered || []);
        }
      }
    } catch (error) {
      console.error('❌ 모델 목록 로드 실패:', error);
    } finally {
      setIsLoadingModels(false);
    }
  };

  /**
   * Modbus RTU Slave ID 중복 체크
   */
  const checkSlaveIdDuplication = async () => {
    if (!editData?.site_id || !editData?.endpoint) return;

    // config 파싱
    let sId = rtuConfig.slave_id;
    if (!sId || !isRtuDevice) {
      setIsSlaveIdDuplicate(false);
      return;
    }

    try {
      setCheckingSlaveId(true);
      // 동일 사이트, 동일 엔드포인트의 디바이스 목록 조회
      const response = await DeviceApiService.getDevices({
        site_id: editData.site_id,
        limit: 1000 // 충분히 크게
      });

      if (response.success && response.data?.items) {
        const duplicate = response.data.items.some(d => {
          // 자기 자신은 제외 (수정 모드일 때)
          if (device && d.id === device.id) return false;

          // 엔드포인트 동일 여부
          if (d.endpoint !== editData.endpoint) return false;

          // Slave ID 동일 여부 (config 내에 존재)
          try {
            const dConfig = typeof d.config === 'string' ? JSON.parse(d.config) : d.config;
            return dConfig?.slave_id === parseInt(sId.toString());
          } catch {
            return false;
          }
        });
        setIsSlaveIdDuplicate(duplicate);
      }
    } catch (error) {
      console.error('Slave ID 중복 체크 실패:', error);
    } finally {
      setCheckingSlaveId(false);
    }
  };

  /**
   * 사이트 목록 로드
   */
  const loadSites = async () => {
    try {
      setIsLoadingSites(true);
      const response = await SiteApiService.getSites({ limit: 100 });
      if (response.success && response.data) {
        setAvailableSites(response.data.items || []);
      }
    } catch (error) {
      console.error('❌ 사이트 목록 로드 실패:', error);
    } finally {
      setIsLoadingSites(false);
    }
  };

  /**
   * 🔥 NEW: 프로토콜 변경 감지하여 인스턴스 로드
   */
  useEffect(() => {
    const pId = getCurrentProtocolId();
    if (pId) {
      loadProtocolInstances(pId);
    } else {
      setAvailableInstances([]);
    }
  }, [editData?.protocol_id, availableProtocols]);

  /**
   * 기본 프로토콜 목록 - API 호출 실패 시 백업용
   */
  const getDefaultProtocols = (): ProtocolInfo[] => [
    {
      id: 1,
      protocol_type: 'MODBUS_TCP',
      name: 'Modbus TCP',
      value: 'MODBUS_TCP',
      description: 'Modbus TCP/IP Protocol',
      display_name: 'Modbus TCP',
      default_port: 502
    },
    {
      id: 2,
      protocol_type: 'MODBUS_RTU',
      name: 'Modbus RTU',
      value: 'MODBUS_RTU',
      description: 'Modbus RTU Serial Protocol',
      display_name: 'Modbus RTU',
      uses_serial: true
    },
    {
      id: 3,
      protocol_type: 'MQTT',
      name: 'MQTT',
      value: 'MQTT',
      description: 'MQTT Protocol',
      display_name: 'MQTT',
      default_port: 1883,
      requires_broker: true
    },
    {
      id: 4,
      protocol_type: 'BACNET',
      name: 'BACnet',
      value: 'BACNET',
      description: 'Building Automation and Control Networks',
      display_name: 'BACnet',
      default_port: 47808
    }
  ];

  /**
   * 연결 테스트
   */
  const handleTestConnection = async () => {
    if (!device?.id) {
      if (showModal) {
        showModal({
          type: 'error',
          title: '연결 테스트 불가',
          message: '저장된 디바이스만 연결 테스트가 가능합니다.',
          onConfirm: () => { }
        });
      } else {
        alert('저장된 디바이스만 연결 테스트가 가능합니다.');
      }
      return;
    }

    try {
      setIsTestingConnection(true);
      const response = await DeviceApiService.testDeviceConnection(device.id);

      if (response.success && response.data) {
        const result = response.data;
        if (result.test_successful) {
          if (showModal) {
            showModal({
              type: 'success',
              title: '연결 성공',
              message: `✅ 실제 장비와 정상적으로 통신되었습니다.\n측정된 응답 시간: ${result.response_time_ms}ms`,
              onConfirm: () => { }
            });
          } else {
            alert(`✅ 연결 성공!\n응답시간: ${result.response_time_ms}ms`);
          }
        } else {
          if (showModal) {
            showModal({
              type: 'error',
              title: '연결 실패',
              message: `❌ 하드웨어와 연결할 수 없습니다.\n오류 원인: ${result.error_message}`,
              onConfirm: () => { }
            });
          } else {
            alert(`❌ 연결 실패!\n오류: ${result.error_message}`);
          }
        }
      } else {
        if (showModal) {
          showModal({
            type: 'error',
            title: '테스트 실패',
            message: `❌ 서버 시스템 오류: ${response.error}`,
            onConfirm: () => { }
          });
        } else {
          alert(`❌ 테스트 실패: ${response.error}`);
        }
      }
    } catch (error) {
      console.error('연결 테스트 실패:', error);
      const errMsg = error instanceof Error ? error.message : 'Unknown error';
      if (showModal) {
        showModal({
          type: 'error',
          title: '연결 테스트 오류',
          message: `❌ 네트워크 또는 서버 통신에 문제가 발생했습니다.\n${errMsg}`,
          onConfirm: () => { }
        });
      } else {
        alert(`❌ 연결 테스트 오류: ${errMsg}`);
      }
    } finally {
      setIsTestingConnection(false);
    }
  };

  // ========================================================================
  // RTU 설정 관리 함수들
  // ========================================================================

  /**
   * RTU config 업데이트
   */
  const updateRtuConfig = (key: string, value: any) => {
    const newConfig = { ...rtuConfig, [key]: value };
    onUpdateField('config', newConfig);
  };

  /**
   * 프로토콜 변경 시 처리 - protocol_id 기반
   */
  const handleProtocolChange = (protocolId: string) => {
    const selectedProtocol = availableProtocols.find(p => p.id === parseInt(protocolId));
    if (!selectedProtocol) return;

    // protocol_id와 protocol_type 모두 업데이트
    onUpdateField('protocol_id', selectedProtocol.id);
    onUpdateField('protocol_type', selectedProtocol.protocol_type);

    // RTU로 변경 시 기본 설정 적용
    if (selectedProtocol.protocol_type === 'MODBUS_RTU') {
      const defaultRtuConfig = {
        device_role: 'master',
        baud_rate: 9600,
        data_bits: 8,
        stop_bits: 1,
        parity: 'N',
        flow_control: 'none'
      };
      onUpdateField('config', defaultRtuConfig);

      // 엔드포인트 기본값 설정
      if (!editData?.endpoint) {
        onUpdateField('endpoint', '/dev/ttyUSB0');
      }
    } else {
      // 다른 프로토콜로 변경 시 config 초기화
      onUpdateField('config', {});
    }
  };

  /**
   * 디바이스 역할 변경 시 처리
   */
  const handleDeviceRoleChange = (role: string) => {
    const newConfig = { ...rtuConfig, device_role: role };

    if (role === 'slave') {
      // 슬래이브로 변경 시 기본 슬래이브 ID 설정
      if (!newConfig.slave_id) {
        newConfig.slave_id = 1;
      }
    } else {
      // 마스터로 변경 시 슬래이브 ID 제거
      delete newConfig.slave_id;
      delete newConfig.master_device_id;
    }

    onUpdateField('config', newConfig);
  };

  // ========================================================================
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    loadAvailableProtocols();
    loadGroups();
    loadCollectors();
    loadManufacturers();
    loadSites();
  }, []);

  // 제조사 변경 시 모델 목록 로드 (내부적으로 캐시하거나 필요 시 사용)
  useEffect(() => {
    if (editData?.manufacturer) {
      loadModels(editData.manufacturer);
    }
  }, [editData?.manufacturer, availableManufacturers]);

  // Slave ID 중복 체크 트리거
  useEffect(() => {
    const timer = setTimeout(() => {
      if (isRtuDevice && rtuConfig.slave_id && editData?.endpoint && editData?.site_id) {
        checkSlaveIdDuplication();
      } else {
        setIsSlaveIdDuplicate(false);
      }
    }, 500);

    return () => clearTimeout(timer);
  }, [editData?.site_id, editData?.endpoint, rtuConfig.slave_id, isRtuDevice]);

  // 🔥 NEW: 엔드포인트 입력 시 자동 인스턴스 매칭
  useEffect(() => {
    if (mode === 'view' || editData?.protocol_type !== 'MQTT' || !editData?.endpoint || availableInstances.length === 0) {
      return;
    }

    const endpoint = editData.endpoint.toLowerCase();

    // 엔드포인트 문자열 내에 vhost 또는 호스트가 포함되어 있는지 확인
    const matchedInstance = availableInstances.find(inst => {
      const isExternal = inst.broker_type === 'EXTERNAL';
      const params = typeof inst.connection_params === 'string'
        ? JSON.parse(inst.connection_params)
        : (inst.connection_params || {});

      if (isExternal) {
        // 외부 브로커: 호스트 주소가 엔드포인트에 포함되어 있는지 확인
        if (!params.host) return false;
        const host = params.host.toLowerCase();
        return endpoint.includes(host);
      } else {
        // 내부 브로커: vhost 기반 매칭
        if (!inst.vhost) return false;

        // vhost 정규화 (양끝 공백 및 앞쪽 슬래시 제거 후 소문자화)
        const normalizedVhost = inst.vhost.replace(/^\/+/, '').trim().toLowerCase();
        if (!normalizedVhost) return false;

        // 엔드포인트의 경로 부분에서 vhost 매칭 (예: mqtt://host/vhost)
        // /vhost/ 형태거나 /vhost로 끝나는 경우 매칭
        return endpoint.includes(`/${normalizedVhost}/`) ||
          endpoint.endsWith(`/${normalizedVhost}`) ||
          endpoint.endsWith(normalizedVhost);
      }
    });

    if (matchedInstance && matchedInstance.id !== editData.protocol_instance_id) {
      onUpdateField('protocol_instance_id', matchedInstance.id);
      onUpdateField('instance_name', matchedInstance.instance_name);
      setIsAutoMatched(true);
    }
  }, [editData?.endpoint, editData?.protocol_type, availableInstances]);

  // JSON 초기화
  useEffect(() => {
    if (editData && mode !== 'view') {
      try {
        setMetadataStr(JSON.stringify(editData.metadata || {}, null, 2));
        setCustomFieldsStr(JSON.stringify(editData.custom_fields || {}, null, 2));
      } catch (e) {
        console.warn('JSON parsing failed in BasicInfoTab init');
      }
    }
  }, [mode, editData?.id]); // id가 바뀔 때(다른 기기 선택 시) 재초기화

  // ========================================================================
  // 헬퍼 함수들
  // ========================================================================

  /**
   * 현재 선택된 protocol_id 가져오기
   */
  const getCurrentProtocolId = (): number => {
    // 1. 직접 protocol_id가 있는 경우
    if (editData?.protocol_id) {
      return editData.protocol_id;
    }

    // 2. protocol_type으로 protocol_id 찾기
    if (editData?.protocol_type) {
      const protocol = availableProtocols.find(p => p.protocol_type === editData.protocol_type);
      if (protocol) {
        return protocol.id;
      }
    }

    // 3. 기본값 (첫 번째 프로토콜)
    return availableProtocols.length > 0 ? availableProtocols[0].id : 1;
  };

  /**
   * 프로토콜 이름 표시 가져오기
   */
  const getProtocolDisplayName = (protocolType?: string, protocolId?: number): string => {
    if (protocolId) {
      const protocol = availableProtocols.find(p => p.id === protocolId);
      return protocol?.display_name || protocol?.name || `Protocol ${protocolId}`;
    }

    if (protocolType) {
      const protocol = availableProtocols.find(p => p.protocol_type === protocolType);
      return protocol?.display_name || protocol?.name || protocolType;
    }

    return 'N/A';
  };

  /**
   * 엔드포인트 예시 텍스트 생성
   */
  const getEndpointPlaceholder = (protocolType?: string): string => {
    switch (protocolType) {
      case 'MODBUS_TCP':
        return '192.168.1.100:502';
      case 'MODBUS_RTU':
        return '/dev/ttyUSB0 또는 COM1';
      case 'MQTT':
        return 'mqtt://192.168.1.100:1883';
      case 'BACNET':
        return '192.168.1.100:47808';
      case 'OPC_UA':
      case 'OPCUA':
        return 'opc.tcp://192.168.1.100:4840';
      case 'ETHERNET_IP':
        return '192.168.1.100:44818';
      case 'HTTP_REST':
        return 'http://192.168.1.100/api';
      case 'SNMP':
        return '192.168.1.100:161';
      default:
        return '연결 주소를 입력하세요';
    }
  };

  // ========================================================================
  // 렌더링
  // ========================================================================

  return (
    <div className="bi-container">
      {/* 4단 그리드 레이아웃 */}
      <div className="bi-grid-row">

        {/* 1. 기본 정보 */}
        <div className="bi-card">
          <h3>📋 기본 정보</h3>
          <div className="bi-form-stack">
            <div className="bi-field">
              <label>디바이스명 *</label>
              {mode === 'view' ? (
                <div className="form-val">{displayData?.name || 'N/A'}</div>
              ) : (
                <input
                  type="text"
                  className="bi-input"
                  value={editData?.name || ''}
                  onChange={(e) => onUpdateField('name', e.target.value)}
                  placeholder="디바이스명"
                  required
                />
              )}
            </div>

            <div className="bi-field">
              <label>설치 사이트 *</label>
              {mode === 'view' ? (
                <div className="form-val">{displayData?.site_name || displayData?.site_id || 'N/A'}</div>
              ) : (
                <select
                  className="bi-select"
                  value={editData?.site_id || ''}
                  onChange={(e) => onUpdateField('site_id', parseInt(e.target.value))}
                  required
                >
                  <option value="">사이트를 선택하세요</option>
                  {availableSites.map(site => (
                    <option key={site.id} value={site.id}>{site.name}</option>
                  ))}
                  {isLoadingSites && <option disabled>로딩 중...</option>}
                </select>
              )}
            </div>

            <div className="bi-field">
              <label>디바이스 타입</label>
              {mode === 'view' ? (
                <div className="form-val">{displayData?.device_type || 'N/A'}</div>
              ) : (
                <select
                  className="bi-select"
                  value={editData?.device_type || 'PLC'}
                  onChange={(e) => onUpdateField('device_type', e.target.value)}
                >
                  <option value="PLC">PLC</option>
                  <option value="HMI">HMI</option>
                  <option value="SENSOR">센서</option>
                  <option value="ACTUATOR">액추에이터</option>
                  <option value="METER">미터</option>
                  <option value="CONTROLLER">컨트롤러</option>
                  <option value="GATEWAY">게이트웨이</option>
                  <option value="OTHER">기타</option>
                </select>
              )}
            </div>

            <div className="bi-field">
              <label>설명</label>
              {mode === 'view' ? (
                <div className="form-val">{displayData?.description || 'N/A'}</div>
              ) : (
                <textarea
                  className="bi-input"
                  value={editData?.description || ''}
                  onChange={(e) => onUpdateField('description', e.target.value)}
                  placeholder="설명"
                  rows={2}
                />
              )}
            </div>

            <div className="bi-field">
              <label>장치 그룹 (다중 선택 가능)</label>
              {mode === 'view' ? (
                <div className="bi-tag-container">
                  {displayData?.groups && displayData.groups.length > 0 ? (
                    displayData.groups.map(g => (
                      <span key={g.id} className="bi-tag group-tag">
                        <i className="fas fa-folder"></i> {g.name}
                      </span>
                    ))
                  ) : (
                    <div className="form-val">그룹 없음</div>
                  )}
                </div>
              ) : (
                <div className="bi-multi-select-container">
                  <div className="bi-group-list-scroll">
                    {availableGroups.length > 0 ? (
                      availableGroups.map(group => {
                        const isSelected = (editData?.group_ids || []).includes(group.id);
                        return (
                          <div
                            key={group.id}
                            className={`bi-group-item ${isSelected ? 'selected' : ''}`}
                            onClick={() => {
                              const currentIds = editData?.group_ids || [];
                              let newIds;
                              if (currentIds.includes(group.id)) {
                                newIds = currentIds.filter(id => id !== group.id);
                              } else {
                                newIds = [...currentIds, group.id];
                              }
                              onUpdateField('group_ids', newIds);
                              // 대표 서버 호환성 유지 (첫 번째 선택된 그룹)
                              onUpdateField('device_group_id', newIds.length > 0 ? newIds[0] : null);
                            }}
                          >
                            <i className={`fas ${isSelected ? 'fa-check-square' : 'fa-square'}`}></i>
                            <span>{group.name}</span>
                          </div>
                        );
                      })
                    ) : (
                      <div className="bi-no-data">사용 가능한 그룹이 없습니다.</div>
                    )}
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* 2. 제조사 정보 */}
        <div className="bi-card">
          <h3>🏭 제조사 정보</h3>
          <div className="bi-form-stack">
            <div className="bi-field">
              <label>제조사</label>
              {mode === 'view' ? (
                <div className="form-val">{displayData?.manufacturer || 'N/A'}</div>
              ) : (
                <select
                  className="bi-input"
                  value={editData?.manufacturer || ''}
                  onChange={(e) => {
                    const value = e.target.value;
                    onUpdateField('manufacturer', value);
                    // 제조사 변경 시 모델명 초기화
                    onUpdateField('model', '');
                  }}
                >
                  <option value="">제조사를 선택하세요</option>
                  {availableManufacturers.map(m => (
                    <option key={m.id} value={m.name}>{m.name}</option>
                  ))}
                  {/* 기존에 목록에 없는 수동 입력값 지원 */}
                  {editData?.manufacturer && !availableManufacturers.find(m => m.name === editData.manufacturer) && (
                    <option value={editData.manufacturer}>{editData.manufacturer} (수동 입력됨)</option>
                  )}
                </select>
              )}
            </div>

            <div className="bi-field">
              <label>모델명</label>
              {mode === 'view' ? (
                <div className="form-val">{displayData?.model || 'N/A'}</div>
              ) : (
                <input
                  type="text"
                  className="bi-input"
                  value={editData?.model || ''}
                  onChange={(e) => onUpdateField('model', e.target.value)}
                  placeholder="모델명 직접 입력"
                />
              )}
            </div>

            <div className="bi-field">
              <label>시리얼 번호</label>
              {mode === 'view' ? (
                <div className="form-val">{displayData?.serial_number || 'N/A'}</div>
              ) : (
                <input
                  type="text"
                  className="bi-input"
                  value={editData?.serial_number || ''}
                  onChange={(e) => onUpdateField('serial_number', e.target.value)}
                  placeholder="시리얼 번호"
                />
              )}
            </div>

            <div className="bi-field">
              <label>태그 (쉼표로 구분)</label>
              {mode === 'view' ? (
                <div className="bi-tag-container">
                  {Array.isArray(displayData?.tags) ? (
                    displayData.tags.map(tag => <span key={tag} className="bi-tag">{tag}</span>)
                  ) : displayData?.tags ? (
                    (() => {
                      try {
                        const parsed = typeof displayData.tags === 'string' ? JSON.parse(displayData.tags) : displayData.tags;
                        return Array.isArray(parsed)
                          ? parsed.map((tag: string) => <span key={tag} className="bi-tag">{tag}</span>)
                          : <span className="bi-tag">{parsed}</span>;
                      } catch (e) {
                        return <span className="bi-tag">{displayData.tags}</span>;
                      }
                    })()
                  ) : 'N/A'}
                </div>
              ) : (
                <input
                  type="text"
                  className="bi-input"
                  value={Array.isArray(editData?.tags) ? editData.tags.join(', ') : editData?.tags || ''}
                  onChange={(e) => onUpdateField('tags', e.target.value.split(',').map(s => s.trim()).filter(s => s))}
                  placeholder="예: 센서, 1층, 중요"
                />
              )}
            </div>
          </div>
        </div>

        {/* 3. 운영 설정 */}
        <div className="bi-card">
          <h3>⚙️ 운영 설정</h3>
          <div className="bi-form-stack">
            <div className="bi-field-row">
              <div className="bi-field flex-1">
                <label>폴링 (ms)</label>
                {mode === 'view' ? (
                  <div className="form-val">{displayData?.polling_interval || 'N/A'}</div>
                ) : (
                  <input
                    type="number"
                    className="bi-input"
                    value={editData?.polling_interval || (isRtuDevice ? 2000 : 1000)}
                    onChange={(e) => onUpdateField('polling_interval', parseInt(e.target.value))}
                    min="100"
                    step="100"
                  />
                )}
              </div>
              <div className="bi-field flex-1">
                <label>타임아웃</label>
                {mode === 'view' ? (
                  <div className="form-val">{displayData?.timeout || 'N/A'}</div>
                ) : (
                  <input
                    type="number"
                    className="bi-input"
                    value={editData?.timeout || (isRtuDevice ? 3000 : 5000)}
                    onChange={(e) => onUpdateField('timeout', parseInt(e.target.value))}
                    min="1000"
                    step="1000"
                  />
                )}
              </div>
            </div>

            <div className="bi-field">
              <label>재시도 횟수</label>
              <div className="flex-row gap-2">
                {mode === 'view' ? (
                  <div className="form-val">{displayData?.retry_count || 'N/A'}</div>
                ) : (
                  <input
                    type="number"
                    className="bi-input"
                    value={editData?.retry_count || 3}
                    onChange={(e) => onUpdateField('retry_count', parseInt(e.target.value))}
                    min="0"
                    max="10"
                    style={{ width: '60px' }}
                  />
                )}
              </div>
            </div>

            <div className="bi-field">
              <label>활성화</label>
              {mode === 'view' ? (
                <span className={`status-badge-left ${displayData?.is_enabled ? 'enabled' : 'disabled'}`}>
                  {displayData?.is_enabled ? '활성화됨' : '비활성화됨'}
                </span>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.is_enabled !== false}
                    onChange={(e) => onUpdateField('is_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>

            <div className="bi-field">
              <label>담당 콜렉터 (Edge Server) *</label>
              {mode === 'view' ? (
                <div className="form-val">
                  {availableCollectors.find(c => c.id === displayData?.edge_server_id)?.name || `ID: ${displayData?.edge_server_id || '0 (Default)'}`}
                </div>
              ) : (
                <select
                  className="bi-select"
                  value={editData?.edge_server_id || 0}
                  onChange={(e) => onUpdateField('edge_server_id', parseInt(e.target.value))}
                  required
                >
                  <option value={0}>기본 콜렉터 (ID: 0)</option>
                  {availableCollectors.map(collector => (
                    <option key={collector.id} value={collector.id}>
                      {collector.name} ({collector.host})
                    </option>
                  ))}
                </select>
              )}
              {mode !== 'view' && (
                <div className="hint-text">
                  이 장치의 데이터를 수집할 물리적 서버를 선택하세요.
                </div>
              )}
            </div>
          </div>
        </div>

        {/* 4. 네트워크 설정 (Moved here) */}
        <div className="bi-card">
          <h3>🌐 네트워크 설정</h3>

          <div className="bi-form-stack">
            <div className="bi-field">
              <label>프로토콜 *</label>
              {mode === 'view' ? (
                <div className="form-val">
                  {getProtocolDisplayName(displayData?.protocol_type, displayData?.protocol_id)}
                </div>
              ) : (
                <select
                  className="bi-select"
                  value={getCurrentProtocolId()}
                  onChange={(e) => handleProtocolChange(e.target.value)}
                  disabled={isLoadingProtocols}
                >
                  {isLoadingProtocols ? (
                    <option>로딩 중...</option>
                  ) : availableProtocols.length > 0 ? (
                    availableProtocols.map(protocol => (
                      <option key={protocol.id} value={protocol.id}>
                        {protocol.display_name || protocol.name}
                      </option>
                    ))
                  ) : (
                    <option>프로토콜 없음</option>
                  )}
                </select>
              )}
            </div>

            {/* 🔥 NEW: 프로토콜 인스턴스 선택 (인스턴스가 존재할 때만 표시) */}
            {availableInstances.length > 0 && (
              <div className="bi-field">
                <label>연결 인스턴스 (Broker/VHost)</label>
                {mode === 'view' ? (
                  <div className="form-val">
                    {editData?.instance_name ||
                      availableInstances.find(i => i.id === editData?.protocol_instance_id)?.instance_name ||
                      '-'}
                  </div>
                ) : (
                  <select
                    className="bi-select"
                    value={editData?.protocol_instance_id || ''}
                    onChange={(e) => {
                      const val = e.target.value ? parseInt(e.target.value) : null;
                      onUpdateField('protocol_instance_id', val);
                      // 인스턴스 이름도 함께 업데이트 (표시용)
                      const inst = availableInstances.find(i => i.id === val);
                      if (inst) onUpdateField('instance_name', inst.instance_name);
                      setIsAutoMatched(false); // 수동으로 바꾸면 자동 매칭 표시 제거
                    }}
                    disabled={isLoadingInstances}
                    style={isAutoMatched ? { borderColor: 'var(--primary-400)', backgroundColor: 'var(--primary-50)' } : {}}
                  >
                    <option value="">자동 할당 / 기본값</option>
                    {availableInstances.map(inst => (
                      <option key={inst.id} value={inst.id}>
                        {inst.instance_name} {inst.vhost ? `(${inst.vhost})` : ''}
                      </option>
                    ))}
                  </select>
                )}
                {mode !== 'view' && (
                  <div className="hint-text" style={{ display: 'flex', alignItems: 'center', gap: '4px', color: isAutoMatched ? 'var(--primary-600)' : 'inherit' }}>
                    {isAutoMatched ? (
                      <><i className="fas fa-magic" style={{ fontSize: '10px' }}></i> 엔드포인트 정보를 바탕으로 인스턴스가 자동 매칭되었습니다.</>
                    ) : (
                      '특정 브로커나 연결 인스턴스를 지정하려면 선택하세요.'
                    )}
                  </div>
                )}
              </div>
            )}

            <div className="bi-field">
              <label>{isRtuDevice ? '포트 *' : '엔드포인트 *'}</label>
              {mode === 'view' ? (
                <div className="form-val text-break">{displayData?.endpoint || 'N/A'}</div>
              ) : (
                <input
                  type="text"
                  className="bi-input"
                  value={editData?.endpoint || ''}
                  onChange={(e) => onUpdateField('endpoint', e.target.value)}
                  placeholder={getEndpointPlaceholder(editData?.protocol_type)}
                  required
                />
              )}
              {mode !== 'view' && (
                <div className="hint-text">
                  예시: {getEndpointPlaceholder(editData?.protocol_type)}
                </div>
              )}
            </div>

            {/* 🔥 MQTT Base Topic (MQTT 일 때 엔드포인트 바로 아래 배치) */}
            {editData?.protocol_type === 'MQTT' && (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                <div className="bi-field">
                  <label style={{ color: 'var(--primary-600)', fontWeight: 600 }}>
                    <i className="fas fa-satellite-dish"></i> MQTT Base Topic *
                  </label>
                  {mode === 'view' ? (
                    <div className="form-val text-break" style={{ color: 'var(--primary-700)', fontWeight: 600 }}>
                      {rtuConfig.topic || '-'}
                    </div>
                  ) : (
                    <>
                      <input
                        type="text"
                        className="bi-input"
                        value={rtuConfig.topic || ''}
                        onChange={(e) => updateRtuConfig('topic', e.target.value)}
                        placeholder="예: factory/line1/#"
                        style={{ borderColor: 'var(--primary-300)', background: 'var(--primary-50)' }}
                        required
                      />
                      <div className="hint-text">
                        데이터 구독을 위한 <strong>최상위 토픽</strong>입니다. (예: <code>factory/1/#</code>)
                      </div>
                    </>
                  )}
                </div>

                {/* 🔥 MQTT Auto-Registration Toggle */}
                <div className="bi-field" style={{ marginTop: '4px' }}>
                  <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', color: 'var(--success-700)', fontWeight: 600 }}>
                    <input
                      type="checkbox"
                      checked={!!(editData?.settings as any)?.is_auto_registration_enabled}
                      disabled={mode === 'view'}
                      onChange={(e) => onUpdateSettings?.('is_auto_registration_enabled', e.target.checked)}
                      style={{ width: '16px', height: '16px', cursor: 'pointer' }}
                    />
                    <span><i className="fas fa-magic"></i> MQTT 자동 데이터 등록 (Auto-Discovery)</span>
                  </label>
                  <div className="hint-text" style={{ marginLeft: '24px' }}>
                    수집 시 등록되지 않은 JSON 키를 <strong>데이터 포인트로 자동 등록</strong>합니다.
                  </div>
                </div>
              </div>
            )}


            {mode === 'view' && displayData?.protocol && displayData.protocol.default_port && (
              <div className="bi-field">
                <label>기본 포트</label>
                <div className="form-val">{displayData.protocol.default_port}</div>
              </div>
            )}

            {/* 연결 테스트 버튼 (View 모드) */}
            {mode === 'view' && device?.id && (
              <div className="bi-field" style={{ marginTop: 'auto' }}>
                <button
                  type="button"
                  className="bi-btn-test"
                  onClick={handleTestConnection}
                  disabled={isTestingConnection}
                >
                  {isTestingConnection ? (
                    <i className="fas fa-spinner fa-spin"></i>
                  ) : (
                    <i className="fas fa-plug"></i>
                  )}
                  {isTestingConnection ? '...' : '테스트'}
                </button>
              </div>
            )}
          </div>
        </div>

      </div>
      {/* 4단 그리드 END */}

      {/* RTU 상세 설정 (If applicable, shows below) */}
      {isRtuDevice && mode !== 'view' && (
        <div className="bi-card full-width">
          <div className="bi-rtu-header">
            <i className="fas fa-microchip"></i> RTU 상세 설정
          </div>

          <div className="bi-grid-row"> {/* Use grid for RTU params too */}
            <div className="bi-form-stack">
              <div className="bi-field">
                <label>역할</label>
                <select
                  className="bi-select"
                  value={rtuConfig.device_role || 'master'}
                  onChange={(e) => handleDeviceRoleChange(e.target.value)}
                >
                  <option value="master">마스터</option>
                  <option value="slave">슬래이브</option>
                </select>
              </div>
              {rtuConfig.device_role === 'slave' && (
                <div className="bi-field">
                  <label>Slave ID</label>
                  <input
                    type="number"
                    className="bi-input"
                    value={rtuConfig.slave_id || 1}
                    onChange={(e) => updateRtuConfig('slave_id', parseInt(e.target.value))}
                    min="1"
                    max="247"
                  />
                </div>
              )}
            </div>

            <div className="bi-form-stack">
              <div className="bi-field">
                <label>Baud Rate</label>
                <select
                  className="bi-select"
                  value={rtuConfig.baud_rate || 9600}
                  onChange={(e) => updateRtuConfig('baud_rate', parseInt(e.target.value))}
                >
                  {[1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200].map(bps => (
                    <option key={bps} value={bps}>{bps} bps</option>
                  ))}
                </select>
              </div>
            </div>

            <div className="bi-form-stack">
              <div className="bi-field">
                <label>Data Bits</label>
                <select
                  className="bi-select"
                  value={rtuConfig.data_bits || 8}
                  onChange={(e) => updateRtuConfig('data_bits', parseInt(e.target.value))}
                >
                  <option value={7}>7</option>
                  <option value={8}>8</option>
                </select>
              </div>
            </div>

            <div className="bi-form-stack">
              <div className="bi-field">
                <label>Parity / Stop</label>
                <div className="bi-field-row">
                  <select
                    className="bi-select"
                    value={rtuConfig.parity || 'N'}
                    onChange={(e) => updateRtuConfig('parity', e.target.value)}
                  >
                    <option value="N">N</option>
                    <option value="E">E</option>
                    <option value="O">O</option>
                  </select>
                  <select
                    className="bi-select"
                    value={rtuConfig.stop_bits || 1}
                    onChange={(e) => updateRtuConfig('stop_bits', parseInt(e.target.value))}
                  >
                    <option value={1}>1</option>
                    <option value={2}>2</option>
                  </select>
                </div>
              </div>
            </div>

          </div>
          {isSlaveIdDuplicate && (
            <div className="bi-error-banner" style={{ marginTop: '10px', padding: '8px', background: 'rgba(255, 0, 0, 0.1)', color: '#d32f2f', borderRadius: '4px', fontSize: '0.85rem' }}>
              <i className="fas fa-exclamation-triangle"></i> 동일 사이트 및 엔드포인트에 이미 같은 Slave ID가 존재합니다.
            </div>
          )}
        </div>
      )}

      {/* MQTT 상세 설정은 엔드포인트 아래로 이동됨 */}

      {/* 5. 고급 데이터 (엔지니어링 메타데이터) */}
      {(mode === 'edit' || mode === 'create' || (mode === 'view' && (Object.keys(displayData?.metadata || {}).length > 0 || Object.keys(displayData?.custom_fields || {}).length > 0))) && (
        <div className="bi-card full-width" style={{ marginTop: '16px' }}>
          <h3><i className="fas fa-database"></i> 확장 엔지니어링 데이터</h3>
          <div className="bi-grid-row" style={{ gridTemplateColumns: '1fr 1fr' }}>
            <div className="bi-field">
              <label>시스템 메타데이터 (JSON)</label>
              {mode === 'view' ? (
                <pre className="json-preview">{JSON.stringify(displayData?.metadata || {}, null, 2)}</pre>
              ) : (
                <textarea
                  className="bi-input json-input"
                  value={metadataStr}
                  onChange={(e) => {
                    setMetadataStr(e.target.value);
                    try {
                      const parsed = JSON.parse(e.target.value);
                      onUpdateField('metadata', parsed);
                    } catch { }
                  }}
                  placeholder='{"key": "value"}'
                  rows={6}
                />
              )}
            </div>
            <div className="bi-field">
              <label>사용자 정의 필드 (JSON)</label>
              {mode === 'view' ? (
                <pre className="json-preview">{JSON.stringify(displayData?.custom_fields || {}, null, 2)}</pre>
              ) : (
                <textarea
                  className="bi-input json-input"
                  value={customFieldsStr}
                  onChange={(e) => {
                    setCustomFieldsStr(e.target.value);
                    try {
                      const parsed = JSON.parse(e.target.value);
                      onUpdateField('custom_fields', parsed);
                    } catch { }
                  }}
                  placeholder='{"field": "info"}'
                  rows={6}
                />
              )}
            </div>
          </div>
        </div>
      )}

      {/* 5. 추가 정보 (View Mode Only) */}
      {mode === 'view' && (
        <div className="bi-card full-width">
          <h3>ℹ️ 추가 정보</h3>
          <div className="bi-field-row">
            <div className="bi-field">
              <label>생성: </label>
              <span className="form-val-inline">{displayData?.created_at ? new Date(displayData.created_at).toLocaleString() : '-'}</span>
            </div>
            <div className="bi-field">
              <label>수정: </label>
              <span className="form-val-inline">{displayData?.updated_at ? new Date(displayData.updated_at).toLocaleString() : '-'}</span>
            </div>
            {displayData?.installation_date && (
              <div className="bi-field">
                <label>설치: </label>
                <span className="form-val-inline">{new Date(displayData.installation_date).toLocaleDateString()}</span>
              </div>
            )}
          </div>
        </div>
      )}


      <style>{`
        .bi-container {
          display: flex;
          flex-direction: column;
          gap: 12px;
          height: 100%;
          overflow-y: auto;
          box-sizing: border-box;
          padding: 8px;
          background: #f8fafc;
        }

        .bi-grid-row {
          display: grid;
          grid-template-columns: repeat(4, 1fr);
          gap: 12px;
          flex-shrink: 0;
        }

        .bi-card {
           background: white;
           border: 1px solid #e2e8f0;
           border-radius: 8px;
           padding: 16px;
           box-shadow: 0 1px 2px rgba(0,0,0,0.05);
           display: flex;
           flex-direction: column;
           gap: 12px;
           min-width: 0; /* Prevent flex overflow */
        }

        .bi-card.full-width {
           width: 100%;
        }

        .bi-card h3 {
           margin: 0;
           font-size: 13px;
           font-weight: 600;
           color: #475569;
           padding-bottom: 8px;
           border-bottom: 1px solid #f1f5f9;
        }

        .bi-form-stack {
           display: flex;
           flex-direction: column;
           gap: 12px;
        }

        .bi-field-row {
           display: flex;
           gap: 12px;
           width: 100%;
        }

        .bi-field {
           display: flex;
           flex-direction: column;
           gap: 4px;
           min-width: 0; /* Text truncation */
        }
        .bi-field.flex-1 { flex: 1; }
        .bi-field.flex-2 { flex: 2; }

        .bi-field label {
           font-size: 11px;
           font-weight: 500;
           color: #64748b;
        }

        .bi-input, .bi-select {
           padding: 6px 10px;
           border: 1px solid #cbd5e1;
           border-radius: 4px;
           font-size: 13px;
           color: #1e293b;
           width: 100%;
           box-sizing: border-box;
           height: 32px;
        }
        .bi-input:focus, .bi-select:focus {
           outline: none;
           border-color: #3b82f6;
           box-shadow: 0 0 0 1px rgba(59,130,246,0.1);
        }
        textarea.bi-input {
           height: auto;
           resize: vertical;
        }

        .form-val {
           font-size: 13px;
           color: #334155;
           font-weight: 500;
           padding: 6px 0;
           white-space: nowrap;
           overflow: hidden;
           text-overflow: ellipsis;
        }
        .text-break {
            white-space: normal;
            word-break: break-all;
        }

        .form-val-inline {
           font-size: 13px;
           color: #334155;
           font-weight: 500;
        }

        .hint-text {
           font-size: 10px;
           color: #94a3b8;
           margin-top: 2px;
        }

        /* Status Badge Override for Left Alignment */
        .status-badge-left {
           display: inline-flex;
           align-items: center;
           padding: 2px 8px;
           border-radius: 999px;
           font-size: 11px;
           font-weight: 600;
           align-self: flex-start; /* FORCE LEFT */
        }
        .status-badge-left.enabled { background: #dcfce7; color: #166534; }
        .status-badge-left.disabled { background: #f1f5f9; color: #64748b; }

        .bi-btn-test {
           height: 32px;
           padding: 0 12px;
           background: #0ea5e9;
           color: white;
           border: none;
           border-radius: 4px;
           cursor: pointer;
           font-size: 12px;
           display: flex;
           align-items: center;
           gap: 6px;
           width: fit-content;
        }
        .bi-btn-test:hover { background: #0284c7; }
        .bi-btn-test:disabled { opacity: 0.6; cursor: not-allowed; }

        /* Responsive */
        @media (max-width: 1400px) {
           .bi-grid-row {
              grid-template-columns: repeat(2, 1fr);
           }
        }
        @media (max-width: 768px) {
           .bi-grid-row {
              grid-template-columns: 1fr;
           }
        }

        .bi-tag-container {
          display: flex;
          flex-wrap: wrap;
          gap: 4px;
          margin-top: 4px;
        }

        .bi-tag {
          background: #e0f2fe;
          color: #0369a1;
          padding: 2px 8px;
          border-radius: 4px;
          font-size: 11px;
          font-weight: 500;
          display: flex;
          align-items: center;
          gap: 4px;
        }

        .bi-tag.group-tag {
          background: #f0fdf4;
          color: #166534;
          border: 1px solid #bbf7d0;
        }

        /* 다중 선택 컨테이너 */
        .bi-multi-select-container {
          border: 1px solid #e2e8f0;
          border-radius: 4px;
          background: #fff;
          overflow: hidden;
          display: flex;
          flex-direction: column;
        }

        .bi-group-list-scroll {
          max-height: 120px;
          overflow-y: auto;
          padding: 4px;
        }

        .bi-group-item {
          display: flex;
          align-items: center;
          gap: 8px;
          padding: 6px 8px;
          border-radius: 4px;
          cursor: pointer;
          font-size: 12px;
          color: #475569;
          transition: all 0.2s;
        }

        .bi-group-item:hover {
          background: #f1f5f9;
          color: #1e293b;
        }

        .bi-group-item.selected {
          background: #eff6ff;
          color: #2563eb;
          font-weight: 500;
        }

        .bi-group-item i {
          font-size: 14px;
          width: 16px;
        }

        .bi-no-data {
          padding: 8px;
          font-size: 11px;
          color: #94a3b8;
          text-align: center;
        }

        .json-input {
          font-family: 'Monaco', 'Menlo', 'Ubuntu Mono', 'Consolas', monospace;
          font-size: 11px;
          line-height: 1.5;
          resize: vertical;
          background: #fafafa;
          color: #334155;
        }

        .json-preview {
          background: #f8fafc;
          border: 1px solid #e2e8f0;
          border-radius: 4px;
          padding: 8px;
          font-family: inherit;
          font-size: 11px;
          margin: 0;
          white-space: pre-wrap;
          word-break: break-all;
          color: #475569;
          max-height: 156px;
          overflow-y: auto;
        }
      `}</style>
    </div>
  );
};

export default DeviceBasicInfoTab;