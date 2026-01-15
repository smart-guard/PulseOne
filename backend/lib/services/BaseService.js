/**
 * backend/lib/services/BaseService.js
 * 모든 비즈니스 로직 서비스의 기본 클래스
 */

class BaseService {
    constructor(repository) {
        this._repository = repository;
        this.logger = console;
    }

    get repository() {
        return this._repository;
    }

    /**
     * 표준 응답 생성
     */
    createResponse(success, data, message = '', errorCode = null) {
        return {
            success,
            data,
            message: message || (success ? 'Success' : 'Error'),
            error_code: errorCode,
            timestamp: new Date().toISOString()
        };
    }

    /**
     * 예외 처리 래퍼
     */
    async handleRequest(operation, errorContext = 'Operation') {
        try {
            const result = await operation();
            return this.createResponse(true, result);
        } catch (error) {
            this.logger.error(`❌ [${errorContext}] Error:`, error.message);
            if (error.stack) {
                // 스택 트레이스도 함께 출력하여 디버깅 용이성 확보
                this.logger.error(error.stack);
            }
            return this.createResponse(false, null, error.message, 'INTERNAL_SERVICE_ERROR');
        }
    }

    /**
     * 트랜잭션 도우미
     */
    async transaction(callback) {
        if (!this.repository || !this.repository.knex) {
            throw new Error('트랜잭션을 지원하는 레포지토리가 설정되지 않았습니다.');
        }
        return await this.repository.knex.transaction(callback);
    }
}

module.exports = BaseService;
