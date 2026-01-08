const path = require('path');
const { DatabaseFactory } = require('../lib/database/DatabaseFactory');
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');

async function verify() {
    console.log('🔍 Backend Verification Start...');

    try {
        const repo = new DeviceRepository();

        // 1. Get all protocols
        const protocols = await repo.getAvailableProtocols(1);
        console.log(`✅ Protocols found: ${protocols.length}`);
        if (protocols.length > 0) {
            console.log('   Sample Protocol:', {
                type: protocols[0].protocol_type,
                uses_serial: protocols[0].uses_serial,
                requires_broker: protocols[0].requires_broker
            });
        }

        // 2. Get data points for a device (if any)
        const devicesResult = await repo.findAllDevices({ tenantId: 1, limit: 1 });
        const device = devicesResult.items[0];

        if (device) {
            console.log(`✅ Device found: ${device.name} (ID: ${device.id})`);
            const points = await repo.getDataPointsByDevice(device.id, 1);
            console.log(`✅ Data points found: ${points.length}`);
            if (points.length > 0) {
                console.log('   Sample Point Boolean Flags:', {
                    is_log_enabled: points[0].is_log_enabled,
                    is_quality_check_enabled: points[0].is_quality_check_enabled
                });
            }
        } else {
            console.warn('⚠️ No devices found in seed data.');
        }

        console.log('🎉 Verification Successful');
    } catch (error) {
        console.error('❌ Verification Failed:', error);
    }
}

verify();
