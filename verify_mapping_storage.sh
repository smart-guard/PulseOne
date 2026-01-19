#!/bin/bash

# Configuration
API_URL="http://localhost:3000/api/export/targets"
DB_PATH="./data/db/pulseone.db"
TEMPLATE_ID=14
TIMESTAMP=$(date +%s)
TARGET_NAME="Verify_Storage_$TIMESTAMP"

echo "=== Verifying Data Entry and DB Storage ==="

# 1. Create a Target with Mappings
echo "1. Creating Target with Mappings via API..."
PAYLOAD=$(cat <<EOF
{
    "profile_id": 1,
    "name": "$TARGET_NAME",
    "target_type": "http",
    "config": {
        "url": "https://insite-collector.example.com",
        "method": "POST",
        "headers": {"Content-Type": "application/json"}
    },
    "template_id": $TEMPLATE_ID,
    "is_enabled": 1,
    "export_mode": "on_change"
}
EOF
)

RESPONSE=$(curl -s -X POST "$API_URL" -H "Content-Type: application/json" -d "$PAYLOAD")
echo "API Response (Target): $RESPONSE"

# Extract ID from response (simple grep/sed as we might not have jq)
TARGET_ID=$(echo "$RESPONSE" | grep -o '"id":[0-9]*' | head -1 | awk -F: '{print $2}')
echo "Created Target ID: $TARGET_ID"

if [ -z "$TARGET_ID" ]; then
    echo "❌ Failed to create target. Aborting."
    exit 1
fi

# 1.1 Save Mappings
echo "1.1 Saving Mappings..."
MAPPINGS_PAYLOAD=$(cat <<EOF
{
    "mappings": [
        {
            "point_id": 101,
            "target_field_name": "temp_sensor_01",
            "target_description": "Main Hall Temperature",
            "is_enabled": true
        },
        {
            "point_id": 102,
            "target_field_name": "humid_sensor_01",
            "target_description": "Main Hall Humidity",
            "is_enabled": true
        }
    ]
}
EOF
)

MAPPING_RESPONSE=$(curl -s -X POST "$API_URL/$TARGET_ID/mappings" -H "Content-Type: application/json" -d "$MAPPINGS_PAYLOAD")
echo "API Response (Mappings): $MAPPING_RESPONSE"

# 2. Verify DB Storage
echo "2. Checking Database Storage..."
if [ ! -f "$DB_PATH" ]; then
    echo "❌ Database file not found at $DB_PATH"
    exit 1
fi

# We query the 'config' and 'mapping_json' (if it exists) or check if mappings are embedded in config or separate table.
# Based on schema.sql (implied), mappings might be in specific table or config column.
# Let's check the schema first or assume standard behavior.
# Actually, typically mappings are stored in a separate table or JSON column.
# Let's inspect the `export_targets` table.

echo "Querying export_targets for ID $TARGET_ID..."
sqlite3 "$DB_PATH" "SELECT id, name, template_id FROM export_targets WHERE id=$TARGET_ID;"

# Check for mappings. If there is an export_target_mappings table, query it.
# If unrelated, check if valid.
# Assuming mappings are stored in a related table `export_target_mappings` based on common patterns,
# OR purely inside the `config` JSON if not normalized.
# Let's try to list tables to be sure.
TABLES=$(sqlite3 "$DB_PATH" ".tables")

if [[ $TABLES == *"export_target_mappings"* ]]; then
    echo "Checking export_target_mappings table..."
    COUNT=$(sqlite3 "$DB_PATH" "SELECT count(*) FROM export_target_mappings WHERE target_id=$TARGET_ID;")
    echo "Mappings found in DB: $COUNT"
    if [ "$COUNT" -eq "2" ]; then
        echo "✅ Success: 2 mappings stored in export_target_mappings."
    else
        echo "❌ Failure: Expected 2 mappings, found $COUNT."
    fi
else
    # Maybe stored in config?
    echo "No export_target_mappings table. Checking config column..."
    CONFIG=$(sqlite3 "$DB_PATH" "SELECT config FROM export_targets WHERE id=$TARGET_ID;")
    echo "Config: $CONFIG"
    if [[ $CONFIG == *"temp_sensor_01"* ]]; then
        echo "✅ Success: Mapping found in config JSON."
    else
         echo "⚠️ Mapping not found in config. Please verify schema."
    fi
fi

# 3. Cleanup
echo "3. Cleaning up..."
curl -s -X DELETE "$API_URL/$TARGET_ID" > /dev/null
echo "Test Target Deleted."

echo "=== Verification Complete ==="
