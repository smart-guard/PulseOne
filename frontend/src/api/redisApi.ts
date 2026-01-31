import { apiClient as api } from './client';

export interface RedisKeyInfo {
    key: string;
    type: string;
    ttl?: number;
}

export interface RedisScanResult {
    cursor: string;
    keys: RedisKeyInfo[];
}

export interface RedisKeyDetails {
    key: string;
    type: string;
    ttl: number;
    value: any;
    isJson: boolean;
}

export interface RedisConnectionConfig {
    host: string;
    port: number;
    password?: string;
    db?: number;
}

const getHeaders = (config?: RedisConnectionConfig) => {
    if (!config) return {};
    return {
        'x-redis-config': JSON.stringify(config)
    };
};

export const redisApi = {
    // Get Server Info
    getInfo: async (config?: RedisConnectionConfig) => {
        try {
            const response = await api.request<any>({
                url: '/api/redis/info',
                method: 'GET',
                headers: getHeaders(config)
            });
            return response.data; // Now returns { info: ... }
        } catch (error) {
            console.error('Failed to fetch Redis info', error);
            throw error;
        }
    },

    // Scan Keys
    scanKeys: async (cursor = '0', match = '*', count = 100, config?: RedisConnectionConfig): Promise<RedisScanResult> => {
        const response = await api.request<RedisScanResult>({
            url: '/api/redis/scan',
            method: 'GET',
            body: null, // GET doesn't have body, but params are needed. 
            // UnifiedHttpClient doesn't support params in request() config directly? 
            // Wait, preprocessRequest uses config.body for body.
            // UnifiedHttpClient.request doesn't support params!
            // I must build query params manually or use buildQueryParams manually.
        });
        // Check UnifiedHttpClient again.
        // It does NOT support params in request(). It only supports params in get().

        // I need to construct URL with params manually if I use request().
        const queryParams = new URLSearchParams();
        queryParams.append('cursor', cursor);
        queryParams.append('match', match);
        queryParams.append('count', String(count));

        const response2 = await api.request<RedisScanResult>({
            url: `/api/redis/scan?${queryParams.toString()}`,
            method: 'GET',
            headers: getHeaders(config)
        });
        return response2.data as RedisScanResult;
    },

    // Get Key Details
    getKey: async (key: string, config?: RedisConnectionConfig): Promise<RedisKeyDetails> => {
        const response = await api.request<RedisKeyDetails>({
            url: `/api/redis/key/${encodeURIComponent(key)}`,
            method: 'GET',
            headers: getHeaders(config)
        });
        return response.data as RedisKeyDetails;
    },

    // Set Key Value
    setValue: async (key: string, value: any, type = 'string', ttl?: number, config?: RedisConnectionConfig) => {
        const response = await api.request<any>({
            url: '/api/redis',
            method: 'POST',
            body: {
                key,
                value,
                type,
                ttl
            },
            headers: getHeaders(config)
        });
        return response.data;
    },

    // Delete Key
    deleteKey: async (key: string, config?: RedisConnectionConfig) => {
        const response = await api.request<any>({
            url: `/api/redis/${encodeURIComponent(key)}`,
            method: 'DELETE',
            headers: getHeaders(config)
        });
        return response.data;
    }
};
