// ============================================================================
// frontend/src/pages/AlarmRuleTemplates.tsx
// 수정된 알람 템플릿 관리 페이지 - 중복 선언 오류 해결
// ============================================================================

import React, { useState, useEffect } from 'react';
import '../styles/alarm-rule-templates.css';
import alarmTemplatesApi, { 
  AlarmTemplate, 
  DataPoint, 
  CreatedAlarmRule,
  ApplyTemplateRequest 
} from '../api/services/alarmTemplatesApi';
import TemplateApplyModal from '../components/modals/TemplateApplyModal';
import TemplateCreateModal, { CreateTemplateRequest } from '../components/modals/TemplateCreateModal';
import { useConfirmContext } from '../components/common/ConfirmProvider';

const AlarmRuleTemplates: React.FC = () => {
  // ===================================================================
  // ConfirmProvider 사용 및 상태 관리
  // ===================================================================
  const { showConfirm } = useConfirmContext();
  
  const [templates, setTemplates] = useState<AlarmTemplate[]>([]);
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [createdRules, setCreatedRules] = useState<CreatedAlarmRule[]>([]);
  const [selectedTemplate, setSelectedTemplate] = useState<AlarmTemplate | null>(null);
  
  // UI 상태
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'browse' | 'created'>('browse');
  const [showApplyModal, setShowApplyModal] = useState(false);
  const [showCreateModal, setShowCreateModal] = useState(false);
  
  // 필터 상태
  const [templateFilter, setTemplateFilter] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');

  // ===================================================================
  // 헬퍼 함수들
  // ===================================================================
  const safeToString = (value: any): string => {
    if (value === null || value === undefined) return 'N/A';
    if (typeof value === 'string') return value;
    if (typeof value === 'number') return value.toString();
    if (typeof value === 'boolean') return value ? 'true' : 'false';
    if (typeof value === 'object') {
      try {
        return JSON.stringify(value);
      } catch {
        return '[객체]';
      }
    }
    return String(value);
  };

  const getConfigValue = (config: any, key: string, fallback: string = 'N/A'): string => {
    if (!config || typeof config !== 'object') return fallback;
    const value = config[key];
    return value !== undefined ? safeToString(value) : fallback;
  };

  const getTemplateTypeIcon = (type: string) => {
    switch(type) {
      case 'simple': return '🔧';
      case 'advanced': return '⚙️';
      case 'script': return '📝';
      default: return '❓';
    }
  };

  const renderTemplateConfig = (template: AlarmTemplate) => {
    const { condition_type, default_config } = template;
    
    switch (condition_type) {
      case 'threshold':
        return `임계값: ${getConfigValue(default_config, 'threshold')}, 데드밴드: ${getConfigValue(default_config, 'deadband') || getConfigValue(default_config, 'hysteresis')}`;
      
      case 'range':
        if (default_config && (default_config.min_value !== undefined && default_config.max_value !== undefined)) {
          return `범위: ${getConfigValue(default_config, 'min_value')} ~ ${getConfigValue(default_config, 'max_value')}`;
        } else if (default_config && default_config.high_high_limit !== undefined) {
          return `HH: ${getConfigValue(default_config, 'high_high_limit')}, H: ${getConfigValue(default_config, 'high_limit')}, L: ${getConfigValue(default_config, 'low_limit')}, LL: ${getConfigValue(default_config, 'low_low_limit')}`;
        }
        return '범위 설정';
      
      case 'digital':
        return `디지털 조건: ${getConfigValue(default_config, 'trigger_state') || getConfigValue(default_config, 'condition_template')}`;
      
      case 'pattern':
        return `패턴: ${getConfigValue(default_config, 'trigger_state', 'state_change')}, 시간: ${getConfigValue(default_config, 'hold_time', '1000')}ms`;
      
      default:
        return '사용자 정의 설정';
    }
  };

  // ===================================================================
  // 데이터 로딩 함수들
  // ===================================================================
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
      let templatesData: any[] = [];
      if (response && response.success && response.data) {
        if (Array.isArray(response.data.items)) {
          templatesData = response.data.items;
        } else if (Array.isArray(response.data)) {
          templatesData = response.data;
        }
      } else if (Array.isArray(response)) {
        templatesData = response;
      }
      
      // 데이터 변환 및 안전성 처리
      const transformedTemplates = templatesData.map((template: any) => {
        let frontendTemplateType = 'simple';
        if (template.condition_type === 'range') {
          frontendTemplateType = 'simple';
        } else if (template.condition_type === 'pattern') {
          frontendTemplateType = 'script';
        } else if (template.condition_type === 'script') {
          frontendTemplateType = 'script';
        }
        
        let parsedConfig = {};
        try {
          if (typeof template.default_config === 'string') {
            parsedConfig = JSON.parse(template.default_config);
          } else if (typeof template.default_config === 'object') {
            parsedConfig = template.default_config || {};
          }
        } catch (e) {
          console.warn('default_config 파싱 실패:', e);
          parsedConfig = {};
        }
        
        let parsedDataTypes: string[] = [];
        try {
          if (typeof template.applicable_data_types === 'string') {
            parsedDataTypes = JSON.parse(template.applicable_data_types);
          } else if (Array.isArray(template.applicable_data_types)) {
            parsedDataTypes = template.applicable_data_types;
          }
        } catch (e) {
          console.warn('applicable_data_types 파싱 실패:', e);
          parsedDataTypes = ['unknown'];
        }
        
        return {
          id: template.id,
          name: template.name || 'Unknown Template',
          description: template.description || '',
          category: template.category || 'general',
          template_type: frontendTemplateType,
          condition_type: template.condition_type || 'threshold',
          default_config: parsedConfig,
          severity: template.severity || 'medium',
          message_template: template.message_template || '',
          usage_count: template.usage_count || 0,
          is_active: template.is_active !== false,
          supports_hh_ll: template.condition_type === 'range',
          supports_script: template.condition_type === 'script',
          applicable_data_types: parsedDataTypes,
          created_at: template.created_at || new Date().toISOString(),
          updated_at: template.updated_at || new Date().toISOString()
        };
      });
      
      setTemplates(transformedTemplates);
      
    } catch (error) {
      console.error('템플릿 로딩 실패:', error);
      setError(error instanceof Error ? error.message : '템플릿을 불러오는데 실패했습니다.');
      setTemplates([]);
    } finally {
      setLoading(false);
    }
  };

  const loadDataPoints = async () => {
    try {
      console.log('🔍 데이터포인트 로딩 시작...');
      
      const data = await alarmTemplatesApi.getDataPoints({});
      
      console.log('📊 API에서 받은 데이터:', data);
      console.log('📊 데이터 타입:', typeof data, Array.isArray(data));
      
      if (Array.isArray(data)) {
        console.log(`✅ ${data.length}개 데이터포인트 로드 성공`);
        
        // 처음 몇 개 항목 상세 로그
        if (data.length > 0) {
          console.log('🔍 첫 번째 데이터포인트 샘플:', data[0]);
        }
        
        setDataPoints(data);
      } else {
        console.warn('❌ 예상과 다른 데이터 형식:', data);
        throw new Error('데이터포인트 데이터 형식이 올바르지 않습니다.');
      }
    } catch (error) {
      console.error('❌ 데이터포인트 로딩 실패:', error);
      setDataPoints([]);
    }
  };

  const loadCreatedRules = async () => {
    try {
      const response = await alarmTemplatesApi.getCreatedRules({
        ...(searchTerm && { search: searchTerm })
      });
      
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
      
      const transformedRules = rulesData.map((rule: any) => ({
        id: rule.id || 0,
        name: rule.name || 'Unknown Rule',
        template_name: rule.template_name || "기본 템플릿",
        data_point_name: rule.data_point_name || `데이터포인트 ${rule.target_id || 'Unknown'}`,
        device_name: rule.device_name || "알 수 없는 장치",
        site_name: rule.site_name || "기본 사이트",
        severity: (rule.severity ? rule.severity.toString().toUpperCase() : "MEDIUM"),
        enabled: rule.is_enabled !== undefined ? rule.is_enabled : true,
        created_at: rule.created_at || new Date().toISOString(),
        threshold_config: {
          threshold: rule.high_limit || rule.low_limit || 0,
          deadband: rule.deadband || 0
        }
      }));
      
      setCreatedRules(transformedRules);
    } catch (error) {
      console.error('생성된 규칙 로딩 실패:', error);
      setCreatedRules([]);
    }
  };

  // ===================================================================
  // 이벤트 핸들러들
  // ===================================================================
  const executeTemplateApplication = async (dataPointIds: number[]) => {
    console.log('🚀 템플릿 적용 시작:', {
      templateId: selectedTemplate!.id,
      templateName: selectedTemplate!.name,
      dataPointCount: dataPointIds.length,
      dataPointIds: dataPointIds
    });

    setLoading(true);
    try {
      // 백엔드 API와 일치하는 요청 형식
      const request = {
        target_ids: dataPointIds,        // data_point_ids → target_ids 변경
        target_type: 'data_point',       // 새로 추가
        custom_configs: {},
        rule_group_name: `${selectedTemplate!.name}_${new Date().toISOString().split('T')[0]}`
      };

      console.log('📤 API 요청 데이터:', request);

      const result = await alarmTemplatesApi.applyTemplate(selectedTemplate!.id, request);
      
      console.log('📥 API 응답:', result);

      // 응답 구조 체크 강화
      if (result && result.success) {
        await loadCreatedRules();
        setShowApplyModal(false);
        setSelectedTemplate(null);
        
        const rulesCreated = result.data?.rules_created || 0;
        
        // 성공 모달로 결과 표시
        alert(`성공! ${rulesCreated}개의 알람 규칙이 생성되었습니다.\n\n규칙 그룹: ${result.data?.rule_group_id || 'Unknown'}\n"생성된 규칙" 탭에서 확인할 수 있습니다.`);
        
        // 성공 후 생성된 규칙 탭으로 이동
        setActiveTab('created');
        
        console.log('✅ 템플릿 적용 성공:', {
          rulesCreated,
          ruleGroupId: result.data?.rule_group_id
        });
      } else {
        // 더 구체적인 에러 메시지
        const errorMessage = result?.message || result?.error || '알 수 없는 오류가 발생했습니다.';
        throw new Error(`템플릿 적용 실패: ${errorMessage}`);
      }
    } catch (error) {
      console.error('❌ 템플릿 적용 실패:', error);
      
      // 에러 타입별 처리
      let errorMessage = '템플릿 적용에 실패했습니다.';
      let errorDetails = '';
      
      if (error instanceof Error) {
        errorMessage = error.message;
      } else if (typeof error === 'object' && error !== null) {
        errorMessage = JSON.stringify(error);
      }
      
      // 사용자 친화적 에러 메시지
      if (errorMessage.includes('404')) {
        errorMessage = '템플릿을 찾을 수 없습니다.';
        errorDetails = '페이지를 새로고침하고 다시 시도해주세요.';
      } else if (errorMessage.includes('500')) {
        errorMessage = '서버 오류가 발생했습니다.';
        errorDetails = '잠시 후 다시 시도해주세요.\n문제가 지속되면 관리자에게 문의하세요.';
      } else if (errorMessage.includes('Network') || errorMessage.includes('fetch')) {
        errorMessage = '네트워크 연결 문제가 발생했습니다.';
        errorDetails = '인터넷 연결을 확인하고 다시 시도해주세요.';
      }
      
      // 에러 모달로 표시
      alert(`템플릿 적용 실패: ${errorMessage}\n\n${errorDetails}`);
    } finally {
      setLoading(false);
    }
  };

  const handleApplyTemplate = async (dataPointIds: number[]) => {
    if (!selectedTemplate) {
      alert('템플릿이 선택되지 않았습니다.');
      return;
    }

    if (dataPointIds.length === 0) {
      alert('최소 하나의 데이터포인트를 선택해주세요.');
      return;
    }

    // 적용 전 확인
    const confirmed = confirm(`"${selectedTemplate.name}" 템플릿을\n${dataPointIds.length}개의 데이터포인트에 적용하시겠습니까?\n\n이 작업으로 ${dataPointIds.length}개의 새로운 알람 규칙이 생성됩니다.`);
    
    if (!confirmed) return;

    await executeTemplateApplication(dataPointIds);
  };

  const handleTemplateSelect = (template: AlarmTemplate) => {
    console.log('🎯 템플릿 선택:', template);
    setSelectedTemplate(template);
    setShowApplyModal(true);
  };

  const handleModalClose = () => {
    setShowApplyModal(false);
    setSelectedTemplate(null);
  };

  const handleCreateTemplate = async (templateData: CreateTemplateRequest) => {
    setLoading(true);
    try {
      const response = await alarmTemplatesApi.createTemplate(templateData);
      
      if (response.success) {
        await loadTemplates();
        setShowCreateModal(false);
        
        // ConfirmProvider로 성공 메시지
        alert(`템플릿이 성공적으로 생성되었습니다: "${templateData.name}"\n\n템플릿 목록에서 확인할 수 있습니다.`);
      } else {
        throw new Error(response.message || '템플릿 생성에 실패했습니다.');
      }
    } catch (error) {
      console.error('템플릿 생성 실패:', error);
      
      // ConfirmProvider로 에러 메시지
      alert(`템플릿 생성 실패: ${error instanceof Error ? error.message : '템플릿 생성에 실패했습니다.'}`);
    } finally {
      setLoading(false);
    }
  };

  // ===================================================================
  // 초기 데이터 로딩
  // ===================================================================
  useEffect(() => {
    loadTemplates();
    loadDataPoints(); 
    loadCreatedRules();
  }, []);

  useEffect(() => {
    if (templateFilter !== 'all' || searchTerm) {
      loadTemplates();
    }
  }, [templateFilter, searchTerm]);

  useEffect(() => {
    if (searchTerm && activeTab === 'created') {
      loadCreatedRules();
    }
  }, [searchTerm, activeTab]);

  // ===================================================================
  // 계산된 값들
  // ===================================================================
  const totalRules = createdRules.length;
  const enabledRules = createdRules.filter(r => r.enabled).length;
  const disabledRules = totalRules - enabledRules;
  const criticalRules = createdRules.filter(r => r.severity === 'CRITICAL').length;

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

  const filteredRules = createdRules.filter(rule => {
    const matchesSearch = searchTerm === '' ||
      rule.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.data_point_name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      rule.site_name.toLowerCase().includes(searchTerm.toLowerCase());
    return matchesSearch;
  });

  // ===================================================================
  // 렌더링
  // ===================================================================
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
            <button className="btn btn-primary" disabled={loading} onClick={() => setShowCreateModal(true)}>
              ➕ 새 템플릿 생성
            </button>
          </div>
        </div>

        {/* 에러 표시 */}
        {error && (
          <div style={{ 
            background: '#fee2e2', border: '1px solid #fecaca', borderRadius: '8px', 
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
                        <h3 className="template-name">{template.name}</h3>
                      </div>
                      <p className="template-description">{template.description}</p>
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
                    <div className="config-label">기본 설정:</div>
                    <div className="config-value">{renderTemplateConfig(template)}</div>
                  </div>

                  <div className="template-footer">
                    <div className="usage-count">
                      <span className="usage-number">{template.usage_count}</span>회 사용됨
                    </div>
                    <button
                      onClick={() => handleTemplateSelect(template)}
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
                        <input type="checkbox" checked={false} onChange={() => {}} />
                      </div>
                      <div className="rule-title">
                        <h4 className="rule-name">{rule.name}</h4>
                        <div className="rule-template-name">템플릿: {rule.template_name}</div>
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

        {/* 모달들 */}
        <TemplateApplyModal
          isOpen={showApplyModal}
          template={selectedTemplate}
          dataPoints={dataPoints}
          onClose={handleModalClose}
          onApply={handleApplyTemplate}
          loading={loading}
        />

        <TemplateCreateModal
          isOpen={showCreateModal}
          onClose={() => setShowCreateModal(false)}
          onCreate={handleCreateTemplate}
          loading={loading}
        />
      </div>
    </div>
  );
};

export default AlarmRuleTemplates;