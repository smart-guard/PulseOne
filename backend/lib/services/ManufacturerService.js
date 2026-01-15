const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * ManufacturerService class
 * Handles business logic for manufacturers.
 */
class ManufacturerService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getManufacturerRepository();
        }
        return this._repository;
    }

    /**
     * 모든 제조사 목록을 조회합니다.
     */
    async getAllManufacturers(options = {}) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(options);
        }, 'GetAllManufacturers');
    }

    /**
     * 제조사 통계 정보를 조회합니다.
     */
    async getStatistics() {
        return await this.handleRequest(async () => {
            return await this.repository.getStatistics();
        }, 'GetStatistics');
    }

    /**
     * ID로 제조사를 조회합니다.
     */
    async getManufacturerById(id) {
        return await this.handleRequest(async () => {
            const manufacturer = await this.repository.findById(id);
            if (!manufacturer) {
                throw new Error(`제조사를 찾을 수 없습니다. (ID: ${id})`);
            }
            return manufacturer;
        }, 'GetManufacturerById');
    }

    /**
     * 새로운 제조사를 성합니다.
     */
    async createManufacturer(data) {
        return await this.handleRequest(async () => {
            // 중복 이름 체크
            const existing = await this.repository.findByName(data.name);
            if (existing) {
                throw new Error(`이미 존재하는 제조사 이름입니다: ${data.name}`);
            }
            return await this.repository.create(data);
        }, 'CreateManufacturer');
    }

    /**
     * 제조사 정보를 업데이트합니다.
     */
    async updateManufacturer(id, data) {
        return await this.handleRequest(async () => {
            const manufacturer = await this.repository.update(id, data);
            if (!manufacturer) {
                throw new Error(`제조사 정보를 업데이트할 수 없습니다. (ID: ${id})`);
            }
            return manufacturer;
        }, 'UpdateManufacturer');
    }

    /**
     * 제조사를 삭제합니다.
     */
    async deleteManufacturer(id) {
        return await this.handleRequest(async () => {
            // 연관된 모델이 있는지 체크 (추후 구현)
            const success = await this.repository.deleteById(id);
            if (!success) {
                throw new Error(`제조사 삭제에 실패했습니다. (ID: ${id})`);
            }
            return { id, success: true };
        }, 'DeleteManufacturer');
    }

    /**
     * 제조사를 복구합니다.
     */
    async restoreManufacturer(id) {
        return await this.handleRequest(async () => {
            const success = await this.repository.restoreById(id);
            if (!success) {
                throw new Error(`제조사 복구에 실패했습니다. (ID: ${id})`);
            }
            return { id, success: true };
        }, 'RestoreManufacturer');
    }
}

module.exports = ManufacturerService;
