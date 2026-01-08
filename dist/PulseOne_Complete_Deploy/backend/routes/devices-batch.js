// =============================================================================
// backend/routes/devices-batch.js (새 파일 생성)
// 대량 디바이스 제어 및 성능 최적화된 API
// =============================================================================

const express = require('express');
const router = express.Router();
const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');

// Redis 캐싱 (선택적)
let RedisClient = null;
try {
  RedisClient = require('../lib/connection/redis').getInstance();
} catch (error) {
  console.warn('Redis caching disabled:', error.message);
}

// 요청 큐잉 및 배치 처리 시스템
class RequestBatcher {
  constructor() {
    this.queues = new Map(); // 디바이스별 큐
    this.processing = new Map(); // 처리 중 플래그
    this.batchSize = 10;
    this.batchTimeout = 500; // 0.5초
  }

  async addRequest(deviceId, operation, options = {}) {
    const key = `${deviceId}-${operation}`;
    
    if (!this.queues.has(key)) {
      this.queues.set(key, []);
    }
    
    return new Promise((resolve, reject) => {
      this.queues.get(key).push({
        deviceId,
        operation,
        options,
        resolve,
        reject,
        timestamp: Date.now()
      });
      
      // 배치 처리 스케줄링
      this.scheduleProcessing(key);
    });
  }

  scheduleProcessing(key) {
    if (this.processing.get(key)) return;
    
    this.processing.set(key, setTimeout(async () => {
      await this.processBatch(key);
      this.processing.delete(key);
    }, this.batchTimeout));
  }

  async processBatch(key) {
    const queue = this.queues.get(key);
    if (!queue || queue.length === 0) return;

    // 배치 크기만큼 처리
    const batch = queue.splice(0, this.batchSize);
    
    try {
      const proxy = getCollectorProxy();
      const operations = batch.map(req => ({
        deviceId: req.deviceId,
        action: req.operation,
        options: req.options
      }));

      const results = await proxy.batchDeviceControl(operations);
      
      // 각 요청에 결과 전달
      batch.forEach((req, index) => {
        const result = results.results[index];
        if (result.success) {
          req.resolve(result);
        } else {
          req.reject(new Error(result.error));
        }
      });

    } catch (error) {
      // 배치 전체 실패시 개별 에러 전달
      batch.forEach(req => req.reject(error));
    }

    // 남은 요청이 있으면 계속 처리
    if (queue.length > 0) {
      this.scheduleProcessing(key);
    }
  }
}

const requestBatcher = new RequestBatcher();

// =============================================================================
// 캐시 관리 시스템
// =============================================================================

class DeviceStatusCache {
  constructor() {
    this.memoryCache = new Map();
    this.ttl = 30000; // 30초
  }

  async get(deviceId) {
    const key = `device_status:${deviceId}`;
    
    // 1. 메모리 캐시 확인
    const cached = this.memoryCache.get(key);
    if (cached && Date.now() - cached.timestamp < this.ttl) {
      return cached.data;
    }

    // 2. Redis 캐시 확인 (있을 경우)
    if (RedisClient) {
      try {
        const redisData = await RedisClient.get(key);
        if (redisData) {
          const parsed = JSON.parse(redisData);
          this.memoryCache.set(key, { data: parsed, timestamp: Date.now() });
          return parsed;
        }
      } catch (error) {
        console.warn('Redis cache read failed:', error.message);
      }
    }

    return null;
  }

  async set(deviceId, data) {
    const key = `device_status:${deviceId}`;
    
    // 1. 메모리 캐시 저장
    this.memoryCache.set(key, { data, timestamp: Date.now() });
    
    // 2. Redis 캐시 저장 (있을 경우)
    if (RedisClient) {
      try {
        await RedisClient.setex(key, Math.floor(this.ttl / 1000), JSON.stringify(data));
      } catch (error) {
        console.warn('Redis cache write failed:', error.message);
      }
    }
  }

  clear(deviceId) {
    const key = `device_status:${deviceId}`;
    this.memoryCache.delete(key);
    
    if (RedisClient) {
      RedisClient.del(key).catch(err => 
        console.warn('Redis cache clear failed:', err.message)
      );
    }
  }

  getStats() {
    return {
      memoryItems: this.memoryCache.size,
      ttl: this.ttl,
      redisEnabled: !!RedisClient
    };
  }
}

const statusCache = new DeviceStatusCache();

// =============================================================================
// 고성능 배치 API 엔드포인트들
// =============================================================================

/**
 * 대량 디바이스 제어 (최적화된 버전)
 * POST /api/devices/batch/control
 */
router.post('/batch/control', async (req, res) => {
  const startTime = Date.now();
  
  try {
    const { operations, options = {} } = req.body;
    
    if (!Array.isArray(operations) || operations.length === 0) {
      return res.status(400).json({
        success: false,
        error: 'operations array is required'
      });
    }

    console.log(`🚀 Batch control request: ${operations.length} operations`);
    
    // 요청 검증
    const validOps = ['start', 'stop', 'restart', 'status'];
    const invalidOps = operations.filter(op => !validOps.includes(op.action));
    
    if (invalidOps.length > 0) {
      return res.status(400).json({
        success: false,
        error: `Invalid operations: ${invalidOps.map(op => op.action).join(', ')}`
      });
    }

    // 배치 처리 방식 선택
    let results;
    
    if (options.useBatcher && operations.length <= 50) {
      // 소규모 배치: Request Batcher 사용
      console.log('Using Request Batcher for small batch');
      
      const promises = operations.map(op => 
        requestBatcher.addRequest(op.deviceId, op.action, op.options || {})
      );
      
      const batchResults = await Promise.allSettled(promises);
      results = {
        success: batchResults.every(r => r.status === 'fulfilled'),
        results: batchResults.map((r, i) => ({
          deviceId: operations[i].deviceId,
          action: operations[i].action,
          success: r.status === 'fulfilled',
          data: r.status === 'fulfilled' ? r.value.data : null,
          error: r.status === 'rejected' ? r.reason.message : null
        })),
        summary: {
          total: operations.length,
          successful: batchResults.filter(r => r.status === 'fulfilled').length,
          failed: batchResults.filter(r => r.status === 'rejected').length
        }
      };
      
    } else {
      // 대규모 배치: Direct Collector API 사용
      console.log('Using Direct Collector API for large batch');
      
      const proxy = getCollectorProxy();
      results = await proxy.batchDeviceControl(operations);
    }

    const duration = Date.now() - startTime;
    console.log(`✅ Batch completed in ${duration}ms: ${results.summary.successful}/${results.summary.total} successful`);

    // 성공한 디바이스들의 캐시 무효화
    results.results
      .filter(r => r.success && ['start', 'stop', 'restart'].includes(r.action))
      .forEach(r => statusCache.clear(r.deviceId));

    res.json({
      success: results.success,
      message: `Batch operation completed: ${results.summary.successful}/${results.summary.total} successful`,
      data: results,
      performance: {
        duration_ms: duration,
        throughput: Math.round(operations.length / (duration / 1000))
      },
      timestamp: new Date().toISOString()
    });

  } catch (error) {
    const duration = Date.now() - startTime;
    console.error(`❌ Batch control failed in ${duration}ms:`, error.message);
    
    res.status(500).json({
      success: false,
      error: 'Batch control operation failed',
      details: error.message,
      performance: { duration_ms: duration },
      timestamp: new Date().toISOString()
    });
  }
});

/**
 * 대량 디바이스 상태 조회 (캐시 적용)
 * POST /api/devices/batch/status
 */
router.post('/batch/status', async (req, res) => {
  const startTime = Date.now();
  
  try {
    const { device_ids, use_cache = true } = req.body;
    
    if (!Array.isArray(device_ids) || device_ids.length === 0) {
      return res.status(400).json({
        success: false,
        error: 'device_ids array is required'
      });
    }

    console.log(`📊 Batch status request: ${device_ids.length} devices (cache: ${use_cache})`);

    const results = [];
    const toFetch = [];
    let cacheHits = 0;

    // 1. 캐시에서 먼저 조회
    if (use_cache) {
      for (const deviceId of device_ids) {
        const cached = await statusCache.get(deviceId);
        if (cached) {
          results.push({
            deviceId,
            success: true,
            data: cached,
            source: 'cache'
          });
          cacheHits++;
        } else {
          toFetch.push(deviceId);
        }
      }
    } else {
      toFetch.push(...device_ids);
    }

    // 2. 캐시 미스된 디바이스들은 Collector에서 조회
    if (toFetch.length > 0) {
      console.log(`🔍 Fetching ${toFetch.length} devices from Collector`);
      
      const proxy = getCollectorProxy();
      const operations = toFetch.map(deviceId => ({
        deviceId,
        action: 'status'
      }));

      try {
        const fetchResults = await proxy.batchDeviceControl(operations);
        
        fetchResults.results.forEach((result, index) => {
          const deviceId = toFetch[index];
          
          if (result.success) {
            // 캐시에 저장
            statusCache.set(deviceId, result.data);
            
            results.push({
              deviceId,
              success: true,
              data: result.data,
              source: 'collector'
            });
          } else {
            results.push({
              deviceId,
              success: false,
              error: result.error,
              source: 'collector'
            });
          }
        });
        
      } catch (collectorError) {
        // Collector 전체 실패시 개별 에러 처리
        toFetch.forEach(deviceId => {
          results.push({
            deviceId,
            success: false,
            error: collectorError.message,
            source: 'collector'
          });
        });
      }
    }

    // deviceId 순서대로 정렬
    results.sort((a, b) => {
      const aIndex = device_ids.indexOf(a.deviceId);
      const bIndex = device_ids.indexOf(b.deviceId);
      return aIndex - bIndex;
    });

    const duration = Date.now() - startTime;
    const successful = results.filter(r => r.success).length;
    
    console.log(`✅ Batch status completed in ${duration}ms: ${successful}/${device_ids.length} successful, ${cacheHits} cache hits`);

    res.json({
      success: true,
      message: `Batch status retrieved: ${successful}/${device_ids.length} successful`,
      data: results,
      performance: {
        duration_ms: duration,
        cache_hits: cacheHits,
        cache_hit_rate: Math.round((cacheHits / device_ids.length) * 100),
        collector_requests: toFetch.length
      },
      timestamp: new Date().toISOString()
    });

  } catch (error) {
    const duration = Date.now() - startTime;
    console.error(`❌ Batch status failed in ${duration}ms:`, error.message);
    
    res.status(500).json({
      success: false,
      error: 'Batch status operation failed',
      details: error.message,
      performance: { duration_ms: duration },
      timestamp: new Date().toISOString()
    });
  }
});

/**
 * 실시간 데이터 조회 (WebSocket 대체)
 * POST /api/devices/batch/realtime
 */
router.post('/batch/realtime', async (req, res) => {
  try {
    const { device_ids, point_types = [], sampling_rate = 1000 } = req.body;
    
    if (!Array.isArray(device_ids) || device_ids.length === 0) {
      return res.status(400).json({
        success: false,
        error: 'device_ids array is required'
      });
    }

    console.log(`📡 Batch realtime request: ${device_ids.length} devices`);

    const proxy = getCollectorProxy();
    const promises = device_ids.map(async (deviceId) => {
      try {
        const result = await proxy.getCurrentData(deviceId, point_types);
        return {
          deviceId,
          success: true,
          data: result.data,
          timestamp: new Date().toISOString()
        };
      } catch (error) {
        return {
          deviceId,
          success: false,
          error: error.message,
          timestamp: new Date().toISOString()
        };
      }
    });

    const results = await Promise.allSettled(promises);
    const successful = results.filter(r => r.status === 'fulfilled' && r.value.success).length;

    res.json({
      success: true,
      message: `Realtime data retrieved: ${successful}/${device_ids.length} devices`,
      data: results.map(r => r.status === 'fulfilled' ? r.value : { success: false, error: 'Promise rejected' }),
      sampling_rate: sampling_rate,
      timestamp: new Date().toISOString()
    });

  } catch (error) {
    console.error('❌ Batch realtime failed:', error.message);
    
    res.status(500).json({
      success: false,
      error: 'Batch realtime operation failed',
      details: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

/**
 * 성능 모니터링 API
 * GET /api/devices/batch/metrics
 */
router.get('/batch/metrics', (req, res) => {
  const proxy = getCollectorProxy();
  
  res.json({
    success: true,
    data: {
      collector_proxy: proxy.getConnectionStats(),
      request_batcher: {
        active_queues: requestBatcher.queues.size,
        processing_batches: requestBatcher.processing.size
      },
      status_cache: statusCache.getStats(),
      system: {
        memory: process.memoryUsage(),
        uptime: process.uptime()
      }
    },
    timestamp: new Date().toISOString()
  });
});

module.exports = router;