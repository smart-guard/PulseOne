// lib/connection/influx.js
const { InfluxDB, Point } = require('@influxdata/influxdb-client');

function getInfluxClient() {
  const url = process.env.INFLUX_URL || 'http://localhost:8086';
  const token = process.env.INFLUX_TOKEN || 'my-token';

  return new InfluxDB({ url, token });
}

function createWritePoint(measurement, tags = {}, fields = {}, timestamp = new Date()) {
  const point = new Point(measurement);
  Object.entries(tags).forEach(([k, v]) => point.tag(k, v));
  Object.entries(fields).forEach(([k, v]) => point.floatField(k, v));
  point.timestamp(timestamp);
  return point;
}

module.exports = {
  getInfluxClient,
  createWritePoint,
};
