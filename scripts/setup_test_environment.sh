#!/bin/bash
# scripts/setup_test_environment.sh

DB_PATH="/Users/kyungho/Project/PulseOne/data/db/pulseone.db"

echo "🛠️ Ensuring schema is up to date..."
# mapping_key 컬럼이 없으면 추가
sqlite3 "$DB_PATH" "ALTER TABLE data_points ADD COLUMN mapping_key VARCHAR(255);" 2>/dev/null || echo "Column 'mapping_key' already exists or other error (ignored)"

echo "🧹 Cleaning up old test data..."
sqlite3 "$DB_PATH" "DELETE FROM alarm_rules WHERE name LIKE 'Sim_%';"
sqlite3 "$DB_PATH" "DELETE FROM virtual_points WHERE name LIKE 'Sim_%';"
sqlite3 "$DB_PATH" "DELETE FROM data_points WHERE name LIKE 'Sim_%';"
sqlite3 "$DB_PATH" "DELETE FROM devices WHERE name LIKE 'Simulator-%';"

echo "🚀 Inserting Simulator Devices..."
# Modbus Device
sqlite3 "$DB_PATH" "INSERT INTO devices (tenant_id, site_id, protocol_id, name, device_type, endpoint, config, is_enabled) \
VALUES (1, 1, 1, 'Simulator-PLC', 'PLC', '127.0.0.1:50502', '{\"slave_id\": 1, \"timeout_ms\": 3000, \"unit_id\": 1}', 1);"
MODBUS_DEV_ID=$(sqlite3 "$DB_PATH" "SELECT id FROM devices WHERE name='Simulator-PLC';")

# MQTT Device
sqlite3 "$DB_PATH" "INSERT INTO devices (tenant_id, site_id, protocol_id, name, device_type, endpoint, config, is_enabled) \
VALUES (1, 1, 4, 'Simulator-MQTT', 'GATEWAY', 'localhost:1883', '{\"topic\": \"sensors/simulator/data\", \"qos\": 1}', 1);"
MQTT_DEV_ID=$(sqlite3 "$DB_PATH" "SELECT id FROM devices WHERE name='Simulator-MQTT';")

echo "ID 확인: MODBUS=$MODBUS_DEV_ID, MQTT=$MQTT_DEV_ID"

echo "📍 Inserting Data Points..."
# Modbus Points (Address 1003, 1004)
sqlite3 "$DB_PATH" "INSERT INTO data_points (device_id, name, address, data_type, scaling_factor, is_enabled, log_enabled) \
VALUES ($MODBUS_DEV_ID, 'Sim_Temp', 1004, 'FLOAT32', 0.1, 1, 1);"
TEMP_POINT_ID=$(sqlite3 "$DB_PATH" "SELECT id FROM data_points WHERE name='Sim_Temp' AND device_id=$MODBUS_DEV_ID;")

sqlite3 "$DB_PATH" "INSERT INTO data_points (device_id, name, address, data_type, scaling_factor, is_enabled, log_enabled) \
VALUES ($MODBUS_DEV_ID, 'Sim_Current', 1003, 'FLOAT32', 0.1, 1, 1);"
CURR_POINT_ID=$(sqlite3 "$DB_PATH" "SELECT id FROM data_points WHERE name='Sim_Current' AND device_id=$MODBUS_DEV_ID;")

# MQTT Points (address=0, address_string=topic)
sqlite3 "$DB_PATH" "INSERT INTO data_points (device_id, name, address, address_string, data_type, mapping_key, is_enabled, log_enabled) \
VALUES ($MQTT_DEV_ID, 'Sim_MQTT_Temp', 0, 'sensors/simulator/data', 'FLOAT32', 'temp', 1, 1);"
MQTT_TEMP_POINT_ID=$(sqlite3 "$DB_PATH" "SELECT id FROM data_points WHERE name='Sim_MQTT_Temp' AND device_id=$MQTT_DEV_ID;")

echo "DataPoint ID 확인: Temp=$TEMP_POINT_ID, Current=$CURR_POINT_ID, MqttTemp=$MQTT_TEMP_POINT_ID"

echo "🔔 Inserting Alarm Rules..."
# Modbus Alarms
sqlite3 "$DB_PATH" "INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, high_limit, severity, is_enabled) \
VALUES (1, 'Sim_Temp_High', 'data_point', $TEMP_POINT_ID, 'analog', 35.0, 'high', 1);"

sqlite3 "$DB_PATH" "INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, high_limit, severity, is_enabled) \
VALUES (1, 'Sim_Current_Overload', 'data_point', $CURR_POINT_ID, 'analog', 30.0, 'critical', 1);"

# MQTT Alarm
if [ ! -z "$MQTT_TEMP_POINT_ID" ]; then
    sqlite3 "$DB_PATH" "INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, high_limit, severity, is_enabled) \
    VALUES (1, 'Sim_MQTT_High_Temp', 'data_point', $MQTT_TEMP_POINT_ID, 'analog', 40.0, 'high', 1);"
else
    echo "⚠️  Skipping MQTT Alarm: Sim_MQTT_Temp point ID not found"
fi

echo "🧮 Inserting Virtual Points..."
# VP 1: Modbus Temp * 2 (식: formula, 간격: calculation_interval, 범위: site)
sqlite3 "$DB_PATH" "INSERT INTO virtual_points (tenant_id, site_id, scope_type, name, formula, data_type, is_enabled, calculation_interval) \
VALUES (1, 1, 'site', 'Sim_VP_Double_Temp', 'getValue(\"Sim_Temp\") * 2', 'float', 1, 5000);"

# VP 2: Modbus Temp + MQTT Temp
sqlite3 "$DB_PATH" "INSERT INTO virtual_points (tenant_id, site_id, scope_type, name, formula, data_type, is_enabled, calculation_interval) \
VALUES (1, 1, 'site', 'Sim_VP_MQTT_Total', 'getValue(\"Sim_Temp\") + getValue(\"Sim_MQTT_Temp\")', 'float', 1, 5000);"

echo "✅ Test environment setup complete!"
