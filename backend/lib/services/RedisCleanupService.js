const { getRedisClient } = require('../connection/redis');

/**
 * RedisCleanupService
 * 🎯 목적: 설정 변경(이름 변경, 삭제 등) 시 Redis에 남는 불필요한 데이터를 즉시 정리
 */
class RedisCleanupService {
    /**
     * 특정 디바이스와 관련된 모든 Redis 키 삭제
     * @param {string|number} deviceId 디바이스 ID (숫자형 또는 문자열)
     */
    async cleanupDevice(deviceId) {
        try {
            const redis = await getRedisClient();
            if (!redis) return;

            const id = String(deviceId);
            console.log(`🧹 Cleaning up Redis keys for device: ${id}`);

            // 삭제할 패턴 목록
            const patterns = [
                `device:${id}:*`,       // 실시간 포인트 데이터 및 상태
                `worker:${id}:*`,       // 워커 상태 및 통계
                `current_values:${id}`, // 묶음 현재값 데이터
                `device:full:${id}`     // 전체 데이터 스냅샷
            ];

            for (const pattern of patterns) {
                await this.deleteByPattern(redis, pattern);
            }
        } catch (error) {
            console.error(`❌ Failed to cleanup device ${deviceId} from Redis:`, error.message);
        }
    }

    /**
     * 특정 데이터포인트와 관련된 Redis 키 삭제
     * @param {string|number} deviceId 디바이스 ID
     * @param {string|number} pointId 포인트 ID
     * @param {string} pointName 포인트 이름 (있는 경우)
     */
    async cleanupDataPoint(deviceId, pointId, pointName) {
        try {
            const redis = await getRedisClient();
            if (!redis) return;

            const dId = String(deviceId);
            const pId = String(pointId);

            console.log(`🧹 Cleaning up Redis keys for point: ${pId} (Device: ${dId}, Name: ${pointName || 'N/A'})`);

            const keysToDelete = [
                `point:${pId}:current`,
                `point:${pId}:latest`,
                `point:${pId}:light`
            ];

            if (pointName) {
                keysToDelete.push(`device:${dId}:${pointName}`);
            }

            // 개별 키 삭제
            for (const key of keysToDelete) {
                await redis.del(key);
            }
        } catch (error) {
            console.error(`❌ Failed to cleanup point ${pointId} from Redis:`, error.message);
        }
    }

    /**
     * 패턴 기반 키 삭제 (내부 유틸리티)
     * @param {object} redis 
     * @param {string} pattern 
     */
    async deleteByPattern(redis, pattern) {
        try {
            // KEYS 대신 SCAN을 사용하여 성능 영향 최소화 (대규모 환경 대비)
            let cursor = '0';
            do {
                const reply = await redis.scan(cursor, { MATCH: pattern, COUNT: 100 });
                cursor = reply.cursor;
                const keys = reply.keys;

                if (keys && keys.length > 0) {
                    await redis.del(keys);
                    console.log(`   - Deleted ${keys.length} keys for pattern: ${pattern}`);
                }
            } while (cursor !== '0');
        } catch (error) {
            console.error(`   - Error deleting pattern ${pattern}:`, error.message);
        }
    }
}

module.exports = new RedisCleanupService();
