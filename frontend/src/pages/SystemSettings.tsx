import React, { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { SUPPORTED_LANGUAGES } from '../i18n';
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
  // General Settings
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

  // Database Settings
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

  // Data Collection Settings
  [SETTING_CATEGORIES.COLLECTION]: {
    default_polling_interval: 1000, // ms
    max_concurrent_devices: 1000,
    device_timeout: 5000, // ms
    retry_attempts: 3,
    influxdb_storage_interval: 0, // ms (0: 스캔 시 즉시 저장)
    rdb_sync_interval: 60, // 초 (Redis→SQLite 아날로그 주기 동기화)

    packet_log_retention_hours: 24,
    virtual_point_calculation_interval: 500, // ms
    alarm_check_interval: 1000 // ms
  },

  // Notification Settings
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

  // Security Settings
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

  // Performance Settings
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

  // Logging Settings
  [SETTING_CATEGORIES.LOGGING]: {
    log_level: 'info',
    log_retention_days: 30,
    enable_debug_logging: false,
    log_to_file: true, // 항상 파일 저장 (변경 불가)
    max_log_file_size: 100, // MB
    enable_packet_logging: false,
    enable_error_notifications: true
  }
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
          whiteSpace: 'pre-line',
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
function CheckboxField({ label, checked, onChange, description = '' }: {
  label: string,
  checked: boolean,
  onChange: (val: boolean) => void,
  description?: string
}) {
  const { t } = useTranslation(['systemSettings']);
  return (
    <SettingRow label={label} description={description}>
      <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', height: '40px' }}>
        <input
          type="checkbox"
          checked={checked}
          onChange={(e) => onChange(e.target.checked)}
        />
        <span style={{ fontSize: '16px', fontWeight: '700', color: 'var(--neutral-700)' }}>{t('systemSettings:activate')}</span>
      </label>
    </SettingRow>
  );
}

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
      style={{ width: '100%', minWidth: '180px' }}
    >
      {options.map(option => (
        <option key={option.value} value={option.value}>{option.label}</option>
      ))}
    </select>
  </SettingRow>
);

const SystemSettings = () => {
  const [settings, setSettings] = useState(initialSettings);
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const { tab } = useParams<{ tab: string }>();
  const navigate = useNavigate();
  const { i18n, t } = useTranslation(['systemSettings', 'common']);

  const activeCategory = (tab && Object.values(SETTING_CATEGORIES).includes(tab))
    ? tab
    : SETTING_CATEGORIES.GENERAL;

  const setActiveCategory = (category: string) => {
    navigate(`/system/settings/${category}`);
  };
  const [hasChanges, setHasChanges] = useState(false);
  const [testResults, setTestResults] = useState<{ [key: string]: string }>({});

  // 마운트 시 DB에서 설정 로드
  useEffect(() => {
    const loadSettings = async () => {
      try {
        const [loggingRes, generalRes, collectionRes] = await Promise.all([
          fetch('/api/system/logging-settings'),
          fetch('/api/system/general-settings'),
          fetch('/api/system/data-collection-settings'),
        ]);
        const [loggingData, generalData, collectionData] = await Promise.all([
          loggingRes.json(),
          generalRes.json(),
          collectionRes.json(),
        ]);

        setSettings(prev => {
          const next = { ...prev };

          if (loggingData.success && loggingData.data) {
            const s = loggingData.data;
            next[SETTING_CATEGORIES.LOGGING] = {
              ...prev[SETTING_CATEGORIES.LOGGING],
              log_level: s.log_level?.toLowerCase() || prev[SETTING_CATEGORIES.LOGGING].log_level,
              log_retention_days: s.log_retention_days ?? prev[SETTING_CATEGORIES.LOGGING].log_retention_days,
              max_log_file_size: s.max_log_file_size_mb ?? prev[SETTING_CATEGORIES.LOGGING].max_log_file_size,
              enable_packet_logging: s.packet_logging_enabled ?? prev[SETTING_CATEGORIES.LOGGING].enable_packet_logging,
              enable_debug_logging: s.enable_debug_logging ?? prev[SETTING_CATEGORIES.LOGGING].enable_debug_logging,
            };
          }

          if (generalData.success && generalData.data) {
            const g = generalData.data;
            next[SETTING_CATEGORIES.GENERAL] = {
              ...prev[SETTING_CATEGORIES.GENERAL],
              system_name: g.system_name ?? prev[SETTING_CATEGORIES.GENERAL].system_name,
              timezone: g.timezone ?? prev[SETTING_CATEGORIES.GENERAL].timezone,
              language: g.language ?? prev[SETTING_CATEGORIES.GENERAL].language,
              date_format: g.date_format ?? prev[SETTING_CATEGORIES.GENERAL].date_format,
              time_format: g.time_format ?? prev[SETTING_CATEGORIES.GENERAL].time_format,
              session_timeout: g.session_timeout ?? prev[SETTING_CATEGORIES.GENERAL].session_timeout,
              max_login_attempts: g.max_login_attempts ?? prev[SETTING_CATEGORIES.GENERAL].max_login_attempts,
              account_lockout_duration: g.account_lockout_duration ?? prev[SETTING_CATEGORIES.GENERAL].account_lockout_duration,
            };
          }

          if (collectionData.success && collectionData.data) {
            const c = collectionData.data;
            next[SETTING_CATEGORIES.COLLECTION] = {
              ...prev[SETTING_CATEGORIES.COLLECTION],
              default_polling_interval: c.default_polling_interval ?? prev[SETTING_CATEGORIES.COLLECTION].default_polling_interval,
              max_concurrent_devices: c.max_concurrent_connections ?? prev[SETTING_CATEGORIES.COLLECTION].max_concurrent_devices,
              device_timeout: c.device_response_timeout ?? prev[SETTING_CATEGORIES.COLLECTION].device_timeout,
              influxdb_storage_interval: c.influxdb_storage_interval ?? prev[SETTING_CATEGORIES.COLLECTION].influxdb_storage_interval,
              rdb_sync_interval: c.rdb_sync_interval ?? prev[SETTING_CATEGORIES.COLLECTION].rdb_sync_interval,
            };
          }

          return next;
        });
      } catch (e) {
        // 로드 실패 시 기본값 유지
      }
    };
    loadSettings();
  }, []);


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
      // 필드명 정규화 (프런트 키 → 백엔드 키)
      const L = settings[SETTING_CATEGORIES.LOGGING];
      const loggingPayload = {
        log_level: L.log_level,
        log_retention_days: L.log_retention_days,
        enable_debug_logging: L.enable_debug_logging,
        log_to_file: L.log_to_file,
        max_log_file_size_mb: L.max_log_file_size,         // 키 변환
        packet_logging_enabled: L.enable_packet_logging,     // 키 변환
        enable_error_notifications: L.enable_error_notifications,
      };

      const response = await fetch('/api/system/settings', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          logging: loggingPayload,
          general: settings[SETTING_CATEGORIES.GENERAL],
          data_collection: {
            default_polling_interval: settings[SETTING_CATEGORIES.COLLECTION].default_polling_interval,
            max_concurrent_connections: settings[SETTING_CATEGORIES.COLLECTION].max_concurrent_devices, // 프론트는 devices, 백엔드는 connections
            device_response_timeout: settings[SETTING_CATEGORIES.COLLECTION].device_timeout, // 프론트는 timeout, 백엔드는 response_timeout
            influxdb_storage_interval: settings[SETTING_CATEGORIES.COLLECTION].influxdb_storage_interval,
            rdb_sync_interval: settings[SETTING_CATEGORIES.COLLECTION].rdb_sync_interval,
          }
        }),
      });
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || 'Save failed');
      setHasChanges(false);
      alert('Settings saved successfully.');
    } catch (error: any) {
      alert('Error saving settings: ' + error.message);
    } finally {
      setSaving(false);
    }
  };


  // 기본값으로 복원
  const handleResetToDefaults = () => {
    if (confirm('Restore all settings to defaults?')) {
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
      [SETTING_CATEGORIES.GENERAL]: t('systemSettings:category.general'),
      [SETTING_CATEGORIES.DATABASE]: t('systemSettings:category.database'),
      [SETTING_CATEGORIES.COLLECTION]: t('systemSettings:category.collection'),
      [SETTING_CATEGORIES.NOTIFICATIONS]: t('systemSettings:category.notifications'),
      [SETTING_CATEGORIES.SECURITY]: t('systemSettings:category.security'),
      [SETTING_CATEGORIES.PERFORMANCE]: t('systemSettings:category.performance'),
      [SETTING_CATEGORIES.LOGGING]: t('systemSettings:category.logging')
    };
    return titles[category] || t('systemSettings:category.default');
  };

  // 테스트 결과 아이콘
  const getTestIcon = (result) => {
    if (result === 'testing') return <RefreshCw className="h-4 w-4 animate-spin text-blue-500" />;
    if (result === 'success') return <div className="h-4 w-4 bg-green-500 rounded-full" />;
    if (result === 'error') return <AlertTriangle className="h-4 w-4 text-red-500" />;
    return null;
  };



  return (
    <ManagementLayout>
      <PageHeader
        title={t('systemSettings:pageTitle')}
        description={t('systemSettings:pageDesc')}
        icon="fas fa-cog"
        actions={
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            {hasChanges && (
              <span className="badge badge-warning" style={{ padding: '6px 12px' }}>
                {t('systemSettings:unsavedChanges')}
              </span>
            )}

            <button
              onClick={handleResetToDefaults}
              className="mgmt-btn mgmt-btn-outline"
            >
              <RefreshCw className="mr-2 h-4 w-4" />
              {t('systemSettings:restoreDefaults')}
            </button>

            <button
              onClick={handleSaveSettings}
              disabled={!hasChanges || saving}
              className="mgmt-btn mgmt-btn-primary"
            >
              {saving ? (
                <>
                  <RefreshCw className="mr-2 h-4 w-4 animate-spin" />
                  {t('systemSettings:saving')}
                </>
              ) : (
                <>
                  <Save className="mr-2 h-4 w-4" />
                  {t('systemSettings:saveSettings')}
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
              {t('systemSettings:settingsCategory')}
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
              {/* General Settings */}
              {activeCategory === SETTING_CATEGORIES.GENERAL && (
                <div>
                  <InputField
                    label={t('systemSettings:general.systemName')}
                    value={settings.general.system_name}
                    onChange={(value) => handleSettingChange('general', 'system_name', value)}
                    description={t('systemSettings:general.systemNameDesc')}
                    required
                  />
                  <SettingRow
                    label={t('systemSettings:general.displayLang')}
                    description={t('systemSettings:general.displayLangDesc')}
                  >
                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px', maxWidth: '420px' }}>
                      {SUPPORTED_LANGUAGES.map(lang => {
                        const isSelected = settings.general.language === lang.code
                          || (settings.general.language.startsWith(lang.code.split('-')[0])
                            && !SUPPORTED_LANGUAGES.some(l => l !== lang && settings.general.language === l.code));
                        return (
                          <button
                            key={lang.code}
                            onClick={() => {
                              handleSettingChange('general', 'language', lang.code);
                              i18n.changeLanguage(lang.code);
                            }}
                            style={{
                              display: 'flex', alignItems: 'center', gap: '6px',
                              padding: '8px 14px', borderRadius: '8px', cursor: 'pointer',
                              border: isSelected ? '2px solid #4f46e5' : '1px solid #e5e7eb',
                              background: isSelected ? '#f0f5ff' : '#fff',
                              color: isSelected ? '#4f46e5' : '#374151',
                              fontWeight: isSelected ? 700 : 400,
                              fontSize: '13px', transition: 'all 0.15s'
                            }}
                          >
                            <span style={{ fontSize: '17px' }}>{lang.flag}</span>
                            <span>{lang.name}</span>
                            {isSelected && <i className="fas fa-check" style={{ fontSize: '11px' }} />}
                          </button>
                        );
                      })}
                    </div>
                  </SettingRow>
                  <SelectField
                    label={t('systemSettings:general.timezone')}
                    value={settings.general.timezone}
                    onChange={(value) => handleSettingChange('general', 'timezone', value)}
                    description={t('systemSettings:general.timezoneDesc')}
                    options={[
                      { value: 'Asia/Seoul', label: 'Asia/Seoul (UTC+09:00)' },
                      { value: 'UTC', label: 'UTC (Universal Coordinated Time)' }
                    ]}
                  />
                  <InputField
                    label={t('systemSettings:general.sessionTimeout')}
                    type="number"
                    value={settings.general.session_timeout}
                    onChange={(value) => handleSettingChange('general', 'session_timeout', value)}
                    description={t('systemSettings:general.sessionTimeoutDesc')}
                    suffix={t('systemSettings:general.sessionTimeoutSuffix')}
                    min={10}
                    max={1440}
                  />
                  <InputField
                    label={t('systemSettings:general.maxLoginAttempts')}
                    type="number"
                    value={settings.general.max_login_attempts}
                    onChange={(value) => handleSettingChange('general', 'max_login_attempts', value)}
                    description={t('systemSettings:general.maxLoginAttemptsDesc')}
                    suffix={t('systemSettings:general.maxLoginAttemptsSuffix')}
                    min={3}
                    max={10}
                  />
                </div>
              )}

              {/* Database Settings */}
              {activeCategory === SETTING_CATEGORIES.DATABASE && (
                <div>
                  <SettingSubHeader title={t('systemSettings:database.retentionPolicy')} />
                  <InputField
                    label={t('systemSettings:database.retentionDays')}
                    type="number"
                    value={settings.database.data_retention_days}
                    onChange={(value) => handleSettingChange('database', 'data_retention_days', value)}
                    description={t('systemSettings:database.retentionDaysDesc')}
                    suffix={t('systemSettings:database.retentionDaysSuffix')}
                    min={30}
                    max={3650}
                  />

                  <SettingSubHeader title={t('systemSettings:database.backupConfig')}>
                    <button
                      onClick={testDatabaseConnection}
                      className="mgmt-btn mgmt-btn-outline"
                      style={{ fontSize: '15px', padding: '6px 14px' }}
                    >
                      {getTestIcon(testResults.database)}
                      <span className="ml-2">{t('systemSettings:connectionTest')}</span>
                    </button>
                  </SettingSubHeader>

                  <CheckboxField
                    label={t('systemSettings:database.autoBackup')}
                    checked={settings.database.backup_enabled}
                    onChange={(value) => handleSettingChange('database', 'backup_enabled', value)}
                    description={t('systemSettings:database.autoBackupDesc')}
                  />
                  {settings.database.backup_enabled && (
                    <>
                      <SelectField
                        label={t('systemSettings:database.backupSchedule')}
                        value={settings.database.backup_schedule}
                        onChange={(value) => handleSettingChange('database', 'backup_schedule', value)}
                        description={t('systemSettings:database.backupScheduleDesc')}
                        options={[
                          { value: 'daily', label: t('systemSettings:database.backupScheduleDaily') },
                          { value: 'weekly', label: t('systemSettings:database.backupScheduleWeekly') },
                          { value: 'monthly', label: t('systemSettings:database.backupScheduleMonthly') }
                        ]}
                      />
                      <InputField
                        label={t('systemSettings:database.backupRetention')}
                        type="number"
                        value={settings.database.backup_retention_days}
                        onChange={(value) => handleSettingChange('database', 'backup_retention_days', value)}
                        description={t('systemSettings:database.backupRetentionDesc')}
                        suffix={t('systemSettings:database.backupRetentionSuffix')}
                        min={7}
                        max={90}
                      />
                    </>
                  )}
                </div>
              )}

              {/* Data Collection Settings */}
              {activeCategory === SETTING_CATEGORIES.COLLECTION && (
                <div>
                  <InputField
                    label={t('systemSettings:collection.defaultPolling')}
                    type="number"
                    value={settings.collection.default_polling_interval}
                    onChange={(value) => handleSettingChange('collection', 'default_polling_interval', value)}
                    description={t('systemSettings:collection.defaultPollingDesc')}
                    suffix="ms"
                    min={100}
                    max={60000}
                  />
                  <InputField
                    label={t('systemSettings:collection.maxConcurrent')}
                    type="number"
                    value={settings.collection.max_concurrent_devices}
                    onChange={(value) => handleSettingChange('collection', 'max_concurrent_devices', value)}
                    description={t('systemSettings:collection.maxConcurrentDesc')}
                    suffix={t('systemSettings:collection.maxConcurrentSuffix')}
                    min={100}
                    max={5000}
                  />
                  <InputField
                    label={t('systemSettings:collection.deviceTimeout')}
                    type="number"
                    value={settings.collection.device_timeout}
                    onChange={(value) => handleSettingChange('collection', 'device_timeout', value)}
                    description={t('systemSettings:collection.deviceTimeoutDesc')}
                    suffix="ms"
                    min={1000}
                    max={30000}
                  />
                  <InputField
                    label={t('systemSettings:collection.influxInterval')}
                    type="number"
                    value={settings.collection.influxdb_storage_interval}
                    onChange={(value) => handleSettingChange('collection', 'influxdb_storage_interval', value)}
                    description={t('systemSettings:collection.influxIntervalDesc')}
                    suffix="ms"
                    min={0}
                    max={3600000}
                  />
                  <InputField
                    label={t('systemSettings:collection.rdbSyncInterval')}
                    type="number"
                    value={settings.collection.rdb_sync_interval}
                    onChange={(value) => handleSettingChange('collection', 'rdb_sync_interval', value)}
                    description={t('systemSettings:collection.rdbSyncIntervalDesc')}
                    suffix="초"
                    min={10}
                    max={3600}
                  />
                </div>
              )}

              {/* Notification Settings */}
              {activeCategory === SETTING_CATEGORIES.NOTIFICATIONS && (
                <div>
                  <SettingSubHeader title={t('systemSettings:notifications.emailServer')}>
                    <button
                      onClick={testEmailConnection}
                      className="mgmt-btn mgmt-btn-outline"
                      style={{ fontSize: '15px', padding: '6px 14px' }}
                    >
                      {getTestIcon(testResults.email)}
                      <span className="ml-2">{t('systemSettings:sendTest')}</span>
                    </button>
                  </SettingSubHeader>

                  <CheckboxField
                    label={t('systemSettings:notifications.emailEnabled')}
                    checked={settings.notifications.email_enabled}
                    onChange={(value) => handleSettingChange('notifications', 'email_enabled', value)}
                    description={t('systemSettings:notifications.emailEnabledDesc')}
                  />

                  {settings.notifications.email_enabled && (
                    <>
                      <InputField
                        label={t('systemSettings:notifications.smtpServer')}
                        value={settings.notifications.email_server}
                        onChange={(value) => handleSettingChange('notifications', 'email_server', value)}
                        description={t('systemSettings:notifications.smtpServerDesc')}
                        required
                      />
                      <InputField
                        label={t('systemSettings:notifications.smtpPort')}
                        type="number"
                        value={settings.notifications.email_port}
                        onChange={(value) => handleSettingChange('notifications', 'email_port', value)}
                        description={t('systemSettings:notifications.smtpPortDesc')}
                        min={1}
                        max={65535}
                      />
                      <InputField
                        label={t('systemSettings:notifications.fromName')}
                        value={settings.notifications.email_from_name}
                        onChange={(value) => handleSettingChange('notifications', 'email_from_name', value)}
                        description={t('systemSettings:notifications.fromNameDesc')}
                      />
                    </>
                  )}

                  <SettingSubHeader title={t('systemSettings:notifications.otherMethods')} />

                  <CheckboxField
                    label={t('systemSettings:notifications.smsEnabled')}
                    checked={settings.notifications.sms_enabled}
                    onChange={(value) => handleSettingChange('notifications', 'sms_enabled', value)}
                    description={t('systemSettings:notifications.smsEnabledDesc')}
                  />

                  <CheckboxField
                    label={t('systemSettings:notifications.pushEnabled')}
                    checked={settings.notifications.push_notifications_enabled}
                    onChange={(value) => handleSettingChange('notifications', 'push_notifications_enabled', value)}
                    description={t('systemSettings:notifications.pushEnabledDesc')}
                  />

                  <InputField
                    label={t('systemSettings:notifications.cooldown')}
                    type="number"
                    value={settings.notifications.notification_cooldown}
                    onChange={(value) => handleSettingChange('notifications', 'notification_cooldown', value)}
                    description={t('systemSettings:notifications.cooldownDesc')}
                    suffix={t('systemSettings:notifications.cooldownSuffix')}
                    min={0}
                    max={3600}
                  />
                </div>
              )}

              {/* Security Settings */}
              {activeCategory === SETTING_CATEGORIES.SECURITY && (
                <div>
                  <SettingSubHeader title={t('systemSettings:security.passwordPolicy')} />
                  <InputField
                    label={t('systemSettings:security.minLength')}
                    type="number"
                    value={settings.security.password_min_length}
                    onChange={(value) => handleSettingChange('security', 'password_min_length', value)}
                    description={t('systemSettings:security.minLengthDesc')}
                    suffix={t('systemSettings:security.minLengthSuffix')}
                    min={6}
                    max={20}
                  />
                  <CheckboxField
                    label={t('systemSettings:security.requireUppercase')}
                    checked={settings.security.password_require_uppercase}
                    onChange={(value) => handleSettingChange('security', 'password_require_uppercase', value)}
                  />
                  <CheckboxField
                    label={t('systemSettings:security.requireSymbols')}
                    checked={settings.security.password_require_symbols}
                    onChange={(value) => handleSettingChange('security', 'password_require_symbols', value)}
                  />
                  <InputField
                    label={t('systemSettings:security.expiryDays')}
                    type="number"
                    value={settings.security.password_expiry_days}
                    onChange={(value) => handleSettingChange('security', 'password_expiry_days', value)}
                    description={t('systemSettings:security.expiryDaysDesc')}
                    suffix={t('systemSettings:security.expiryDaysSuffix')}
                    min={0}
                    max={180}
                  />

                  <SettingSubHeader title={t('systemSettings:security.accessControl')} />
                  <CheckboxField
                    label={t('systemSettings:security.twoFactor')}
                    checked={settings.security.two_factor_auth_required}
                    onChange={(value) => handleSettingChange('security', 'two_factor_auth_required', value)}
                    description={t('systemSettings:security.twoFactorDesc')}
                  />
                  <CheckboxField
                    label={t('systemSettings:security.auditLog')}
                    checked={settings.security.enable_audit_logging}
                    onChange={(value) => handleSettingChange('security', 'enable_audit_logging', value)}
                    description={t('systemSettings:security.auditLogDesc')}
                  />
                </div>
              )}

              {/* Performance Settings */}
              {activeCategory === SETTING_CATEGORIES.PERFORMANCE && (
                <div>
                  <SettingSubHeader title={t('systemSettings:performance.resourceThreshold')} />
                  <InputField
                    label={t('systemSettings:performance.cpuThreshold')}
                    type="number"
                    value={settings.performance.cpu_usage_threshold}
                    onChange={(value) => handleSettingChange('performance', 'cpu_usage_threshold', value)}
                    description={t('systemSettings:performance.cpuThresholdDesc')}
                    suffix="%"
                    min={50}
                    max={95}
                  />
                  <InputField
                    label={t('systemSettings:performance.memoryThreshold')}
                    type="number"
                    value={settings.performance.memory_usage_threshold}
                    onChange={(value) => handleSettingChange('performance', 'memory_usage_threshold', value)}
                    description={t('systemSettings:performance.memoryThresholdDesc')}
                    suffix="%"
                    min={50}
                    max={95}
                  />
                  <InputField
                    label={t('systemSettings:performance.diskThreshold')}
                    type="number"
                    value={settings.performance.disk_usage_threshold}
                    onChange={(value) => handleSettingChange('performance', 'disk_usage_threshold', value)}
                    description={t('systemSettings:performance.diskThresholdDesc')}
                    suffix="%"
                    min={50}
                    max={95}
                  />

                  <SettingSubHeader title={t('systemSettings:performance.optimizeOptions')} />
                  <CheckboxField
                    label={t('systemSettings:performance.cacheEnabled')}
                    checked={settings.performance.cache_enabled}
                    onChange={(value) => handleSettingChange('performance', 'cache_enabled', value)}
                    description={t('systemSettings:performance.cacheEnabledDesc')}
                  />
                </div>
              )}

              {/* Logging Settings */}
              {activeCategory === SETTING_CATEGORIES.LOGGING && (
                <div>
                  <SelectField
                    label={t('systemSettings:logging.logLevel')}
                    value={settings.logging.log_level}
                    onChange={(value) => handleSettingChange('logging', 'log_level', value)}
                    description={t('systemSettings:logging.logLevelDesc')}
                    options={[
                      { value: 'debug', label: t('systemSettings:logging.logLevelDebug') },
                      { value: 'info', label: t('systemSettings:logging.logLevelInfo') },
                      { value: 'warn', label: t('systemSettings:logging.logLevelWarn') },
                      { value: 'error', label: t('systemSettings:logging.logLevelError') }
                    ]}
                  />

                  <InputField
                    label={t('systemSettings:logging.retentionDays')}
                    type="number"
                    value={settings.logging.log_retention_days}
                    onChange={(value) => handleSettingChange('logging', 'log_retention_days', value)}
                    description={t('systemSettings:logging.retentionDaysDesc')}
                    suffix={t('systemSettings:logging.retentionDaysSuffix')}
                    min={7}
                    max={365}
                  />

                  <InputField
                    label={t('systemSettings:logging.maxFileSize')}
                    type="number"
                    value={settings.logging.max_log_file_size}
                    onChange={(value) => handleSettingChange('logging', 'max_log_file_size', value)}
                    description={t('systemSettings:logging.maxFileSizeDesc')}
                    suffix="MB"
                    min={10}
                    max={1000}
                  />

                  <CheckboxField
                    label={t('systemSettings:logging.packetLogging')}
                    checked={settings.logging.enable_packet_logging}
                    onChange={(value) => handleSettingChange('logging', 'enable_packet_logging', value)}
                    description={t('systemSettings:logging.packetLoggingDesc')}
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