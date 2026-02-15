# Core Architecture Restoration & ROS Export Fix Plan

## 1. Current Status (Completed)

- [x] **AlarmEngine Reversion**: Removed `getSiteIdForPoint` and all `site_id` enforcement logic from `AlarmEngine.cpp/h`.
- [x] **Database Restoration**: Reverted ROS Device (Mock Robot Arm, ID: 19001) `site_id` to its original value of **1**.
- [x] **Root Cause Analysis**: Identified that ROS export failure was not due to the Alarm Engine, but a configuration gap in Point-Based mapping/subscription within the Gateway.

## 2. Immediate Action Items (Next Steps)

### Phase 1: Environment Stabilization
- [ ] **Zombie Cleanup**: Kill all residual `pulseone-collector`, `export-gateway`, and `ros_simulator.js` processes in Docker containers.
- [ ] **Service Restart**: Restart Collector and Gateway services with clean configurations.

### Phase 2: Point-Based Verification Setup (User Managed)
- [ ] **Verification Target (ID: 9001)**: Create a dedicated HTTP verification target in the database.
- [ ] **Point Mappings**:
    - [ ] ROS: Point `1900101` -> `battery_voltage`
    - [ ] MQTT: Point `2017` -> `mqtt_point`
    - [ ] Modbus: Point `5` -> `modbus_point`

### Phase 3: E2E Verification
- [ ] **Trigger ROS Alarm**: Run `ros_simulator.js` to trigger an alarm for point `1900101`.
- [ ] **Verify Gateway 12 Logs**: Confirm the alarm is received via Redis and successfully exported to Target 9001.
- [ ] **Trigger MQTT/Modbus Alarms**: Verify the same point-based flow for other protocols.

## 3. Future Maintenance & Safety
- **Strict Adherence**: No further `site_id` hacks in the C++ core. All export routing must be managed via `export_target_mappings`.
- **Documentation**: Update `task.md` and `walkthrough.md` with the final verification results.
