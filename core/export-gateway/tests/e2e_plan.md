# Export Gateway E2E Test Plan (Detailed)

This document provides a comprehensive walkthrough and rationale for verifying the PulseOne end-to-end data export pipeline in a Docker environment.

## 1. Environment & Architecture Overview

Before testing, it's essential to understand the components involved and their communication paths.

*   **Modbus Simulator (`docker-simulator-modbus-1`)**: Acts as the physical PLC hardware. It maintains registers that the Collector reads.
*   **Collector (`docker-collector-1`)**: The acquisition engine. It polls the simulator via Modbus TCP, evaluates data against alarm rules, and persists state to Redis and SQLite.
*   **Redis (`docker-redis-1`)**: The high-speed event bus. It enables real-time communication between the Collector (publisher) and the Export Gateway (subscriber).
*   **Export Gateway (`pulseone-export-gateway`)**: The dispatch engine. It listens for events on Redis, applies payload transformations (templates), and sends data to external targets (HTTP/S3).

| Resource | Value | Rationale |
| :--- | :--- | :--- |
| **Simulator IP** | `172.18.0.10` | Internal bridge IP for container-to-container communication. |
| **Gateway ID** | `6` | Specified ID for the instance to test isolation and assignment logic. |
| **Database** | `./data/db/pulseone.db` | Shared volume ensuring all containers see the same configuration. |

---

## 2. Phase 1: Atomic Reset (The "Clean Slate" Protocol)

Verification requires observing a clear **state transition** (e.g., Normal -> Alarm). If the system is already in an alarm state, the edge-detection logic may not fire.

```bash
# 1. Stop processing to prevent race conditions during reset
docker stop docker-collector-1 pulseone-export-gateway

# 2. Wipe volatile and recent persistence
# - alarm_occurrences: Deletes previous incidents.
# - current_values: Resets the Collector's memory to 'Normal'.
# - export_logs: Clears history to ensure logs for THIS test are obvious.
docker exec docker-backend-1 sqlite3 /app/data/db/pulseone.db \
"DELETE FROM alarm_occurrences; \
 UPDATE current_values SET current_value = '{\"value\":0.0}', alarm_state = 'normal', alarm_active = 0; \
 DELETE FROM export_logs;"
```

---

## 3. Phase 2: Configuration (Building the Scenario)

We manually inject the test scenario directly into the database. This bypasses potential UI bugs and ensures precision.

### 3.1 Resource Registry (SQL)
```sql
-- Register Device (PLC)
-- Ensures the Collector knows where to poll. edge_server_id=1 maps to docker-collector-1.
INSERT OR REPLACE INTO devices (id, tenant_id, site_id, name, device_type, protocol_id, endpoint, edge_server_id, is_enabled, config)
VALUES (200, 1, 1, 'E2E-PLC', 'PLC', 1, '172.18.0.10:50502', 1, 1, '{"slave_id": 1}');

-- Register Data Point (Sensor)
-- Address 100 corresponds to Coil 100 in the simulator.
INSERT OR REPLACE INTO data_points (id, device_id, name, address, data_type, is_enabled, alarm_enabled)
VALUES (200, 200, 'E2E.ALARM', 100, 'BOOL', 1, 1);

-- Register Alarm Rule (Trigger)
-- digital/on_true: Fires as soon as the point value becomes 1.0 (True).
INSERT OR REPLACE INTO alarm_rules (id, tenant_id, name, point_id, rule_type, condition, severity, is_enabled)
VALUES (200, 1, 'E2E_ALARM_RULE', 200, 'digital', 'on_true', 'critical', 1);

-- Map to Export Targets
-- Maps the point to existing HTTP (18) and S3 (19) targets.
-- target_field_name: The key used in the exported JSON payload.
INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, site_id, target_field_name, is_enabled)
VALUES 
(18, 200, 1, 'ENVS_TEST.E2E_POINT', 1),
(19, 200, 1, 'ENVS_TEST.E2E_POINT', 1);
```

---

## 4. Phase 3: Execution & Monitoring (The Audit Trail)

We monitor the logs sequentially to trace the "life of a data point."

### Step 1: Start & Stabilization
```bash
docker start docker-collector-1 pulseone-export-gateway
```
*Rationale: Ensure the Collector establishes a connection to the simulator and starts polling 0.0.*

### Step 2: External Trigger (Simulating a Fault)
```bash
docker exec docker-simulator-modbus-1 node scripts/force_modbus.js 100 1
```
*Action: Changes the virtual sensor from 0 to 1.*

### Step 3: Audit the Chain
1.  **Collector Acquisition**: 
    Look for: `Read Point 200 (E2E.ALARM) -> Value: true`.
    *Verification: Did it actually see the change?*
2.  **Alarm Engine**: 
    Look for: `eval.should_trigger: 1` and `Publishing alarm event to Redis...`.
    *Verification: Did it detect the violation?*
3.  **Redis Dispatch**: 
    Use: `docker exec docker-redis-1 redis-cli MONITOR`.
    Look for: `PUBLISH "alarms:all"`.
    *Verification: Is the event on the wire?*
4.  **Gateway Reception**: 
    Look for: `[INFO] Received alarm event for rule_id: 200`.
    *Verification: Did the Gateway hear the message?*
5.  **Target Dispatch**: 
    Look for: `Target result: [HTTP Target] success=1`.
    *Verification: Did it reach the final destination?*

---

## 5. Phase 4: Data Content Verification (Final Audit)

Finally, we verify that the **content** of the exported data is correct.

1.  **Timestamp**: Check if `tm` is in `YYYY-MM-DD HH:mm:ss` format (KST).
2.  **Identification**: Check if `bd` (site ID) matches `1`.
3.  **Mapping**: Check if the field name is `ENVS_TEST.E2E_POINT`.
4.  **Persistence**: Check the database for the final log.
    ```sql
    SELECT * FROM export_logs ORDER BY id DESC LIMIT 1;
    ```
