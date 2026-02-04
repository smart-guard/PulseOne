const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database('/app/data/db/pulseone.db');

console.log("--- Active Alarms (state = 'active' or 'ACTIVE' or 1) ---");
db.all("SELECT * FROM alarm_occurrences WHERE state LIKE 'active%' OR state = '1'", [], (err, rows) => {
    if (err) console.error(err.message);
    else console.log(JSON.stringify(rows, null, 2));
    db.close();
});
