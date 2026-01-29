-- 1. Insert specific JSON payload template requested by user
INSERT INTO payload_templates (id, name, code, description, template_json, version, created_at, updated_at)
VALUES (5, 'Insite JSON Template', 'insite_json', 'User requested JSON format', '{
  "alarm_flag": {{alarm_flag}},
  "alarm_status": "{{alarm_status}}",
  "building_id": {{building_id}},
  "description": "{{description}}",
  "point_name": "{{point_name}}",
  "source": "PulseOne-CSPGateway",
  "status": {{status}},
  "timestamp": "{{timestamp}}",
  "upload_timestamp": "{{timestamp_iso8601}}",
  "value": {{value}},
  "version": "2.0"
}', 1, DATETIME('now'), DATETIME('now'));

-- 2. Update S3 Target (ID 21) to use the new Payload Template (ID 5)
-- Also update ObjectKeyTemplate to be strictly timestamp-based as requested: {year}{month}{day}{hour}{minute}{second}.json
UPDATE export_targets 
SET 
    payload_template_id = 5,
    config = '{"S3ServiceUrl":"https://s3.ap-northeast-2.amazonaws.com","BucketName":"hdcl-csp-stg","Folder":"icos/alarm","ObjectKeyTemplate":"{year}{month}{day}{hour}{minute}{second}.json","AccessKeyID":"AKIAQKDMIGZZECNAU4X7","SecretAccessKey":"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","execution_order":1}'
WHERE id = 21;

-- 3. Verify the update
SELECT id, name, payload_template_id, config FROM export_targets WHERE id = 21;
