// ===========================================================================
// backend/lib/connection/timeseries.js - ConfigManager 사용하도록 수정
// ===========================================================================
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

class TimeSeriesManager {
    constructor() {
        // ConfigManager에서 설정 로드
        this.influxEnabled = config.getBoolean('USE_INFLUXDB', true);
        this.redisEnabled = config.getBoolean('REDIS_PRIMARY_ENABLED', true);
        
        this.influxDB = null;
        this.redisClient = null;
        
        console.log(`📋 TimeSeries 설정:
   InfluxDB 사용: ${this.influxEnabled}
   Redis 사용: ${this.redisEnabled}`);
    }

    async initialize() {
        try {
            // InfluxDB 초기화
            if (this.influxEnabled) {
                const { InfluxDB } = require('@influxdata/influxdb-client');
                
                const host = config.get('INFLUXDB_HOST', 'localhost');
                const port = config.get('INFLUXDB_PORT', '8086');
                const url = host.includes('http') ? host : `http://${host}:${port}`;
                const token = config.get('INFLUXDB_TOKEN', 'mytoken');
                const org = config.get('INFLUXDB_ORG', 'pulseone');
                const bucket = config.get('INFLUXDB_BUCKET', 'metrics');
                
                this.influxDB = {
                    client: new InfluxDB({ url, token }),
                    org,
                    bucket,
                    writeApi: null,
                    queryApi: null
                };
                
                this.influxDB.writeApi = this.influxDB.client.getWriteApi(org, bucket);
                this.influxDB.queryApi = this.influxDB.client.getQueryApi(org);
                
                console.log('✅ InfluxDB TimeSeries 초기화 성공');
            }

            // Redis 초기화 (캐시용)
            if (this.redisEnabled) {
                const redis = require('./redis');
                this.redisClient = redis;
                console.log('✅ Redis TimeSeries 캐시 초기화 성공');
            }

            return true;
        } catch (error) {
            console.error('❌ TimeSeries 초기화 실패:', error.message);
            console.log('⚠️  TimeSeries 없이 계속 진행합니다.');
            return false;
        }
    }

    /**
     * 시계열 데이터 쓰기
     */
    async writeData(measurement, tags, fields, timestamp = null) {
        const results = {
            influx: false,
            redis: false
        };

        // InfluxDB에 쓰기
        if (this.influxEnabled && this.influxDB) {
            try {
                const { Point } = require('@influxdata/influxdb-client');
                const point = new Point(measurement);
                
                // tags 추가
                if (tags) {
                    Object.entries(tags).forEach(([key, value]) => {
                        point.tag(key, value);
                    });
                }
                
                // fields 추가
                if (fields) {
                    Object.entries(fields).forEach(([key, value]) => {
                        if (typeof value === 'number') {
                            point.floatField(key, value);
                        } else if (typeof value === 'boolean') {
                            point.booleanField(key, value);
                        } else {
                            point.stringField(key, String(value));
                        }
                    });
                }
                
                // timestamp 추가
                if (timestamp) {
                    point.timestamp(new Date(timestamp));
                }
                
                this.influxDB.writeApi.writePoint(point);
                results.influx = true;
                
            } catch (error) {
                console.error('❌ InfluxDB 쓰기 실패:', error.message);
            }
        }

        // Redis에 캐시 (최근 데이터용)
        if (this.redisEnabled && this.redisClient) {
            try {
                const cacheKey = `timeseries:${measurement}:${Object.values(tags || {}).join(':')}`;
                const cacheData = {
                    measurement,
                    tags,
                    fields,
                    timestamp: timestamp || new Date().toISOString()
                };
                
                // 5분간 캐시
                await this.redisClient.setEx(cacheKey, 300, JSON.stringify(cacheData));
                results.redis = true;
                
            } catch (error) {
                console.error('❌ Redis 캐시 쓰기 실패:', error.message);
            }
        }

        return results;
    }

    /**
     * 시계열 데이터 조회 (InfluxDB)
     */
    async queryData(fluxQuery) {
        if (!this.influxEnabled || !this.influxDB) {
            throw new Error('InfluxDB가 활성화되지 않음');
        }

        try {
            const results = [];
            await this.influxDB.queryApi.queryRows(fluxQuery, {
                next(row, tableMeta) {
                    const o = tableMeta.toObject(row);
                    results.push(o);
                },
                error(error) {
                    console.error('❌ InfluxDB 쿼리 오류:', error);
                }
            });
            return results;
        } catch (error) {
            console.error('❌ InfluxDB 쿼리 실패:', error.message);
            return [];
        }
    }

    /**
     * 최근 데이터 조회 (Redis 캐시)
     */
    async getRecentData(measurement, tags = {}) {
        if (!this.redisEnabled || !this.redisClient) {
            return null;
        }

        try {
            const cacheKey = `timeseries:${measurement}:${Object.values(tags).join(':')}`;
            const cached = await this.redisClient.get(cacheKey);
            
            if (cached) {
                return JSON.parse(cached);
            }
            return null;
        } catch (error) {
            console.error('❌ Redis 캐시 조회 실패:', error.message);
            return null;
        }
    }

    /**
     * 기본 메트릭 조회
     */
    async getMetrics(measurement, timeRange = '1h') {
        const fluxQuery = `
            from(bucket: "${this.influxDB?.bucket || 'metrics'}")
                |> range(start: -${timeRange})
                |> filter(fn: (r) => r._measurement == "${measurement}")
                |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)
                |> yield(name: "mean")
        `;
        
        return await this.queryData(fluxQuery);
    }

    /**
     * 리소스 정리
     */
    async close() {
        try {
            if (this.influxDB?.writeApi) {
                await this.influxDB.writeApi.close();
            }
            console.log('📴 TimeSeries 연결 종료');
        } catch (error) {
            console.error('❌ TimeSeries 종료 오류:', error.message);
        }
    }

    /**
     * 상태 확인
     */
    getStatus() {
        return {
            influxEnabled: this.influxEnabled,
            redisEnabled: this.redisEnabled,
            influxReady: !!this.influxDB,
            redisReady: !!this.redisClient
        };
    }
}

// 싱글톤 인스턴스 생성
const timeSeriesManager = new TimeSeriesManager();

module.exports = timeSeriesManager;