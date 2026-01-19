#!/bin/bash

BASE_URL="http://localhost:3000/api"

echo "=== Export Gateway API Verification ==="

# 1. Get Profiles (to needed for Target creation)
echo "1. Fetching Profiles..."
PROFILES=$(curl -s "$BASE_URL/export/profiles")
# Profiles return { success: true, data: [...] }, no double wrap
PROFILE_ID=$(echo $PROFILES | jq -r '.data[0].id // empty')

if [ "$PROFILE_ID" == "null" ] || [ -z "$PROFILE_ID" ]; then
    echo "No profiles found. Creating a temporary profile..."
    # Create a profile if none exists (assuming endpoint exists)
    # Actually, we might need to skip if we can't create one easily via this script without more context.
    # Let's hope there is one.
    echo "Skipping Target creation tests due to missing Profile."
else
    echo "Found Profile ID: $PROFILE_ID"
fi

# 2. Payload Templates
echo "2. Testing Payload Templates..."

# Create Template
TIMESTAMP=$(date +%s)
TEMPLATE_PAYLOAD="{
    \"name\": \"Test Template $TIMESTAMP\",
    \"system_type\": \"custom\",
    \"description\": \"Created by verify_export_api.sh\",
    \"template_json\": \"{\\\"test\\\": \\\"value\\\"}\",
    \"is_active\": true
}"

echo "Creating Template..."
CREATE_RES=$(curl -s -X POST -H "Content-Type: application/json" -d "$TEMPLATE_PAYLOAD" "$BASE_URL/export/templates")
echo "CREATE_RES: $CREATE_RES"
TEMPLATE_ID=$(echo $CREATE_RES | jq -r '.data.data.id // .data.id // .id // empty')
echo "Created Template ID: $TEMPLATE_ID"

# Get Templates
echo "Fetching Templates..."
curl -s "$BASE_URL/export/templates" | jq '.'

# Update Template
echo "Updating Template..."
UPDATE_PAYLOAD='{
    "name": "Updated Test Template"
}'
curl -s -X PUT -H "Content-Type: application/json" -d "$UPDATE_PAYLOAD" "$BASE_URL/export/templates/$TEMPLATE_ID" | jq '.'

# 3. Export Targets & Mappings (Only if Profile ID exists)
if [ ! -z "$PROFILE_ID" ] && [ "$PROFILE_ID" != "null" ]; then
    echo "3. Testing Export Targets & Mappings..."
    
    # Create Target
    TARGET_PAYLOAD="{
        \"profile_id\": $PROFILE_ID,
        \"name\": \"Test Target $TIMESTAMP\",
        \"target_type\": \"http\",
        \"config\": \"{}\",
        \"is_enabled\": true,
        \"template_id\": $TEMPLATE_ID
    }"
    
    echo "Creating Target..."
    # Note: Using /api/export/targets endpoint which presumably exists from previous sessions
    TARGET_RES=$(curl -s -X POST -H "Content-Type: application/json" -d "$TARGET_PAYLOAD" "$BASE_URL/export/targets")
    echo "TARGET_RES: $TARGET_RES"
    TARGET_ID=$(echo $TARGET_RES | jq -r '.data.data.id // .data.id // .id // empty')
    echo "Created Target ID: $TARGET_ID"
    
    # Create Mappings
    MAPPINGS_PAYLOAD="{
        \"mappings\": [
            {
                \"point_id\": 1,
                \"target_field_name\": \"voltage\",
                \"target_description\": \"Voltage L1\",
                \"is_enabled\": true
            },
            {
                \"point_id\": 2,
                \"target_field_name\": \"current\",
                \"target_description\": \"Current L1\",
                \"is_enabled\": true
            }
        ]
    }"
    
    echo "Saving Mappings..."
    curl -s -X POST -H "Content-Type: application/json" -d "$MAPPINGS_PAYLOAD" "$BASE_URL/export/targets/$TARGET_ID/mappings" | jq '.'
    
    echo "Fetching Mappings..."
    curl -s "$BASE_URL/export/targets/$TARGET_ID/mappings" | jq '.'
    
    # Cleanup Target
    echo "Deleting Target..."
    curl -s -X DELETE "$BASE_URL/export/targets/$TARGET_ID"
fi

# Cleanup Template
echo "Deleting Template..."
curl -s -X DELETE "$BASE_URL/export/templates/$TEMPLATE_ID"

echo "=== Verification Complete ==="
