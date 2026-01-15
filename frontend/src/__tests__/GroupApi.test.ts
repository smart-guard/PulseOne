
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { GroupApiService } from '../api/services/groupApi';

// Mock global fetch
const mockFetch = vi.fn();
global.fetch = mockFetch;

// Mock localStorage for token
const localStorageMock = (function () {
    let store: Record<string, string> = {};
    return {
        getItem: (key: string) => store[key] || null,
        setItem: (key: string, value: string) => { store[key] = value.toString(); },
        removeItem: (key: string) => { delete store[key]; },
        clear: () => { store = {}; }
    };
})();
Object.defineProperty(global, 'localStorage', { value: localStorageMock });

describe('Group API Integration Tests', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        localStorageMock.setItem('auth_token', 'dev-dummy-token');
    });

    it('should successfully create a group when backend returns 201', async () => {
        const mockResponse = {
            success: true,
            data: { id: 1, name: 'New Group', tenant_id: 1 }
        };

        mockFetch.mockResolvedValueOnce({
            ok: true,
            status: 201,
            json: async () => mockResponse
        });

        const result = await GroupApiService.createGroup({ name: 'New Group' });

        expect(result.success).toBe(true);
        expect(result.data).toEqual(mockResponse.data);
        expect(mockFetch).toHaveBeenCalledWith(
            expect.stringContaining('/api/groups'),
            expect.objectContaining({
                method: 'POST',
                headers: expect.objectContaining({
                    'Authorization': 'Bearer dev-dummy-token'
                })
            })
        );
    });

    it('should handle creation failure when backend returns 400', async () => {
        const errorResponse = {
            success: false,
            message: 'Tenant ID required',
            error: 'SQLITE_CONSTRAINT'
        };

        mockFetch.mockResolvedValueOnce({
            ok: false,
            status: 400,
            json: async () => errorResponse
        });

        const result = await GroupApiService.createGroup({ name: 'Fail Group' });

        expect(result.success).toBe(false);
        expect(result.message).toBe('Tenant ID required');
    });
});
