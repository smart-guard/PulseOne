const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database('/app/data/db/pulseone.db');

db.run("UPDATE alarm_occurrences SET state = 'CLEARED', cleared_time = CURRENT_TIMESTAMP WHERE point_id = 2002 AND state = 'ACTIVE'", [], function (err) {
    if (err) {
        console.error(err.message);
    } else {
        console.log(`Row(s) updated: ${this.changes}`);
    }
    db.close();
});
