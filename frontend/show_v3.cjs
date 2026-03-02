/**
 * DeviceRtuMonitorTab + DeviceDataPointsBulkModal + ExportGatewayWizard 잔여 한글 추출
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx',
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/components/modals/DeviceTemplateWizard.tsx',
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
        found.slice(0, 15).forEach(l => console.log(l));
        if (found.length > 15) console.log('  ... +' + (found.length - 15) + ' more');
    }
}
