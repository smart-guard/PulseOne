// ============================================================================
// frontend/src/api/services/controlScheduleApi.ts
// [translated comment]
// ============================================================================

const BASE = '';

export interface ControlSchedule {
    id: number;
    tenant_id?: number;
    point_id: number;
    device_id: number;
    point_name?: string;
    device_name?: string;
    value: string;
    cron_expr?: string;   // [translated comment]
    once_at?: string;     // [translated comment]
    enabled: boolean;
    last_run?: string;
    description?: string;
    created_at: string;
}

export interface CreateSchedulePayload {
    point_id: number;
    device_id: number;
    point_name?: string;
    device_name?: string;
    value: string;
    cron_expr?: string;
    once_at?: string;
    enabled?: boolean;
    description?: string;
}

export interface ControlSchedulePagination {
    page: number;
    pageSize: number;
    totalCount: number;
    totalPages: number;
}

async function apiFetch<T>(path: string, options: RequestInit = {}): Promise<{ success: boolean; data?: T; pagination?: ControlSchedulePagination; message?: string }> {
    const token = localStorage.getItem('token') || sessionStorage.getItem('token') || '';
    const res = await fetch(`${BASE}${path}`, {
        headers: { 'Content-Type': 'application/json', ...(token ? { Authorization: `Bearer ${token}` } : {}) },
        ...options,
    });
    return res.json();
}

export const ControlScheduleApi = {
    list: (page = 1, pageSize = 20) =>
        apiFetch<ControlSchedule[]>(`/api/control-schedules?page=${page}&pageSize=${pageSize}`),

    create: (payload: CreateSchedulePayload) =>
        apiFetch<ControlSchedule>('/api/control-schedules', {
            method: 'POST', body: JSON.stringify(payload),
        }),

    toggle: (id: number, enabled: boolean) =>
        apiFetch<ControlSchedule>(`/api/control-schedules/${id}`, {
            method: 'PUT', body: JSON.stringify({ enabled }),
        }),

    remove: (id: number) =>
        apiFetch<void>(`/api/control-schedules/${id}`, { method: 'DELETE' }),
};
