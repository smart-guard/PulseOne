/**
 * 남은 파일 전수 직접 추출
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointDetailModal.tsx',
    '/app/src/pages/Dashboard.tsx',
    '/app/src/pages/DataExport.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/pages/RealTimeMonitor.tsx',
    '/app/src/components/modals/DeviceModal/DeviceSettingsTab.tsx',
    '/app/src/components/modals/TemplateImportExportModal.tsx',
    '/app/src/pages/HistoricalData.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointModal.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    lines.forEach((l, i) => {
        if (!koreanRe.test(l)) return;
        found.push((i + 1) + ': ' + l.trim().substring(0, 80));
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 8).forEach(l => console.log(l));
        if (found.length > 8) console.log('  ... +' + (found.length - 8));
    }
}
