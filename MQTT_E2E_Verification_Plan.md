# PulseOne MQTT End-to-End Verification Plan (Final)

**Topology**: Simulator (Client) -> RabbitMQ (Broker) -> Collector (Client/Worker)

**Objective**: Verify the complete data pipeline including **Auto-Discovery**, **File Chunking (Split Transfer)**, **Alarm Generation**, and **External S3 Export**.

## Phase 1: Environment & Setup (Clean State)
- [x] **1.1. Database Cleanup & Topology Setup**:
    - [x] Clear existing `data_points` for device 300.
    - [x] **Modbus (ID 2, 12346)** -> Assign to **Edge Server 1**.
    - [x] **MQTT (ID 300)** -> Assign to **Edge Server 6**.
    - [x] **Reset Identities**: Set `instance_key` to NULL for Edge Servers 1 and 6.
- [ ] **1.2. Multi-Instance Startup (Zero-Config)**:
    - [ ] Start **Collector A** (Standard path) -> Verify it claims **ID 1** (Modbus).
    - [ ] Start **Collector B** (Unique path to differentiate hash) -> Verify it claims **ID 6** (MQTT).
    - [ ] **Verify**: Check `startup_vfinal.log` for both to ensure they load correct workers.



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
