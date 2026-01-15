const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * TenantService class
 * Handles business logic for tenants (customers).
 */
class TenantService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getTenantRepository();
        }
        return this._repository;
    }

    /**
     * 모든 테넌트 목록을 조회합니다.
     */
    async getAllTenants(options = {}) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(options);
        }, 'GetAllTenants');
    }

    /**
     * 테넌트 상세 정보를 조회합니다.
     */
    async getTenantById(id, trx = null) {
        return await this.handleRequest(async () => {
            const tenant = await this.repository.getTenantStatistics(id, trx);
            if (!tenant) {
                throw new Error(`테넌트를 찾을 수 없습니다. (ID: ${id})`);
            }
            return tenant;
        }, 'GetTenantById');
    }

    /**
     * 새로운 테넌트를 생성합니다.
     */
    async createTenant(data) {
        return await this.handleRequest(async () => {
            // 중복 회사명 체크
            const nameExists = await this.repository.checkCompanyNameExists(data.company_name);
            if (nameExists) {
                throw new Error(`이미 존재하는 회사명입니다: ${data.company_name}`);
            }

            // 중복 회사 코드 체크
            const codeExists = await this.repository.checkCompanyCodeExists(data.company_code);
            if (codeExists) {
                throw new Error(`이미 존재하는 회사 코드입니다: ${data.company_code}`);
            }

            const id = await this.repository.create(data);
            return await this.repository.findById(id);
        }, 'CreateTenant');
    }

    /**
     * 테넌트 정보를 업데이트합니다.
     */
    async updateTenant(id, data) {
        return await this.handleRequest(async () => {
            const success = await this.repository.update(id, data);
            if (!success) {
                throw new Error(`테넌트 정보를 업데이트할 수 없습니다. (ID: ${id})`);
            }
            return await this.repository.findById(id);
        }, 'UpdateTenant');
    }

    /**
     * 테넌트 구독 정보를 업데이트합니다.
     */
    async updateSubscription(id, data) {
        return await this.handleRequest(async () => {
            const success = await this.repository.updateSubscription(id, data);
            if (!success) {
                throw new Error(`구독 정보를 업데이트할 수 없습니다. (ID: ${id})`);
            }
            return await this.repository.findById(id);
        }, 'UpdateSubscription');
    }

    /**
     * 테넌트를 삭제합니다 (소프트 삭제).
     */
    async deleteTenant(id) {
        return await this.handleRequest(async () => {
            const success = await this.repository.deleteById(id);
            if (!success) {
                throw new Error(`테넌트 삭제에 실패했습니다. (ID: ${id})`);
            }
            return { id, success: true };
        }, 'DeleteTenant');
    }

    /**
     * 테넌트를 복구합니다.
     */
    async restoreTenant(id) {
        return await this.handleRequest(async () => {
            const success = await this.repository.restoreById(id);
            if (!success) {
                throw new Error(`테넌트 복구에 실패했습니다. (ID: ${id})`);
            }
            return { id, success: true };
        }, 'RestoreTenant');
    }

    /**
     * 전체 테넌트 통계(성능상 전체 목록 조회 대신 집계 쿼리 사용)
     */
    async getGlobalStatistics() {
        return await this.handleRequest(async () => {
            return await this.repository.getGlobalStatistics();
        }, 'GetGlobalStatistics');
    }

    /**
     * 테넌트 상세 통계 (사용량 등)
     */
    async getTenantStatistics(id, trx = null) {
        return await this.handleRequest(async () => {
            return await this.repository.getTenantStatistics(id, trx);
        }, 'GetTenantStatistics');
    }
}

module.exports = TenantService;
