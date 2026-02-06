# Refactoring Plan: Export Gateway CSP Module

This plan outlines the refactoring of the `core/export-gateway/src/CSP` directory to address technical debt, reduce class bloating, and improve project-wide consistency.

## Engagement Rules & Workflow

**1. Verification First**: I will thoroughly analyze and verify existing code before proposing any new code or refactors.
**2. Reference Check**: I will reference the C# code in `/Users/kyungho/Project/icos5` (especially `iCos5CSPGateway`) to ensure feature parity, focusing on whether HTTP target logic differs between AWS and other systems.
**3. Explicit Approval Required**: I will **ALWAYS** ask for your explicit permission before applying any code changes.
**4. Prioritize Communication**: If you ask a question while I am analyzing, compiling, or performing any task, I will immediately stop my current activity and provide a clear answer first. No technical task takes precedence over your inquiries.

## Environment & Infrastructure

- **Database Source of Truth**: `config/database.env` (Always resolve the DB path from this environment file. Do NOT hardcode or use other database paths.)
- **Expected DB Path**: `data/db/pulseone.db` (Verification required against `config/database.env`).
- **Execution Constraint**: All build and run commands **MUST** be executed within the Docker container. No commands are to be run directly on the Mac host terminal.

## Current Issues

- **Bloated Classes**: `ExportCoordinator` (>1600 lines) and `DynamicTargetManager` handle too many responsibilities (DB, Redis, business logic, threading).
- **Singleton Overuse**: Massive use of singletons (`DynamicTargetManager`, `PayloadTransformer`, etc.) makes unit testing difficult.
- **Inconsistent Namespaces**: Mixing `PulseOne::CSP`, `PulseOne::Coordinator`, `PulseOne::Export`, and `PulseOne::Transform`.
- **Code Duplication**: Template expansion logic is scattered across different handlers and the transformer.
- **Resource Management**: Each handler and manager often manages its own threads, leading to potential resource explosion.

## Proposed Strategy

### 1. Architectural Reorganization

We will move from a monolithic coordinator to a service-oriented architecture.

#### [NEW] Namespace Hierarchy
- `PulseOne::Gateway`: Root namespace for the export gateway.
- `PulseOne::Gateway::Model`: Data structures like `AlarmMessage`.
- `PulseOne::Gateway::Target`: Target management and handler implementations.
- `PulseOne::Gateway::Service`: Core services like `EventDispatcher`, `HeartbeatService`, `BatchManager`.

#### Decomposition of `ExportCoordinator`
- **`EventDispatcher`**: Manages Redis Subscriptions and routes events to handlers.
- **`HeartbeatService`**: Handles DB and Redis heartbeats for the gateway.
- **`BatchManager`**: Handles accumulation and flushing of alarm/value batches.

#### Decomposition of `DynamicTargetManager`
- **`TargetRegistry`**: Pure storage and lookup for `DynamicTarget` entities.
- **`TargetHandlerFactory`**: Dedicated factory for creating target handlers.
- **`TargetRunner`**: Execution logic for sending data to targets, incorporating `FailureProtector`.

### 2. Dependency Injection

- Remove `getInstance()` calls from internal logic.
- Introduce `GatewayContext` to hold and provide service instances.
- Pass dependencies via constructors to facilitate mocking in unit tests.

### 3. Logic Centralization

- **Template Expansion**: All handlers must use `PayloadTransformer::transform`. Remove local `expandVariables` implementations from handlers (e.g., `MqttTargetHandler`).
- **Standardized Logging**: Use a unified `MetricService` for tracking successes/failures.

### 4. Concurrency Model

- Implement a shared `TaskRunner` (Worker Pool).
- Eliminate per-handler threads where possible.
- Use asynchronous non-blocking patterns for network operations (as already partially done with Paho MQTT).

## Proposed Changes (Incremental)

### Phase 1: Foundation & Cleanup
- Standardize all files to use `PulseOne::Gateway` namespace (aliasing older namespaces for backward compatibility).
- Move `AlarmMessage` and `ValueMessage` to a central `Model` sub-namespace.

### Phase 2: Target Management Refactor
- Split `DynamicTargetManager` into `TargetRegistry` and `TargetRunner`.
- Move `FailureProtector` logic into a decorator or a dedicated interceptor in `TargetRunner`.

### Phase 3: Coordinator Decomposition
- Extract `HeartbeatService` from `ExportCoordinator`.
- Extract `EventDispatcher` (handling selective/all subscriptions).
- Extract `BatchingEngine`.

### Phase 4: Handler Refinement
- Create `BaseTargetHandler` to handle common tasks (Initial validation, status reporting).
- Standardize all configuration loading and variable expansion via `PayloadTransformer`.

## Strict Quality Control & Regression Prevention

> [!IMPORTANT]
> **Refactoring must NOT break existing features.** Every change must be meticulously cross-referenced with the original implementation.

1. **Mandatory Analysis & Verification**: Analyze the target code and its dependencies thoroughly before proposing changes.
2. **Approval Gate**: Present the specific code diff and wait for your permission before applying it.
3. **Feature Parity Audit**: For every new class or service extracted, the original logic must be reviewed three times to ensure no line of code or edge-case handling is omitted.
4. **Side-by-Side Comparison**: Before deleting any legacy code, perform a direct comparison between the new implementation and the old one.
5. **No-Gap Implementation**: Ensure all specific patches and bug fixes added over time (e.g., v6.2.2 fixes in `DynamicTargetManager`) are preserved in the new structure.

## Verification Plan

### Automated Tests
- Create unit tests for decomposed components using Mock objects for `ITargetHandler` and `IRepository`.
- Run existing integration tests in `tests/integration` to ensure no regression in end-to-end functionality.
- **Verification MUST run within Docker.**

### Manual Verification
- Monitor Redis heartbeat and status logs to ensure `HeartbeatService` functions correctly.
- Verify that manual export commands still trigger correct target sends.
- Check log files for consistent formatting across all services.
