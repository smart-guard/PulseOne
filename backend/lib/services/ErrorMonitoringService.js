// =============================================================================
// backend/lib/services/ErrorMonitoringService.js
// =============================================================================

const BaseService = require('./BaseService');

/**
 * Service class for Error Monitoring and Statistics.
 */
class ErrorMonitoringService extends BaseService {
    constructor() {
        super(null);
        // In-memory storage moved from route
        this.errorStats = {
            totalErrors: 0,
            errorsByCategory: {},
            errorsByDevice: {},
            errorHistory: [],
            recoveryStats: {
                autoRecoveryAttempts: 0,
                autoRecoverySuccess: 0,
                manualInterventionRequired: 0
            }
        };
    }

    /**
     * Get error statistics.
     */
    async getStatistics() {
        return await this.handleRequest(async () => {
            const last24h = this.errorStats.errorHistory.filter(error =>
                Date.now() - new Date(error.timestamp).getTime() < 24 * 60 * 60 * 1000
            );

            return {
                total_errors: this.errorStats.totalErrors,
                last_24h_errors: last24h.length,
                error_distribution: this._getCategoryDistribution(),
                device_error_summary: this._getDeviceErrorSummary()
            };
        }, 'ErrorMonitoringService.getStatistics');
    }

    /**
     * Get recent errors with filtering.
     */
    async getRecentErrors(limit = 50, category = null, deviceId = null) {
        return await this.handleRequest(async () => {
            let filtered = [...this.errorStats.errorHistory];
            if (category) filtered = filtered.filter(e => e.category === category);
            if (deviceId) filtered = filtered.filter(e => e.device_id === deviceId);

            return filtered.slice(0, limit);
        }, 'ErrorMonitoringService.getRecentErrors');
    }

    /**
     * Report a new error.
     */
    async reportError(errorData) {
        return await this.handleRequest(async () => {
            const errorRecord = {
                id: Date.now() + Math.random(),
                ...errorData,
                timestamp: new Date().toISOString()
            };

            this.errorStats.totalErrors++;
            this.errorStats.errorHistory.unshift(errorRecord);

            // Limit history
            if (this.errorStats.errorHistory.length > 1000) {
                this.errorStats.errorHistory = this.errorStats.errorHistory.slice(0, 1000);
            }

            // Categorize
            if (errorData.category) {
                if (!this.errorStats.errorsByCategory[errorData.category]) {
                    this.errorStats.errorsByCategory[errorData.category] = [];
                }
                this.errorStats.errorsByCategory[errorData.category].push(errorRecord);
            }

            // Device mapping
            if (errorData.device_id) {
                if (!this.errorStats.errorsByDevice[errorData.device_id]) {
                    this.errorStats.errorsByDevice[errorData.device_id] = [];
                }
                this.errorStats.errorsByDevice[errorData.device_id].push(errorRecord);
            }

            return errorRecord;
        }, 'ErrorMonitoringService.reportError');
    }

    // -- Internal methods --

    _getCategoryDistribution() {
        const dist = {};
        Object.entries(this.errorStats.errorsByCategory).forEach(([cat, list]) => {
            dist[cat] = list.length;
        });
        return dist;
    }

    _getDeviceErrorSummary() {
        return Object.entries(this.errorStats.errorsByDevice)
            .map(([deviceId, errors]) => ({
                device_id: deviceId,
                error_count: errors.length,
                last_error: errors[0]?.timestamp
            }))
            .sort((a, b) => b.error_count - a.error_count)
            .slice(0, 10);
    }
}

module.exports = new ErrorMonitoringService();
