/**
 * ExportGatewayWizard + TemplateManagementTab + MqttBrokerDashboard + ScheduleManagementTab + DeviceDetailModal 잔여 v27
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
    '/app/src/pages/export-gateway/tabs/ScheduleManagementTab.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    lines.forEach((l, i) => {
        const tr = l.trim();
        if (!koreanRe.test(l)) return;
        if (tr.startsWith('//') || tr.startsWith('*')) return;
        if (l.includes("t('") || l.includes('t("')) return;
        if (l.includes('console.')) return;
        found.push((i + 1) + ': ' + tr.substring(0, 85));
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 12).forEach(l => console.log(l));
        if (found.length > 12) console.log('  ... +' + (found.length - 12));
    }
}
