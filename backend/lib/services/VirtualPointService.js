// =============================================================================
// backend/lib/services/VirtualPointService.js
// 가상포인트 레이어 비즈니스 로직
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

class VirtualPointService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getRepository('VirtualPointRepository');
        }
        return this._repository;
    }

    /**
     * 가상포인트 목록 조회
     */
    async findAll(filters) {
        return await this.handleRequest(async () => {
            return await this.repository.findAllVirtualPoints(filters);
        }, 'VirtualPointService.findAll');
    }

    /**
     * 가상포인트 상세 조회
     */
    async findById(id, tenantId) {
        return await this.handleRequest(async () => {
            const vp = await this.repository.findById(id, tenantId);
            if (!vp) throw new Error(`가상포인트(ID: ${id})를 찾을 수 없습니다.`);
            return vp;
        }, 'VirtualPointService.findById');
    }

    /**
     * 가상포인트 생성
     */
    async create(data, inputs, tenantId) {
        return await this.handleRequest(async () => {
            // 중복 이름 체크
            const existing = await this.repository.findByName(data.name, tenantId);
            if (existing) throw new Error('동일한 이름의 가상포인트가 이미 존재합니다.');

            return await this.repository.createVirtualPoint(data, inputs, tenantId);
        }, 'VirtualPointService.create');
    }

    /**
     * 가상포인트 업데이트
     */
    async update(id, data, inputs, tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.updateVirtualPoint(id, data, inputs, tenantId);
        }, 'VirtualPointService.update');
    }

    /**
     * 가상포인트 삭제
     */
    async delete(id, tenantId) {
        return await this.handleRequest(async () => {
            const success = await this.repository.deleteById(id, tenantId);
            if (!success) throw new Error('가상포인트 삭제 실패 (존재하지 않거나 권한 없음)');
            return { id, success: true };
        }, 'VirtualPointService.delete');
    }

    /**
     * 활성화 상태 토글
     */
    async toggleEnabled(id, isEnabled, tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.updateEnabledStatus(id, isEnabled, tenantId);
        }, 'VirtualPointService.toggleEnabled');
    }

    /**
     * 카테고리별 통계
     */
    async getStatsByCategory(tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.getStatsByCategory(tenantId);
        }, 'VirtualPointService.getStatsByCategory');
    }

    /**
     * 성능 통계
     */
    async getPerformanceStats(tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.getPerformanceStats(tenantId);
        }, 'VirtualPointService.getPerformanceStats');
    }

    /**
     * 고아 레코드 정리
     */
    async cleanupOrphaned(tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.cleanupOrphanedRecords();
        }, 'VirtualPointService.cleanupOrphaned');
    }
}

module.exports = new VirtualPointService();
