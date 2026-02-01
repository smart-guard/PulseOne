const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * DeviceModelService class
 * Handles business logic for device models.
 */
class DeviceModelService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getDeviceModelRepository();
        }
        return this._repository;
    }

    /**
     * 모든 디바이스 모델 목록을 조회합니다.
     */
    async getAllModels(options = {}) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(options);
        }, 'GetAllModels');
    }

    /**
     * ID로 디바이스 모델을 조회합니다.
     */
    async getModelById(id) {
        return await this.handleRequest(async () => {
            const model = await this.repository.findById(id);
            if (!model) {
                throw new Error(`디바이스 모델을 찾을 수 없습니다. (ID: ${id})`);
            }
            return model;
        }, 'GetModelById');
    }

    /**
     * 특정 제조사의 모델 목록을 조회합니다.
     */
    async getModelsByManufacturer(manufacturerId) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll({ manufacturerId });
        }, 'GetModelsByManufacturer');
    }

    /**
     * 새로운 디바이스 모델을 생성합니다.
     */
    async createModel(data) {
        return await this.handleRequest(async () => {
            // Check for existing model with same manufacturer and name
            if (data.manufacturer_id && data.name) {
                const existing = await this.repository.findUnique(data.manufacturer_id, data.name);
                if (existing) {
                    // Reuse existing model, verify ownership or just return/update
                    // We update it to ensure latest metadata is applied
                    return await this.repository.update(existing.id, data);
                }
            }
            return await this.repository.create(data);
        }, 'CreateModel');
    }

    /**
     * 디바이스 모델 정보를 업데이트합니다.
     */
    async updateModel(id, data) {
        return await this.handleRequest(async () => {
            const model = await this.repository.update(id, data);
            if (!model) {
                throw new Error(`디바이스 모델 정보를 업데이트할 수 없습니다. (ID: ${id})`);
            }
            return model;
        }, 'UpdateModel');
    }

    /**
     * 디바이스 모델을 삭제합니다.
     */
    async deleteModel(id) {
        return await this.handleRequest(async () => {
            const success = await this.repository.deleteById(id);
            if (!success) {
                throw new Error(`디바이스 모델 삭제에 실패했습니다. (ID: ${id})`);
            }
            return { id, success: true };
        }, 'DeleteModel');
    }
}

module.exports = DeviceModelService;
