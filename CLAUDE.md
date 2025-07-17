# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PulseOne is a Node.js-based IoT data collection system backend that handles REST APIs, database storage, user management, and data processing. The system uses Express.js with multiple database connections including Redis, PostgreSQL, InfluxDB, and supports message queuing with RabbitMQ.

**Note**: This codebase does NOT contain the following components mentioned in typical IoT systems:
- `/collector/` directory with C++ code (not present)
- `/driver/` folder with plugin architecture (not present) 
- `docker-compose.dev.yml` (not present)
- Virtual point processing logic (not found)
- Mobile app related code or APIs (not found)

## Key Architecture

### Directory Structure
- `/backend/` - Main Node.js application with Express server
- `/config/` - Centralized configuration with environment-specific settings
- `/secrets/` - Sensitive data (git-ignored)

### Core Components

#### Backend Application (`/backend/`)
- **Main App**: `app.js` - Express application with middleware setup and route configuration
- **Database Connections**: `lib/connection/` - Modular database connection handlers
  - Redis (primary/secondary with replicas)
  - PostgreSQL (main/log databases)  
  - InfluxDB for time-series data
  - SQLite, MariaDB support
  - RPC and Message Queue connections
- **Routes**: `routes/` - API endpoints
  - `health.js` - Health check endpoints
  - `api.js` - Main API endpoints with database test routes
  - `user.js` - User management endpoints
- **Tests**: `__tests__/` - Jest test suite with database and API tests
- **Scripts**: `scripts/validate-env.js` - Environment validation utility

#### Configuration System (`/config/`)
- **Environment Loader**: `env.js` - dotenv-safe configuration with validation
- **Config Index**: `index.js` - Centralized configuration aggregator
- **Environment Files**: Stage-specific `.env` files (dev, staging, prod, etc.)
- **Config JSON**: `config.json` - Static service configuration

### Database Architecture
The system implements a sophisticated multi-database strategy:
- **Redis**: Primary/secondary instances with replica support for caching and sessions
- **PostgreSQL**: Dual database setup (main application data + separate logging database)
- **InfluxDB**: Time-series IoT metrics storage
- **Message Queue**: RabbitMQ for asynchronous data processing
- **Fallback**: SQLite support for development/testing

### Docker Setup
- **Production**: `backend/Dockerfile` - Standard Node.js container
- **Development**: `backend/Dockerfile.dev` - Enhanced with Claude Code CLI and development tools
- Container names follow `pulseone-*` convention (pulseone-postgres, pulseone-redis, etc.)

### Connection Management
Database connections use a factory pattern with graceful error handling:
- Asynchronous initialization via `initializeConnections()` in app.js
- Available to routes through `app.locals.getDB()`
- Connection pools and retry logic implemented
- Environment-specific configuration per database type

## Development Commands

```bash
# Install dependencies
cd backend && npm install

# Start the application
npm start

# Run tests (sequential execution to avoid DB conflicts)
npm test

# Run individual test
npx jest __tests__/specific-test.test.js

# Validate environment configuration
node scripts/validate-env.js
```

## Configuration Management

### Environment Variables
- **Stage Selection**: `ENV_STAGE` environment variable (defaults to 'dev')
- **File Pattern**: `/config/.env.{stage}` (e.g., `.env.dev`, `.env.prod`)
- **Validation**: Required vs optional environment variables enforced
- **Security**: Uses dotenv-safe with `.env.example` validation

### Required Environment Variables
- Database connections (PostgreSQL main/log, Redis main/secondary)
- InfluxDB configuration (host, token, org, bucket)
- RPC server settings
- Application port

### Docker Integration
Environment files reference Docker container names:
- `pulseone-postgres` - PostgreSQL database
- `pulseone-redis` - Redis cache
- `pulseone-influx` - InfluxDB time-series
- `pulseone-rabbitmq` - Message queue

## Testing Strategy

Tests are located in `backend/__tests__/` using Jest + Supertest:
- **Database Tests**: Connection validation for all database types
- **API Tests**: Route testing with mocked database connections
- **Integration Tests**: Full application flow testing
- **Time-series Tests**: InfluxDB data writing and querying
- **Message Queue Tests**: RabbitMQ publish/subscribe patterns

**Important**: Tests run sequentially (`--runInBand`) to prevent database connection conflicts.