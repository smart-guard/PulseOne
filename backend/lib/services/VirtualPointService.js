// =============================================================================
// backend/lib/services/VirtualPointService.js
// 가상포인트 레이어 비즈니스 로직
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');
const ScriptEngineService = require('./ScriptEngineService');

class VirtualPointService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * 감사 로그 기록 (내부 헬퍼)
     */
    async logAction(pointId, action, previousState, newState, userId = null, details = null) {
        try {
            await this.repository.createLog({
                point_id: pointId,
                action,
                previous_state: previousState ? JSON.stringify(previousState) : null,
                new_state: newState ? JSON.stringify(newState) : null,
                user_id: userId,
                details
            });
        } catch (error) {
            this.logger.error(`감사 로그 기록 실패 [${action}]:`, error);
            // 로그 실패가 메인 트랜잭션을 방해하지 않도록 예외 무시
        }
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

            const newPoint = await this.repository.createVirtualPoint(data, inputs, tenantId);

            // Audit Log
            await this.logAction(newPoint.id, 'CREATE', null, newPoint, null, 'Virtual Point Created');

            return newPoint;
        }, 'VirtualPointService.create');
    }

    /**
     * 가상포인트 업데이트
     */
    async update(id, data, inputs, tenantId) {
        return await this.handleRequest(async () => {
            // 이전 상태 조회 (로그용)
            const previousState = await this.repository.findById(id, tenantId);
            const updatedPoint = await this.repository.updateVirtualPoint(id, data, inputs, tenantId);

            // Audit Log
            await this.logAction(id, 'UPDATE', previousState, updatedPoint, null, 'Virtual Point Updated');

            return updatedPoint;
        }, 'VirtualPointService.update');
    }

    /**
     * 가상포인트 삭제
     */
    async delete(id, tenantId) {
        return await this.handleRequest(async () => {
            // 이전 상태 조회 (로그용)
            const previousState = await this.repository.findById(id, tenantId);

            const success = await this.repository.deleteById(id, tenantId);
            if (!success) throw new Error('가상포인트 삭제 실패 (존재하지 않거나 권한 없음)');

            // Audit Log
            await this.logAction(id, 'DELETE', previousState, { is_deleted: true }, null, 'Virtual Point Soft Deleted');

            return { id, success: true, mode: 'soft-delete' };
        }, 'VirtualPointService.delete');
    }

    /**
     * 가상포인트 복원
     */
    async restore(id, tenantId) {
        return await this.handleRequest(async () => {
            const success = await this.repository.restoreById(id, tenantId);
            if (!success) throw new Error('가상포인트 복원 실패 (존재하지 않거나 권한 없음)');

            // Audit Log
            await this.logAction(id, 'RESTORE', { is_deleted: true }, { is_deleted: false }, null, 'Virtual Point Restored');

            return { id, success: true };
        }, 'VirtualPointService.restore');
    }

    /**
     * 활성화 상태 토글
     */
    async toggleEnabled(id, isEnabled, tenantId) {
        return await this.handleRequest(async () => {
            const result = await this.repository.updateEnabledStatus(id, isEnabled, tenantId);

            // Audit Log
            await this.logAction(id, isEnabled ? 'ENABLE' : 'DISABLE', null, { is_enabled: isEnabled }, null, `Virtual Point ${isEnabled ? 'Enabled' : 'Disabled'}`);

            return result;
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

    /**
     * 감사 로그 조회
     */
    async getHistory(pointId, tenantId) {
        return await this.handleRequest(async () => {
            // 포인트 존재 및 권한 확인
            await this.findById(pointId, tenantId);
            return await this.repository.getLogs(pointId);
        }, 'VirtualPointService.getHistory');
    }

    /**
     * 가상포인트 수동 실행 (Execute)
     */
    async execute(id, options, tenantId) {
        return await this.handleRequest(async () => {
            // 1. 가져오기
            const vp = await this.findById(id, tenantId);

            // 2. inputs 추출 및 기본 context 구성
            const context = options.context || {};

            // 3. ScriptEngineService를 활용하여 계산
            // 엔진쪽 testScript와 유사하게 호출
            const result = await ScriptEngineService.testScript({
                script: vp.formula,
                context: context
            }, tenantId);

            // 성공/실패 여부에 따라 히스토리나 로그 기록을 추가할 수 있음
            if (result.success) {
                await this.logAction(id, 'EXECUTE', null, { result: result.result }, null, 'Manual Execution Success');
            } else {
                await this.logAction(id, 'EXECUTE_ERROR', null, { error: result.error }, null, 'Manual Execution Failed');
            }

            return result;
        }, 'VirtualPointService.execute');
    }
}

module.exports = new VirtualPointService();
