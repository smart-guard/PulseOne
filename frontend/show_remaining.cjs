/**
 * 남은 UI 컴포넌트 한글 추출 - console 제외, JSX/message만
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/components/modals/DeviceDetailModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx',
    '/app/src/components/modals/DeviceTemplateWizard.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
    '/app/src/components/modals/DeviceModal/DataPointModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx',
    '/app/src/components/protocols/ProtocolDashboard.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
    '/app/src/pages/export-gateway/tabs/GatewayListTab.tsx',
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
        if (l.includes('console.')) return; // console 제외
        found.push((i + 1) + ': ' + tr.substring(0, 90));
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 12).forEach(l => console.log(l));
        if (found.length > 12) console.log('  ... +' + (found.length - 12) + ' more');
    }
}
