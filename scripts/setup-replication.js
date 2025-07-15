// scripts/setup-replication.js
const redis = require('redis');
const { env } = require('../lib/config');

async function setupReplication() {
  const mainHost = env.REDIS_MAIN_HOST || 'localhost';
  const mainPort = parseInt(env.REDIS_MAIN_PORT || '6379', 10);

  const replicas = (env.REDIS_REPLICAS || '')
    .split(',')
    .map(rep => rep.trim())
    .filter(Boolean); // e.g. ['replica1:6380', 'replica2:6381']

  if (!replicas.length) {
    console.log('[INFO] No REDIS_REPLICAS defined. Skipping replication setup.');
    return;
  }

  console.log(`[INFO] Configuring ${replicas.length} Redis replicas for ${mainHost}:${mainPort}`);

  for (const replica of replicas) {
    const [host, portStr] = replica.split(':');
    const port = parseInt(portStr || '6379', 10);

    const client = redis.createClient({
      host,
      port,
      socket_keepalive: true,
    });

    client.on('error', (err) => {
      console.error(`[ERROR] Redis error at ${host}:${port} →`, err.message);
    });

    await new Promise((resolve) => {
      client.connect()
        .then(() => {
          console.log(`[INFO] Connected to replica ${host}:${port}`);
          return client.sendCommand(['REPLICAOF', mainHost, mainPort.toString()]);
        })
        .then(() => {
          console.log(`[✅] ${host}:${port} now replicates from ${mainHost}:${mainPort}`);
        })
        .catch(err => {
          console.error(`[❌] Failed to configure replica ${host}:${port}:`, err.message);
        })
        .finally(async () => {
          await client.disconnect();
          resolve();
        });
    });
  }

  console.log('[DONE] Redis replication setup completed.');
}

setupReplication();
