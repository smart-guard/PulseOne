// frontend/src/api/services/databaseApi.ts
import { apiClient, ApiResponse } from '../client';

export interface TableInfo {
    name: string;
    type: string;
}

export interface ColumnInfo {
    name: string;
    type: string;
    notNull: boolean;
    pk: boolean;
    defaultValue: any;
}

export interface TableDataResponse {
    items: any[];
    pagination: {
        total: number;
        page: number;
        limit: number;
        totalPages: number;
    };
}

export class DatabaseApiService {
    private static readonly BASE_URL = '/api/database';

    // 1. 테이블 목록 조회
    static async listTables(): Promise<ApiResponse<{ tables: TableInfo[], databasePath?: string }>> {
        return await apiClient.get<{ tables: TableInfo[], databasePath?: string }>(`${this.BASE_URL}/tables`);
    }

    // 2. 테이블 스키마 조회
    static async getTableSchema(tableName: string): Promise<ApiResponse<{ schema: ColumnInfo[] }>> {
        return await apiClient.get<{ schema: ColumnInfo[] }>(`${this.BASE_URL}/tables/${tableName}/schema`);
    }

    // 3. 테이블 데이터 조회
    static async getTableData(
        tableName: string,
        params: { page?: number; limit?: number; sort?: string; order?: 'ASC' | 'DESC' }
    ): Promise<ApiResponse<TableDataResponse>> {
        return await apiClient.get<TableDataResponse>(`${this.BASE_URL}/tables/${tableName}/data`, params);
    }

    // 4. 데이터 수정
    static async updateRow(
        tableName: string,
        pkColumn: string,
        pkValue: any,
        updates: any
    ): Promise<ApiResponse<{ changes: number }>> {
        return await apiClient.put<{ changes: number }>(`${this.BASE_URL}/tables/${tableName}/rows`, {
            pkColumn,
            pkValue,
            updates
        });
    }

    // 5. 데이터 삭제
    static async deleteRow(
        tableName: string,
        pkColumn: string,
        pkValue: any
    ): Promise<ApiResponse<{ changes: number }>> {
        return await apiClient.delete<{ changes: number }>(`${this.BASE_URL}/tables/${tableName}/rows`, {
            data: { pkColumn, pkValue } // Axios delete body
        });
    }
}
