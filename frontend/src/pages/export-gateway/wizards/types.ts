// ============================================================================
// wizards/types.ts
// ExportGatewayWizard 공유 타입 및 상수
// ============================================================================

import {
    Gateway,
    Assignment,
    ExportTarget,
    ExportProfile,
    PayloadTemplate,
    DataPoint,
    ExportTargetMapping,
    ExportSchedule
} from '../../../api/services/exportGatewayApi';

// ─── Props ────────────────────────────────────────────────────────────────────

export interface ExportGatewayWizardProps {
    visible: boolean;
    onClose: () => void;
    onSuccess: () => void;
    profiles: ExportProfile[];
    targets: ExportTarget[];
    templates: PayloadTemplate[];
    allPoints: DataPoint[];
    editingGateway?: Gateway | null;
    assignments?: Assignment[];
    schedules?: ExportSchedule[];
    onDelete?: (gateway: Gateway) => Promise<void>;
    siteId?: number | null;
    tenantId?: number | null;
}

// ─── Step 내부 상태 타입 ───────────────────────────────────────────────────────

export interface GatewayFormData {
    name: string;
    ip_address: string;
    description: string;
    subscription_mode: 'all' | 'selective';
    config: Record<string, any>;
}

export interface NewProfileData {
    name: string;
    description: string;
    data_points: DataPoint[];
}

export interface NewTemplateData {
    name: string;
    system_type: string;
    template_json: string;
}

export interface TargetFormData {
    name: string;
    config_http: any[];
    config_mqtt: any[];
    config_s3: any[];
}

export interface ScheduleFormData {
    schedule_name: string;
    cron_expression: string;
    data_range: 'minute' | 'hour' | 'day';
    lookback_periods: number;
}

// ─── Variable Explorer 상수 ───────────────────────────────────────────────────

export const VARIABLE_CATEGORIES = [
    {
        name: 'Mapping (Target Mapping)',
        items: [
            { label: 'Mapping Name (TARGET KEY)', value: '{{target_key}}', desc: 'Final name of data configured in the Profile tab' },
            { label: 'Data Value (VALUE)', value: '{{measured_value}}', desc: 'Actual measured value with Scale/Offset applied' },
            { label: 'Target Description (DESC)', value: '{{target_description}}', desc: 'Data description set in Profile' }
        ]
    },
    {
        name: 'Collector Source (Collector)',
        items: [
            { label: 'Building ID', value: '{{site_id}}', desc: 'Original building ID' },
            { label: 'Point ID', value: '{{point_id}}', desc: 'Original point ID' },
            { label: 'Original Name', value: '{{original_name}}', desc: 'Internal original point name in collector' },
            { label: 'Data Type', value: '{{type}}', desc: 'num:number, bit:digital, str:string' }
        ]
    },
    {
        name: 'Status & Alarms (Status)',
        items: [
            { label: 'Comm Status', value: '{{status_code}}', desc: '0:normal, 1:disconnected' },
            { label: 'Alarm Level', value: '{{alarm_level}}', desc: '0:normal, 1:caution, 2:warning' },
            { label: 'Alarm State Name', value: '{{alarm_status}}', desc: 'NORMAL, WARNING, CRITICAL, etc.' }
        ]
    },
    {
        name: 'Ranges & Limits (Ranges)',
        items: [
            { label: 'Measure Range (Min)', value: '{{mi}}', desc: 'Metering minimum limit value' },
            { label: 'Measure Range (Max)', value: '{{mx}}', desc: 'Metering maximum limit value' },
            { label: 'Info Limit', value: '{{il}}', desc: 'Information threshold limit' },
            { label: 'Danger Limit', value: '{{xl}}', desc: 'Danger threshold limit' }
        ]
    },
    {
        name: 'Timestamp (Timestamp)',
        items: [
            { label: 'Standard Timestamp', value: '{{timestamp}}', desc: 'YYYY-MM-DD HH:mm:ss.fff' },
            { label: 'ISO8601', value: '{{timestamp_iso8601}}', desc: 'Standard datetime format' },
            { label: 'Unix (ms)', value: '{{timestamp_unix_ms}}', desc: 'Milliseconds since 1970' }
        ]
    }
];

export const SAMPLE_DATA: Record<string, any> = {
    // New Aliases
    target_key: 'PRO_FILE_TARGET_KEY',
    mapping_name: 'PRO_FILE_TARGET_KEY',
    measured_value: '45.2 (MEASURED_VALUE)',
    point_value: '45.2 (MEASURED_VALUE)',
    target_description: 'Data description defined in Profile',
    site_id: '280 (SITE_ID)',
    status_code: '0 (NORMAL)',
    alarm_level: '1 (WARNING)',
    timestamp: new Date().toISOString().replace('T', ' ').split('.')[0],
    type: 'num',

    // Backward Compatibility
    nm: 'PRO_FILE_TARGET_KEY',
    vl: '45.2 (MEASURED_VALUE)',
    tm: new Date().toISOString().replace('T', ' ').split('.')[0],
    des: 'Data description defined in Profile',
    bd: '280 (SITE_ID)',
    st: '0 (NORMAL)',
    al: '1 (WARNING)',
    ty: 'num',

    // Others
    point_id: '1024',
    original_name: 'PUMP_01_STS',
    original_nm: 'PUMP_01_STS',
    timestamp_iso8601: new Date().toISOString(),
    timestamp_unix_ms: Date.now(),
    alarm_status: 'WARNING',
    il: '-',
    xl: '1',
    mi: '[0]',
    mx: '[100]'
};

// ─── Helper ───────────────────────────────────────────────────────────────────

export const extractItems = (data: any): any[] => {
    if (!data) return [];
    if (Array.isArray(data)) return data;
    if (Array.isArray(data.items)) return data.items;
    if (Array.isArray(data.rows)) return data.rows;
    return [];
};
