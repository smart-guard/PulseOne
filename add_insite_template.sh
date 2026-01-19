#!/bin/bash

# API Settings
API_URL="http://localhost:3000/api/export/templates"

# Template Data
# Note: "vl" is a string in the user's example, so we keep quotes around {{value}}.
TEMPLATE_JSON='{
    "bd": "{{building_id}}",
    "ty": "num",
    "nm": "{{point_name}}",
    "vl": "{{value}}",
    "tm": "{{timestamp}}",
    "st": "{{status}}"
}'

# Create Payload
# We use jq to safely stringify the embedded JSON for the template_json field
PAYLOAD=$(jq -n \
                  --arg name "Insite 주기 데이터" \
                  --arg system_type "insite" \
                  --arg description "Insite 플랫폼용 주기 데이터 전송 템플릿 (S3 포맷)" \
                  --arg t_json "$TEMPLATE_JSON" \
                  '{
                    name: $name,
                    system_type: $system_type,
                    description: $description,
                    template_json: $t_json
                  }')

echo "=== Registering Insite Template ==="
echo "Payload: $PAYLOAD"

# Execute Request
RESPONSE=$(curl -s -X POST "$API_URL" \
  -H "Content-Type: application/json" \
  -d "$PAYLOAD")

echo ""
echo "Response: $RESPONSE"
echo "=== Done ==="
