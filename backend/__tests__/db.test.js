const db = require('../lib/connection/db');

describe('Database Connection Test', () => {
  afterAll(async () => {
    if (db.end) await db.end(); // PostgreSQL/MariaDB
    if (db.close) db.close();   // SQLite
  });

  it('should connect and run a simple SELECT 1 query', async () => {
    let result;

    // PostgreSQL / MariaDB
    if (db.query) {
      result = await db.query('SELECT 1 AS value');
      expect(result[0]?.value || result.rows?.[0]?.value).toBe(1);

    // SQLite
    } else if (db.get) {
      result = await new Promise((resolve, reject) => {
        db.get('SELECT 1 AS value', [], (err, row) => {
          if (err) reject(err);
          else resolve(row);
        });
      });
      expect(result.value).toBe(1);
    } else {
      throw new Error('Unsupported DB type or no query method found');
    }
  });
});
