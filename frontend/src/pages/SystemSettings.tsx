import React, { useState, useEffect } from 'react';
import { Settings, Save, RefreshCw, AlertTriangle, Database, Clock, Shield, Mail, Smartphone, HardDrive, Cpu, Activity } from 'lucide-react';

// 시스템 설정 카테고리 상수
const SETTING_CATEGORIES = {
  GENERAL: 'general',
  DATABASE: 'database', 
  COLLECTION: 'collection',
  NOTIFICATIONS: 'notifications',
  SECURITY: 'security',
  PERFORMANCE: 'performance',
  LOGGING: 'logging'
};

// 샘플 시스템 설정 데이터 (실제로는 API에서 가져옴)
const initialSettings = {
  // 일반 설정
  [SETTING_CATEGORIES.GENERAL]: {
    system_name: 'PulseOne IoT Platform',
    timezone: 'Asia/Seoul',
    language: 'ko-KR',
    date_format: 'YYYY-MM-DD',
    time_format: '24h',
    session_timeout: 480, // 분
    max_login_attempts: 5,
    account_lockout_duration: 30 // 분
  },

  // 데이터베이스 설정
  [SETTING_CATEGORIES.DATABASE]: {
    data_retention_days: 365,
    backup_enabled: true,
    backup_schedule: 'daily',
    backup_retention_days: 30,
    auto_vacuum_enabled: true,
    vacuum_schedule: 'weekly',
    connection_pool_size: 20,
    query_timeout: 30000 // ms
  },

  // 데이터 수집 설정
  [SETTING_CATEGORIES.COLLECTION]: {
    default_polling_interval: 1000, // ms
    max_concurrent_devices: 1000,
    device_timeout: 5000, // ms
    retry_attempts: 3,
    enable_packet_logging: false,
    packet_log_retention_hours: 24,
    virtual_point_calculation_interval: 500, // ms
    alarm_check_interval: 1000 // ms
  },

  // 알림 설정
  [SETTING_CATEGORIES.NOTIFICATIONS]: {
    email_enabled: true,
    email_server: 'smtp.company.com',
    email_port: 587,
    email_use_tls: true,
    email_username: 'noreply@company.com',
    email_from_name: 'PulseOne System',
    sms_enabled: false,
    sms_provider: 'none',
    push_notifications_enabled: true,
    notification_cooldown: 300 // 초
  },

  // 보안 설정
  [SETTING_CATEGORIES.SECURITY]: {
    password_min_length: 8,
    password_require_uppercase: true,
    password_require_lowercase: true,
    password_require_numbers: true,
    password_require_symbols: true,
    password_expiry_days: 90,
    two_factor_auth_required: false,
    ip_whitelist_enabled: false,
    allowed_ip_ranges: '',
    api_rate_limit: 100, // requests per minute
    enable_audit_logging: true
  },

  // 성능 설정
  [SETTING_CATEGORIES.PERFORMANCE]: {
    cpu_usage_threshold: 80, // %
    memory_usage_threshold: 85, // %
    disk_usage_threshold: 90, // %
    response_time_threshold: 2000, // ms
    enable_performance_monitoring: true,
    auto_scaling_enabled: false,
    cache_enabled: true,
    cache_ttl: 300 // 초
  },

  // 로깅 설정
  [SETTING_CATEGORIES.LOGGING]: {
    log_level: 'info',
    log_retention_days: 30,
    enable_debug_logging: false,
    log_to_file: true,
    log_to_database: true,
    max_log_file_size: 100, // MB
    compress_old_logs: true,
    enable_error_notifications: true
  }
};

const SystemSettings = () => {
  const [settings, setSettings] = useState(initialSettings);
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [activeCategory, setActiveCategory] = useState(SETTING_CATEGORIES.GENERAL);
  const [hasChanges, setHasChanges] = useState(false);
  const [testResults, setTestResults] = useState({});

  // 설정값 변경 핸들러
  const handleSettingChange = (category, key, value) => {
    setSettings(prev => ({
      ...prev,
      [category]: {
        ...prev[category],
        [key]: value
      }
    }));
    setHasChanges(true);
  };

  // 설정 저장
  const handleSaveSettings = async () => {
    setSaving(true);
    try {
      // API 호출로 설정 저장
      await new Promise(resolve => setTimeout(resolve, 1000)); // 시뮬레이션
      setHasChanges(false);
      alert('설정이 성공적으로 저장되었습니다.');
    } catch (error) {
      alert('설정 저장 중 오류가 발생했습니다.');
    } finally {
      setSaving(false);
    }
  };

  // 기본값으로 복원
  const handleResetToDefaults = () => {
    if (confirm('모든 설정을 기본값으로 복원하시겠습니까?')) {
      setSettings(initialSettings);
      setHasChanges(true);
    }
  };

  // 이메일 연결 테스트
  const testEmailConnection = async () => {
    setTestResults(prev => ({ ...prev, email: 'testing' }));
    try {
      await new Promise(resolve => setTimeout(resolve, 2000));
      setTestResults(prev => ({ ...prev, email: 'success' }));
    } catch (error) {
      setTestResults(prev => ({ ...prev, email: 'error' }));
    }
  };

  // 데이터베이스 연결 테스트
  const testDatabaseConnection = async () => {
    setTestResults(prev => ({ ...prev, database: 'testing' }));
    try {
      await new Promise(resolve => setTimeout(resolve, 1500));
      setTestResults(prev => ({ ...prev, database: 'success' }));
    } catch (error) {
      setTestResults(prev => ({ ...prev, database: 'error' }));
    }
  };

  // 카테고리별 아이콘 반환
  const getCategoryIcon = (category) => {
    const icons = {
      [SETTING_CATEGORIES.GENERAL]: Settings,
      [SETTING_CATEGORIES.DATABASE]: Database,
      [SETTING_CATEGORIES.COLLECTION]: RefreshCw,
      [SETTING_CATEGORIES.NOTIFICATIONS]: Mail,
      [SETTING_CATEGORIES.SECURITY]: Shield,
      [SETTING_CATEGORIES.PERFORMANCE]: Cpu,
      [SETTING_CATEGORIES.LOGGING]: Activity
    };
    return icons[category] || Settings;
  };

  // 카테고리별 제목 반환
  const getCategoryTitle = (category) => {
    const titles = {
      [SETTING_CATEGORIES.GENERAL]: '일반 설정',
      [SETTING_CATEGORIES.DATABASE]: '데이터베이스',
      [SETTING_CATEGORIES.COLLECTION]: '데이터 수집',
      [SETTING_CATEGORIES.NOTIFICATIONS]: '알림 설정',
      [SETTING_CATEGORIES.SECURITY]: '보안 설정',
      [SETTING_CATEGORIES.PERFORMANCE]: '성능 설정',
      [SETTING_CATEGORIES.LOGGING]: '로깅 설정'
    };
    return titles[category] || '설정';
  };

  // 테스트 결과 아이콘
  const getTestIcon = (result) => {
    if (result === 'testing') return <RefreshCw className="h-4 w-4 animate-spin text-blue-500" />;
    if (result === 'success') return <div className="h-4 w-4 bg-green-500 rounded-full" />;
    if (result === 'error') return <AlertTriangle className="h-4 w-4 text-red-500" />;
    return null;
  };

  // 입력 필드 컴포넌트
  const InputField = ({ label, value, onChange, type = 'text', suffix = '', min, max, required = false }) => (
    <div className="mb-4">
      <label className="block text-sm font-medium text-gray-700 mb-2">
        {label} {required && <span className="text-red-500">*</span>}
      </label>
      <div className="flex items-center">
        <input
          type={type}
          value={value}
          onChange={(e) => onChange(type === 'number' ? Number(e.target.value) : e.target.value)}
          min={min}
          max={max}
          className="flex-1 px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
        />
        {suffix && <span className="ml-2 text-sm text-gray-500">{suffix}</span>}
      </div>
    </div>
  );

  // 체크박스 컴포넌트
  const CheckboxField = ({ label, checked, onChange, description }) => (
    <div className="mb-4">
      <label className="flex items-start">
        <input
          type="checkbox"
          checked={checked}
          onChange={(e) => onChange(e.target.checked)}
          className="mt-1 mr-3 h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
        />
        <div>
          <span className="text-sm font-medium text-gray-700">{label}</span>
          {description && <p className="text-xs text-gray-500 mt-1">{description}</p>}
        </div>
      </label>
    </div>
  );

  // 셀렉트 컴포넌트
  const SelectField = ({ label, value, onChange, options, required = false }) => (
    <div className="mb-4">
      <label className="block text-sm font-medium text-gray-700 mb-2">
        {label} {required && <span className="text-red-500">*</span>}
      </label>
      <select
        value={value}
        onChange={(e) => onChange(e.target.value)}
        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
      >
        {options.map(option => (
          <option key={option.value} value={option.value}>{option.label}</option>
        ))}
      </select>
    </div>
  );

  return (
    <div className="p-6 max-w-7xl mx-auto">
      {/* 헤더 */}
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-bold text-gray-900 flex items-center">
          <Settings className="mr-2 h-6 w-6 text-blue-500" />
          시스템 설정
        </h1>
        
        <div className="flex items-center space-x-3">
          {hasChanges && (
            <span className="text-sm text-orange-600 bg-orange-100 px-3 py-1 rounded-full">
              저장되지 않은 변경사항이 있습니다
            </span>
          )}
          
          <button
            onClick={handleResetToDefaults}
            className="px-4 py-2 bg-gray-500 text-white rounded-lg hover:bg-gray-600"
          >
            기본값 복원
          </button>
          
          <button
            onClick={handleSaveSettings}
            disabled={!hasChanges || saving}
            className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed flex items-center"
          >
            {saving ? (
              <>
                <RefreshCw className="mr-2 h-4 w-4 animate-spin" />
                저장 중...
              </>
            ) : (
              <>
                <Save className="mr-2 h-4 w-4" />
                설정 저장
              </>
            )}
          </button>
        </div>
      </div>

      <div className="flex gap-6">
        {/* 사이드바 - 카테고리 목록 */}
        <div className="w-64 bg-white rounded-lg shadow">
          <div className="p-4">
            <h2 className="text-lg font-semibold text-gray-900 mb-4">설정 카테고리</h2>
            <nav className="space-y-2">
              {Object.values(SETTING_CATEGORIES).map(category => {
                const Icon = getCategoryIcon(category);
                return (
                  <button
                    key={category}
                    onClick={() => setActiveCategory(category)}
                    className={`w-full flex items-center px-3 py-2 text-left rounded-lg transition-colors ${
                      activeCategory === category
                        ? 'bg-blue-100 text-blue-700 border-l-4 border-blue-500'
                        : 'text-gray-700 hover:bg-gray-100'
                    }`}
                  >
                    <Icon className="mr-3 h-5 w-5" />
                    {getCategoryTitle(category)}
                  </button>
                );
              })}
            </nav>
          </div>
        </div>

        {/* 메인 콘텐츠 - 설정 상세 */}
        <div className="flex-1 bg-white rounded-lg shadow">
          <div className="p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-6">
              {getCategoryTitle(activeCategory)}
            </h2>

            {/* 일반 설정 */}
            {activeCategory === SETTING_CATEGORIES.GENERAL && (
              <div className="space-y-6">
                <InputField
                  label="시스템 이름"
                  value={settings.general.system_name}
                  onChange={(value) => handleSettingChange('general', 'system_name', value)}
                  required
                />
                
                <SelectField
                  label="시간대"
                  value={settings.general.timezone}
                  onChange={(value) => handleSettingChange('general', 'timezone', value)}
                  options={[
                    { value: 'Asia/Seoul', label: 'Asia/Seoul (한국 표준시)' },
                    { value: 'UTC', label: 'UTC (협정 세계시)' },
                    { value: 'America/New_York', label: 'America/New_York (동부 표준시)' }
                  ]}
                />

                <SelectField
                  label="언어"
                  value={settings.general.language}
                  onChange={(value) => handleSettingChange('general', 'language', value)}
                  options={[
                    { value: 'ko-KR', label: '한국어' },
                    { value: 'en-US', label: 'English' },
                    { value: 'ja-JP', label: '日本語' }
                  ]}
                />

                <InputField
                  label="세션 타임아웃"
                  type="number"
                  value={settings.general.session_timeout}
                  onChange={(value) => handleSettingChange('general', 'session_timeout', value)}
                  suffix="분"
                  min={30}
                  max={1440}
                />

                <InputField
                  label="최대 로그인 시도 횟수"
                  type="number"
                  value={settings.general.max_login_attempts}
                  onChange={(value) => handleSettingChange('general', 'max_login_attempts', value)}
                  min={3}
                  max={10}
                />
              </div>
            )}

            {/* 데이터베이스 설정 */}
            {activeCategory === SETTING_CATEGORIES.DATABASE && (
              <div className="space-y-6">
                <div className="bg-blue-50 p-4 rounded-lg border border-blue-200 mb-6">
                  <div className="flex items-center justify-between">
                    <div>
                      <h3 className="font-medium text-blue-900">데이터베이스 연결 테스트</h3>
                      <p className="text-sm text-blue-700 mt-1">현재 데이터베이스 연결 상태를 확인합니다</p>
                    </div>
                    <div className="flex items-center space-x-2">
                      {getTestIcon(testResults.database)}
                      <button
                        onClick={testDatabaseConnection}
                        disabled={testResults.database === 'testing'}
                        className="px-3 py-1 bg-blue-600 text-white text-sm rounded hover:bg-blue-700 disabled:opacity-50"
                      >
                        연결 테스트
                      </button>
                    </div>
                  </div>
                </div>

                <InputField
                  label="데이터 보존 기간"
                  type="number"
                  value={settings.database.data_retention_days}
                  onChange={(value) => handleSettingChange('database', 'data_retention_days', value)}
                  suffix="일"
                  min={30}
                  max={3650}
                />

                <CheckboxField
                  label="자동 백업 활성화"
                  checked={settings.database.backup_enabled}
                  onChange={(value) => handleSettingChange('database', 'backup_enabled', value)}
                  description="정기적으로 데이터베이스를 백업합니다"
                />

                {settings.database.backup_enabled && (
                  <>
                    <SelectField
                      label="백업 주기"
                      value={settings.database.backup_schedule}
                      onChange={(value) => handleSettingChange('database', 'backup_schedule', value)}
                      options={[
                        { value: 'hourly', label: '매시간' },
                        { value: 'daily', label: '매일' },
                        { value: 'weekly', label: '매주' }
                      ]}
                    />

                    <InputField
                      label="백업 보존 기간"
                      type="number"
                      value={settings.database.backup_retention_days}
                      onChange={(value) => handleSettingChange('database', 'backup_retention_days', value)}
                      suffix="일"
                      min={7}
                      max={365}
                    />
                  </>
                )}

                <InputField
                  label="연결 풀 크기"
                  type="number"
                  value={settings.database.connection_pool_size}
                  onChange={(value) => handleSettingChange('database', 'connection_pool_size', value)}
                  min={5}
                  max={100}
                />
              </div>
            )}

            {/* 데이터 수집 설정 */}
            {activeCategory === SETTING_CATEGORIES.COLLECTION && (
              <div className="space-y-6">
                <InputField
                  label="기본 폴링 간격"
                  type="number"
                  value={settings.collection.default_polling_interval}
                  onChange={(value) => handleSettingChange('collection', 'default_polling_interval', value)}
                  suffix="ms"
                  min={100}
                  max={60000}
                />

                <InputField
                  label="최대 동시 디바이스"
                  type="number"
                  value={settings.collection.max_concurrent_devices}
                  onChange={(value) => handleSettingChange('collection', 'max_concurrent_devices', value)}
                  min={10}
                  max={10000}
                />

                <InputField
                  label="디바이스 타임아웃"
                  type="number"
                  value={settings.collection.device_timeout}
                  onChange={(value) => handleSettingChange('collection', 'device_timeout', value)}
                  suffix="ms"
                  min={1000}
                  max={30000}
                />

                <InputField
                  label="재시도 횟수"
                  type="number"
                  value={settings.collection.retry_attempts}
                  onChange={(value) => handleSettingChange('collection', 'retry_attempts', value)}
                  min={1}
                  max={10}
                />

                <CheckboxField
                  label="패킷 로깅 활성화"
                  checked={settings.collection.enable_packet_logging}
                  onChange={(value) => handleSettingChange('collection', 'enable_packet_logging', value)}
                  description="통신 패킷을 상세히 기록합니다 (성능에 영향을 줄 수 있음)"
                />

                <InputField
                  label="가상포인트 계산 간격"
                  type="number"
                  value={settings.collection.virtual_point_calculation_interval}
                  onChange={(value) => handleSettingChange('collection', 'virtual_point_calculation_interval', value)}
                  suffix="ms"
                  min={100}
                  max={10000}
                />
              </div>
            )}

            {/* 알림 설정 */}
            {activeCategory === SETTING_CATEGORIES.NOTIFICATIONS && (
              <div className="space-y-6">
                <CheckboxField
                  label="이메일 알림 활성화"
                  checked={settings.notifications.email_enabled}
                  onChange={(value) => handleSettingChange('notifications', 'email_enabled', value)}
                />

                {settings.notifications.email_enabled && (
                  <div className="bg-gray-50 p-4 rounded-lg border space-y-4">
                    <div className="flex items-center justify-between mb-4">
                      <h4 className="font-medium text-gray-900">이메일 서버 설정</h4>
                      <div className="flex items-center space-x-2">
                        {getTestIcon(testResults.email)}
                        <button
                          onClick={testEmailConnection}
                          disabled={testResults.email === 'testing'}
                          className="px-3 py-1 bg-green-600 text-white text-sm rounded hover:bg-green-700 disabled:opacity-50"
                        >
                          연결 테스트
                        </button>
                      </div>
                    </div>

                    <InputField
                      label="SMTP 서버"
                      value={settings.notifications.email_server}
                      onChange={(value) => handleSettingChange('notifications', 'email_server', value)}
                      required
                    />

                    <InputField
                      label="포트"
                      type="number"
                      value={settings.notifications.email_port}
                      onChange={(value) => handleSettingChange('notifications', 'email_port', value)}
                      min={1}
                      max={65535}
                    />

                    <CheckboxField
                      label="TLS 사용"
                      checked={settings.notifications.email_use_tls}
                      onChange={(value) => handleSettingChange('notifications', 'email_use_tls', value)}
                    />

                    <InputField
                      label="사용자명"
                      value={settings.notifications.email_username}
                      onChange={(value) => handleSettingChange('notifications', 'email_username', value)}
                    />
                  </div>
                )}

                <CheckboxField
                  label="SMS 알림 활성화"
                  checked={settings.notifications.sms_enabled}
                  onChange={(value) => handleSettingChange('notifications', 'sms_enabled', value)}
                />

                <CheckboxField
                  label="푸시 알림 활성화"
                  checked={settings.notifications.push_notifications_enabled}
                  onChange={(value) => handleSettingChange('notifications', 'push_notifications_enabled', value)}
                />

                <InputField
                  label="알림 쿨다운"
                  type="number"
                  value={settings.notifications.notification_cooldown}
                  onChange={(value) => handleSettingChange('notifications', 'notification_cooldown', value)}
                  suffix="초"
                  min={60}
                  max={3600}
                />
              </div>
            )}

            {/* 보안 설정 */}
            {activeCategory === SETTING_CATEGORIES.SECURITY && (
              <div className="space-y-6">
                <div className="bg-yellow-50 p-4 rounded-lg border border-yellow-200 mb-6">
                  <div className="flex items-center">
                    <Shield className="h-5 w-5 text-yellow-600 mr-2" />
                    <p className="text-sm text-yellow-800">
                      보안 설정 변경 시 모든 사용자가 다시 로그인해야 할 수 있습니다.
                    </p>
                  </div>
                </div>

                <InputField
                  label="최소 패스워드 길이"
                  type="number"
                  value={settings.security.password_min_length}
                  onChange={(value) => handleSettingChange('security', 'password_min_length', value)}
                  min={6}
                  max={32}
                />

                <CheckboxField
                  label="대문자 필수"
                  checked={settings.security.password_require_uppercase}
                  onChange={(value) => handleSettingChange('security', 'password_require_uppercase', value)}
                />

                <CheckboxField
                  label="소문자 필수"
                  checked={settings.security.password_require_lowercase}
                  onChange={(value) => handleSettingChange('security', 'password_require_lowercase', value)}
                />

                <CheckboxField
                  label="숫자 필수"
                  checked={settings.security.password_require_numbers}
                  onChange={(value) => handleSettingChange('security', 'password_require_numbers', value)}
                />

                <CheckboxField
                  label="특수문자 필수"
                  checked={settings.security.password_require_symbols}
                  onChange={(value) => handleSettingChange('security', 'password_require_symbols', value)}
                />

                <InputField
                  label="패스워드 만료 기간"
                  type="number"
                  value={settings.security.password_expiry_days}
                  onChange={(value) => handleSettingChange('security', 'password_expiry_days', value)}
                  suffix="일"
                  min={30}
                  max={365}
                />

                <CheckboxField
                  label="2단계 인증 필수"
                  checked={settings.security.two_factor_auth_required}
                  onChange={(value) => handleSettingChange('security', 'two_factor_auth_required', value)}
                  description="모든 사용자에게 2단계 인증을 요구합니다"
                />

                <InputField
                  label="API 요청 제한"
                  type="number"
                  value={settings.security.api_rate_limit}
                  onChange={(value) => handleSettingChange('security', 'api_rate_limit', value)}
                  suffix="요청/분"
                  min={10}
                  max={1000}
                />
              </div>
            )}

            {/* 성능 설정 */}
            {activeCategory === SETTING_CATEGORIES.PERFORMANCE && (
              <div className="space-y-6">
                <InputField
                  label="CPU 사용률 임계값"
                  type="number"
                  value={settings.performance.cpu_usage_threshold}
                  onChange={(value) => handleSettingChange('performance', 'cpu_usage_threshold', value)}
                  suffix="%"
                  min={50}
                  max={95}
                />

                <InputField
                  label="메모리 사용률 임계값"
                  type="number"
                  value={settings.performance.memory_usage_threshold}
                  onChange={(value) => handleSettingChange('performance', 'memory_usage_threshold', value)}
                  suffix="%"
                  min={50}
                  max={95}
                />

                <InputField
                  label="디스크 사용률 임계값"
                  type="number"
                  value={settings.performance.disk_usage_threshold}
                  onChange={(value) => handleSettingChange('performance', 'disk_usage_threshold', value)}
                  suffix="%"
                  min={70}
                  max={95}
                />

                <CheckboxField
                  label="성능 모니터링 활성화"
                  checked={settings.performance.enable_performance_monitoring}
                  onChange={(value) => handleSettingChange('performance', 'enable_performance_monitoring', value)}
                />

                <CheckboxField
                  label="캐시 활성화"
                  checked={settings.performance.cache_enabled}
                  onChange={(value) => handleSettingChange('performance', 'cache_enabled', value)}
                />

                {settings.performance.cache_enabled && (
                  <InputField
                    label="캐시 TTL"
                    type="number"
                    value={settings.performance.cache_ttl}
                    onChange={(value) => handleSettingChange('performance', 'cache_ttl', value)}
                    suffix="초"
                    min={60}
                    max={3600}
                  />
                )}
              </div>
            )}

            {/* 로깅 설정 */}
            {activeCategory === SETTING_CATEGORIES.LOGGING && (
              <div className="space-y-6">
                <SelectField
                  label="로그 레벨"
                  value={settings.logging.log_level}
                  onChange={(value) => handleSettingChange('logging', 'log_level', value)}
                  options={[
                    { value: 'debug', label: 'DEBUG (모든 로그)' },
                    { value: 'info', label: 'INFO (일반 정보)' },
                    { value: 'warn', label: 'WARN (경고 이상)' },
                    { value: 'error', label: 'ERROR (오류만)' }
                  ]}
                />

                <InputField
                  label="로그 보존 기간"
                  type="number"
                  value={settings.logging.log_retention_days}
                  onChange={(value) => handleSettingChange('logging', 'log_retention_days', value)}
                  suffix="일"
                  min={7}
                  max={365}
                />

                <CheckboxField
                  label="디버그 로깅 활성화"
                  checked={settings.logging.enable_debug_logging}
                  onChange={(value) => handleSettingChange('logging', 'enable_debug_logging', value)}
                  description="상세한 디버그 정보를 로그에 포함합니다 (성능에 영향)"
                />

                <CheckboxField
                  label="파일로 로그 저장"
                  checked={settings.logging.log_to_file}
                  onChange={(value) => handleSettingChange('logging', 'log_to_file', value)}
                />

                <CheckboxField
                  label="데이터베이스에 로그 저장"
                  checked={settings.logging.log_to_database}
                  onChange={(value) => handleSettingChange('logging', 'log_to_database', value)}
                />

                <InputField
                  label="최대 로그 파일 크기"
                  type="number"
                  value={settings.logging.max_log_file_size}
                  onChange={(value) => handleSettingChange('logging', 'max_log_file_size', value)}
                  suffix="MB"
                  min={10}
                  max={1000}
                />

                <CheckboxField
                  label="오래된 로그 압축"
                  checked={settings.logging.compress_old_logs}
                  onChange={(value) => handleSettingChange('logging', 'compress_old_logs', value)}
                />
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default SystemSettings;