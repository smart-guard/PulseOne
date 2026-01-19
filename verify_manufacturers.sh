#!/bin/bash
DB_PATH="./data/db/pulseone.db"

if [ ! -f "$DB_PATH" ]; then
    echo "❌ Database file not found at $DB_PATH"
    exit 1
fi

echo "Checking manufacturers table..."
sqlite3 "$DB_PATH" "SELECT count(*) FROM manufacturers;"
sqlite3 "$DB_PATH" "SELECT * FROM manufacturers LIMIT 5;"
