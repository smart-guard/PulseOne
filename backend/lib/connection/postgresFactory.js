// lib/connection/postgresFactory.js
const { Client } = require('pg');
const config = require('../../config');

function createPostgresClient({ host, port, user, password, database }) {
  return new Client({ host, port, user, password, database });
}

module.exports = {
  main: () => createPostgresClient(config.postgres.main),
  log: () => createPostgresClient(config.postgres.log),
};