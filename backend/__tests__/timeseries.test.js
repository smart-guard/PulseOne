// __tests__/timeseries.test.js

const { getInfluxClient } = require('../../lib/connection/influx');

describe('InfluxDB 연결 테스트', () => {
  let influx;

  beforeAll(() => {
    influx = getInfluxClient();
  });

  test('Ping test', async () => {
    const health = await influx.ping(5000);
    expect(Array.isArray(health)).toBe(true);
    expect(health[0].online).toBe(true);
  });

  test('Write & Query test', async () => {
    const writeApi = influx.getWriteApi('org', 'bucket');
    writeApi.writePoint({
      measurement: 'temperature',
      tags: { location: 'server-room' },
      fields: { value: 23.5 },
      timestamp: new Date(),
    });

    await writeApi.close();

    const queryApi = influx.getQueryApi('org');
    const result = await queryApi.collectRows(`from(bucket:"bucket") |> range(start: -1h)`);

    expect(Array.isArray(result)).toBe(true);
  });
});
