const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database('/app/data/db/pulseone.db');

console.log("--- Schema of data_points ---");
db.all("PRAGMA table_info(data_points)", [], (err, rows) => {
    if (err) console.error(err.message);
    else console.log(JSON.stringify(rows, null, 2));

    console.log("\n--- Data Points for Device 12346 ---");
    db.all("SELECT * FROM data_points WHERE device_id = 12346", [], (err, rows) => {
        if (err) console.error(err.message);
        else console.log(JSON.stringify(rows, null, 2));
        db.close();
    });
});
