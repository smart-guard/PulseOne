// ===========================================================================
// lib/connection/influx.js  
// ===========================================================================
const { InfluxDB, Point } = require('@influxdata/influxdb-client');
const env = require('../config/ConfigManager');

class InfluxDBManager {
  constructor() {
    this.url = env.INFLUXDB_HOST 
      ? `http://${env.INFLUXDB_HOST}:${env.INFLUXDB_PORT || 8086}`
      : 'http://localhost:8086';
    this.token = env.INFLUXDB_TOKEN || 'mytoken';
    this.org = env.INFLUXDB_ORG || 'pulseone';
    this.bucket = env.INFLUXDB_BUCKET || 'metrics';

    this.client = new InfluxDB({ url: this.url, token: this.token });
    this.writeApi = this.client.getWriteApi(this.org, this.bucket);
    this.queryApi = this.client.getQueryApi(this.org);

    // 배치 설정
    this.writeApi.useDefaultTags({ source: 'pulseone-backend' });
    
    console.log(`✅ InfluxDB 클라이언트 초기화: ${this.url}`);
  }

  // 포인트 생성 헬퍼
  createPoint(measurement, tags = {}, fields = {}, timestamp = new Date()) {
    const point = new Point(measurement);
    
    Object.entries(tags).forEach(([key, value]) => {
      point.tag(key, String(value));
    });

    Object.entries(fields).forEach(([key, value]) => {
      if (typeof value === 'number') {
        point.floatField(key, value);
      } else if (typeof value === 'boolean') {
        point.booleanField(key, value);
      } else {
        point.stringField(key, String(value));
      }
    });

    point.timestamp(timestamp);
    return point;
  }

  // 단일 포인트 쓰기
  writePoint(measurement, tags = {}, fields = {}, timestamp = new Date()) {
    try {
      const point = this.createPoint(measurement, tags, fields, timestamp);
      this.writeApi.writePoint(point);
      console.log(`📈 InfluxDB 포인트 기록: ${measurement}`);
    } catch (error) {
      console.error('❌ InfluxDB 쓰기 실패:', error.message);
    }
  }

  // 배치 포인트 쓰기
  writePoints(points) {
    try {
      this.writeApi.writePoints(points);
      console.log(`📈 InfluxDB 배치 포인트 기록: ${points.length}개`);
    } catch (error) {
      console.error('❌ InfluxDB 배치 쓰기 실패:', error.message);
    }
  }

  // 쿼리 실행
  async query(fluxQuery) {
    try {
      const rows = [];
      await this.queryApi.queryRows(fluxQuery, {
        next(row, tableMeta) {
          const o = tableMeta.toObject(row);
          rows.push(o);
        },
        error(error) {
          console.error('❌ InfluxDB 쿼리 실패:', error);
        },
      });
      return rows;
    } catch (error) {
      console.error('❌ InfluxDB 쿼리 실행 실패:', error.message);
      throw error;
    }
  }

  // 연결 종료
  async close() {
    try {
      await this.writeApi.close();
      console.log('✅ InfluxDB 연결 종료됨');
    } catch (error) {
      console.error('❌ InfluxDB 종료 실패:', error.message);
    }
  }
}

const influxManager = new InfluxDBManager();

module.exports = {
  getInfluxClient: () => influxManager.client,
  writePoint: influxManager.writePoint.bind(influxManager),
  writePoints: influxManager.writePoints.bind(influxManager),
  query: influxManager.query.bind(influxManager),
  createPoint: influxManager.createPoint.bind(influxManager),
  close: influxManager.close.bind(influxManager)
};