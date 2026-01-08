const { getInfluxClient } = require('../lib/connection/influx');
const { Point } = require('@influxdata/influxdb-client');
const axios = require('axios');

describe('InfluxDB 연결 테스트', () => {
    let influx;
    const ORG = process.env.INFLUXDB_ORG;
    const BUCKET = process.env.INFLUXDB_BUCKET;
    const URL = `http://${process.env.INFLUXDB_HOST}:${process.env.INFLUXDB_PORT}`;

    beforeAll(() => {
        influx = getInfluxClient();
    });

    test('Ping test', async () => {
        const res = await axios.get(`${URL}/ping`);
        expect(res.status).toBe(204);
    });

    test('Write & Query test', async () => {
        const writeApi = influx.getWriteApi(ORG, BUCKET);
        const point = new Point('temperature')
            .tag('location', 'server-room')
            .floatField('value', 23.5)
            .timestamp(new Date());

        writeApi.writePoint(point);
        await writeApi.close();

        const queryApi = influx.getQueryApi(ORG);
        const fluxQuery = `from(bucket: "${BUCKET}")
      |> range(start: -1h)
      |> filter(fn: (r) => r._measurement == "temperature")`;

        const rows = [];
        await queryApi.collectRows(fluxQuery, {
            next: (row, tableMeta) => {
                rows.push(tableMeta.toObject(row));
            },
            error: (error) => {
                throw error;
            },
            complete: () => {
                expect(rows.length).toBeGreaterThan(0);
            },
        });
    });
});
