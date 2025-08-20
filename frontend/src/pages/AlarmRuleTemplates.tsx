import React, { useState, useEffect } from 'react';
import '../styles/alarm-rule-templates.css';
import alarmTemplatesApi, { 
  AlarmTemplate, 
  DataPoint, 
  CreatedAlarmRule,
  ApplyTemplateRequest 
} from '../api/services/alarmTemplatesApi';

const AlarmRuleTemplates: React.FC = () => {
  const [templates, setTemplates] = useState<AlarmTemplate[]>([]);
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [createdRules, setCreatedRules] = useState<CreatedAlarmRule[]>([]);
  const [selectedTemplate, setSelectedTemplate] = useState<AlarmTemplate | null>(null);
  const [selectedDataPoints, setSelectedDataPoints] = useState<number[]>([]);
  
  // 로딩 상태
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 탭 및 필터 상태
  const [activeTab, setActiveTab] = useState<'browse' | 'created'>('browse');
  const [templateFilter, setTemplateFilter] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  
  // 계층 필터 (올바른 순서)
  const [siteFilter, setSiteFilter] = useState('all');
  const [deviceFilter, setDeviceFilter] = useState('all');
  const [dataTypeFilter, setDataTypeFilter] = useState('all');
  
  const [showApplyModal, setShowApplyModal] = useState(false);

  useEffect(() => {
    loadTemplates();
    loadDataPoints(); 
    loadCreatedRules();
  }, []);

  // 필터 변경시 데이터 재로딩
  useEffect(() => {
    if (templateFilter !== 'all' || searchTerm) {
      loadTemplates();
    }
  }, [templateFilter, searchTerm]);

  useEffect(() => {
    if (siteFilter !== 'all' || deviceFilter !== 'all' || dataTypeFilter !== 'all') {
      loadDataPoints();
    }
  }, [siteFilter, deviceFilter, dataTypeFilter]);

  useEffect(() => {
    if (searchTerm && activeTab === 'created') {
      loadCreatedRules();
    }
  }, [searchTerm, activeTab]);

  const loadTemplates = async () => {
    try {
      setLoading(true);
      setError(null);
      
      const params = {
        is_active: true,
        ...(templateFilter !== 'all' && { 
          category: templateFilter === 'simple' || templateFilter === 'advanced' || templateFilter === 'script' 
            ? undefined 
            : templateFilter,
          template_type: ['simple', 'advanced', 'script'].includes(templateFilter) 
            ? templateFilter as 'simple' | 'advanced' | 'script'
            : undefined
        }),
        ...(searchTerm && { search: searchTerm })
      };

      const response = await alarmTemplatesApi.getTemplates(params);
      
      // 백엔드 API 응답 구조 처리
      let templatesData = [];
      if (response && response.success && response.data) {
        templatesData = Array.isArray(response.data) ? response.data : [];
      } else if (Array.isArray(response)) {
        templatesData = response;
      }
      
      // 백엔드 템플릿 구조를 프론트엔드 형태로 변환
      const transformedTemplates = templatesData.map(template => ({
        id: template.id,
        name: template.name,
        description: template.description,
        category: template.category,
        template_type: template.condition_type === 'threshold' ? 'simple' : 
                     template.condition_type === 'range' ? 'advanced' : 'script',
        condition_type: template.condition_type,
        default_config: template.default_config,
        severity: template.severity,
        message_template: template.message_template,
        usage_count: template.usage_count || 0,
        is_active: template.is_active,
        supports_hh_ll: template.condition_type === 'range',
        supports_script: template.condition_type === 'script',
        applicable_data_types: template.applicable_data_types,
        created_at: template.created_at,
        updated_at: template.updated_at
      }));
      
      setTemplates(transformedTemplates);
    } catch (error) {
      console.error('템플릿 로딩 실패:', error);
      setError(error instanceof Error ? error.message : '템플릿을 불러오는데 실패했습니다.');
      
      // 실패 시 목업 데이터 사용
      const mockTemplates: AlarmTemplate[] = [
        {
          id: 1,
          name: "온도 상한 기본",
          description: "온도 센서용 단순 상한 임계값 알람", 
          category: "Temperature",
          template_type: "simple",
          condition_type: "threshold",
          default_config: { 
            threshold: 80, 
            deadband: 2,
            alarm_type: "analog_high"
          },
          severity: "high",
          message_template: "{site_name} {device_name} 온도가 {threshold}°C를 초과했습니다 (현재: {value}°C)",
          usage_count: 45,
          is_active: true,
          supports_hh_ll: false,
          supports_script: false,
          applicable_data_types: ["temperature"],
          created_at: "2025-01-01T00:00:00Z",
          updated_at: "2025-01-01T00:00:00Z"
        },
        {
          id: 2,
          name: "온도 4단계 경보",
          description: "온도 센서용 HH/H/L/LL 4단계 임계값 알람",
          category: "Temperature", 
          template_type: "advanced",
          condition_type: "threshold",
          default_config: { 
            high_high_limit: 100,
            high_limit: 80,
            low_limit: 5,
            low_low_limit: 0,
            deadband: 2,
            alarm_type: "analog_multi"
          },
          severity: "critical",
          message_template: "{site_name} {device_name} 온도 {level} 알람: {value}°C (임계값: {threshold}°C)",
          usage_count: 12,
          is_active: true,
          supports_hh_ll: true,
          supports_script: false,
          applicable_data_types: ["temperature"],
          created_at: "2025-01-01T00:00:00Z",
          updated_at: "2025-01-01T00:00:00Z"
        },
        {
          id: 3,
          name: "압력 범위 모니터링",
          description: "압력이 정상 범위(1~5bar)를 벗어날 때 알람",
          category: "Pressure",
          template_type: "simple",
          condition_type: "range", 
          default_config: { 
            min_value: 1.0,
            max_value: 5.0,
            deadband: 0.1,
            alarm_type: "analog_range"
          },
          severity: "medium",
          message_template: "{site_name} {device_name} 압력이 정상범위를 벗어났습니다: {value}bar",
          usage_count: 28,
          is_active: true,
          supports_hh_ll: false,
          supports_script: false,
          applicable_data_types: ["pressure"],
          created_at: "2025-01-01T00:00:00Z",
          updated_at: "2025-01-01T00:00:00Z"
        }
      ];
      setTemplates(mockTemplates);
    } finally {
      setLoading(false);
    }
  };

  const loadDataPoints = async () => {
    try {
      const filters = {
        ...(siteFilter !== 'all' && { site_name: siteFilter }),
        ...(deviceFilter !== 'all' && { device_name: deviceFilter }),
        ...(dataTypeFilter !== 'all' && { data_type: dataTypeFilter })
      };

      const data = await alarmTemplatesApi.getDataPoints(filters);
      
      // API 응답이 배열인지 확인
      if (Array.isArray(data)) {
        setDataPoints(data);
      } else {
        console.warn('데이터포인트 응답이 배열이 아님:', data);
        throw new Error('데이터포인트 데이터 형식이 올바르지 않습니다.');
      }
    } catch (error) {
      console.error('데이터포인트 로딩 실패:', error);
      
      // 실패 시 목업 데이터 사용
      const mockDataPoints: DataPoint[] = [
        { 
          id: 1, 
          name: "Temperature_Sensor_1", 
          device_name: "PLC-001", 
          site_name: "공장A 생산라인1", 
          data_type: "temperature", 
          unit: "°C", 
          current_value: 23.5, 
          last_updated: "2025-01-20T15:30:00Z",
          supports_analog: true,
          supports_digital: false
        },
        { 
          id: 2, 
          name: "Pressure_Main", 
          device_name: "RTU-001", 
          site_name: "공장A 유틸리티", 
          data_type: "pressure", 
          unit: "bar", 
          current_value: 3.2, 
          last_updated: "2025-01-20T15:30:00Z",
          supports_analog: true,
          supports_digital: false
        },
        { 
          id: 3, 
          name: "Motor_Status", 
          device_name: "Drive-001", 
          site_name: "공장B 컨베이어", 
          data_type: "digital", 
          unit: "", 
          current_value: 1, 
          last_updated: "2025-01-20T15:30:00Z",
          supports_analog: false,
          supports_digital: true
        },
        { 
          id: 4, 
          name: "Flow_Rate_01", 
          device_name: "RTU-002", 
          site_name: "공장C 냉각수", 
          data_type: "flow", 
          unit: "L/min", 
          current_value: 150.3, 
          last_updated: "2025-01-20T15:30:00Z",
          supports_analog: true,
          supports_digital: false
        }
      ];
      setDataPoints(mockDataPoints);
    }
  };

  const loadCreatedRules = async () => {
    try {
      const response = await alarmTemplatesApi.getCreatedRules({
        ...(searchTerm && { search: searchTerm })
      });
      
      // 백엔드 API 응답 구조 처리: { success: true, data: { items: [...] } }
      let rulesData = [];
      if (response && response.success && response.data) {
        if (Array.isArray(response.data.items)) {
          rulesData = response.data.items;
        } else if (Array.isArray(response.data)) {
          rulesData = response.data;
        }
      } else if (Array.isArray(response)) {
        rulesData = response;
      }
      
      // 백엔드 데이터 구조를 프론트엔드에서 사용하는 형태로 변환
      const transformedRules = rulesData.map((rule: any) => ({
        id: rule.id,
        name: rule.name,
        template_name: rule.template_name || "기본 템플릿",
        data_point_name: rule.data_point_name || `데이터포인트 ${rule.target_id}`,
        device_name: rule.device_name || "알 수 없는 장치",
        site_name: rule.site_name || "기본 사이트",
        severity: rule.severity ? rule.severity.toUpperCase() : "MEDIUM",
        enabled: rule.is_enabled !== undefined ? rule.is_enabled : true,
        created_at: rule.created_at,
        threshold_config: {
          threshold: rule.high_limit || rule.low_limit,
          deadband: rule.deadband || 0
        }
      }));
      
      setCreatedRules(transformedRules);
    } catch (error) {
      console.error('생성된 규칙 로딩 실패:', error);
      setCreatedRules([]);
    }
  };

  // 통계 계산
  const totalRules = createdRules.length;
  const enabledRules = createdRules.filter(r => r.enabled).length;
  const disabledRules = totalRules - enabledRules;
  const criticalRules = createdRules.filter(r => r.severity === 'CRITICAL').length;

  // 필터링된 템플릿
  const filteredTemplates = templates.filter(template => {
    const matchesSearch = searchTerm === '' || 
      template.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      template.description.toLowerCase().includes(searchTerm.toLowerCase());
    const matchesFilter = templateFilter === 'all' || 
      (templateFilter === 'simple' && template.template_type === 'simple') ||
      (templateFilter === 'advanced' && template.template_type === 'advanced') ||
      (templateFilter === 'script' && template.template_type === 'script') ||
      template.category.toLowerCase() === templateFilter.toLowerCase();
    return matchesSearch && matchesFilter && template.is_active;
  });

  // 필터링된 생성 규칙
  const filteredRules = createdRules.filter(rule => {
    const matchesSearch = searchTerm === '' ||
      rule.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.data_point_name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.site_name.toLowerCase().includes(searchTerm.toLowerCase());
    return matchesSearch;
  });

  // 필터링된 데이터포인트 (올바른 계층 순서)
  const filteredDataPoints = dataPoints.filter(point => {
    const matchesSite = siteFilter === 'all' || point.site_name === siteFilter;
    const matchesDevice = deviceFilter === 'all' || point.device_name === deviceFilter;
    const matchesType = dataTypeFilter === 'all' || point.data_type === dataTypeFilter;
    
    // 선택된 템플릿과 호환성 체크
    if (selectedTemplate) {
      if (selectedTemplate.template_type === 'script') return true; // 스크립트는 모든 타입 지원
      if (selectedTemplate.condition_type === 'pattern' && !point.supports_digital) return false;
      if (selectedTemplate.condition_type === 'threshold' && !point.supports_analog) return false;
      if (selectedTemplate.condition_type === 'range' && !point.supports_analog) return false;
    }
    
    return matchesSite && matchesDevice && matchesType;
  });

  // 고유 값들 (올바른 계층 순서)
  const sites = ['all', ...new Set(dataPoints.map(d => d.site_name))];
  const devices = ['all', ...new Set(dataPoints.filter(d => siteFilter === 'all' || d.site_name === siteFilter).map(d => d.device_name))];
  const dataTypes = ['all', ...new Set(dataPoints.map(d => d.data_type))];

  const handleApplyTemplate = async () => {
    if (!selectedTemplate || selectedDataPoints.length === 0) {
      alert('템플릿과 데이터포인트를 선택해주세요.');
      return;
    }

    // 호환성 재검증
    const incompatiblePoints = selectedDataPoints.filter(pointId => {
      const point = dataPoints.find(p => p.id === pointId);
      if (!point) return true;
      
      if (selectedTemplate.condition_type === 'pattern' && !point.supports_digital) return true;
      if (selectedTemplate.condition_type === 'threshold' && !point.supports_analog) return true;
      if (selectedTemplate.condition_type === 'range' && !point.supports_analog) return true;
      
      return false;
    });

    if (incompatiblePoints.length > 0) {
      alert(`선택된 데이터포인트 중 ${incompatiblePoints.length}개가 이 템플릿과 호환되지 않습니다.`);
      return;
    }

    setLoading(true);
    try {
      const request: ApplyTemplateRequest = {
        data_point_ids: selectedDataPoints,
        custom_configs: {},
        rule_group_name: `${selectedTemplate.name}_${new Date().toISOString().split('T')[0]}`
      };

      const result = await alarmTemplatesApi.applyTemplate(selectedTemplate.id, request);
      
      if (result.success) {
        await loadCreatedRules();
        setShowApplyModal(false);
        setSelectedDataPoints([]);
        alert(`${result.data.rules_created}개의 알람 규칙이 생성되었습니다.`);
      } else {
        throw new Error(result.message || '템플릿 적용에 실패했습니다.');
      }
    } catch (error) {
      console.error('템플릿 적용 실패:', error);
      alert(error instanceof Error ? error.message : '템플릿 적용에 실패했습니다.');
    } finally {
      setLoading(false);
    }
  };

  const getTemplateTypeIcon = (type: string) => {
    switch(type) {
      case 'simple': return '🔧';
      case 'advanced': return '⚙️';
      case 'script': return '📝';
      default: return '❓';
    }
  };

  const getTemplateTypeColor = (type: string) => {
    switch(type) {
      case 'simple': return '#10b981';
      case 'advanced': return '#f59e0b';
      case 'script': return '#8b5cf6';
      default: return '#6b7280';
    }
  };

  const getSeverityColor = (severity: string) => {
    switch(severity.toLowerCase()) {
      case 'critical': return '#dc2626';
      case 'high': return '#f59e0b'; 
      case 'medium': return '#3b82f6';
      case 'low': return '#6b7280';
      default: return '#6b7280';
    }
  };

  return (
    <div className="alarm-rule-templates-container">
      <div style={{ maxWidth: '1400px', margin: '0 auto', padding: '24px' }}>
        {/* 헤더 */}
        <div className="page-header">
          <div>
            <h1 className="page-title">알람 템플릿 관리</h1>
            <p className="page-subtitle">
              사전 정의된 템플릿으로 빠르고 일관된 알람 규칙을 생성하세요
            </p>
          </div>
          <div className="page-actions">
            <button className="btn" disabled={loading}>
              📥 템플릿 내보내기
            </button>
            <button className="btn" disabled={loading}>
              📤 템플릿 가져오기
            </button>
            <button className="btn btn-primary" disabled={loading}>
              ➕ 새 템플릿 생성
            </button>
          </div>
        </div>

        {/* 에러 표시 */}
        {error && (
          <div style={{ 
            background: '#fef2f2', border: '1px solid #fecaca', borderRadius: '8px', 
            padding: '16px', marginBottom: '24px', color: '#dc2626'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span>⚠️ {error}</span>
              <button 
                onClick={() => {
                  setError(null);
                  loadTemplates();
                  loadDataPoints();
                  loadCreatedRules();
                }}
                className="btn"
                style={{ marginLeft: '16px' }}
              >
                🔄 다시 시도
              </button>
            </div>
          </div>
        )}

        {/* 로딩 표시 */}
        {loading && (
          <div style={{ 
            background: '#eff6ff', border: '1px solid #bfdbfe', borderRadius: '8px', 
            padding: '16px', marginBottom: '24px', color: '#1e40af', textAlign: 'center'
          }}>
            🔄 데이터를 불러오는 중...
          </div>
        )}

        {/* 탭 네비게이션 */}
        <div className="tab-navigation">
          <button
            onClick={() => setActiveTab('browse')}
            className={`tab-button ${activeTab === 'browse' ? 'active' : ''}`}
          >
            ⚙️ 템플릿 찾기
            <span className="tab-badge">{filteredTemplates.length}</span>
          </button>
          <button
            onClick={() => setActiveTab('created')}
            className={`tab-button ${activeTab === 'created' ? 'active' : ''}`}
          >
            👁️ 생성된 규칙
            <span className="tab-badge">{totalRules}</span>
          </button>
        </div>

        {/* 템플릿 찾기 탭 */}
        {activeTab === 'browse' && (
          <div>
            {/* 필터 패널 */}
            <div className="filter-panel">
              <div className="filter-row">
                <div className="filter-group flex-1">
                  <label>템플릿 검색</label>
                  <div className="search-container">
                    <span className="search-icon">🔍</span>
                    <input
                      type="text"
                      placeholder="템플릿명, 설명, 카테고리로 검색..."
                      value={searchTerm}
                      onChange={(e) => setSearchTerm(e.target.value)}
                      className="search-input"
                    />
                  </div>
                </div>
                <div className="filter-group">
                  <label>템플릿 유형</label>
                  <select 
                    value={templateFilter} 
                    onChange={(e) => setTemplateFilter(e.target.value)} 
                    className="filter-select"
                  >
                    <option value="all">모든 유형</option>
                    <option value="simple">🔧 단순 템플릿</option>
                    <option value="advanced">⚙️ 고급 템플릿</option>
                    <option value="script">📝 스크립트 템플릿</option>
                    <option value="temperature">🌡️ 온도</option>
                    <option value="pressure">⚡ 압력</option>
                    <option value="digital">📱 디지털</option>
                    <option value="custom">🔧 커스텀</option>
                  </select>
                </div>
              </div>
            </div>

            {/* 템플릿 카드 목록 */}
            <div className="templates-grid">
              {filteredTemplates.map(template => (
                <div key={template.id} className="template-card">
                  <div className="template-header">
                    <div className="template-title">
                      <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '8px' }}>
                        <span className="template-icon">{getTemplateTypeIcon(template.template_type)}</span>
                        <h3 className="template-name">
                          {template.name}
                        </h3>
                      </div>
                      <p className="template-description">
                        {template.description}
                      </p>
                      <div className="template-badges">
                        <span className={`template-badge type-${template.template_type}`}>
                          {template.template_type}
                        </span>
                        <span className={`template-badge severity-${template.severity}`}>
                          {template.severity}
                        </span>
                        <span className="template-badge category">
                          {template.category}
                        </span>
                      </div>
                    </div>
                  </div>

                  <div className="template-config">
                    <div className="config-label">
                      기본 설정:
                    </div>
                    <div className="config-value">
                      {template.template_type === 'simple' && template.condition_type === 'threshold' && (
                        `임계값: ${template.default_config.threshold}, 데드밴드: ${template.default_config.deadband}`
                      )}
                      {template.template_type === 'advanced' && template.supports_hh_ll && (
                        `HH: ${template.default_config.high_high_limit}, H: ${template.default_config.high_limit}, L: ${template.default_config.low_limit}, LL: ${template.default_config.low_low_limit}`
                      )}
                      {template.template_type === 'simple' && template.condition_type === 'range' && (
                        `범위: ${template.default_config.min_value} ~ ${template.default_config.max_value}`
                      )}
                      {template.template_type === 'script' && template.supports_script && (
                        `JavaScript 기반 복합 조건`
                      )}
                    </div>
                  </div>

                  <div className="template-footer">
                    <div className="usage-count">
                      <span className="usage-number">{template.usage_count}</span>회 사용됨
                    </div>
                    <button
                      onClick={() => {
                        setSelectedTemplate(template);
                        setShowApplyModal(true);
                      }}
                      className="apply-button"
                      disabled={loading}
                    >
                      ⚙️ 적용하기
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* 생성된 규칙 탭 */}
        {activeTab === 'created' && (
          <div>
            {/* 통계 카드 */}
            <div className="stats-grid">
              <div className="stat-card total">
                <div className="stat-value">{totalRules}</div>
                <div className="stat-label">총 알람 규칙</div>
              </div>
              <div className="stat-card enabled">
                <div className="stat-value enabled">{enabledRules}</div>
                <div className="stat-label">활성화됨</div>
              </div>
              <div className="stat-card disabled">
                <div className="stat-value disabled">{disabledRules}</div>
                <div className="stat-label">비활성화됨</div>
              </div>
              <div className="stat-card critical">
                <div className="stat-value critical">{criticalRules}</div>
                <div className="stat-label">Critical</div>
              </div>
            </div>

            {/* 생성된 규칙 목록 */}
            <div style={{ background: 'white', borderRadius: '12px', border: '1px solid #e5e7eb', padding: '24px' }}>
              <div style={{ marginBottom: '20px' }}>
                <div style={{ position: 'relative', maxWidth: '400px' }}>
                  <span style={{ position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)', color: '#9ca3af' }}>🔍</span>
                  <input
                    type="text"
                    placeholder="규칙명, 데이터포인트, 사이트명으로 검색..."
                    value={searchTerm}
                    onChange={(e) => setSearchTerm(e.target.value)}
                    style={{ 
                      width: '100%', padding: '12px 12px 12px 44px', 
                      border: '2px solid #e5e7eb', borderRadius: '8px', fontSize: '14px'
                    }}
                  />
                </div>
              </div>

              <div className="rules-grid">
                {filteredRules.map(rule => (
                  <div key={rule.id} className={`rule-card ${rule.enabled ? 'enabled' : 'disabled'}`}>
                    <div className="rule-header">
                      <div className="rule-select">
                        <input
                          type="checkbox"
                          checked={false}
                          onChange={() => {}}
                        />
                      </div>
                      <div className="rule-title">
                        <h4 className="rule-name">
                          {rule.name}
                        </h4>
                        <div className="rule-template-name">
                          템플릿: {rule.template_name}
                        </div>
                      </div>
                      <span className={`template-badge ${rule.enabled ? 'enabled' : 'disabled'}`}>
                        {rule.enabled ? '활성' : '비활성'}
                      </span>
                    </div>
                    
                    <div className="rule-content">
                      <div className="rule-location">
                        <span className="rule-label">위치:</span> {rule.site_name} → {rule.device_name}
                      </div>
                      <div className="rule-datapoint">
                        <span className="rule-label">데이터포인트:</span> {rule.data_point_name}
                      </div>
                      <div className="rule-severity">
                        <span className="rule-label">심각도:</span>
                        <span className={`template-badge severity-${rule.severity.toLowerCase()}`}>
                          {rule.severity}
                        </span>
                      </div>
                      <div className="rule-created">
                        생성일: {new Date(rule.created_at).toLocaleDateString('ko-KR')}
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {/* 템플릿 적용 모달 */}
        {showApplyModal && selectedTemplate && (
          <div className="modal-overlay">
            <div className="modal-container">
              <div className="modal-header">
                <h2 className="modal-title">템플릿 적용: {selectedTemplate.name}</h2>
                <p className="modal-subtitle">
                  이 템플릿을 적용할 데이터포인트를 선택하세요
                </p>
                {selectedTemplate.template_type !== 'simple' && (
                  <div className="modal-warning">
                    <div className="modal-warning-text">
                      ⚠️ 이 템플릿은 {selectedTemplate.template_type === 'advanced' ? '고급 설정' : '스크립트 기반'} 템플릿입니다. 
                      적용 후 세부 설정을 확인해주세요.
                    </div>
                  </div>
                )}
              </div>
              
              <div className="modal-content">
                {/* 계층 필터 (올바른 순서) */}
                <div className="hierarchy-filters">
                  <div className="filter-step">
                    <label className="filter-step-label">1️⃣ 사이트 선택</label>
                    <select 
                      value={siteFilter} 
                      onChange={(e) => {
                        setSiteFilter(e.target.value);
                        setDeviceFilter('all'); // 하위 필터 리셋
                      }}
                      className="filter-step-select"
                    >
                      {sites.map(site => <option key={site} value={site}>{site === 'all' ? '모든 사이트' : site}</option>)}
                    </select>
                  </div>
                  <div className="filter-step">
                    <label className="filter-step-label">2️⃣ 디바이스 선택</label>
                    <select 
                      value={deviceFilter} 
                      onChange={(e) => setDeviceFilter(e.target.value)}
                      className="filter-step-select"
                    >
                      {devices.map(device => <option key={device} value={device}>{device === 'all' ? '모든 디바이스' : device}</option>)}
                    </select>
                  </div>
                  <div className="filter-step">
                    <label className="filter-step-label">3️⃣ 데이터 타입</label>
                    <select 
                      value={dataTypeFilter} 
                      onChange={(e) => setDataTypeFilter(e.target.value)}
                      className="filter-step-select"
                    >
                      {dataTypes.map(type => <option key={type} value={type}>{type === 'all' ? '모든 타입' : type}</option>)}
                    </select>
                  </div>
                </div>

                {/* 호환성 정보 */}
                {selectedTemplate && (
                  <div className="compatibility-info">
                    <div className="compatibility-title">
                      ℹ️ 템플릿 호환성 정보
                    </div>
                    <div className="compatibility-text">
                      이 템플릿은 <strong>{selectedTemplate.condition_type}</strong> 타입으로, 
                      {selectedTemplate.condition_type === 'threshold' || selectedTemplate.condition_type === 'range' 
                        ? ' 아날로그 데이터포인트' : ' 디지털 데이터포인트'}에만 적용 가능합니다.
                      {selectedTemplate.supports_script && ' JavaScript 스크립트를 지원합니다.'}
                    </div>
                  </div>
                )}

                {/* 데이터포인트 목록 */}
                <div className="datapoints-table">
                  <div className="table-header">
                    <div>
                      <input
                        type="checkbox"
                        onChange={(e) => {
                          if (e.target.checked) {
                            setSelectedDataPoints(filteredDataPoints.map(p => p.id));
                          } else {
                            setSelectedDataPoints([]);
                          }
                        }}
                        checked={selectedDataPoints.length === filteredDataPoints.length && filteredDataPoints.length > 0}
                        className="table-checkbox"
                      />
                    </div>
                    <div>데이터포인트 정보</div>
                    <div>디바이스</div>
                    <div>사이트</div>
                    <div>현재값</div>
                  </div>
                  
                  {filteredDataPoints.map(point => {
                    const isSelected = selectedDataPoints.includes(point.id);
                    const isCompatible = (
                      selectedTemplate.template_type === 'script' ||
                      (selectedTemplate.condition_type === 'pattern' && point.supports_digital) ||
                      ((selectedTemplate.condition_type === 'threshold' || selectedTemplate.condition_type === 'range') && point.supports_analog)
                    );
                    
                    return (
                      <div 
                        key={point.id} 
                        className={`table-row ${isSelected ? 'selected' : ''} ${!isCompatible ? 'incompatible' : ''}`}
                      >
                        <div>
                          <input
                            type="checkbox"
                            checked={isSelected}
                            disabled={!isCompatible}
                            onChange={(e) => {
                              if (e.target.checked) {
                                setSelectedDataPoints([...selectedDataPoints, point.id]);
                              } else {
                                setSelectedDataPoints(selectedDataPoints.filter(id => id !== point.id));
                              }
                            }}
                            className="table-checkbox"
                          />
                        </div>
                        <div className="datapoint-info">
                          <div className="datapoint-name">
                            {point.name}
                          </div>
                          <div className="datapoint-details">
                            {point.data_type} • {point.unit || 'N/A'}
                            {!isCompatible && (
                              <span className="incompatible-label">
                                (호환 불가)
                              </span>
                            )}
                          </div>
                        </div>
                        <div className="table-cell">{point.device_name}</div>
                        <div className="table-cell">{point.site_name}</div>
                        <div className="current-value">
                          {point.current_value} {point.unit}
                        </div>
                      </div>
                    );
                  })}
                </div>

                {filteredDataPoints.length === 0 && (
                  <div className="empty-state">
                    선택한 조건에 맞는 데이터포인트가 없습니다.
                  </div>
                )}
              </div>
              
              <div className="modal-footer">
                <div>
                  {selectedDataPoints.length > 0 && (
                    <span style={{ fontWeight: 600, color: '#1f2937' }}>
                      {selectedDataPoints.length}개 포인트 선택됨
                    </span>
                  )}
                </div>
                <div className="modal-actions">
                  <button
                    onClick={() => {
                      setShowApplyModal(false);
                      setSelectedDataPoints([]);
                      setSiteFilter('all');
                      setDeviceFilter('all');
                      setDataTypeFilter('all');
                    }}
                    className="btn"
                  >
                    취소
                  </button>
                  <button
                    onClick={handleApplyTemplate}
                    disabled={selectedDataPoints.length === 0 || loading}
                    className="btn btn-primary"
                  >
                    적용하기 ({selectedDataPoints.length}개)
                  </button>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default AlarmRuleTemplates;