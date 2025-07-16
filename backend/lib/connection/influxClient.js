// lib/connection/influxClient.js
const { InfluxDB } = require('@influxdata/influxdb-client');
const config = require('../../../config/env');

const influx = new InfluxDB({
  url: config.influx.url,
  token: config.influx.token,
});

module.exports = influx.getWriteApi(config.influx.org, config.influx.bucket);
