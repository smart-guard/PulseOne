export interface Manufacturer {
    id: number;
    name: string;
    description?: string;
    country?: string;
    website?: string;
    logo_url?: string;
    is_active: boolean;
    is_deleted?: boolean;
    model_count?: number;
    created_at: string;
    updated_at: string;
}

export interface DeviceModel {
    id: number;
    manufacturer_id: number;
    name: string;
    description?: string;
    device_type: string;
    protocol_id: number;
    config_schema?: any;
    default_config?: any;
    is_active: boolean;
    manufacturer_name?: string;
    manual_url?: string; // Added metadata
    created_at: string;
    updated_at: string;
}

export interface Protocol {
    id: number;
    name: string;
    protocol_type: string;
    display_name: string;
    description?: string;
    is_active: boolean;
    config_schema?: any;
    connection_params?: any;
    default_config?: any;
}

export interface TemplatePoint {
    id: number;
    template_device_id: number;
    name: string;
    description?: string;
    address: number | string;
    address_string?: string;
    data_type: string;
    access_mode: 'RO' | 'WO' | 'RW' | 'read' | 'write' | 'read_write';
    unit?: string;
    scaling_factor?: number;
    scaling_offset?: number;
    is_writable?: boolean;
    metadata?: any;
}

export interface DeviceTemplate {
    id: number;
    manufacturer_id: number;
    model_id: number;
    name: string;
    description?: string;
    device_type: string;
    protocol_id: number;
    config?: any;
    polling_interval: number;
    timeout: number;
    is_public: boolean;
    author_id?: number;
    version: string;
    manufacturer_name?: string;
    model_name?: string;
    protocol_name?: string;
    manual_url?: string;
    model_description?: string;
    point_count?: number;
    device_count?: number;
    data_points?: TemplatePoint[];
    created_at: string;
    updated_at: string;
}

export interface AuditLog {
    id: number;
    tenant_id: number;
    user_id: number;
    user_name?: string;
    action: 'CREATE' | 'UPDATE' | 'DELETE' | 'INSTANTIATE' | 'ACTION';
    entity_type: 'DEVICE' | 'TEMPLATE' | 'SYSTEM' | 'USER';
    entity_id?: string;
    entity_name?: string;
    old_value?: any;
    new_value?: any;
    change_summary?: string;
    details?: any;
    ip_address?: string;
    created_at: string;
}
