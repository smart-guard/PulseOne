const BaseService = require('./BaseService');

class DataPointService extends BaseService {
    constructor() {
        super();
        this._repository = null;

        // Lazy load hooks to avoid circular dependencies
        this._configSyncHooks = null;
    }

    get repository() {
        if (!this._repository) {
            const RepositoryFactory = require('../database/repositories/RepositoryFactory');
            this._repository = RepositoryFactory.getInstance().getRepository('DeviceRepository');
        }
        return this._repository;
    }

    get configSyncHooks() {
        if (!this._configSyncHooks) {
            const { getInstance } = require('../hooks/ConfigSyncHooks');
            this._configSyncHooks = getInstance();
        }
        return this._configSyncHooks;
    }

    /**
     * 디바이스의 데이터포인트 목록 조회
     */
    async getDeviceDataPoints(deviceId, options = {}) {
        return await this.handleRequest(async () => {
            const knex = this.repository.knex;
            let query = knex('data_points').where('device_id', deviceId);

            if (options.page && options.limit) {
                const offset = (options.page - 1) * options.limit;
                const totalQuery = query.clone().count('id as count').first();
                const itemsQuery = query.clone().limit(options.limit).offset(offset);

                const [totalRes, items] = await Promise.all([totalQuery, itemsQuery]);
                return {
                    items,
                    total: totalRes.count
                };
            }

            const items = await query;
            return {
                items,
                total: items.length
            };
        }, 'GetDeviceDataPoints');
    }

    /**
     * 데이터포인트 일괄 저장 (Upsert)
     * 이 방법은 DeviceService.updateDevice를 통하지 않고 데이터포인트만 따로 저장할 때 사용
     */
    async bulkUpsert(deviceId, dataPoints, tenantId) {
        return await this.handleRequest(async () => {
            // DeviceRepository의 update 로직 재사용 (데이터포인트만 업데이트)
            const result = await this.repository.knex.transaction(async (trx) => {
                // Update implementation logic from DeviceRepository.update but narrowed to data_points
                const updateData = { data_points: dataPoints };

                // Logic derived from refined DeviceRepository.update
                const existingPoints = await trx('data_points').where('device_id', deviceId).select('*');
                const existingIds = existingPoints.map(p => p.id);
                const existingMap = new Map(existingPoints.map(p => [p.id, p]));

                const toInsert = [];
                const toUpdate = [];
                const retainedIds = [];

                for (const dp of dataPoints) {
                    const isNew = !dp.id || dp.id > 2000000000 || !existingIds.includes(Number(dp.id));

                    const existingPoint = !isNew ? existingMap.get(Number(dp.id)) : null;
                    const pointData = this.repository._mapDataPointToDb(dp, deviceId, existingPoint);

                    if (isNew) {
                        pointData.created_at = trx.fn.now();
                        toInsert.push(pointData);
                    } else {
                        toUpdate.push({ id: Number(dp.id), data: pointData });
                        retainedIds.push(Number(dp.id));
                    }
                }

                const idsToDelete = existingIds.filter(eid => !retainedIds.includes(eid));
                if (idsToDelete.length > 0) await trx('data_points').whereIn('id', idsToDelete).del();
                for (const item of toUpdate) await trx('data_points').where('id', item.id).update(item.data);
                if (toInsert.length > 0) await trx('data_points').insert(toInsert);

                return { count: dataPoints.length, device_id: deviceId };
            });

            // 데이터포인트 일괄 변경 후 디바이스 재시작 (Sync)
            try {
                const device = await this.repository.findById(deviceId, tenantId);
                if (device) {
                    await this.configSyncHooks.afterDeviceUpdate(device, device); // Trigger restart
                }
            } catch (err) {
                this.logger.warn('Failed to trigger sync after bulk upsert', err.message);
            }

            return result;
        }, 'BulkUpsertDataPoints');
    }
}

module.exports = new DataPointService();
