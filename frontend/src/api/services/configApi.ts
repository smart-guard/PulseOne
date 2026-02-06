// frontend/src/api/services/configApi.ts
import { apiClient, ApiResponse } from '../client';

export interface ConfigFile {
    name: string;
    size: number;
    updatedAt: string;
}

export class ConfigApiService {
    private static readonly BASE_URL = '/api/config';

    // 1. 설정 파일 목록 조회
    static async listFiles(): Promise<ApiResponse<{ files: ConfigFile[], path?: string }>> {
        return await apiClient.get<{ files: ConfigFile[], path?: string }>(`${this.BASE_URL}/files`);
    }

    // 2. 파일 내용 조회
    static async getFileContent(filename: string): Promise<ApiResponse<{ content: string }>> {
        return await apiClient.get<{ content: string }>(`${this.BASE_URL}/files/${filename}`);
    }

    // 3. 파일 내용 저장
    static async saveFileContent(filename: string, content: string): Promise<ApiResponse<void>> {
        return await apiClient.put<void>(`${this.BASE_URL}/files/${filename}`, { content });
    }

    // 4. 시크릿 암호화
    static async encryptSecret(value: string): Promise<ApiResponse<{ encrypted: string }>> {
        return await apiClient.post<{ encrypted: string }>(`${this.BASE_URL}/encrypt`, { value });
    }

    // 5. 시크릿 복호화 (Smart Unlock)
    static async decryptSecret(value: string): Promise<ApiResponse<{ decrypted: string }>> {
        return await apiClient.post<{ decrypted: string }>(`${this.BASE_URL}/decrypt`, { value });
    }
}
