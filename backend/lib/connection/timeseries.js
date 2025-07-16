const { getInfluxClient, createWritePoint } = require('./influx');
const { influxdb } = require('../../../config/env'); // ✅ config에서 가져오기

class TimeSeriesDB {
  constructor() {
    this.client = getInfluxClient();
    this.org = influxdb.org || 'pulseone';
    this.bucket = influxdb.bucket || 'metrics';
  }

  async writePoint(measurement, tags = {}, fields = {}, timestamp = new Date()) {
    try {
      const writeApi = this.client.getWriteApi(this.org, this.bucket);
      const point = createWritePoint(measurement, tags, fields, timestamp);

      writeApi.writePoint(point);
      await writeApi.close(); // ⚠️ 무조건 close 되도록
    } catch (err) {
      console.error('[Influx Write Error]', err.message || err);
      throw err; // 호출자에게 전파
    }
  }

  async query(flux) {
    const queryApi = this.client.getQueryApi(this.org);
    const rows = [];

    return new Promise((resolve, reject) => {
      queryApi.queryRows(flux, {
        next(row, tableMeta) {
          rows.push(tableMeta.toObject(row));
        },
        error(error) {
          console.error('[Influx Query Error]', error.message || error);
          reject(error);
        },
        complete() {
          resolve(rows);
        },
      });
    });
  }

  // 선택: 향후 flush 또는 close 필요 시
  close() {
    // this.client.close?.(); // 필요 시 Influx 클라이언트 정리
  }
}

module.exports = TimeSeriesDB;
