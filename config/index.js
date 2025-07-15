const env = require('./env');

module.exports = {
  env,

  redis: {
    main: {
      host: env.REDIS_MAIN_HOST,
      port: env.REDIS_MAIN_PORT,
      password: env.REDIS_MAIN_PASSWORD,
    },
    replicas: env.REDIS_REPLICAS.map((entry) => {
      const [host, port] = entry.split(':');
      return {
        host,
        port: parseInt(port, 10),
      };
    }),
    secondary: {
      host: env.REDIS_SECONDARY_HOST,
      port: env.REDIS_SECONDARY_PORT,
      password: env.REDIS_SECONDARY_PASSWORD,
    },
  },

  postgres: {
    main: {
      host: env.POSTGRES_MAIN_DB_HOST,
      port: env.POSTGRES_MAIN_DB_PORT,
      user: env.POSTGRES_MAIN_DB_USER,
      password: env.POSTGRES_MAIN_DB_PASSWORD,
      database: env.POSTGRES_MAIN_DB_NAME,
    },
    log: {
      host: env.POSTGRES_LOG_DB_HOST,
      port: env.POSTGRES_LOG_DB_PORT,
      user: env.POSTGRES_LOG_DB_USER,
      password: env.POSTGRES_LOG_DB_PASSWORD,
      database: env.POSTGRES_LOG_DB_NAME,
    },
  },

  influxdb: {
    host: env.INFLUXDB_HOST,
    port: env.INFLUXDB_PORT,
    token: env.INFLUXDB_TOKEN,
    org: env.INFLUXDB_ORG,
    bucket: env.INFLUXDB_BUCKET,
  },

  rpc: {
    host: env.RPC_HOST,
    port: env.RPC_PORT,
  },

  rabbitmq: {
    host: env.RABBITMQ_HOST,
    port: env.RABBITMQ_PORT,
    managementPort: env.RABBITMQ_MANAGEMENT_PORT,
  },

  app: {
    port: env.BACKEND_PORT,
    dbType: env.DB_TYPE,
    sqlitePath: env.SQLITE_PATH,
    stage: env.ENV_STAGE,
  },
};
