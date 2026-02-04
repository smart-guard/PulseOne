# PulseOne MQTT End-to-End Verification Plan (Final)

**Topology**: Simulator (Client) -> RabbitMQ (Broker) -> Collector (Client/Worker)

**Objective**: Verify the complete data pipeline including **Auto-Discovery**, **File Chunking (Split Transfer)**, **Alarm Generation**, and **External S3 Export**.

## Phase 1: Environment & Setup (Clean State)
- [ ] **1.1. Database Cleanup**:
    - Ensure `pulseone.db` is clean (restored from backup).
    - **Crucial**: Ensure NO manual `data_points` exist for the test device initially.
- [ ] **1.2. Register MQTT Device**:
    - Create Device ID 300 (`MQTT-Test-Device`) in `devices` table.
    - Config: Endpoint `tcp://rabbitmq:1883`, Topics `vfd/#`.
    - **Verify**: Restart Collector and check logs for "Worker 300 started".

## Phase 2: Auto-Discovery Verification
- [ ] **2.1. Execute Simulator (Chunked)**:
    - Run `mqtt_chunked_simulator.js` (connects to RabbitMQ).
    - Publishes:
        - `vfd/data` (Telemetry JSON)
        - `vfd/file` (Split File Chunks)
- [ ] **2.2. Verify Point Creation**:
    - Check `data_points` table in DB.
    - **Expectation**: Points for `vfd/data` and `vfd/file` should appear automatically.

## Phase 3: Configuration (Alarm & Export)
- [ ] **3.1. Configure Alarm**:
    - Insert Alarm Rule for the *newly discovered* point `vfd/data`.
    - Condition: Value > 150.
- [ ] **3.2. Configure Export**:
    - Insert `export_target_mappings` linking the Alarm to S3 Target (ID 19).
    - **Action**: Restart Collector & Export Gateway to load rules.

## Phase 4: Full End-to-End Execution
- [ ] **4.1. Re-Run Simulator**:
    - Execute `mqtt_chunked_simulator.js` again.
- [ ] **4.2. Verify Collector Actions**:
    - **Reassembly**: Log confirmation of "File reassembly complete".
    - **Alarm**: Log confirmation of "Alarm Triggered" (Value > 150).
    - **Metadata**: Confirm `file_ref` is attached to the alarm event.
- [ ] **4.3. Verify Export Actions**:
    - **Dispatch**: Export Gateway receives the alarm event.
    - **Upload**: Export Gateway logs "S3 Upload Success".
    - **S3 Verification**: (Optional) Check S3 bucket if credentials allow.
