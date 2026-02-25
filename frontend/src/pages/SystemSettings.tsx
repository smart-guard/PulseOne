import React, { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { Settings, Save, RefreshCw, AlertTriangle, Database, Clock, Shield, Mail, Smartphone, HardDrive, Cpu, Activity } from 'lucide-react';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';

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
  const { tab } = useParams<{ tab: string }>();
  const navigate = useNavigate();

  const activeCategory = (tab && Object.values(SETTING_CATEGORIES).includes(tab))
    ? tab
    : SETTING_CATEGORIES.GENERAL;

  const setActiveCategory = (category: string) => {
    navigate(`/system/settings/${category}`);
  };
  const [hasChanges, setHasChanges] = useState(false);
  const [testResults, setTestResults] = useState<{ [key: string]: string }>({});

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
      const response = await fetch('/api/system/settings', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          logging: settings[SETTING_CATEGORIES.LOGGING],
          general: settings[SETTING_CATEGORIES.GENERAL],
        }),
      });
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || '저장 실패');
      setHasChanges(false);
      alert('설정이 성공적으로 저장되었습니다.');
    } catch (error: any) {
      alert('설정 저장 중 오류가 발생했습니다: ' + error.message);
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

  // 공통 설정 행 컴포넌트 (프리미엄 Property-Grid 레이아웃)
  const SettingRow = ({ label, description, children, required = false }: {
    label: string,
    description?: string,
    children: React.ReactNode,
    required?: boolean
  }) => (
    <div style={{
      display: 'flex',
      borderBottom: '1px solid var(--neutral-100)',
      minHeight: '80px'
    }}>
      {/* 레이블 컬럼: 우측 정렬 + 배경 음영 + 구분선 */}
      <div style={{
        width: '280px',
        flexShrink: 0,
        backgroundColor: 'var(--neutral-50)',
        borderRight: '1px solid var(--neutral-200)',
        padding: 'var(--space-6) var(--space-8)',
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        textAlign: 'right'
      }}>
        <label style={{
          display: 'block',
          fontSize: '17px',
          fontWeight: '800',
          color: 'var(--neutral-800)',
          marginBottom: description ? '8px' : '0',
          textTransform: 'uppercase',
          letterSpacing: '0.05em',
          lineHeight: '1.2'
        }}>
          {label} {required && <span style={{ color: 'var(--error-500)' }}>*</span>}
        </label>
        {description && (
          <p style={{
            fontSize: '16px',
            color: 'var(--neutral-500)',
            lineHeight: '1.6',
            margin: 0,
            fontWeight: '500',
            wordBreak: 'keep-all',
            opacity: 0.9
          }}>
            {description}
          </p>
        )}
      </div>

      {/* 설정값 컬럼 */}
      <div style={{
        flex: 1,
        padding: 'var(--space-5) var(--space-8)',
        display: 'flex',
        alignItems: 'center',
        backgroundColor: 'white'
      }}>
        <div style={{ width: '100%', maxWidth: '500px' }}>
          {children}
        </div>
      </div>
    </div>
  );

  // 공통 설정 서브 섹션 제목 (Spine 정렬 유지)
  const SettingSubHeader = ({ title, children }: { title: string, children?: React.ReactNode }) => (
    <div style={{
      display: 'flex',
      backgroundColor: 'var(--neutral-50)',
      borderBottom: '1px solid var(--neutral-100)'
    }}>
      <div style={{
        width: '280px',
        flexShrink: 0,
        backgroundColor: 'var(--neutral-50)',
        borderRight: '1px solid var(--neutral-200)',
        padding: 'var(--space-4) var(--space-8)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'flex-end',
        minHeight: '50px'
      }}>
        <h4 style={{
          fontSize: '16px',
          fontWeight: '900',
          color: 'var(--primary-600)',
          margin: 0,
          textTransform: 'uppercase',
          letterSpacing: '0.1em',
          wordBreak: 'keep-all'
        }}>
          {title}
        </h4>
      </div>
      <div style={{
        flex: 1,
        padding: 'var(--space-4) var(--space-8)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'flex-end',
        backgroundColor: 'var(--neutral-50)'
      }}>
        {children}
      </div>
    </div>
  );

  // 입력 필드 컴포넌트
  const InputField = ({ label, value, onChange, type = 'text', suffix = '', min = undefined, max = undefined, required = false, description = '' }: {
    label: string,
    value: any,
    onChange: (val: any) => void,
    type?: string,
    suffix?: string,
    min?: number,
    max?: number,
    required?: boolean,
    description?: string
  }) => (
    <SettingRow label={label} description={description} required={required}>
      <div style={{ position: 'relative', display: 'flex', alignItems: 'center' }}>
        <input
          type={type}
          value={value}
          onChange={(e) => onChange(type === 'number' ? Number(e.target.value) : e.target.value)}
          min={min}
          max={max}
          className="mgmt-input"
          style={{ flex: 1 }}
        />
        {suffix && (
          <span style={{
            position: 'absolute',
            right: '12px',
            fontSize: '12px',
            color: 'var(--neutral-400)',
            fontWeight: '500'
          }}>
            {suffix}
          </span>
        )}
      </div>
    </SettingRow>
  );

  // 체크박스 컴포넌트
  const CheckboxField = ({ label, checked, onChange, description = '' }: {
    label: string,
    checked: boolean,
    onChange: (val: boolean) => void,
    description?: string
  }) => (
    <SettingRow label={label} description={description}>
      <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', height: '40px' }}>
        <input
          type="checkbox"
          checked={checked}
          onChange={(e) => onChange(e.target.checked)}
        />
        <span style={{ fontSize: '16px', fontWeight: '700', color: 'var(--neutral-700)' }}>활성화</span>
      </label>
    </SettingRow>
  );

  // 셀렉트 컴포넌트
  const SelectField = ({ label, value, onChange, options, required = false, description = '' }: {
    label: string,
    value: any,
    onChange: (val: any) => void,
    options: { value: string, label: string }[],
    required?: boolean,
    description?: string
  }) => (
    <SettingRow label={label} description={description} required={required}>
      <select
        value={value}
        onChange={(e) => onChange(e.target.value)}
        className="mgmt-select"
      >
        {options.map(option => (
          <option key={option.value} value={option.value}>{option.label}</option>
        ))}
      </select>
    </SettingRow>
  );

  return (
    <ManagementLayout>
      <PageHeader
        title="시스템 설정"
        description="시스템 전반에 걸친 정책 및 환경 설정을 관리합니다."
        icon="fas fa-cog"
        actions={
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            {hasChanges && (
              <span className="badge badge-warning" style={{ padding: '6px 12px' }}>
                저장되지 않은 변경사항이 있습니다
              </span>
            )}

            <button
              onClick={handleResetToDefaults}
              className="mgmt-btn mgmt-btn-outline"
            >
              <RefreshCw className="mr-2 h-4 w-4" />
              기본값 복원
            </button>

            <button
              onClick={handleSaveSettings}
              disabled={!hasChanges || saving}
              className="mgmt-btn mgmt-btn-primary"
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
        }
      />

      <div style={{ display: 'flex', gap: 'var(--space-4)', flex: 1, minHeight: 0, marginTop: 'var(--space-4)' }}>
        {/* 사이드바 - 카테고리 목록 */}
        <div className="mgmt-card" style={{ width: '280px', flexShrink: 0, padding: 0 }}>
          <div style={{ padding: 'var(--space-4) var(--space-2)' }}>
            <h2 style={{
              fontSize: '16px',
              fontWeight: '800',
              color: 'var(--neutral-400)',
              textTransform: 'uppercase',
              letterSpacing: '0.05em',
              padding: '0 var(--space-4) var(--space-4)',
              borderBottom: '1px solid var(--neutral-100)',
              marginBottom: 'var(--space-2)'
            }}>
              설정 카테고리
            </h2>
            <nav style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
              {Object.values(SETTING_CATEGORIES).map(category => {
                const Icon = getCategoryIcon(category);
                const isActive = activeCategory === category;
                return (
                  <button
                    key={category}
                    onClick={() => setActiveCategory(category)}
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      padding: 'var(--space-4) var(--space-4)',
                      borderRadius: 'var(--radius-md)',
                      border: 'none',
                      background: isActive ? 'var(--primary-50)' : 'transparent',
                      color: isActive ? 'var(--primary-700)' : 'var(--neutral-600)',
                      fontWeight: isActive ? '700' : '500',
                      fontSize: '17px',
                      cursor: 'pointer',
                      transition: 'all 0.2s ease',
                      textAlign: 'left',
                      width: '100%'
                    }}
                    onMouseOver={(e) => {
                      if (!isActive) {
                        e.currentTarget.style.background = 'var(--neutral-50)';
                        e.currentTarget.style.color = 'var(--primary-600)';
                      }
                    }}
                    onMouseOut={(e) => {
                      if (!isActive) {
                        e.currentTarget.style.background = 'transparent';
                        e.currentTarget.style.color = 'var(--neutral-600)';
                      }
                    }}
                  >
                    <Icon className="mr-3 h-6 w-6" style={{ color: isActive ? 'var(--primary-500)' : 'inherit' }} />
                    {getCategoryTitle(category)}
                  </button>
                );
              })}
            </nav>
          </div>
        </div>

        {/* 메인 콘텐츠 - 설정 상세 */}
        <div className="mgmt-card" style={{ flex: 1, display: 'flex', flexDirection: 'column', padding: 0 }}>
          <div className="mgmt-card-header" style={{
            padding: 'var(--space-6) var(--space-8)',
            borderBottom: '1px solid var(--neutral-100)',
            backgroundColor: 'white'
          }}>
            <h2 className="mgmt-card-title" style={{
              fontSize: '22px',
              fontWeight: '800',
              color: 'var(--neutral-800)',
              margin: 0
            }}>
              {getCategoryTitle(activeCategory)}
            </h2>
          </div>
          <div className="mgmt-card-body" style={{ overflowY: 'auto', padding: 0 }}>
            <div style={{ display: 'flex', flexDirection: 'column' }}>
              {/* 일반 설정 */}
              {activeCategory === SETTING_CATEGORIES.GENERAL && (
                <div>
                  <InputField
                    label="시스템 이름"
                    value={settings.general.system_name}
                    onChange={(value) => handleSettingChange('general', 'system_name', value)}
                    description="사용자 인터페이스 및 보고서에 표시될 플랫폼의 명칭입니다."
                    required
                  />
                  <SelectField
                    label="표시 언어"
                    value={settings.general.language}
                    onChange={(value) => handleSettingChange('general', 'language', value)}
                    description="시스템 전체에서 사용할 기본 언어를 선택합니다."
                    options={[
                      { value: 'ko-KR', label: '한국어 (Korean)' },
                      { value: 'en-US', label: 'English (US)' },
                      { value: 'ja-JP', label: '日本語 (Japanese)' }
                    ]}
                  />
                  <SelectField
                    label="타임존"
                    value={settings.general.timezone}
                    onChange={(value) => handleSettingChange('general', 'timezone', value)}
                    description="데이터 수집 및 로그 기록의 기준이 되는 표준 시간대입니다."
                    options={[
                      { value: 'Asia/Seoul', label: 'Asia/Seoul (UTC+09:00)' },
                      { value: 'UTC', label: 'UTC (Universal Coordinated Time)' }
                    ]}
                  />
                  <InputField
                    label="세션 타임아웃"
                    type="number"
                    value={settings.general.session_timeout}
                    onChange={(value) => handleSettingChange('general', 'session_timeout', value)}
                    description="비활동 시 자동으로 로그아웃될 때까지의 시간입니다."
                    suffix="분"
                    min={10}
                    max={1440}
                  />
                  <InputField
                    label="최대 로그인 시도"
                    type="number"
                    value={settings.general.max_login_attempts}
                    onChange={(value) => handleSettingChange('general', 'max_login_attempts', value)}
                    description="계정 잠금 전 허용되는 최대 로그인 실패 횟수입니다."
                    suffix="회"
                    min={3}
                    max={10}
                  />
                </div>
              )}

              {/* 데이터베이스 설정 */}
              {activeCategory === SETTING_CATEGORIES.DATABASE && (
                <div>
                  <SettingSubHeader title="데이터 보관 정책" />
                  <InputField
                    label="데이터 보존 기간"
                    type="number"
                    value={settings.database.data_retention_days}
                    onChange={(value) => handleSettingChange('database', 'data_retention_days', value)}
                    description="수집된 텔레메트리 데이터를 DB에 보관할 최대 날짜입니다."
                    suffix="일"
                    min={30}
                    max={3650}
                  />

                  <SettingSubHeader title="백업 구성">
                    <button
                      onClick={testDatabaseConnection}
                      className="mgmt-btn mgmt-btn-outline"
                      style={{ fontSize: '15px', padding: '6px 14px' }}
                    >
                      {getTestIcon(testResults.database)}
                      <span className="ml-2">연결 테스트</span>
                    </button>
                  </SettingSubHeader>

                  <CheckboxField
                    label="자동 백업 활성화"
                    checked={settings.database.backup_enabled}
                    onChange={(value) => handleSettingChange('database', 'backup_enabled', value)}
                    description="시스템 스케줄에 따라 정기적으로 데이터베이스를 백업합니다."
                  />
                  {settings.database.backup_enabled && (
                    <>
                      <SelectField
                        label="백업 스케줄"
                        value={settings.database.backup_schedule}
                        onChange={(value) => handleSettingChange('database', 'backup_schedule', value)}
                        description="백업이 수행될 주기를 설정합니다."
                        options={[
                          { value: 'daily', label: '매일 (Daily)' },
                          { value: 'weekly', label: '매주 (Weekly)' },
                          { value: 'monthly', label: '매월 (Monthly)' }
                        ]}
                      />
                      <InputField
                        label="백업 보존 기간"
                        type="number"
                        value={settings.database.backup_retention_days}
                        onChange={(value) => handleSettingChange('database', 'backup_retention_days', value)}
                        description="생성된 백업 파일을 보관할 기간입니다."
                        suffix="일"
                        min={7}
                        max={90}
                      />
                    </>
                  )}
                </div>
              )}

              {/* 데이터 수집 설정 */}
              {activeCategory === SETTING_CATEGORIES.COLLECTION && (
                <div>
                  <InputField
                    label="기본 폴링 주기"
                    type="number"
                    value={settings.collection.default_polling_interval}
                    onChange={(value) => handleSettingChange('collection', 'default_polling_interval', value)}
                    description="장치별 설정이 없을 때 적용될 기본 데이터 수집 간격입니다."
                    suffix="ms"
                    min={100}
                    max={60000}
                  />
                  <InputField
                    label="최대 동시 접속 수"
                    type="number"
                    value={settings.collection.max_concurrent_devices}
                    onChange={(value) => handleSettingChange('collection', 'max_concurrent_devices', value)}
                    description="서버가 동시에 처리할 수 있는 최대 장치 세션 수입니다."
                    suffix="대"
                    min={100}
                    max={5000}
                  />
                  <InputField
                    label="장치 응답 대기 시간"
                    type="number"
                    value={settings.collection.device_timeout}
                    onChange={(value) => handleSettingChange('collection', 'device_timeout', value)}
                    description="장치로부터 응답이 없을 때 타임아웃으로 간주할 시간입니다."
                    suffix="ms"
                    min={1000}
                    max={30000}
                  />
                  <CheckboxField
                    label="패킷 로깅 활성화"
                    checked={settings.collection.enable_packet_logging}
                    onChange={(value) => handleSettingChange('collection', 'enable_packet_logging', value)}
                    description="분석을 위해 송수신되는 원시 패킷 데이터를 로그로 기록합니다."
                  />
                </div>
              )}

              {/* 알림 설정 */}
              {activeCategory === SETTING_CATEGORIES.NOTIFICATIONS && (
                <div>
                  <SettingSubHeader title="이메일 서버 상세 설정">
                    <button
                      onClick={testEmailConnection}
                      className="mgmt-btn mgmt-btn-outline"
                      style={{ fontSize: '15px', padding: '6px 14px' }}
                    >
                      {getTestIcon(testResults.email)}
                      <span className="ml-2">발송 테스트</span>
                    </button>
                  </SettingSubHeader>

                  <CheckboxField
                    label="이메일 알림 활성화"
                    checked={settings.notifications.email_enabled}
                    onChange={(value) => handleSettingChange('notifications', 'email_enabled', value)}
                    description="시스템 장애 및 주요 이벤트를 이메일로 전송합니다."
                  />

                  {settings.notifications.email_enabled && (
                    <>
                      <InputField
                        label="SMTP 서버 주소"
                        value={settings.notifications.email_server}
                        onChange={(value) => handleSettingChange('notifications', 'email_server', value)}
                        description="알림 발송에 사용할 SMTP 메일 서버 호스트 이름입니다."
                        required
                      />
                      <InputField
                        label="SMTP 포트"
                        type="number"
                        value={settings.notifications.email_port}
                        onChange={(value) => handleSettingChange('notifications', 'email_port', value)}
                        description="메일 서버 연결 포트 (일반적으로 587 또는 465)입니다."
                        min={1}
                        max={65535}
                      />
                      <InputField
                        label="발신자 이름"
                        value={settings.notifications.email_from_name}
                        onChange={(value) => handleSettingChange('notifications', 'email_from_name', value)}
                        description="수신자가 보는 메일 발신자 명칭입니다."
                      />
                    </>
                  )}

                  <SettingSubHeader title="기타 알림 수단" />

                  <CheckboxField
                    label="SMS 알림 활성화"
                    checked={settings.notifications.sms_enabled}
                    onChange={(value) => handleSettingChange('notifications', 'sms_enabled', value)}
                    description="발송용 문자 서버를 사용하여 긴급 상황을 전송합니다."
                  />

                  <CheckboxField
                    label="푸시 알림 활성화"
                    checked={settings.notifications.push_notifications_enabled}
                    onChange={(value) => handleSettingChange('notifications', 'push_notifications_enabled', value)}
                    description="모바일 기기로 실시간 앱 푸시를 전송합니다."
                  />

                  <InputField
                    label="알림 재전송 제한"
                    type="number"
                    value={settings.notifications.notification_cooldown}
                    onChange={(value) => handleSettingChange('notifications', 'notification_cooldown', value)}
                    description="동일한 알림이 반복될 때 재발송을 차단할 최소 시간입니다."
                    suffix="초"
                    min={0}
                    max={3600}
                  />
                </div>
              )}

              {/* 보안 설정 */}
              {activeCategory === SETTING_CATEGORIES.SECURITY && (
                <div>
                  <SettingSubHeader title="비밀번호 정책" />
                  <InputField
                    label="최소 암호 길이"
                    type="number"
                    value={settings.security.password_min_length}
                    onChange={(value) => handleSettingChange('security', 'password_min_length', value)}
                    description="사용자 비밀번호가 가져야 할 최소 문자 수입니다."
                    suffix="자리"
                    min={6}
                    max={20}
                  />
                  <CheckboxField
                    label="영문 대문자 포함"
                    checked={settings.security.password_require_uppercase}
                    onChange={(value) => handleSettingChange('security', 'password_require_uppercase', value)}
                  />
                  <CheckboxField
                    label="특수 문자 포함"
                    checked={settings.security.password_require_symbols}
                    onChange={(value) => handleSettingChange('security', 'password_require_symbols', value)}
                  />
                  <InputField
                    label="비밀번호 만료 주기"
                    type="number"
                    value={settings.security.password_expiry_days}
                    onChange={(value) => handleSettingChange('security', 'password_expiry_days', value)}
                    description="보안을 위해 주기적으로 암호를 변경해야 하는 기간입니다."
                    suffix="일"
                    min={0}
                    max={180}
                  />

                  <SettingSubHeader title="접근 통제" />
                  <CheckboxField
                    label="2단계 인증 강제"
                    checked={settings.security.two_factor_auth_required}
                    onChange={(value) => handleSettingChange('security', 'two_factor_auth_required', value)}
                    description="모든 관리자 계정에 대해 2단계 인증(OTP)을 필수화합니다."
                  />
                  <CheckboxField
                    label="감사 로그 활성화"
                    checked={settings.security.enable_audit_logging}
                    onChange={(value) => handleSettingChange('security', 'enable_audit_logging', value)}
                    description="모든 설정 변경 및 주요 사용자 행위를 상세히 기록합니다."
                  />
                </div>
              )}

              {/* 성능 설정 */}
              {activeCategory === SETTING_CATEGORIES.PERFORMANCE && (
                <div>
                  <SettingSubHeader title="리소스 사용량 경고 임계값" />
                  <InputField
                    label="CPU 사용량 상한"
                    type="number"
                    value={settings.performance.cpu_usage_threshold}
                    onChange={(value) => handleSettingChange('performance', 'cpu_usage_threshold', value)}
                    description="지정된 CPU 사용량을 초과할 경우 시스템 경고를 발생시킵니다."
                    suffix="%"
                    min={50}
                    max={95}
                  />
                  <InputField
                    label="메모리 사용량 상한"
                    type="number"
                    value={settings.performance.memory_usage_threshold}
                    onChange={(value) => handleSettingChange('performance', 'memory_usage_threshold', value)}
                    description="물리 메모리 사용량이 임계치를 넘으면 관리자에게 통지합니다."
                    suffix="%"
                    min={50}
                    max={95}
                  />
                  <InputField
                    label="디스크 잔여 공간 경고"
                    type="number"
                    value={settings.performance.disk_usage_threshold}
                    onChange={(value) => handleSettingChange('performance', 'disk_usage_threshold', value)}
                    description="디스크 사용량이 설정값을 넘으면 데이터 수집이 중단될 수 있습니다."
                    suffix="%"
                    min={50}
                    max={95}
                  />

                  <SettingSubHeader title="최적화 옵션" />
                  <CheckboxField
                    label="캐시 시스템 활성화"
                    checked={settings.performance.cache_enabled}
                    onChange={(value) => handleSettingChange('performance', 'cache_enabled', value)}
                    description="빈번하게 조회되는 데이터의 응답 속도를 위해 메모리 캐시를 사용합니다."
                  />
                </div>
              )}

              {/* 로깅 설정 */}
              {activeCategory === SETTING_CATEGORIES.LOGGING && (
                <div>
                  <SelectField
                    label="로그 레벨"
                    value={settings.logging.log_level}
                    onChange={(value) => handleSettingChange('logging', 'log_level', value)}
                    description="기록할 시스템 로그의 상세 수준을 설정합니다."
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
                    description="오래된 로그가 삭제되기 전까지의 보관 기간입니다."
                    suffix="일"
                    min={7}
                    max={365}
                  />

                  <CheckboxField
                    label="디버그 로깅 활성화"
                    checked={settings.logging.enable_debug_logging}
                    onChange={(value) => handleSettingChange('logging', 'enable_debug_logging', value)}
                    description="상세한 디버그 정보를 로그에 포함합니다. 성능에 영향을 줄 수 있습니다."
                  />

                  <CheckboxField
                    label="파일로 로그 저장"
                    checked={settings.logging.log_to_file}
                    onChange={(value) => handleSettingChange('logging', 'log_to_file', value)}
                    description="시스템 로그를 서버 내 물리 파일로 기록합니다."
                  />

                  <CheckboxField
                    label="데이터베이스에 로그 저장"
                    checked={settings.logging.log_to_database}
                    onChange={(value) => handleSettingChange('logging', 'log_to_database', value)}
                    description="로그를 DB에 저장하여 UI에서 쉽게 검색할 수 있도록 합니다."
                  />

                  <InputField
                    label="최대 로그 파일 크기"
                    type="number"
                    value={settings.logging.max_log_file_size}
                    onChange={(value) => handleSettingChange('logging', 'max_log_file_size', value)}
                    description="개별 로그 파일이 가질 수 있는 최대 용량입니다."
                    suffix="MB"
                    min={10}
                    max={1000}
                  />

                  <CheckboxField
                    label="오래된 로그 압축"
                    checked={settings.logging.compress_old_logs}
                    onChange={(value) => handleSettingChange('logging', 'compress_old_logs', value)}
                    description="저장 공간 절약을 위해 지난 로그 파일을 자동으로 압축합니다."
                  />
                </div>
              )}
            </div>
          </div>
        </div>
      </div>
    </ManagementLayout>
  );
};

export default SystemSettings;